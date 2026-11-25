/**
 * ============================================================================
 * ARCHIVO     : ControlRiego.cpp
 * PROYECTO    : SISTEMA DE RIEGO AUTOMÁTICO
 * ----------------------------------------------------------------------------
 * RESPONSABILIDAD DEL MÓDULO:
 *   Implementa el algoritmo completo de control automático en lazo cerrado,
 *   la banda de histéresis, la dosificación por pulsos con pausas de
 *   infiltración, el arranque suave PWM sobre el MOSFET LR7843, los tres
 *   contadores independientes de protección hidráulica, el control manual
 *   unificado y la persistencia no volátil en NVS.
 * ============================================================================
 */

#include "ControlRiego.h"
#include "Sensores.h"
#include "Pantalla.h"
#include <Preferences.h>

static Preferences prefs;

// Marcas internas de referencia temporal e hídrica del ciclo activo
static unsigned long inicioFaseMs            = 0;
static unsigned long inicioCicloRiegoMs      = 0;
static unsigned long ultimoTickBombaMs       = 0;
static int           humedadReferenciaDryRun = 0;

// --- Definición única de parámetros configurables (NVS) ---
bool    modoAutomatico    = true;
bool    riegoPorPulsos    = true;
bool    climaSmartActivo  = true;
bool    usarFahrenheit    = false;
uint8_t brilloPantallaPWM = 230;
uint8_t potenciaBombaPWM  = 215;
int     umbralBasePct     = 40;
int     histeresisPct     = 10;

// --- Definición única del estado hidráulico y de seguridad ---
EstadoRiego   estadoRiego              = ESTADO_ESPERA;
CodigoAlerta  alertaActiva             = ALERTA_NINGUNA;
bool          bombaEncendida           = false;
bool          cicloRiegoActivo         = false;
unsigned long tiempoContinuoBombaMs    = 0;
unsigned long tiempoAcumuladoBombeoMs  = 0;
unsigned long tiempoBombeoCicloTotalMs = 0;

/** Recupera y valida los parámetros almacenados en la partición NVS al iniciar. */
void cargarConfiguracionNVS() {
  prefs.begin("riego_utec", true);
  modoAutomatico    = prefs.getBool("autoM", true);
  riegoPorPulsos    = prefs.getBool("pulsos", true);
  climaSmartActivo  = prefs.getBool("climaS", true);
  usarFahrenheit    = prefs.getBool("useF", false);
  brilloPantallaPWM = prefs.getUChar("brillo", 230);
  potenciaBombaPWM  = prefs.getUChar("pwm", 215);
  umbralBasePct     = prefs.getInt("baseTh", 40);
  histeresisPct     = prefs.getInt("hyst", 10);
  prefs.end();

  if (umbralBasePct != 30 && umbralBasePct != 40 && umbralBasePct != 55) umbralBasePct = 40;
  if (histeresisPct != 5 && histeresisPct != 10 && histeresisPct != 15) histeresisPct = 10;
  if (potenciaBombaPWM != 165 && potenciaBombaPWM != 215 && potenciaBombaPWM != 255) potenciaBombaPWM = 215;
  if (brilloPantallaPWM != 90 && brilloPantallaPWM != 165 && brilloPantallaPWM != 230) brilloPantallaPWM = 230;
}

/** Escribe en NVS únicamente ante cambios reales de configuración o bloqueo. */
void guardarConfiguracionNVS() {
  prefs.begin("riego_utec", false);
  prefs.putBool("autoM", modoAutomatico);
  prefs.putBool("pulsos", riegoPorPulsos);
  prefs.putBool("climaS", climaSmartActivo);
  prefs.putBool("useF", usarFahrenheit);
  prefs.putUChar("brillo", brilloPantallaPWM);
  prefs.putUChar("pwm", potenciaBombaPWM);
  prefs.putInt("baseTh", umbralBasePct);
  prefs.putInt("hyst", histeresisPct);
  prefs.end();
}

/**
 * Inicializa el canal LEDC PWM del MOSFET LR7843 (GPIO 4, 5000 Hz, 8 bits)
 * garantizando estado seguro inicial con la microbomba apagada (duty = 0).
 */
