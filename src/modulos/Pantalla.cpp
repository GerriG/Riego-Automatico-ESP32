/**
 * ============================================================================
 * ARCHIVO     : Pantalla.cpp
 * PROYECTO    : SISTEMA DE RIEGO AUTOMÁTICO
 * ----------------------------------------------------------------------------
 * RESPONSABILIDAD DEL MÓDULO:
 *   Implementa el renderizado directo sobre la memoria GRAM del ILI9341 (0 KB
 *   de framebuffer en RAM), la sobrescritura con color de fondo para evitar
 *   parpadeos y restos de texto, y la captura antirrebote del XPT2046 en HSPI.
 * ============================================================================
 */

#include "Pantalla.h"
#include "Sensores.h"
#include "ControlRiego.h"
#include "ServidorWeb.h"
#include <SPI.h>
#include <Adafruit_GFX.h>
#include <Adafruit_ILI9341.h>
#include <XPT2046_Touchscreen.h>
#include <WiFi.h>
#include <time.h>

// Instancias encapsuladas en buses SPI físicamente separados (VSPI y HSPI)
static Adafruit_ILI9341    tft = Adafruit_ILI9341(TFT_CS, TFT_DC, TFT_RST);
static SPIClass            touchSPI(HSPI);
static XPT2046_Touchscreen ts(T_CS, T_IRQ);

// Estado interno de avisos temporales y antirrebote táctil (240 ms)
static char          avisoTemporalTFT[50] = "";
static unsigned long expiraAvisoTFTMs     = 0;
static unsigned long ultimoToqueMs        = 0;
static bool          estabaTocando        = false;

// Estado público de pestaña activa y bandera de refresco
PestanaTFT pestanaActual     = PESTANA_RIEGO;
bool       forzarRedibujoTFT = true;

/** Regula el brillo de la retroiluminación LED TFT mediante PWM (GPIO 21). */
void aplicarBrilloPantalla(uint8_t duty) {
  #if defined(ESP_ARDUINO_VERSION_MAJOR) && ESP_ARDUINO_VERSION_MAJOR >= 3
    ledcWrite(TFT_LED, duty);
  #else
    ledcWrite(PWM_CH_LED, duty);
  #endif
}

/**
 * Inicializa el canal LEDC 0 (5000 Hz, 8 bits) para el backlight TFT,
 * el controlador de video ILI9341 en VSPI y el panel táctil XPT2046 en HSPI.
 */
void inicializarPantallaTFT() {
  #if defined(ESP_ARDUINO_VERSION_MAJOR) && ESP_ARDUINO_VERSION_MAJOR >= 3
    ledcAttach(TFT_LED, PWM_FREQ, PWM_RES);
    ledcWrite(TFT_LED, brilloPantallaPWM);
  #else
    ledcSetup(PWM_CH_LED, PWM_FREQ, PWM_RES);
    ledcAttachPin(TFT_LED, PWM_CH_LED);
    ledcWrite(PWM_CH_LED, brilloPantallaPWM);
  #endif

  tft.begin();
  tft.setRotation(1);
  tft.setTextWrap(false);

  touchSPI.begin(T_CLK, T_DO, T_DIN, T_CS);
  ts.begin(touchSPI);
  ts.setRotation(0);
}

/** Muestra un mensaje temporal de confirmación o rechazo durante 3.5 s en TFT. */
void mostrarAvisoTFT(const char* texto) {
  snprintf(avisoTemporalTFT, sizeof(avisoTemporalTFT), "%s", texto);
  expiraAvisoTFTMs  = millis() + 3500UL;
  forzarRedibujoTFT = true;
}

/** Reinicio físico en caliente del chip ILI9341 mediante pulso en GPIO 17 (TFT_RST). */
void reiniciarPantallaTFT() {
  pinMode(TFT_RST, OUTPUT);
  digitalWrite(TFT_RST, LOW);
  delay(45);
  digitalWrite(TFT_RST, HIGH);
  delay(90);

  tft.begin();
  tft.setRotation(1);
  tft.setTextWrap(false);
  aplicarBrilloPantalla(brilloPantallaPWM);

  dibujarMarcoFijoTFT();
  dibujarEstructuraPestanaActual();
  actualizarDatosPantallaTFT(true);
}

void dibujarMarcoFijoTFT() {
  tft.fillScreen(COLOR_BG);
  tft.fillRect(0, 0, 320, 28, COLOR_HEADER);
  tft.drawFastHLine(0, 28, 320, COLOR_BORDER);
  tft.fillRect(0, 192, 320, 48, COLOR_HEADER);
  tft.drawFastHLine(0, 192, 320, COLOR_BORDER);
  dibujarBarraPestanasTFT();
}

