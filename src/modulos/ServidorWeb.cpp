/**
 * ============================================================================
 * ARCHIVO     : ServidorWeb.cpp
 * PROYECTO    : SISTEMA DE RIEGO AUTOMÁTICO
 * ----------------------------------------------------------------------------
 * RESPONSABILIDAD DEL MÓDULO:
 *   Implementa la gestión cooperativa Wi-Fi/NTP/Open-Meteo y los endpoints
 *   REST del servidor HTTP local. Incluye "credenciales.h" en esta única
 *   unidad de compilación para mantener privadas las claves Wi-Fi y evitar
 *   definiciones duplicadas en el enlazador C++.
 *
 * NOTA TÉCNICA SOBRE SEGURIDAD EN RED LOCAL:
 *   Las peticiones HTTP POST verifican pertenencia a una subred LAN privada
 *   (RFC 1918) y presencia de la cabecera "X-Control-Local: ESP32". Al ser
 *   enviada desde JavaScript en el navegador, dicha cabecera es visible para
 *   el cliente y actúa únicamente como barrera básica frente a solicitudes
 *   cruzadas accidentales (CSRF simple) en una red local confiable, no como
 *   autenticación criptográfica de usuarios.
 * ============================================================================
 */

#include "ServidorWeb.h"
#include "DashboardWeb.h"
#include "Sensores.h"
#include "ControlRiego.h"
#include "Pantalla.h"
#include "credenciales.h"

#include <WiFi.h>
#include <WebServer.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <time.h>

static WebServer     server(80);
static bool          servidorWebIniciado  = false;
static unsigned long ultimoChequeoWiFiMs  = 0;
static unsigned long ultimoChequeoClimaMs = 0;

bool ntpSincronizado = false;

/**
 * Consulta meteorológica cooperativa a Open-Meteo:
 *   - Configura tanto setConnectTimeout(1500) como setTimeout(1500) sobre un
 *     WiFiClient explícito (sin asumir un límite absoluto garantizado si la
 *     resolución DNS síncrona de LwIP añade espera cuando falta Internet).
 *   - Nunca se ejecuta durante ciclos activos de riego ni pausas de 10 s de
 *     infiltración (cicloHidraulicoEnCurso() == true).
 */
void consultarOpenMeteo() {
  if (WiFi.status() != WL_CONNECTED || cicloHidraulicoEnCurso()) return;

  WiFiClient clienteTCP;
  HTTPClient http;
  if (!http.begin(clienteTCP, WEATHER_URL)) {
    return;
  }
  http.setConnectTimeout(HTTP_TIMEOUT_CLIMA_MS);
  http.setTimeout(HTTP_TIMEOUT_CLIMA_MS);

  int httpCode = http.GET();

  if (httpCode == HTTP_CODE_OK) {
    JsonDocument doc;
    DeserializationError err = deserializeJson(doc, http.getString());
    if (!err && !doc["current"]["weather_code"].isNull()) {
      int wmo = doc["current"]["weather_code"].as<int>();
      if (wmo >= 0 && wmo <= 99) {
        climaCodigoWMO      = wmo;
        climaEsDeDia        = (doc["current"]["is_day"] | 1) == 1;
        climaRemotoRecibido = true;
        ultimaSyncClimaMs   = millis();
        calcularCompensacionClimatica();
      }
    }
  }
  http.end();
}

/** Inicia la conexión Wi-Fi en modo estación y registra las rutas del servidor HTTP. */
void iniciarRedYServidorWeb() {
  WiFi.mode(WIFI_STA);
  WiFi.begin(SSID_1, PASS_1);
  ultimoChequeoWiFiMs = millis();
  configurarServidorWeb();
}

/** Atiende de forma no bloqueante las peticiones entrantes en el puerto 80. */
void atenderClientesWeb() {
  if (servidorWebIniciado) {
    server.handleClient();
  }
}