void inicializarControlRiego() {
  #if defined(ESP_ARDUINO_VERSION_MAJOR) && ESP_ARDUINO_VERSION_MAJOR >= 3
    ledcAttach(MOSFET_PIN, PWM_FREQ, PWM_RES);
    ledcWrite(MOSFET_PIN, 0);
  #else
    ledcSetup(PWM_CH_PUMP, PWM_FREQ, PWM_RES);
    ledcAttachPin(MOSFET_PIN, PWM_CH_PUMP);
    ledcWrite(PWM_CH_PUMP, 0);
  #endif
  bombaEncendida = false;
}

/** Relación de histéresis: Umbral superior = Umbral inferior + Banda de histéresis. */
int obtenerUmbralSuperior() {
  return min(95, umbralDinamicoInf + histeresisPct);
}

/** Aplica el ciclo útil PWM de 8 bits (0..255) al canal del MOSFET en GPIO 4. */
void escribirPWMBomba(uint8_t duty) {
  #if defined(ESP_ARDUINO_VERSION_MAJOR) && ESP_ARDUINO_VERSION_MAJOR >= 3
    ledcWrite(MOSFET_PIN, duty);
  #else
    ledcWrite(PWM_CH_PUMP, duty);
  #endif
}

/**
 * Compuerta centralizada de seguridad antes de energizar la bomba.
 * Impide encender el MOSFET si hay una alerta activa o fallo del sensor en GPIO 34.
 */
bool puedeActivarBomba(const char** motivoRechazo) {
  if (estadoRiego == ESTADO_ALERTA || alertaActiva != ALERTA_NINGUNA) {
    if (motivoRechazo) *motivoRechazo = "Proteccion activa: desbloquee primero la alerta";
    return false;
  }
  if (!humedadSueloValida) {
    if (motivoRechazo) *motivoRechazo = "Lectura de suelo no disponible en puerto D34";
    return false;
  }
  return true;
}

/** Integra al milisegundo el tiempo efectivo real con la bomba encendida. */
static void acumularTiempoBombeoActivo() {
  if (!bombaEncendida) return;
  unsigned long ahora = millis();
  unsigned long delta = ahora - ultimoTickBombaMs;
  ultimoTickBombaMs = ahora;

  tiempoContinuoBombaMs    += delta;
  tiempoAcumuladoBombeoMs  += delta;
  tiempoBombeoCicloTotalMs += delta;
}

/**
 * Única función autorizada para conmutar físicamente la microbomba.
 * Al encender aplica rampa de arranque suave (Soft Start: 90 -> potenciaBombaPWM)
 * para reducir el pico de corriente sobre la línea de 5 V; al apagar fuerza PWM = 0.
 */
bool cambiarEstadoBomba(bool encender, const char** motivoRechazo) {
  if (encender) {
    if (!puedeActivarBomba(motivoRechazo)) {
      escribirPWMBomba(0);
      bombaEncendida = false;
      return false;
    }
    if (bombaEncendida) return true;

    bombaEncendida        = true;
    tiempoContinuoBombaMs = 0;

    for (int pwm = 90; pwm <= potenciaBombaPWM; pwm += 35) {
      escribirPWMBomba((uint8_t)pwm);
      delay(10);
    }
    escribirPWMBomba(potenciaBombaPWM);
    inicioFaseMs      = millis();
    ultimoTickBombaMs = inicioFaseMs;
    forzarRedibujoTFT = true;
    return true;
  } else {
    if (bombaEncendida) {
      acumularTiempoBombeoActivo();
    }
    bombaEncendida        = false;
    escribirPWMBomba(0);
    tiempoContinuoBombaMs = 0;
    inicioFaseMs          = millis();
    forzarRedibujoTFT     = true;
    return true;
  }
}

/** Inicia un ciclo supervisado de riego (automático o manual). */
bool iniciarCicloRiego(bool esAutomatico, const char** motivoRechazo) {
  const char* motivo = nullptr;
  if (!puedeActivarBomba(&motivo)) {
    if (motivoRechazo) *motivoRechazo = motivo;
    return false;
  }
  (void)esAutomatico;

  tiempoAcumuladoBombeoMs  = 0;
  tiempoBombeoCicloTotalMs = 0;
  humedadReferenciaDryRun  = humedadSueloPct;
  inicioCicloRiegoMs       = millis();
  cicloRiegoActivo         = true;
  estadoRiego              = ESTADO_RIEGO;

  if (!cambiarEstadoBomba(true, motivoRechazo)) {
    cicloRiegoActivo = false;
    estadoRiego      = ESTADO_ESPERA;
    return false;
  }
  return true;
}

