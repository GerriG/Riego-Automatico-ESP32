/**
 * ============================================================================
 * PROYECTO    : SISTEMA DE RIEGO AUTOMÁTICO
 * SUBTÍTULO   : Sistema de control y supervisión mediante ESP32
 * ASIGNATURA  : Sistemas Digitales — Ingeniería en Sistemas y Computación
 * INSTITUCIÓN : Universidad Tecnológica de El Salvador (UTEC)
 * PLATAFORMA  : ESP32 DevKit V1 (30 pines) sobre Placa de Expansión G-V-S
 * ============================================================================
 *
 * RESPONSABILIDAD DE ESTE ARCHIVO PRINCIPAL (.ino):
 *   Actúa exclusivamente como punto de entrada del firmware (setup y loop) y
 *   coordinador cooperativo no bloqueante mediante millis().
 *
 *   Nota de compatibilidad con Arduino IDE:
 *   Arduino exige que el archivo .ino principal conserve exactamente el mismo
 *   nombre que la carpeta contenedora del proyecto. Todos los subsistemas se
 *   encuentran separados dentro de "src/modulos/" (bajo "src/" para que
 *   Arduino IDE compile recursivamente las unidades .cpp):
 *
 *   - src/modulos/Configuracion.h       : Pines GPIO, PWM, calibración y tipos.
 *   - src/modulos/Sensores.h / .cpp     : Suelo ADC1, DHT22, VPD y reglas.
 *   - src/modulos/ControlRiego.h / .cpp : Máquina de estados, PWM, Dry-Run, NVS.
 *   - src/modulos/Pantalla.h / .cpp     : TFT ILI9341 (VSPI) y XPT2046 (HSPI).
 *   - src/modulos/DashboardWeb.h / .cpp : HTML5, CSS3, SVG y JS en PROGMEM.
 *   - src/modulos/ServidorWeb.h / .cpp  : Wi-Fi, NTP, Open-Meteo y API REST.
 *   - src/modulos/credenciales.h        : Redes Wi-Fi privadas (excluido en git).
 * ============================================================================
 */

#include "src/modulos/Configuracion.h"
#include "src/modulos/Sensores.h"
#include "src/modulos/ControlRiego.h"
#include "src/modulos/Pantalla.h"
#include "src/modulos/ServidorWeb.h"

// Marcas temporales del planificador cooperativo principal
static unsigned long ultimoMuestreoSueloMs = 0;
static unsigned long ultimoMuestreoDHTMs   = 0;
static unsigned long ultimoSegundoMs       = 0;

/**
 * Secuencia determinista de inicialización:
 *   1. Carga parámetros persistentes desde NVS.
 *   2. Asegura el actuador hidráulico apagado (PWM = 0) antes de cualquier tarea.
 *   3. Inicializa sensores analógicos/digitales y buses SPI separados de la TFT.
 *   4. Realiza lecturas iniciales de validación y calcula el umbral dinámico.
 *   5. Inicia la conexión Wi-Fi, el servidor HTTP local y dibuja el marco TFT.
 */
void setup() {
  Serial.begin(115200);
  delay(150);

  cargarConfiguracionNVS();
  inicializarControlRiego();
  inicializarSensores();
  inicializarPantallaTFT();

  // Tres muestras iniciales permiten confirmar estabilidad (racha >= 3) al arrancar
  for (int i = 0; i < 3; i++) {
    leerSensorSuelo();
    delay(20);
  }
  leerSensorDHT22();

  unsigned long inicioMs = millis();
  ultimoMuestreoSueloMs  = inicioMs;
  ultimoMuestreoDHTMs    = inicioMs;
  ultimoSegundoMs        = inicioMs;

  calcularCompensacionClimatica();
  iniciarRedYServidorWeb();

  dibujarMarcoFijoTFT();
  dibujarEstructuraPestanaActual();
  actualizarDatosPantallaTFT(true);
}

/**
 * Bucle principal cooperativo no bloqueante basado en diferencias con millis():
 * reparte el tiempo de CPU entre comunicaciones HTTP, muestreo de sensores,
 * control en lazo cerrado, auditoría de protecciones e interfaz táctil local.
 */
void loop() {
  unsigned long ahora = millis();

  // 1. Atención inmediata a clientes HTTP del Dashboard Web y API REST JSON
  atenderClientesWeb();

  // 2. Adquisición y filtrado del sensor capacitivo de sustrato (cada 2.0 s)
  if (ahora - ultimoMuestreoSueloMs >= INTERVALO_SUELO_MS) {
    ultimoMuestreoSueloMs = ahora;
    leerSensorSuelo();
    forzarRedibujoTFT = true;
  }

  // 3. Lectura del DHT22 y actualización de compensación por reglas (cada 2.5 s)
  if (ahora - ultimoMuestreoDHTMs >= INTERVALO_DHT_MS) {
    ultimoMuestreoDHTMs = ahora;
    leerSensorDHT22();
    calcularCompensacionClimatica();
    forzarRedibujoTFT = true;
  }

  // 4. Evaluación continua de la máquina finita de estados e histéresis
  actualizarControlAutomatico();

  // 5. Supervisión de protecciones hidráulicas cada 1.0 s (continuo, Dry-Run, ciclo)
  if (ahora - ultimoSegundoMs >= 1000UL) {
    ultimoSegundoMs = ahora;
    supervisarSeguridadHidraulica();
    forzarRedibujoTFT = true;
  }

  // 6. Gestión cooperativa de Wi-Fi, reloj NTP y Open-Meteo (inhibida durante riego)
  gestionarWiFiYClima();

  // 7. Lectura del panel táctil XPT2046 y refresco selectivo sin parpadeo en TFT
  atenderPantallaTactil();
  if (forzarRedibujoTFT) {
    actualizarDatosPantallaTFT(false);
    forzarRedibujoTFT = false;
  }
}