void dibujarBarraPestanasTFT() {
  const char* etiquetas[4] = {"RIEGO", "AMBIENTE", "AJUSTES", "DIAGN."};
  for (int i = 0; i < 4; i++) {
    int16_t x = 4 + i * 79;
    bool activa = ((int)pestanaActual == i);
    uint16_t bg = activa ? COLOR_GREEN : COLOR_CARD_DARK;
    uint16_t fg = activa ? COLOR_BLACK : COLOR_TEXT_MUTED;

    tft.fillRoundRect(x, 197, 74, 38, 6, bg);
    if (!activa) tft.drawRoundRect(x, 197, 74, 38, 6, COLOR_BORDER);

    tft.setTextSize(1);
    tft.setTextColor(fg, bg);
    int16_t tx = x + (74 - (int)strlen(etiquetas[i]) * 6) / 2;
    tft.setCursor(tx, 212);
    tft.print(etiquetas[i]);
  }
}

void cambiarPestanaTFT(PestanaTFT nueva) {
  if (pestanaActual == nueva) return;
  pestanaActual = nueva;

  tft.fillRect(0, 29, 320, 163, COLOR_BG);
  dibujarBarraPestanasTFT();
  dibujarEstructuraPestanaActual();
  actualizarDatosPantallaTFT(true);
}

/** Dibuja uno de los 7 botones independientes de AJUSTES (0..6) sin scroll. */
void dibujarBotonAjusteTFT(int idx) {
  tft.setTextSize(1);

  if (idx == 5 || idx == 6) {
    int16_t x = (idx == 5) ? 163 : 241;
    int16_t y = 136, w = 73, h = 48;
    tft.fillRoundRect(x, y, w, h, 6, COLOR_CARD);
    tft.drawRoundRect(x, y, w, h, 6, COLOR_BORDER);

    tft.setTextColor(COLOR_TEXT_MUTED, COLOR_CARD);
    tft.setCursor(x + 6, y + 8);
    if (idx == 5) {
      const char* niv = (brilloPantallaPWM <= 100) ? "BAJO" : (brilloPantallaPWM <= 180 ? "MEDIO" : "ALTO");
      tft.printf("%-10s", "6. BRILLO");
      tft.setTextColor(COLOR_CYAN, COLOR_CARD);
      tft.setCursor(x + 6, y + 27);
      tft.printf("%-10s", niv);
    } else {
      tft.printf("%-10s", "7. UNIDAD");
      tft.setTextColor(COLOR_AMBER, COLOR_CARD);
      tft.setCursor(x + 6, y + 27);
      tft.printf("Grados %c%s ", (char)247, usarFahrenheit ? "F" : "C");
    }
    return;
  }

  int col = idx % 2;
  int fila = idx / 2;
  int16_t x = (col == 0) ? 6 : 163;
  int16_t y = 32 + fila * 52;
  int16_t w = 151, h = 48;

  tft.fillRoundRect(x, y, w, h, 6, COLOR_CARD);
  tft.drawRoundRect(x, y, w, h, 6, COLOR_BORDER);

  char linea1[24] = "";
  char linea2[24] = "";
  uint16_t colVal = COLOR_WHITE;

  switch (idx) {
    case 0:
      snprintf(linea1, sizeof(linea1), "1. UMBRAL BASE");
      snprintf(linea2, sizeof(linea2), "Activar bajo: %d%%", umbralBasePct);
      colVal = COLOR_GREEN;
      break;
    case 1:
      snprintf(linea1, sizeof(linea1), "2. HISTERESIS");
      snprintf(linea2, sizeof(linea2), "+%d%% (Detiene %d%%)", histeresisPct, obtenerUmbralSuperior());
      colVal = COLOR_CYAN;
      break;
    case 2:
      snprintf(linea1, sizeof(linea1), "3. POTENCIA BOMBA");
      snprintf(linea2, sizeof(linea2), "PWM: %d%% (5000 Hz)", (potenciaBombaPWM * 100) / 255);
      colVal = COLOR_AMBER;
      break;
    case 3:
      snprintf(linea1, sizeof(linea1), "4. RIEGO PULSOS");
      snprintf(linea2, sizeof(linea2), "%s", riegoPorPulsos ? "ACTIVO (6s / 10s)" : "CONTINUO");
      colVal = riegoPorPulsos ? COLOR_GREEN : COLOR_AMBER;
      break;
    case 4:
      snprintf(linea1, sizeof(linea1), "5. COMP. CLIMATICA");
      snprintf(linea2, sizeof(linea2), "%s", climaSmartActivo ? "ACTIVA (Reglas)" : "DESACTIVADA");
      colVal = climaSmartActivo ? COLOR_GREEN : COLOR_TEXT_MUTED;
      break;
  }

  tft.setTextColor(COLOR_TEXT_MUTED, COLOR_CARD);
  tft.setCursor(x + 8, y + 8);
  tft.printf("%-22s", linea1);

  tft.setTextColor(colVal, COLOR_CARD);
  tft.setCursor(x + 8, y + 27);
  tft.printf("%-22s", linea2);
}

