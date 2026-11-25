/**
 * ============================================================================
 * ARCHIVO     : Sensores.cpp
 * PROYECTO    : SISTEMA DE RIEGO AUTOMÁTICO
 * ----------------------------------------------------------------------------
 * RESPONSABILIDAD DEL MÓDULO:
 *   Implementa la digitalización y validación eléctrica del sensor capacitivo
 *   (GPIO 34), la lectura digital temporizada del DHT22 (GPIO 13), las
 *   ecuaciones psicrométricas (Rothfusz y Tetens) y el cálculo de la
 *   compensación climática basada en reglas diferenciando entre el ajuste
 *   calculado y el ajuste efectivamente aplicado tras los límites [20%, 75%].
 * ============================================================================
 */

#include "Sensores.h"
#include "ControlRiego.h"
#include <DHT.h>
#include <WiFi.h>
#include <math.h>

// Instancia física encapsulada del sensor ambiental DHT22
static DHT dht(DHTPIN, DHTTYPE);

// Contadores internos de rachas de validación / fallo de sensores
static uint8_t rachaFalloSuelo    = 0;
static uint8_t rachaValidaSuelo   = 0;
static uint8_t rachaFalloDHT      = 0;
static uint8_t rachaValidaDHT     = 0;
static bool    arranqueInicialDHT = true;

// --- Definición única de variables globales de sensores y compensación ---
int  sueloAdcCrudo      = 0;
int  humedadSueloPct    = 0;
bool humedadSueloValida = false;

EstadoSensorDHT estadoDHT            = DHT_INICIANDO;
float           tempAmbienteC        = 0.0f;
float           humAmbientePct       = 0.0f;
float           indiceCalorC         = 0.0f;
float           dpvActualKpa         = 0.0f;
bool            dhtValido            = false;
bool            indiceCalorAplicable = false;
unsigned long   ultimaLecturaDHTMs   = 0;

bool          climaRemotoRecibido = false;
unsigned long ultimaSyncClimaMs   = 0;
int           climaCodigoWMO      = 0;
bool          climaEsDeDia        = true;

int    deltaClimaCalculadoPct = 0;
int    deltaClimaPct          = 0;
int    umbralDinamicoInf      = 40;
String motivoClimaFactores    = "Sin ajuste (0%)";

/**
 * Configura el convertidor ADC1 (12 bits, atenuación de 11 dB para rango ~0..3.3V)
 * e inicia el protocolo Single-Bus del sensor DHT22.
 */
void inicializarSensores() {
  pinMode(SOIL_PIN, INPUT);
  analogReadResolution(12);
  analogSetPinAttenuation(SOIL_PIN, ADC_11db);
  dht.begin();
}

/**
 * Adquiere el índice relativo calibrado de humedad del sustrato (0..100%).
 * Funcionamiento:
 *   1. Promedia 8 lecturas rápidas en ADC1_CH6 (GPIO 34) y mide la dispersión.
 *   2. Si detecta circuito abierto, corto o ruido excesivo en 2 lecturas seguidas,
 *      marca humedadSueloValida = false y dispara alerta si había riego en curso.
 *   3. Aplica interpolación lineal inversa entre SOIL_DRY_ADC y SOIL_WET_ADC,
 *      exigiendo 3 lecturas válidas seguidas para recuperarse y filtro 70/30.
 */
void leerSensorSuelo() {
  long suma = 0;
  int minVal = 4095, maxVal = 0;

  for (int i = 0; i < 8; i++) {
    int muestra = analogRead(SOIL_PIN);
    suma += muestra;
    if (muestra < minVal) minVal = muestra;
    if (muestra > maxVal) maxVal = muestra;
    delayMicroseconds(250);
  }
  sueloAdcCrudo = (int)(suma / 8L);

  bool lecturaAnomala = (sueloAdcCrudo < SOIL_FAULT_MIN_ADC) ||
                        (sueloAdcCrudo > SOIL_FAULT_MAX_ADC) ||
                        ((maxVal - minVal) > SOIL_NOISE_MAX_SPAN);

  if (lecturaAnomala) {
    rachaValidaSuelo = 0;
    if (rachaFalloSuelo < 10) rachaFalloSuelo++;

    if (rachaFalloSuelo >= 2) {
      humedadSueloValida = false;
      if (cicloHidraulicoEnCurso()) {
        dispararAlertaSeguridad(ALERTA_FALLO_SUELO);
      }
    }
  } else {
    rachaFalloSuelo = 0;
    if (rachaValidaSuelo < 10) rachaValidaSuelo++;

    // Interpolación lineal inversa (Cap. II 2.3.1.4): índice relativo calibrado
    int pctCalibrado = map(sueloAdcCrudo, SOIL_DRY_ADC, SOIL_WET_ADC, 0, 100);
    pctCalibrado = constrain(pctCalibrado, 0, 100);

    if (!humedadSueloValida) {
      if (rachaValidaSuelo >= 3) {
        humedadSueloPct    = pctCalibrado;
        humedadSueloValida = true;
      }
    } else {
      humedadSueloPct = (pctCalibrado * 7 + humedadSueloPct * 3) / 10;
    }
  }
}

