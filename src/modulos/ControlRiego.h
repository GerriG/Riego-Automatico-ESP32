/**
 * ============================================================================
 * ARCHIVO     : ControlRiego.h
 * PROYECTO    : SISTEMA DE RIEGO AUTOMÁTICO
 * ----------------------------------------------------------------------------
 * RESPONSABILIDAD DEL MÓDULO:
 *   Concentra en una única implementación centralizada toda la lógica de
 *   control hidráulico, electrónica de potencia PWM y seguridad del sistema:
 *     - Persistencia de parámetros configurables en memoria Flash NVS.
 *     - Máquina finita de 4 estados (ESPERA, RIEGO, INFILTRACION, ALERTA).
 *     - Evaluación de umbrales e histéresis (U_inf y U_sup).
 *     - Activación automática en lazo cerrado y control manual supervisado.
 *     - Riego por pulsos (6 s ON) y pausas de infiltración (10 s).
 *     - Modulación PWM a 5000 Hz y rampa de arranque suave (Soft Start).
 *     - Protección acumulativa Dry-Run (20 s entre pulsos), límite continuo
 *       (25 s), límite por ciclo (60 s) y rearme verificado de alertas.
 *
 * COMPONENTES QUE GESTIONA:
 *   - Módulo MOSFET LR7843 con optoacoplador PC817 (GPIO 4, LEDC Canal 1).
 *   - Microbomba sumergible DC de 5 V.
 *   - Partición NVS "riego_utec" mediante biblioteca Preferences.
 *
 * COMUNICACIÓN CON OTROS MÓDULOS:
 *   Lee las variables validadas de Sensores y expone las funciones únicas de
 *   mando y consulta utilizadas tanto por Pantalla (TFT) como por ServidorWeb,
 *   garantizando que ninguna interfaz pueda eludir las protecciones.
 * ============================================================================
 */

#ifndef CONTROL_RIEGO_H
#define CONTROL_RIEGO_H

#include "Configuracion.h"

// --- Fuente única de verdad: parámetros persistentes en memoria NVS ---
extern bool    modoAutomatico;
extern bool    riegoPorPulsos;
extern bool    climaSmartActivo;
extern bool    usarFahrenheit;
extern uint8_t brilloPantallaPWM;
extern uint8_t potenciaBombaPWM;
extern int     umbralBasePct;
extern int     histeresisPct;

// --- Fuente única de verdad: estado hidráulico, actuador y protecciones ---
extern EstadoRiego   estadoRiego;
extern CodigoAlerta  alertaActiva;
extern bool          bombaEncendida;
extern bool          cicloRiegoActivo;
extern unsigned long tiempoContinuoBombaMs;     // Funcionamiento continuo sin pausa (máx. 25 s)
extern unsigned long tiempoAcumuladoBombeoMs;   // Bombeo efectivo acumulado entre pulsos (Dry-Run 20 s)
extern unsigned long tiempoBombeoCicloTotalMs;  // Bombeo efectivo total en el ciclo actual (máx. 60 s)

// --- Inicialización y persistencia en memoria Flash NVS ---
void cargarConfiguracionNVS();
void guardarConfiguracionNVS();
void inicializarControlRiego();

// --- Evaluación de umbrales, PWM, Soft Start y control centralizado ---
int  obtenerUmbralSuperior();
bool puedeActivarBomba(const char** motivoRechazo = nullptr);
void escribirPWMBomba(uint8_t duty);
bool cambiarEstadoBomba(bool encender, const char** motivoRechazo = nullptr);
bool iniciarCicloRiego(bool esAutomatico, const char** motivoRechazo = nullptr);
void finalizarCicloRiegoNormal();
bool conmutarBombaManual(const char** mensajeSalida = nullptr);
bool establecerModoOperacion(bool nuevoModoAuto, const char** motivoRechazo = nullptr);

// --- Máquina de estados, protecciones hidráulicas y rearme de alertas ---
void dispararAlertaSeguridad(CodigoAlerta causa);
bool rearmarAlertaPorUsuario(const char** mensajeSalida = nullptr);
void supervisarSeguridadHidraulica();
void actualizarControlAutomatico();
bool cicloHidraulicoEnCurso();

// --- Consultas de tiempos de fase/ciclo y textos descriptivos de estado ---
int           obtenerSegundosFaseActual();
unsigned long obtenerSegundosTotalesCiclo();
const char*   textoAlertaActiva();
const char*   textoEstadoSistema();
const char*   textoEstadoSustrato();

#endif // CONTROL_RIEGO_H