void dibujarEstructuraPestanaActual() {
  const char* titulos[4] = {"SISTEMA RIEGO", "AMBIENTE", "AJUSTES", "DIAGNOSTICO"};
  tft.setTextSize(2);
  tft.setTextColor(COLOR_WHITE, COLOR_HEADER);
  tft.setCursor(8, 6);
  tft.printf("%-13s", titulos[(int)pestanaActual]);

  if (pestanaActual == PESTANA_RIEGO) {
    tft.fillRoundRect(6, 31, 308, 105, 7, COLOR_CARD);
    tft.drawRoundRect(6, 31, 308, 105, 7, COLOR_BORDER);
    tft.fillTriangle(17, 36, 13, 42, 21, 42, COLOR_CYAN);
    tft.fillCircle(17, 43, 4, COLOR_CYAN);
    tft.setTextSize(1);
    tft.setTextColor(COLOR_CYAN, COLOR_CARD);
    tft.setCursor(26, 39);
    tft.print("HUMEDAD SUELO");
    tft.drawFastHLine(12, 50, 296, COLOR_BORDER);
  } else if (pestanaActual == PESTANA_AMBIENTE) {
    tft.fillRoundRect(6, 31, 150, 104, 7, COLOR_CARD);
    tft.drawRoundRect(6, 31, 150, 104, 7, COLOR_BORDER);
    tft.setTextSize(1);
    tft.setTextColor(COLOR_AMBER, COLOR_CARD);
    tft.setCursor(14, 38);
    tft.print("SENSOR LOCAL DHT22");
    tft.drawFastHLine(12, 49, 138, COLOR_BORDER);

    tft.fillRoundRect(162, 31, 152, 104, 7, COLOR_CARD);
    tft.drawRoundRect(162, 31, 152, 104, 7, COLOR_BORDER);
    tft.setTextColor(COLOR_CYAN, COLOR_CARD);
    tft.setCursor(170, 38);
    tft.print("DEMANDA (VPD) Y CLIMA");
    tft.drawFastHLine(168, 49, 140, COLOR_BORDER);

    tft.fillRoundRect(6, 139, 308, 48, 7, COLOR_CARD);
    tft.drawRoundRect(6, 139, 308, 48, 7, COLOR_BORDER);
  } else if (pestanaActual == PESTANA_AJUSTES) {
    for (int i = 0; i < 7; i++) dibujarBotonAjusteTFT(i);
  } else if (pestanaActual == PESTANA_DIAGNOSTICO) {
    tft.fillRoundRect(6, 31, 150, 112, 7, COLOR_CARD);
    tft.drawRoundRect(6, 31, 150, 112, 7, COLOR_BORDER);
    tft.setTextSize(1);
    tft.setTextColor(COLOR_GREEN, COLOR_CARD);
    tft.setCursor(12, 38);
    tft.print("SENSORES Y BOMBA");
    tft.drawFastHLine(12, 49, 138, COLOR_BORDER);

    tft.fillRoundRect(162, 31, 152, 112, 7, COLOR_CARD);
    tft.drawRoundRect(162, 31, 152, 112, 7, COLOR_BORDER);
    tft.setTextColor(COLOR_AMBER, COLOR_CARD);
    tft.setCursor(168, 38);
    tft.print("RED Y PROTECCIONES");
    tft.drawFastHLine(168, 49, 140, COLOR_BORDER);

    tft.fillRoundRect(6, 147, 150, 40, 6, COLOR_CARD_DARK);
    tft.drawRoundRect(6, 147, 150, 40, 6, COLOR_AMBER);
    tft.setTextColor(COLOR_AMBER, COLOR_CARD_DARK);
    tft.setCursor(18, 163);
    tft.print("REARMAR PROTECCION");

    tft.fillRoundRect(162, 147, 152, 40, 6, COLOR_CARD_DARK);
    tft.drawRoundRect(162, 147, 152, 40, 6, COLOR_CYAN);
    tft.setTextColor(COLOR_CYAN, COLOR_CARD_DARK);
    tft.setCursor(184, 163);
    tft.print("REINICIAR PANTALLA");
  }
}

