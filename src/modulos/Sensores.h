/**
 * ============================================================================
 * ARCHIVO     : Sensores.h
 * PROYECTO    : SISTEMA DE RIEGO AUTOMÁTICO
 * ----------------------------------------------------------------------------
 * RESPONSABILIDAD DEL MÓDULO:
 *   Gestiona la adquisición, filtrado y detección de fallos del sensor
 *   capacitivo de humedad del sustrato (GPIO 34, ADC1) y del sensor ambiental
 *   DHT22 (GPIO 13), el cálculo de magnitudes termodinámicas derivadas
 *   (Índice de Calor acotado y VPD mediante ecuación de Tetens) y el Sistema
 *   Adaptativo de Compensación Climática basado en reglas deterministas.
 *
 * COMPONENTES QUE GESTIONA:
 *   - Sensor capacitivo de humedad del suelo v2.0 (ADC1_CH6, 12 bits).
 *   - Sensor digital de temperatura y humedad relativa DHT22.
 *
 * COMUNICACIÓN CON OTROS MÓDULOS:
 *   Expone como única fuente de verdad las lecturas validadas de suelo,
 *   ambiente y compensación climática (calculada vs. efectiva aplicada) para
 *   que ControlRiego, Pantalla y ServidorWeb las consulten sin duplicarlas.
 * ============================================================================
 */

#ifndef SENSORES_H
#define SENSORES_H

#include "Configuracion.h"

// --- Fuente única de verdad: sensor capacitivo de sustrato (GPIO 34) ---
extern int  sueloAdcCrudo;
extern int  humedadSueloPct;
extern bool humedadSueloValida;

// --- Fuente única de verdad: sensor ambiental DHT22 (GPIO 13) ---
extern EstadoSensorDHT estadoDHT;
extern float           tempAmbienteC;
extern float           humAmbientePct;
extern float           indiceCalorC;
extern float           dpvActualKpa;
extern bool            dhtValido;
extern bool            indiceCalorAplicable;
extern unsigned long   ultimaLecturaDHTMs;

// --- Estado meteorológico recibido desde Open-Meteo (actualizado por ServidorWeb) ---
extern bool          climaRemotoRecibido;
extern unsigned long ultimaSyncClimaMs;
extern int           climaCodigoWMO;
extern bool          climaEsDeDia;

// --- Resultados del Sistema Adaptativo de Compensación Climática por reglas ---
extern int    deltaClimaCalculadoPct; // Compensación sumada directamente por las reglas
extern int    deltaClimaPct;          // Compensación efectivamente aplicada tras constrain(20..75)
extern int    umbralDinamicoInf;      // Umbral inferior resultante de activación (< U_inf)
extern String motivoClimaFactores;    // Descripción legible de las reglas activas

// --- Funciones principales de inicialización, adquisición y cálculo ---
void inicializarSensores();
void leerSensorSuelo();
void leerSensorDHT22();
void calcularParametrosAmbientales(float tempC, float humPct);
bool climaExteriorVigente();
void calcularCompensacionClimatica();

// --- Textos descriptivos compartidos entre la pantalla TFT y el Dashboard Web ---
const char* textoEstadoDHT();
const char* textoNivelDPV();
const char* textoExplicacionVPD();
const char* textoExplicacionAmbiental();
const char* textoClimaExterior();

#endif // SENSORES_H