/**
 * Calcula el Índice de Calor (Rothfusz) y el Déficit de Presión de Vapor (VPD).
 * - El Índice de Calor solo es aplicable para T >= 27 °C y HR >= 40%.
 * - Ecuación de Tetens: VP_sat = 0.61078 * exp((17.27 * T) / (T + 237.3)) [kPa];
 *   VPD = VP_sat * (1 - HR / 100) [kPa].
 */
void calcularParametrosAmbientales(float tempC, float humPct) {
  indiceCalorAplicable = (tempC >= 27.0f && humPct >= 40.0f);
  if (indiceCalorAplicable) {
    indiceCalorC = dht.computeHeatIndex(tempC, humPct, false);
  } else {
    indiceCalorC = tempC;
  }

  float vpSatKpa = 0.61078f * expf((17.27f * tempC) / (tempC + 237.3f));
  dpvActualKpa   = max(0.0f, vpSatKpa * (1.0f - (humPct / 100.0f)));
}

/**
 * Lee temperatura y humedad relativa del DHT22 respetando un intervalo >= 2.5 s.
 * Nota metrológica: comprueba ausencia de NaN y rango operativo del fabricante
 * (-40..80 °C, 0..100% HR) para confirmar una última lectura válida recibida,
 * sin presentar dicha validación de trama como calibración experimental.
 */
void leerSensorDHT22() {
  unsigned long ahora = millis();
  float t = dht.readTemperature(false);
  float h = dht.readHumidity();

  bool tecnicamenteValido = !isnan(t) && !isnan(h) &&
                            (t >= -40.0f && t <= 80.0f) &&
                            (h >= 0.0f && h <= 100.0f);

  if (tecnicamenteValido) {
    rachaFalloDHT = 0;
    if (rachaValidaDHT < 10) rachaValidaDHT++;

    if (arranqueInicialDHT || rachaValidaDHT >= 2) {
      arranqueInicialDHT = false;
      tempAmbienteC      = t;
      humAmbientePct     = h;
      ultimaLecturaDHTMs = ahora;
      dhtValido          = true;

      calcularParametrosAmbientales(t, h);

      bool plausible = (t >= 5.0f && t <= 48.0f && h >= 10.0f);
      estadoDHT = plausible ? DHT_OPERATIVO : DHT_RANGO_INUSUAL;
    }
  } else {
    rachaValidaDHT = 0;
    if (rachaFalloDHT < 10) rachaFalloDHT++;

    if (rachaFalloDHT >= 3 || (ahora - ultimaLecturaDHTMs > CADUCIDAD_DHT_MS)) {
      dhtValido            = false;
      indiceCalorAplicable = false;
      dpvActualKpa         = 0.0f;
      estadoDHT            = DHT_SIN_RESPUESTA;
    }
  }
}

/** Verifica si el dato meteorológico de Open-Meteo fue recibido y sigue vigente (<= 45 min). */
bool climaExteriorVigente() {
  return climaRemotoRecibido &&
         (WiFi.status() == WL_CONNECTED) &&
         ((millis() - ultimaSyncClimaMs) <= CADUCIDAD_CLIMA_API_MS);
}

/**
 * Evalúa las seis reglas deterministas del Capítulo II (Sección 2.6.1):
 *   1. Térmica local : T >= 32 °C o HI >= 35 °C (+9%) | T <= 20 °C (-7%).
 *   2. Demanda VPD   : VPD >= 1.6 kPa (+6%) | VPD <= 0.45 kPa (-6%).
 *   3. Open-Meteo    : Precipitación WMO >= 51 (-5%) | Cielo despejado diurno WMO <= 1 (+3%).
 * Distingue entre compensación calculada (deltaClimaCalculadoPct) y compensación
 * efectivamente aplicada (deltaClimaPct) tras limitar el umbral a [20%, 75%].
 */