void actualizarCabeceraTFT(bool forzar) {
  static int ultimoMinuto = -1;
  static bool ultimoWiFi = false;
  static EstadoRiego ultimoEst = ESTADO_ESPERA;
  static bool ultimaBomba = false;

  struct tm horaLocal;
  bool tieneHora = ntpSincronizado && getLocalTime(&horaLocal, 2);
  int minActual = tieneHora ? horaLocal.tm_min : (int)((millis() / 60000UL) % 60UL);
  bool wifiOk = (WiFi.status() == WL_CONNECTED);

  if (!forzar && minActual == ultimoMinuto && wifiOk == ultimoWiFi &&
      estadoRiego == ultimoEst && bombaEncendida == ultimaBomba) {
    return;
  }
  ultimoMinuto = minActual;
  ultimoWiFi   = wifiOk;
  ultimoEst    = estadoRiego;
  ultimaBomba  = bombaEncendida;

  tft.setTextSize(1);
  tft.setTextColor(COLOR_TEXT_MUTED, COLOR_HEADER);
  tft.setCursor(184, 10);
  if (tieneHora) {
    int h12 = horaLocal.tm_hour % 12;
    if (h12 == 0) h12 = 12;
    tft.printf("%02d:%02d %s ", h12, horaLocal.tm_min, (horaLocal.tm_hour >= 12) ? "PM" : "AM");
  } else {
    tft.printf("WIFI:%-4s", wifiOk ? "OK" : "---");
  }

  tft.fillRect(268, 5, 46, 18, COLOR_HEADER);
  if (wifiOk) {
    int rssi = WiFi.RSSI();
    uint16_t colWifi = (rssi > -72) ? COLOR_GREEN : COLOR_AMBER;
    tft.fillRect(272, 16, 3, 5, colWifi);
    if (rssi > -85) tft.fillRect(277, 12, 3, 9, colWifi);
    if (rssi > -70) tft.fillRect(282, 8, 3, 13, colWifi);
  } else {
    tft.setTextColor(COLOR_RED, COLOR_HEADER);
    tft.setCursor(272, 10);
    tft.print("X");
  }

  uint16_t colEstado = COLOR_GREEN;
  if (estadoRiego == ESTADO_ALERTA)            colEstado = COLOR_RED;
  else if (estadoRiego == ESTADO_INFILTRACION) colEstado = COLOR_CYAN;
  else if (bombaEncendida)                     colEstado = COLOR_AMBER;
  tft.fillCircle(302, 14, 5, colEstado);
}