/** Finaliza normalmente el ciclo de riego y reinicia los contadores acumulados. */
void finalizarCicloRiegoNormal() {
  cambiarEstadoBomba(false);
  cicloRiegoActivo         = false;
  tiempoAcumuladoBombeoMs  = 0;
  tiempoBombeoCicloTotalMs = 0;
  estadoRiego              = ESTADO_ESPERA;
  forzarRedibujoTFT        = true;
}

/**
 * Acción manual unificada de encendido/apagado de bomba (usada por TFT y Web).
 * Pasa a modo MANUAL, valida la compuerta de seguridad y guarda en NVS.
 */
bool conmutarBombaManual(const char** mensajeSalida) {
  if (bombaEncendida) {
    modoAutomatico = false;
    guardarConfiguracionNVS();
    finalizarCicloRiegoNormal();
    if (mensajeSalida) *mensajeSalida = "Bomba detenida manualmente";
    return true;
  }

  const char* motivo = nullptr;
  if (!puedeActivarBomba(&motivo)) {
    if (mensajeSalida) *mensajeSalida = motivo ? motivo : "Operacion bloqueada por seguridad";
    return false;
  }

  modoAutomatico = false;
  guardarConfiguracionNVS();
  if (!iniciarCicloRiego(false, &motivo)) {
    if (mensajeSalida) *mensajeSalida = motivo ? motivo : "No se pudo encender la bomba";
    return false;
  }

  if (mensajeSalida) *mensajeSalida = "Bomba encendida en modo manual";
  return true;
}

/**
 * Cambio centralizado entre modo AUTOMÁTICO y MANUAL (usado por TFT y Web).
 * Impide borrar alertas activas al cambiar de modo y exige sensor válido para AUTO.
 */
bool establecerModoOperacion(bool nuevoModoAuto, const char** motivoRechazo) {
  if (estadoRiego == ESTADO_ALERTA || alertaActiva != ALERTA_NINGUNA) {
    if (motivoRechazo) *motivoRechazo = "Restablezca primero la proteccion activa antes de cambiar de modo";
    return false;
  }
  if (nuevoModoAuto && !humedadSueloValida) {
    if (motivoRechazo) *motivoRechazo = "No puede activar el modo AUTOMATICO sin lectura valida del sensor en GPIO 34";
    return false;
  }
  if (modoAutomatico != nuevoModoAuto) {
    finalizarCicloRiegoNormal();
    modoAutomatico = nuevoModoAuto;
    guardarConfiguracionNVS();
  }
  forzarRedibujoTFT = true;
  return true;
}

/** Corta inmediatamente el MOSFET, fija ESTADO_ALERTA y pasa a modo MANUAL seguro. */
void dispararAlertaSeguridad(CodigoAlerta causa) {
  cambiarEstadoBomba(false);
  cicloRiegoActivo  = false;
  alertaActiva      = causa;
  estadoRiego       = ESTADO_ALERTA;
  modoAutomatico    = false;
  guardarConfiguracionNVS();
  forzarRedibujoTFT = true;
}

/**
 * Restablecimiento verificado de alertas por acción explícita del usuario:
 *   1. Mantiene la bomba apagada (PWM = 0).
 *   2. Verifica lecturas consecutivas reales del sensor capacitivo en GPIO 34.
 *   3. Si persiste el fallo, conserva ESTADO_ALERTA e informa el motivo real.
 */