void calcularCompensacionClimatica() {
  if (dhtValido && (millis() - ultimaLecturaDHTMs > CADUCIDAD_DHT_MS)) {
    dhtValido            = false;
    indiceCalorAplicable = false;
    dpvActualKpa         = 0.0f;
    estadoDHT            = DHT_SIN_RESPUESTA;
  }

  if (!climaSmartActivo) {
    deltaClimaCalculadoPct = 0;
    umbralDinamicoInf      = umbralBasePct;
    deltaClimaPct          = 0;
    motivoClimaFactores    = "Compensacion desactivada (0%)";
    return;
  }

  int delta = 0;
  String factores = "";

  if (dhtValido) {
    if (tempAmbienteC >= 32.0f || (indiceCalorAplicable && indiceCalorC >= 35.0f)) {
      delta += 9;
      factores += "Calor(+9%) ";
    } else if (tempAmbienteC <= 20.0f) {
      delta -= 7;
      factores += "Ambiente fresco(-7%) ";
    }

    if (dpvActualKpa >= 1.6f) {
      delta += 6;
      factores += "VPD alto(+6%) ";
    } else if (dpvActualKpa <= 0.45f) {
      delta -= 6;
      factores += "VPD bajo(-6%) ";
    }
  }

  if (climaExteriorVigente()) {
    if (climaCodigoWMO >= 51) {
      delta -= 5;
      factores += "Condicion de lluvia (-5%)";
    } else if (climaCodigoWMO <= 1 && climaEsDeDia) {
      // weather_code <= 1 e is_day == 1 representan cielo despejado diurno,
      // no una medición directa de radiación solar.
      delta += 3;
      factores += "Condiciones de cielo despejado durante el dia (+3%)";
    }
  }

  deltaClimaCalculadoPct = delta;
  umbralDinamicoInf      = constrain(umbralBasePct + deltaClimaCalculadoPct, 20, 75);
  deltaClimaPct          = umbralDinamicoInf - umbralBasePct;

  if (factores.length() == 0) {
    motivoClimaFactores = dhtValido ? "Condiciones equilibradas (0%)" : "Sin DHT22: usa umbral base (0%)";
  } else {
    factores.trim();
    motivoClimaFactores = factores;
  }
}

const char* textoEstadoDHT() {
  switch (estadoDHT) {
    case DHT_OPERATIVO:     return "Operativo";
    case DHT_RANGO_INUSUAL: return "Valor inusual";
    case DHT_SIN_RESPUESTA: return "Sin respuesta";
    default:                return "Iniciando";
  }
}

const char* textoNivelDPV() {
  if (!dhtValido)            return "No disponible";
  if (dpvActualKpa >= 1.6f)  return "Demanda alta";
  if (dpvActualKpa <= 0.45f) return "Ambiente humedo";
  return "Equilibrado";
}

const char* textoExplicacionVPD() {
  if (!dhtValido)            return "Sin datos vigentes del sensor DHT22.";
  if (dpvActualKpa >= 1.6f)  return "VPD elevado: el aire presenta mayor demanda de evaporacion.";
  if (dpvActualKpa <= 0.45f) return "VPD bajo: aire humedo con baja demanda de evaporacion.";
  return "VPD moderado: nivel de evaporacion ambiental equilibrado.";
}

const char* textoExplicacionAmbiental() {
  if (!climaSmartActivo) return "Compensacion desactivada: se usa el umbral base fijo";
  if (!dhtValido && !climaExteriorVigente()) return "Sin datos climaticos: opera con el umbral base";
  if (deltaClimaCalculadoPct != deltaClimaPct) {
    return (umbralDinamicoInf <= 20)
      ? "Compensacion acotada al limite inferior (20%)"
      : "Compensacion acotada al limite superior (75%)";
  }
  if (deltaClimaPct >= 6)  return "Ambiente seco/caluroso: se incremento el umbral";
  if (deltaClimaPct > 0)   return "Cielo despejado diurno: umbral incrementado (+3%)";
  if (deltaClimaPct <= -5) return "Ambiente humedo/lluvioso: se redujo el umbral";
  if (deltaClimaPct < 0)   return "Menor demanda ambiental: umbral reducido";
  return "Ambiente equilibrado: se conserva el umbral base";
}

const char* textoClimaExterior() {
  if (WiFi.status() != WL_CONNECTED) return "Sin Wi-Fi";
  if (!climaRemotoRecibido)          return (millis() < 25000UL) ? "Consultando red..." : "Sin dato externo";
  if (!climaExteriorVigente())       return "Dato caducado";
  if (climaCodigoWMO >= 95)          return "Tormenta";
  if (climaCodigoWMO >= 61)          return "Lluvia";
  if (climaCodigoWMO >= 51)          return "Llovizna";
  if (climaCodigoWMO >= 2)           return "Nublado";
  return climaEsDeDia ? "Despejado (Dia)" : "Despejado (Noche)";
}