/** Supervisa cada 15 s el enlace Wi-Fi/NTP y cada 10 min el clima externo en reposo. */
void gestionarWiFiYClima() {
  unsigned long ahora = millis();

  // Si acaba de obtener enlace Wi-Fi, arranca de inmediato el servidor HTTP local
  if (!servidorWebIniciado && WiFi.status() == WL_CONNECTED) {
    server.begin();
    servidorWebIniciado = true;
    configTime(GMT_OFFSET_SEC, DAYLIGHT_OFFSET_SEC, NTP_SERVER1, NTP_SERVER2);
  }

  if (ahora - ultimoChequeoWiFiMs >= INTERVALO_WIFI_MS) {
    ultimoChequeoWiFiMs = ahora;

    if (WiFi.status() == WL_CONNECTED) {
      if (!ntpSincronizado && !cicloHidraulicoEnCurso()) {
        struct tm horaLocal;
        if (getLocalTime(&horaLocal, 15)) {
          ntpSincronizado = true;
          consultarOpenMeteo();
          ultimoChequeoClimaMs = millis();
        }
      }
    } else if (!cicloHidraulicoEnCurso()) {
      static bool usarSegundaRed = false;
      usarSegundaRed = !usarSegundaRed;
      WiFi.disconnect();
      WiFi.begin(usarSegundaRed ? SSID_2 : SSID_1, usarSegundaRed ? PASS_2 : PASS_1);
    }
  }

  if (WiFi.status() == WL_CONNECTED && !cicloHidraulicoEnCurso() &&
      (ahora - ultimoChequeoClimaMs >= INTERVALO_CLIMA_API_MS)) {
    ultimoChequeoClimaMs = ahora;
    consultarOpenMeteo();
  }
}

/** Valida subred privada LAN (RFC 1918) y cabecera local X-Control-Local: ESP32. */
bool solicitudControlAutorizada() {
  IPAddress ip = server.client().remoteIP();
  bool esRedPrivada = (ip[0] == 10) ||
                      (ip[0] == 192 && ip[1] == 168) ||
                      (ip[0] == 172 && ip[1] >= 16 && ip[1] <= 31) ||
                      (ip[0] == 127);
  if (!esRedPrivada) {
    server.send(403, "text/plain", "Acceso permitido unicamente desde la red local privada");
    return false;
  }
  if (!server.hasHeader("X-Control-Local") || server.header("X-Control-Local") != "ESP32") {
    server.send(403, "text/plain", "Solicitud bloqueada: falta cabecera local X-Control-Local");
    return false;
  }
  return true;
}