bool rearmarAlertaPorUsuario(const char** mensajeSalida) {
  cambiarEstadoBomba(false);
  cicloRiegoActivo = false;

  if (!humedadSueloValida) {
    for (int i = 0; i < 3; i++) {
      leerSensorSuelo();
      delay(15);
    }
  }

  if (!humedadSueloValida) {
    alertaActiva = ALERTA_FALLO_SUELO;
    estadoRiego  = ESTADO_ALERTA;
    mostrarAvisoTFT("No se puede rearmar: persiste fallo en D34");
    if (mensajeSalida) {
      *mensajeSalida = "No es posible restablecer la alerta: el sensor capacitivo en GPIO 34 aun no entrega lecturas validas consecutivas.";
    }
    forzarRedibujoTFT = true;
    return false;
  }

  if (bombaEncendida) {
    escribirPWMBomba(0);
    bombaEncendida = false;
  }

  alertaActiva             = ALERTA_NINGUNA;
  tiempoAcumuladoBombeoMs  = 0;
  tiempoBombeoCicloTotalMs = 0;
  estadoRiego              = ESTADO_ESPERA;
  mostrarAvisoTFT("Proteccion restablecida: bomba apagada");
  if (mensajeSalida) {
    *mensajeSalida = "Proteccion restablecida correctamente. La bomba permanece apagada en modo manual.";
  }
  forzarRedibujoTFT = true;
  return true;
}

/**
 * Supervisa cada segundo la operación activa de la bomba (en AUTO y en MANUAL):
 *   1. Funcionamiento continuo sin pausa >= 25 s -> ALERTA_TIEMPO_MAXIMO.
 *   2. Bombeo efectivo acumulado >= 20 s sin subida >= +2% -> Dry-Run (ALERTA_ANOMALIA_RIEGO).
 *   3. Bombeo efectivo total del ciclo >= 60 s -> ALERTA_TIEMPO_MAXIMO.
 */
void supervisarSeguridadHidraulica() {
  if (!bombaEncendida) return;

  if (!humedadSueloValida) {
    dispararAlertaSeguridad(ALERTA_FALLO_SUELO);
    return;
  }

  acumularTiempoBombeoActivo();

  if (humedadSueloPct >= humedadReferenciaDryRun + INCREMENTO_MIN_HUMEDAD_PCT) {
    humedadReferenciaDryRun = humedadSueloPct;
    tiempoAcumuladoBombeoMs = 0;
  }

  if (tiempoContinuoBombaMs >= MAX_BOMBEO_CONTINUO_MS) {
    dispararAlertaSeguridad(ALERTA_TIEMPO_MAXIMO);
    return;
  }

  if (tiempoAcumuladoBombeoMs >= MAX_DRY_RUN_EFECTIVO_MS) {
    dispararAlertaSeguridad(ALERTA_ANOMALIA_RIEGO);
    return;
  }

  if (tiempoBombeoCicloTotalMs >= MAX_BOMBEO_CICLO_MS) {
    dispararAlertaSeguridad(ALERTA_TIEMPO_MAXIMO);
  }
}

/**
 * Máquina finita de 4 estados del control automático en lazo cerrado:
 *   ESPERA -> RIEGO (pulso 6 s) <-> INFILTRACION (pausa 10 s) -> ESPERA / ALERTA.
 * Durante ESTADO_INFILTRACION se preserva tiempoAcumuladoBombeoMs para que la
 * protección Dry-Run acumule los 20 s efectivos entre pulsos consecutivos.
 */