void actualizarDatosPantallaTFT(bool forzar) {
  actualizarCabeceraTFT(forzar);

  if (pestanaActual == PESTANA_RIEGO) {
    tft.setTextSize(1);
    tft.setTextColor(bombaEncendida ? COLOR_GREEN : COLOR_TEXT_MUTED, COLOR_CARD);
    tft.setCursor(192, 39);
    tft.printf("BOMBA: %-10s", bombaEncendida ? "ENCENDIDA" : "APAGADA");

    static bool prevSueloValidoUI = true;
    if (forzar || prevSueloValidoUI != humedadSueloValida) {
      tft.fillRect(14, 54, 118, 32, COLOR_CARD);
      prevSueloValidoUI = humedadSueloValida;
    }

    tft.setCursor(16, 59);
    if (humedadSueloValida) {
      tft.setTextSize(3);
      tft.setTextColor(COLOR_WHITE, COLOR_CARD);
      tft.printf("%3d%% ", humedadSueloPct);
    } else {
      tft.setTextSize(2);
      tft.setTextColor(COLOR_RED, COLOR_CARD);
      tft.printf("%-9s", "SIN DATO");
    }

    int supActual = obtenerUmbralSuperior();
    tft.setTextSize(1);
    tft.setTextColor(modoAutomatico ? COLOR_AMBER : COLOR_CYAN, COLOR_CARD);
    tft.setCursor(142, 54);
    tft.printf("Modo    : %-10s", modoAutomatico ? "AUTOMATICO" : "MANUAL");

    tft.setTextColor(COLOR_AMBER, COLOR_CARD);
    tft.setCursor(142, 66);
    tft.printf("Activar : < %2d%%      ", umbralDinamicoInf);

    tft.setTextColor(COLOR_GREEN, COLOR_CARD);
    tft.setCursor(142, 78);
    tft.printf("Detener : >= %2d%%     ", supActual);

    static int prevPct = -1, prevInf = -1, prevSup = -1;
    static bool prevVal = false;
    if (forzar || prevPct != humedadSueloPct || prevInf != umbralDinamicoInf ||
        prevSup != supActual || prevVal != humedadSueloValida) {
      prevPct = humedadSueloPct;
      prevInf = umbralDinamicoInf;
      prevSup = supActual;
      prevVal = humedadSueloValida;

      int16_t barraX = 14, barraY = 93, barraAncho = 292, barraAlto = 13;
      tft.drawRoundRect(barraX, barraY, barraAncho, barraAlto, 4, COLOR_BORDER);
      int fillW = humedadSueloValida ? map(humedadSueloPct, 0, 100, 0, barraAncho - 4) : 0;
      uint16_t colBarra = !humedadSueloValida ? COLOR_RED : (humedadSueloPct < umbralDinamicoInf ? COLOR_AMBER : COLOR_GREEN);

      if (fillW > 0) tft.fillRect(barraX + 2, barraY + 2, fillW, barraAlto - 4, colBarra);
      if (fillW < barraAncho - 4) tft.fillRect(barraX + 2 + fillW, barraY + 2, (barraAncho - 4) - fillW, barraAlto - 4, COLOR_CARD_DARK);

      int xInf = barraX + 2 + map(umbralDinamicoInf, 0, 100, 0, barraAncho - 5);
      int xSup = barraX + 2 + map(supActual, 0, 100, 0, barraAncho - 5);
      tft.drawFastVLine(xInf, barraY + 1, barraAlto - 2, COLOR_AMBER);
      tft.drawFastVLine(xSup, barraY + 1, barraAlto - 2, COLOR_WHITE);
    }

    char lineaEstado[52];
    uint16_t colEst = COLOR_GREEN;
    if (millis() < expiraAvisoTFTMs && avisoTemporalTFT[0] != '\0') {
      colEst = COLOR_AMBER;
      snprintf(lineaEstado, sizeof(lineaEstado), "%s", avisoTemporalTFT);
    } else if (estadoRiego == ESTADO_ALERTA) {
      colEst = COLOR_RED;
      if (alertaActiva == ALERTA_FALLO_SUELO)
        snprintf(lineaEstado, sizeof(lineaEstado), "PROTECCION: Fallo sensor suelo D34 (bloqueado)");
      else if (alertaActiva == ALERTA_ANOMALIA_RIEGO)
        snprintf(lineaEstado, sizeof(lineaEstado), "ANOMALIA RIEGO: 20s sin subir +2%% (bloqueado)");
      else
        snprintf(lineaEstado, sizeof(lineaEstado), "PROTECCION: Limite maximo de bombeo alcanzado");
    } else if (estadoRiego == ESTADO_INFILTRACION) {
      colEst = COLOR_CYAN;
      snprintf(lineaEstado, sizeof(lineaEstado), "Pausa infiltracion activa: evaluando en %2d s", obtenerSegundosFaseActual());
    } else if (bombaEncendida) {
      colEst = COLOR_GREEN;
      if (modoAutomatico && riegoPorPulsos)
        snprintf(lineaEstado, sizeof(lineaEstado), "Regando por pulsos: %2d s restantes (Ac:%2lus)",
                 obtenerSegundosFaseActual(), tiempoAcumuladoBombeoMs / 1000UL);
      else
        snprintf(lineaEstado, sizeof(lineaEstado), "Bomba suministrando agua (%2d s / max 25 s)",
                 obtenerSegundosFaseActual());
    } else if (!humedadSueloValida) {
      colEst = COLOR_RED;
      snprintf(lineaEstado, sizeof(lineaEstado), "Lectura no disponible: revise sensor en D34");
    } else {
      colEst = (humedadSueloPct < umbralDinamicoInf) ? COLOR_AMBER : COLOR_GREEN;
      snprintf(lineaEstado, sizeof(lineaEstado), "%s (%s)",
               textoEstadoSustrato(), modoAutomatico ? "Control Auto" : "Control Manual");
    }
    tft.setTextSize(1);
    tft.setTextColor(colEst, COLOR_CARD);
    tft.setCursor(14, 115);
    tft.printf("%-48.48s", lineaEstado);

    static bool prevBombaBtn = false, prevAutoBtn = true, prevSueloBtn = true;
    static EstadoRiego prevEstadoBtn = ESTADO_ESPERA;
    if (forzar || prevBombaBtn != bombaEncendida || prevAutoBtn != modoAutomatico ||
        prevEstadoBtn != estadoRiego || prevSueloBtn != humedadSueloValida) {
      prevBombaBtn  = bombaEncendida;
      prevAutoBtn   = modoAutomatico;
      prevEstadoBtn = estadoRiego;
      prevSueloBtn  = humedadSueloValida;

      uint16_t bgIzq = COLOR_CARD;
      uint16_t fgIzq = COLOR_WHITE;
      if (estadoRiego == ESTADO_ALERTA) { bgIzq = COLOR_RED; fgIzq = COLOR_BLACK; }
      else if (bombaEncendida)          { bgIzq = COLOR_GREEN; fgIzq = COLOR_BLACK; }
      else if (!humedadSueloValida)     { bgIzq = COLOR_CARD_DARK; fgIzq = COLOR_TEXT_MUTED; }

      tft.fillRoundRect(6, 140, 184, 46, 7, bgIzq);
      tft.drawRoundRect(6, 140, 184, 46, 7, (estadoRiego == ESTADO_ALERTA || bombaEncendida) ? COLOR_WHITE : COLOR_CYAN);

      tft.setTextSize(1);
      tft.setTextColor(fgIzq, bgIzq);
      tft.setCursor(16, 147);
      if (estadoRiego == ESTADO_ALERTA) tft.printf("%-26s", "PROTECCION ACTIVADA");
      else if (!humedadSueloValida)     tft.printf("%-26s", "SENSOR SUELO INVALIDO");
      else tft.printf("%-26s", bombaEncendida ? "BOMBA ACTIVA (TOCAR)" : "ACCION MANUAL BOMBA");

      tft.setTextSize(2);
      tft.setCursor(16, 162);
      if (estadoRiego == ESTADO_ALERTA) tft.printf("%-13s", "REARMAR");
      else if (!humedadSueloValida)     tft.printf("%-13s", "BLOQUEADO");
      else tft.printf("%-13s", bombaEncendida ? "APAGAR" : "ENCENDER");

      uint16_t bgDer = modoAutomatico ? COLOR_AMBER : COLOR_CARD;
      uint16_t fgDer = modoAutomatico ? COLOR_BLACK : COLOR_WHITE;
      tft.fillRoundRect(196, 140, 118, 46, 7, bgDer);
      tft.drawRoundRect(196, 140, 118, 46, 7, modoAutomatico ? COLOR_WHITE : COLOR_BORDER);

      tft.setTextSize(1);
      tft.setTextColor(fgDer, bgDer);
      tft.setCursor(206, 147);
      tft.printf("%-16s", "MODO OPERACION");
      tft.setTextSize(2);
      tft.setCursor(206, 162);
      tft.printf("%-8s", modoAutomatico ? "AUTO" : "MANUAL");
    }
  }
  else if (pestanaActual == PESTANA_AMBIENTE) {
    static bool prevDhtUI = true;
    if (forzar || prevDhtUI != dhtValido) {
      tft.fillRect(12, 53, 138, 76, COLOR_CARD);
      tft.fillRect(168, 53, 140, 42, COLOR_CARD);
      prevDhtUI = dhtValido;
    }

    if (dhtValido) {
      float tVis  = usarFahrenheit ? (tempAmbienteC * 1.8f + 32.0f) : tempAmbienteC;
      float hiVis = usarFahrenheit ? (indiceCalorC * 1.8f + 32.0f) : indiceCalorC;

      tft.setTextSize(2);
      tft.setTextColor(COLOR_WHITE, COLOR_CARD);
      tft.setCursor(14, 56);
      tft.printf("%5.1f %c%s  ", tVis, (char)247, usarFahrenheit ? "F" : "C");

      tft.setTextSize(1);
      tft.setTextColor(COLOR_CYAN, COLOR_CARD);
      tft.setCursor(14, 82);
      tft.printf("Humedad rel.: %4.1f%%  ", humAmbientePct);

      tft.setTextColor(COLOR_TEXT_MUTED, COLOR_CARD);
      tft.setCursor(14, 98);
      if (indiceCalorAplicable) tft.printf("Indice calor: %4.1f%c  ", hiVis, (char)247);
      else                      tft.printf("%-21s", "Ind. calor: No aplica");

      tft.setTextColor(estadoDHT == DHT_OPERATIVO ? COLOR_GREEN : COLOR_AMBER, COLOR_CARD);
      tft.setCursor(14, 114);
      tft.printf("DHT22: %-14s", textoEstadoDHT());
    } else {
      tft.setTextSize(1);
      tft.setTextColor(COLOR_RED, COLOR_CARD);
      tft.setCursor(14, 62);
      tft.printf("%-21s", "DHT22 sin respuesta");
      tft.setTextColor(COLOR_TEXT_MUTED, COLOR_CARD);
      tft.setCursor(14, 82);
      tft.printf("%-21s", "Verifique puerto D13.");
      tft.setCursor(14, 98);
      tft.printf("%-21s", "Riego sigue operativo");
      tft.setCursor(14, 114);
      tft.printf("%-21s", "con umbral base.");
    }

    tft.setTextSize(2);
    tft.setTextColor(COLOR_WHITE, COLOR_CARD);
    tft.setCursor(170, 56);
    if (dhtValido) tft.printf("%.2f kPa ", dpvActualKpa);
    else           tft.printf("%-9s", "--- kPa");

    tft.setTextSize(1);
    uint16_t colDpv = !dhtValido ? COLOR_TEXT_MUTED : (dpvActualKpa >= 1.6f ? COLOR_AMBER : COLOR_GREEN);
    tft.setTextColor(colDpv, COLOR_CARD);
    tft.setCursor(170, 82);
    tft.printf("VPD: %-17s", textoNivelDPV());

    tft.setTextColor(COLOR_TEXT_MUTED, COLOR_CARD);
    tft.setCursor(170, 99);
    tft.print("Conexion Open-Meteo:");
    tft.setTextColor(COLOR_CYAN, COLOR_CARD);
    tft.setCursor(170, 114);
    tft.printf("%-22s", textoClimaExterior());

    tft.setTextSize(1);
    tft.setTextColor(COLOR_AMBER, COLOR_CARD);
    tft.setCursor(14, 147);
    char lineaRef[52];
    if (deltaClimaCalculadoPct != deltaClimaPct) {
      snprintf(lineaRef, sizeof(lineaRef), "Base %d%% | Calc %+d%% | Aplic %+d%% (<%d%%)",
               umbralBasePct, deltaClimaCalculadoPct, deltaClimaPct, umbralDinamicoInf);
    } else {
      snprintf(lineaRef, sizeof(lineaRef), "Compensacion: Base %d%% %+d%% = Activar < %d%%",
               umbralBasePct, deltaClimaPct, umbralDinamicoInf);
    }
    tft.printf("%-48.48s", lineaRef);

    tft.setTextColor(COLOR_WHITE, COLOR_CARD);
    tft.setCursor(14, 165);
    tft.printf("%-48.48s", textoExplicacionAmbiental());
  }
  else if (pestanaActual == PESTANA_DIAGNOSTICO) {
    float voltSuelo = (sueloAdcCrudo * 3.3f) / 4095.0f;
    tft.setTextSize(1);

    tft.setTextColor(humedadSueloValida ? COLOR_GREEN : COLOR_RED, COLOR_CARD);
    tft.setCursor(12, 56);
    tft.printf("Suelo D34: %-4s %4d  ", humedadSueloValida ? "OK" : "ERR", sueloAdcCrudo);

    tft.setTextColor(COLOR_TEXT_MUTED, COLOR_CARD);
    tft.setCursor(12, 72);
    tft.printf("Tension  : %.2f V     ", voltSuelo);

    tft.setTextColor(dhtValido ? COLOR_GREEN : COLOR_RED, COLOR_CARD);
    tft.setCursor(12, 88);
    tft.printf("DHT22 D13: %-11s", textoEstadoDHT());

    tft.setTextColor(bombaEncendida ? COLOR_AMBER : COLOR_WHITE, COLOR_CARD);
    tft.setCursor(12, 104);
    tft.printf("Bomba    : %-11s", bombaEncendida ? "ENCENDIDA" : "APAGADA");

    tft.setTextColor(estadoRiego == ESTADO_ALERTA ? COLOR_RED : COLOR_CYAN, COLOR_CARD);
    tft.setCursor(12, 120);
    tft.printf("Dry-Run  : %2lus / 20s  ", tiempoAcumuladoBombeoMs / 1000UL);

    unsigned long seg = millis() / 1000UL;
    if (WiFi.status() == WL_CONNECTED) {
      String red = WiFi.SSID();
      if (red.length() > 14) red = red.substring(0, 14);
      tft.setTextColor(COLOR_GREEN, COLOR_CARD);
      tft.setCursor(168, 56);
      tft.printf("WiFi: %-15s", red.c_str());
      tft.setTextColor(COLOR_CYAN, COLOR_CARD);
      tft.setCursor(168, 72);
      tft.printf("IP: %-17s", WiFi.localIP().toString().c_str());
    } else {
      tft.setTextColor(COLOR_AMBER, COLOR_CARD);
      tft.setCursor(168, 56);
      tft.printf("%-21s", "WiFi: Reconectando...");
      tft.setTextColor(COLOR_TEXT_MUTED, COLOR_CARD);
      tft.setCursor(168, 72);
      tft.printf("%-21s", "IP: Red no disponible");
    }

    tft.setTextColor(estadoRiego == ESTADO_ALERTA ? COLOR_RED : COLOR_GREEN, COLOR_CARD);
    tft.setCursor(168, 88);
    tft.printf("Prot: %-15s", estadoRiego == ESTADO_ALERTA ? "ACTIVADA" : "OPERATIVA");

    tft.setTextColor(estadoRiego == ESTADO_ALERTA ? COLOR_AMBER : COLOR_TEXT_MUTED, COLOR_CARD);
    tft.setCursor(168, 104);
    if (alertaActiva == ALERTA_ANOMALIA_RIEGO)     tft.printf("%-21s", "Err: Sin subida +2%");
    else if (alertaActiva == ALERTA_FALLO_SUELO)   tft.printf("%-21s", "Err: Sensor suelo D34");
    else if (alertaActiva == ALERTA_TIEMPO_MAXIMO) tft.printf("%-21s", "Err: Tiempo maximo");
    else                                           tft.printf("%-21s", "Sin errores activos");

    tft.setTextColor(COLOR_TEXT_MUTED, COLOR_CARD);
    tft.setCursor(168, 120);
    tft.printf("Activo: %02lu:%02lu:%02lu     ", seg / 3600UL, (seg % 3600UL) / 60UL, seg % 60UL);
  }
}