/** Registra la ruta del Dashboard en PROGMEM y los endpoints REST JSON de telemetría y mando. */
void configurarServidorWeb() {
  const char* cabecerasRecolectadas[] = {"X-Control-Local"};
  server.collectHeaders(cabecerasRecolectadas, 1);

  server.on("/", HTTP_GET, []() {
    server.send_P(200, "text/html", INDEX_HTML);
  });

  server.on("/api/data", HTTP_GET, []() {
    unsigned long ahora = millis();
    char relojBuf[20];
    struct tm horaLocal;
    if (ntpSincronizado && getLocalTime(&horaLocal, 5)) {
      int h12 = horaLocal.tm_hour % 12;
      if (h12 == 0) h12 = 12;
      snprintf(relojBuf, sizeof(relojBuf), "%02d:%02d:%02d %s",
               h12, horaLocal.tm_min, horaLocal.tm_sec, (horaLocal.tm_hour >= 12) ? "PM" : "AM");
    } else {
      unsigned long s = ahora / 1000UL;
      snprintf(relojBuf, sizeof(relojBuf), "Activo %02lu:%02lu", (s / 60UL) % 100UL, s % 60UL);
    }

    JsonDocument doc;
    doc["deviceId"]       = "ESP32 DevKit V1";
    doc["ssid"]           = (WiFi.status() == WL_CONNECTED) ? WiFi.SSID() : "Sin red";
    doc["rssi"]           = (WiFi.status() == WL_CONNECTED) ? WiFi.RSSI() : 0;
    doc["ip"]             = (WiFi.status() == WL_CONNECTED) ? WiFi.localIP().toString() : "Sin IP";
    doc["clock"]          = relojBuf;
    doc["uptimeSec"]      = ahora / 1000UL;
    doc["freeHeap"]       = ESP.getFreeHeap();

    doc["soilValid"]      = humedadSueloValida;
    doc["soilPct"]        = humedadSueloPct;
    doc["soilAdc"]        = sueloAdcCrudo;
    doc["soilVolt"]       = (sueloAdcCrudo * 3.3f) / 4095.0f;
    doc["substrateText"]  = textoEstadoSustrato();
    doc["needsWater"]     = (humedadSueloValida && humedadSueloPct < umbralDinamicoInf);
    doc["uInf"]           = umbralDinamicoInf;
    doc["uSup"]           = obtenerUmbralSuperior();

    doc["stateText"]      = textoEstadoSistema();
    doc["autoMode"]       = modoAutomatico;
    doc["pumpOn"]         = bombaEncendida;
    doc["soaking"]        = (estadoRiego == ESTADO_INFILTRACION);
    doc["phaseSec"]       = obtenerSegundosFaseActual();
    doc["cycleSec"]       = obtenerSegundosTotalesCiclo();
    doc["dryRunSec"]      = tiempoAcumuladoBombeoMs / 1000UL;
    doc["contSec"]        = bombaEncendida ? (tiempoContinuoBombaMs / 1000UL) : 0UL;
    doc["pwmRaw"]         = potenciaBombaPWM;
    doc["pwmPct"]         = (potenciaBombaPWM * 100) / 255;
    doc["brightPwm"]      = brilloPantallaPWM;
    doc["alertCode"]      = (int)alertaActiva;
    doc["alertMsg"]       = textoAlertaActiva();

    doc["dhtValid"]       = dhtValido;
    doc["dhtState"]       = textoEstadoDHT();
    doc["dhtAgeSec"]      = dhtValido ? ((ahora - ultimaLecturaDHTMs) / 1000UL) : 0UL;
    doc["tempC"]          = dhtValido ? tempAmbienteC : 0.0f;
    doc["humPct"]         = dhtValido ? humAmbientePct : 0.0f;
    doc["hiApplicable"]   = dhtValido && indiceCalorAplicable;
    doc["heatIdxC"]       = (dhtValido && indiceCalorAplicable) ? indiceCalorC : 0.0f;
    doc["vpdKpa"]         = dhtValido ? dpvActualKpa : 0.0f;
    doc["vpdLabel"]       = textoNivelDPV();
    doc["vpdExplain"]     = textoExplicacionVPD();
    doc["useF"]           = usarFahrenheit;

    doc["weatherText"]    = textoClimaExterior();
    doc["weatherValid"]   = climaExteriorVigente();
    doc["weatherAgeMin"]  = climaRemotoRecibido ? ((ahora - ultimaSyncClimaMs) / 60000UL) : 0UL;
    doc["baseTh"]         = umbralBasePct;
    doc["hystPct"]        = histeresisPct;
    doc["climaCalcDelta"] = deltaClimaCalculadoPct;
    doc["climaDelta"]     = deltaClimaPct;
    doc["climaClamped"]   = (deltaClimaCalculadoPct != deltaClimaPct);
    doc["climaFactors"]   = motivoClimaFactores;
    doc["envSummary"]     = textoExplicacionAmbiental();
    doc["pulseMode"]      = riegoPorPulsos;
    doc["climaSmart"]     = climaSmartActivo;

    String respuesta;
    serializeJson(doc, respuesta);
    server.send(200, "application/json", respuesta);
  });

  server.on("/api/alert/reset", HTTP_POST, []() {
    if (!solicitudControlAutorizada()) return;
    const char* mensaje = nullptr;
    bool restablecida = rearmarAlertaPorUsuario(&mensaje);
    actualizarDatosPantallaTFT(true);
    server.send(restablecida ? 200 : 409, "text/plain",
                mensaje ? mensaje : (restablecida ? "Proteccion restablecida" : "No se pudo restablecer la alerta"));
  });

  server.on("/api/pump/toggle", HTTP_POST, []() {
    if (!solicitudControlAutorizada()) return;
    const char* mensaje = nullptr;
    bool ok = conmutarBombaManual(&mensaje);
    actualizarDatosPantallaTFT(true);
    server.send(ok ? 200 : 409, "text/plain",
                ok ? "OK" : (mensaje ? mensaje : "Operacion bloqueada por seguridad"));
  });

  server.on("/api/pump/mode", HTTP_POST, []() {
    if (!solicitudControlAutorizada()) return;
    const char* motivo = nullptr;
    if (!establecerModoOperacion(!modoAutomatico, &motivo)) {
      server.send(409, "text/plain", motivo ? motivo : "Cambio de modo bloqueado por seguridad");
      return;
    }
    actualizarDatosPantallaTFT(true);
    server.send(200, "text/plain", "OK");
  });

  server.on("/api/cfg/set", HTTP_POST, []() {
    if (!solicitudControlAutorizada()) return;
    if (!server.hasArg("param") || !server.hasArg("val")) {
      server.send(400, "text/plain", "Faltan argumentos param o val");
      return;
    }
    String valStr = server.arg("val");
    for (size_t i = 0; i < valStr.length(); i++) {
      if (!isDigit(valStr[i])) {
        server.send(400, "text/plain", "Valor de configuracion no numerico");
        return;
      }
    }
    String p = server.arg("param");
    int v = valStr.toInt();

    if (p == "mode" && (v == 0 || v == 1)) {
      const char* motivo = nullptr;
      if (!establecerModoOperacion(v == 1, &motivo)) {
        server.send(409, "text/plain", motivo ? motivo : "Cambio de modo bloqueado por seguridad");
        return;
      }
    } else if (p == "base" && (v == 30 || v == 40 || v == 55)) {
      umbralBasePct = v;
      calcularCompensacionClimatica();
      if (pestanaActual == PESTANA_AJUSTES) { dibujarBotonAjusteTFT(0); dibujarBotonAjusteTFT(1); }
    } else if (p == "hyst" && (v == 5 || v == 10 || v == 15)) {
      histeresisPct = v;
      if (pestanaActual == PESTANA_AJUSTES) dibujarBotonAjusteTFT(1);
    } else if (p == "pwm" && (v == 165 || v == 215 || v == 255)) {
      potenciaBombaPWM = (uint8_t)v;
      if (bombaEncendida) escribirPWMBomba(potenciaBombaPWM);
      if (pestanaActual == PESTANA_AJUSTES) dibujarBotonAjusteTFT(2);
    } else if (p == "pulse" && (v == 0 || v == 1)) {
      riegoPorPulsos = (v == 1);
      if (!riegoPorPulsos && estadoRiego == ESTADO_INFILTRACION) {
        finalizarCicloRiegoNormal();
      }
      if (pestanaActual == PESTANA_AJUSTES) dibujarBotonAjusteTFT(3);
    } else if (p == "bright" && (v == 90 || v == 165 || v == 230)) {
      brilloPantallaPWM = (uint8_t)v;
      aplicarBrilloPantalla(brilloPantallaPWM);
      if (pestanaActual == PESTANA_AJUSTES) dibujarBotonAjusteTFT(5);
    } else {
      server.send(400, "text/plain", "Parametro o valor de configuracion no permitido");
      return;
    }

    guardarConfiguracionNVS();
    forzarRedibujoTFT = true;
    server.send(200, "text/plain", "OK");
  });

  server.on("/api/cfg/pulse", HTTP_POST, []() {
    if (!solicitudControlAutorizada()) return;
    riegoPorPulsos = !riegoPorPulsos;
    if (!riegoPorPulsos && estadoRiego == ESTADO_INFILTRACION) {
      finalizarCicloRiegoNormal();
    }
    guardarConfiguracionNVS();
    if (pestanaActual == PESTANA_AJUSTES) dibujarBotonAjusteTFT(3);
    forzarRedibujoTFT = true;
    server.send(200, "text/plain", "OK");
  });

  server.on("/api/cfg/clima", HTTP_POST, []() {
    if (!solicitudControlAutorizada()) return;
    climaSmartActivo = !climaSmartActivo;
    calcularCompensacionClimatica();
    guardarConfiguracionNVS();
    if (pestanaActual == PESTANA_AJUSTES) { dibujarBotonAjusteTFT(4); dibujarBotonAjusteTFT(1); }
    forzarRedibujoTFT = true;
    server.send(200, "text/plain", "OK");
  });

  server.on("/api/cfg/unit", HTTP_POST, []() {
    if (!solicitudControlAutorizada()) return;
    usarFahrenheit = !usarFahrenheit;
    guardarConfiguracionNVS();
    if (pestanaActual == PESTANA_AJUSTES) dibujarBotonAjusteTFT(6);
    forzarRedibujoTFT = true;
    server.send(200, "text/plain", "OK");
  });

  server.on("/api/tft/recover", HTTP_POST, []() {
    if (!solicitudControlAutorizada()) return;
    reiniciarPantallaTFT();
    server.send(200, "text/plain", "OK");
  });
}