void actualizarControlAutomatico() {
  unsigned long ahora = millis();

  if (estadoRiego == ESTADO_ALERTA) {
    if (bombaEncendida) cambiarEstadoBomba(false);
    return;
  }

  if (!modoAutomatico) {
    if (!bombaEncendida && estadoRiego != ESTADO_ESPERA) {
      cicloRiegoActivo = false;
      estadoRiego      = ESTADO_ESPERA;
    }
    return;
  }

  if (!humedadSueloValida) {
    if (bombaEncendida || estadoRiego != ESTADO_ESPERA) {
      dispararAlertaSeguridad(ALERTA_FALLO_SUELO);
    }
    return;
  }

  int umbralSup = obtenerUmbralSuperior();

  switch (estadoRiego) {
    case ESTADO_ESPERA:
      if (humedadSueloPct < umbralDinamicoInf) {
        iniciarCicloRiego(true);
      }
      break;

    case ESTADO_RIEGO:
      if (humedadSueloPct >= umbralSup) {
        finalizarCicloRiegoNormal();
      } else if (riegoPorPulsos && (ahora - inicioFaseMs >= PULSO_RIEGO_MS)) {
        cambiarEstadoBomba(false);
        estadoRiego  = ESTADO_INFILTRACION;
        inicioFaseMs = ahora;
      }
      break;

    case ESTADO_INFILTRACION:
      if (ahora - inicioFaseMs >= PAUSA_INFILTRACION_MS) {
        leerSensorSuelo();
        if (!humedadSueloValida) {
          dispararAlertaSeguridad(ALERTA_FALLO_SUELO);
          return;
        }

        if (humedadSueloPct >= humedadReferenciaDryRun + INCREMENTO_MIN_HUMEDAD_PCT) {
          humedadReferenciaDryRun = humedadSueloPct;
          tiempoAcumuladoBombeoMs = 0;
        }

        if (humedadSueloPct >= umbralSup) {
          finalizarCicloRiegoNormal();
        } else if (tiempoAcumuladoBombeoMs >= MAX_DRY_RUN_EFECTIVO_MS) {
          dispararAlertaSeguridad(ALERTA_ANOMALIA_RIEGO);
        } else {
          estadoRiego = ESTADO_RIEGO;
          if (!cambiarEstadoBomba(true)) {
            dispararAlertaSeguridad(ALERTA_FALLO_SUELO);
          }
        }
      }
      break;

    case ESTADO_ALERTA:
      cambiarEstadoBomba(false);
      break;
  }
}

/** Indica si hay un ciclo de riego activo, incluyendo pausas de infiltración. */
bool cicloHidraulicoEnCurso() {
  return bombaEncendida ||
         cicloRiegoActivo ||
         estadoRiego == ESTADO_RIEGO ||
         estadoRiego == ESTADO_INFILTRACION;
}

int obtenerSegundosFaseActual() {
  unsigned long transcurrido = millis() - inicioFaseMs;
  if (estadoRiego == ESTADO_RIEGO && modoAutomatico && riegoPorPulsos) {
    if (transcurrido >= PULSO_RIEGO_MS) return 0;
    return (int)((PULSO_RIEGO_MS - transcurrido + 999UL) / 1000UL);
  }
  if (estadoRiego == ESTADO_INFILTRACION) {
    if (transcurrido >= PAUSA_INFILTRACION_MS) return 0;
    return (int)((PAUSA_INFILTRACION_MS - transcurrido + 999UL) / 1000UL);
  }
  return bombaEncendida ? (int)(transcurrido / 1000UL) : 0;
}

unsigned long obtenerSegundosTotalesCiclo() {
  if (!cicloRiegoActivo) return 0UL;
  return (millis() - inicioCicloRiegoMs) / 1000UL;
}

const char* textoAlertaActiva() {
  switch (alertaActiva) {
    case ALERTA_ANOMALIA_RIEGO:
      return "Proteccion activada: 20 s de bombeo sin subida de humedad (+2%)";
    case ALERTA_TIEMPO_MAXIMO:
      return "Proteccion activada: tiempo maximo permitido de bombeo alcanzado";
    case ALERTA_FALLO_SUELO:
      return "Fallo en sensor de suelo (puerto D34): riego bloqueado por seguridad";
    default:
      return "Protecciones operativas sin alertas";
  }
}

const char* textoEstadoSistema() {
  if (estadoRiego == ESTADO_ALERTA)       return "PROTECCION ACTIVADA";
  if (estadoRiego == ESTADO_INFILTRACION) return "PAUSA DE INFILTRACION";
  if (bombaEncendida)                     return modoAutomatico ? "REGANDO (AUTOMATICO)" : "REGANDO (MANUAL)";
  if (!humedadSueloValida)                return "LECTURA NO DISPONIBLE";
  return "EN ESPERA";
}

const char* textoEstadoSustrato() {
  if (!humedadSueloValida)                                  return "Lectura no disponible";
  if (bombaEncendida || estadoRiego == ESTADO_INFILTRACION) return "Suministrando hasta objetivo";
  if (humedadSueloPct < umbralDinamicoInf)                  return "Necesita riego";
  if (humedadSueloPct >= obtenerUmbralSuperior())           return "Objetivo alcanzado";
  return "Humedad adecuada";
}