/** Captura coordenadas del XPT2046 (bus HSPI) con antirrebote de 240 ms. */
void atenderPantallaTactil() {
  unsigned long ahora = millis();

  if (ts.touched()) {
    TS_Point puntoCrudo = ts.getPoint();
    if (puntoCrudo.z > 200 && !estabaTocando && (ahora - ultimoToqueMs >= 240UL)) {
      estabaTocando = true;
      ultimoToqueMs = ahora;

      int16_t rx = puntoCrudo.x, ry = puntoCrudo.y;
      if (SWAP_AXES) { int16_t tmp = rx; rx = ry; ry = tmp; }

      int16_t x = map(rx, TS_MINX, TS_MAXX, 0, 320);
      int16_t y = map(ry, TS_MINY, TS_MAXY, 0, 240);
      if (INVERT_X) x = 319 - x;
      if (INVERT_Y) y = 239 - y;
      x = constrain(x, 0, 319);
      y = constrain(y, 0, 239);

      procesarToqueTFT(x, y);
    }
  } else {
    estabaTocando = false;
  }
}

/**
 * Despacha las acciones táctiles delegando toda operación hidráulica y de
 * seguridad a las funciones únicas centralizadas en ControlRiego.cpp.
 */
void procesarToqueTFT(int16_t x, int16_t y) {
  if (y <= 28 && x >= 255) {
    reiniciarPantallaTFT();
    return;
  }

  if (y >= 192) {
    if (x < 81)       cambiarPestanaTFT(PESTANA_RIEGO);
    else if (x < 160) cambiarPestanaTFT(PESTANA_AMBIENTE);
    else if (x < 239) cambiarPestanaTFT(PESTANA_AJUSTES);
    else              cambiarPestanaTFT(PESTANA_DIAGNOSTICO);
    return;
  }

  if (pestanaActual == PESTANA_RIEGO && y >= 140 && y <= 188) {
    if (x >= 6 && x <= 190) {
      if (estadoRiego == ESTADO_ALERTA) {
        rearmarAlertaPorUsuario();
      } else {
        const char* mensaje = nullptr;
        bool ok = conmutarBombaManual(&mensaje);
        if (mensaje && (!ok || !bombaEncendida)) {
          mostrarAvisoTFT(mensaje);
        }
      }
      actualizarDatosPantallaTFT(true);
    } else if (x >= 196 && x <= 314) {
      const char* motivo = nullptr;
      if (!establecerModoOperacion(!modoAutomatico, &motivo)) {
        mostrarAvisoTFT((estadoRiego == ESTADO_ALERTA)
          ? "Rearme primero la proteccion antes de cambiar"
          : "Modo AUTO bloqueado: sensor D34 sin lectura");
      } else {
        mostrarAvisoTFT(modoAutomatico ? "Modo AUTOMATICO activado" : "Modo MANUAL activado");
      }
      actualizarDatosPantallaTFT(true);
    }
    return;
  }

  if (pestanaActual == PESTANA_AJUSTES && y >= 32 && y <= 186) {
    int fila = (y - 32) / 52;
    if (fila < 0 || fila > 2) return;
    int idx = 0;
    if (fila == 2 && x >= 160) {
      idx = (x < 239) ? 5 : 6;
    } else {
      idx = fila * 2 + ((x >= 160) ? 1 : 0);
    }

    switch (idx) {
      case 0:
        if (umbralBasePct == 30) umbralBasePct = 40;
        else if (umbralBasePct == 40) umbralBasePct = 55;
        else umbralBasePct = 30;
        calcularCompensacionClimatica();
        break;
      case 1:
        if (histeresisPct == 5) histeresisPct = 10;
        else if (histeresisPct == 10) histeresisPct = 15;
        else histeresisPct = 5;
        break;
      case 2:
        if (potenciaBombaPWM == 165) potenciaBombaPWM = 215;
        else if (potenciaBombaPWM == 215) potenciaBombaPWM = 255;
        else potenciaBombaPWM = 165;
        if (bombaEncendida) escribirPWMBomba(potenciaBombaPWM);
        break;
      case 3:
        riegoPorPulsos = !riegoPorPulsos;
        if (!riegoPorPulsos && estadoRiego == ESTADO_INFILTRACION) {
          finalizarCicloRiegoNormal();
        }
        break;
      case 4:
        climaSmartActivo = !climaSmartActivo;
        calcularCompensacionClimatica();
        break;
      case 5:
        if (brilloPantallaPWM <= 100) brilloPantallaPWM = 165;
        else if (brilloPantallaPWM <= 180) brilloPantallaPWM = 230;
        else brilloPantallaPWM = 90;
        aplicarBrilloPantalla(brilloPantallaPWM);
        break;
      case 6:
        usarFahrenheit = !usarFahrenheit;
        break;
    }
    guardarConfiguracionNVS();
    dibujarBotonAjusteTFT(idx);
    if (idx == 0 || idx == 4) dibujarBotonAjusteTFT(1);
    return;
  }

  if (pestanaActual == PESTANA_DIAGNOSTICO && y >= 147 && y <= 187) {
    if (x >= 6 && x <= 156) {
      rearmarAlertaPorUsuario();
      actualizarDatosPantallaTFT(true);
    } else if (x >= 162 && x <= 314) {
      reiniciarPantallaTFT();
    }
  }
}
