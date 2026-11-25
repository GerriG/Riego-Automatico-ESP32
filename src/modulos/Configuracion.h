/**
 * ============================================================================
 * ARCHIVO     : Configuracion.h
 * PROYECTO    : SISTEMA DE RIEGO AUTOMÁTICO
 * SUBTÍTULO   : Sistema de control y supervisión mediante ESP32
 * ASIGNATURA  : Sistemas Digitales — UTEC
 * ----------------------------------------------------------------------------
 * RESPONSABILIDAD DEL MÓDULO:
 *   Concentra en un único encabezado de nivel base (sin dependencias de otros
 *   módulos propios) la asignación física definitiva de pines GPIO, los
 *   parámetros del generador PWM (LEDC a 5000 Hz), las constantes de
 *   calibración de dos puntos del sensor capacitivo, los tiempos de control y
 *   seguridad hidráulica, los intervalos del planificador cooperativo millis(),
 *   la paleta cromática RGB565 y las enumeraciones compartidas del sistema.
 *
 * COMUNICACIÓN CON OTROS MÓDULOS:
 *   Incluido por Sensores, ControlRiego, Pantalla, ServidorWeb y el archivo
 *   principal .ino como referencia común de constantes y tipos de datos.
 * ============================================================================
 */

#ifndef CONFIGURACION_H
#define CONFIGURACION_H

#include <Arduino.h>

// =========================================================================
// 1. MAPA DEFINITIVO DE PINES GPIO (ESP32 DEVKIT V1 + PLACA DE EXPANSIÓN)
// =========================================================================

// --- Pantalla Gráfica TFT ILI9341 (320x240 Horizontal) en Bus VSPI ---
// Líneas hardware VSPI nativas: MOSI = GPIO 23, MISO = GPIO 19, SCK = GPIO 18
#define TFT_CS        5   // Selección de chip de video (Chip Select)
#define TFT_DC       22   // Selector Dato (HIGH) / Comando (LOW)
#define TFT_RST      17   // Reinicio físico por hardware del controlador ILI9341
#define TFT_LED      21   // Retroiluminación regulada por PWM (LEDC Canal 0)

// --- Sensores y Etapa de Potencia Hidráulica ---
#define SOIL_PIN     34   // Sensor capacitivo de suelo v2.0 (ADC1_CH6, compatible con Wi-Fi)
#define DHTPIN       13   // Sensor digital ambiental DHT22 / AM2302 con pull-up a 3.3V
#define DHTTYPE   DHT22   // Trama digital serie de 40 bits
#define MOSFET_PIN    4   // Entrada PWM del módulo MOSFET LR7843 con PC817 (Bomba 5V DC)

// --- Panel Táctil Resistivo XPT2046 en Bus HSPI Independiente ---
#define T_CLK        32   // Reloj serie dedicado al digitalizador táctil
#define T_CS         33   // Selección de chip táctil
#define T_DIN        25   // Línea MOSI hacia el XPT2046
#define T_DO         26   // Línea MISO desde el XPT2046
#define T_IRQ        27   // Detección de presión física (activa en nivel bajo)

// --- Periférico Generador PWM por Hardware (LEDC) ---
// 5000 Hz y 8 bits (0..255) conforme al Capítulo II: evita vibración audible
// en el rotor DC y respeta la velocidad de conmutación del optoacoplador PC817.
#define PWM_CH_LED       0
#define PWM_CH_PUMP      1
#define PWM_FREQ      5000
#define PWM_RES          8

// =========================================================================
// 2. CALIBRACIÓN DE SENSORES Y LÍMITES ELÉCTRICOS DE DETECCIÓN DE FALLOS
// =========================================================================

// Sensor capacitivo v2.0 a 3.3V (ADC de 12 bits: 0..4095 cuentas, respuesta inversa)
#define SOIL_DRY_ADC                3050   // ~2.46 V en sustrato seco de referencia (0%)
#define SOIL_WET_ADC                1350   // ~1.09 V en sustrato saturado de referencia (100%)
#define SOIL_FAULT_MIN_ADC           750   // < 0.60 V sugiere cortocircuito a masa
#define SOIL_FAULT_MAX_ADC          3850   // > 3.10 V sugiere circuito abierto o cable suelto
#define SOIL_NOISE_MAX_SPAN          350   // Dispersión máxima admisible entre 8 muestras

// Calibración geométrica del panel táctil XPT2046 en orientación horizontal 320x240
#define SWAP_AXES   false
#define INVERT_X    true
#define INVERT_Y    true
#define TS_MINX     300
#define TS_MAXX     3800
#define TS_MINY     300
#define TS_MAXY     3800

// =========================================================================
// 3. TIEMPOS DE CONTROL HIDRÁULICO, PROTECCIONES Y PLANIFICADOR millis()
// =========================================================================

#define PULSO_RIEGO_MS              6000UL // 6 s de bombeo activo por pulso (Cap. II 2.6.1)
#define PAUSA_INFILTRACION_MS      10000UL // 10 s de pausa para infiltración en el sustrato
#define MAX_DRY_RUN_EFECTIVO_MS    20000UL // 20 s de bombeo activo acumulado sin subir +2%
#define MAX_BOMBEO_CONTINUO_MS     25000UL // 25 s máx de funcionamiento continuo sin pausa
#define MAX_BOMBEO_CICLO_MS        60000UL // 60 s máx de bombeo efectivo total en un ciclo
#define INCREMENTO_MIN_HUMEDAD_PCT     2   // Incremento mínimo (+2%) que confirma llegada de agua

#define INTERVALO_SUELO_MS          2000UL     // Lectura del sensor capacitivo cada 2.0 s
#define INTERVALO_DHT_MS            2500UL     // >= 2.5 s (respeta mínimo físico de 2 s del DHT22)
#define CADUCIDAD_DHT_MS           10000UL     // > 10 s sin trama válida invalida datos ambientales
#define INTERVALO_WIFI_MS          15000UL     // Supervisión y reconexión Wi-Fi cada 15 s
#define INTERVALO_CLIMA_API_MS    600000UL     // Consulta HTTP a Open-Meteo cada 10 minutos
#define CADUCIDAD_CLIMA_API_MS   2700000UL     // El dato meteorológico externo caduca a los 45 min
#define HTTP_TIMEOUT_CLIMA_MS       1500       // Tiempo de espera TCP/HTTP en HTTPClient (1500 ms)

// Configuración horaria NTP (El Salvador UTC-6) y endpoint meteorológico Open-Meteo
static constexpr long        GMT_OFFSET_SEC      = -6L * 3600L;
static constexpr int         DAYLIGHT_OFFSET_SEC = 0;
static constexpr const char* NTP_SERVER1         = "pool.ntp.org";
static constexpr const char* NTP_SERVER2         = "time.google.com";
static constexpr const char* WEATHER_URL         = "http://api.open-meteo.com/v1/forecast?latitude=13.71&longitude=-89.14&current=weather_code,is_day&timezone=America%2FEl_Salvador&forecast_days=1";

// =========================================================================
// 4. PALETA CROMÁTICA DE INSTRUMENTACIÓN DIGITAL (RGB565 PARA ILI9341)
// =========================================================================

#define COLOR_BG          0x0862  // Fondo principal azul pizarra oscuro
#define COLOR_HEADER      0x0925  // Barras estructurales superior e inferior
#define COLOR_CARD        0x1166  // Superficie de tarjetas de información
#define COLOR_CARD_DARK   0x08E4  // Recuadros internos y controles inactivos
#define COLOR_BORDER      0x230B  // Contorno sutil de separación visual
#define COLOR_GREEN       0x16EA  // Verde estado óptimo / bomba activa
#define COLOR_CYAN        0x06BF  // Cian agua: infiltración / datos hídricos
#define COLOR_AMBER       0xFD60  // Ámbar atención / modo automático
#define COLOR_RED         0xF986  // Rojo coral: protección activada / fallo sensor
#define COLOR_TEXT_MUTED  0x9D35  // Texto secundario de alto contraste
#define COLOR_WHITE       0xFFFF
#define COLOR_BLACK       0x0000

// =========================================================================
// 5. TIPOS Y ENUMERACIONES DE ESTADO COMPARTIDAS
// =========================================================================

// Cuatro pestañas principales de la interfaz táctil local TFT
enum PestanaTFT {
  PESTANA_RIEGO       = 0,
  PESTANA_AMBIENTE    = 1,
  PESTANA_AJUSTES     = 2,
  PESTANA_DIAGNOSTICO = 3
};

// Máquina finita de 4 estados del control automático de riego
enum EstadoRiego {
  ESTADO_ESPERA       = 0,  // Supervisión pasiva con bomba apagada
  ESTADO_RIEGO        = 1,  // Pulso o activación de bombeo en curso
  ESTADO_INFILTRACION = 2,  // Pausa de 10 s entre pulsos para percolación
  ESTADO_ALERTA       = 3   // Bloqueo seguro hasta restablecimiento verificado
};

// Causas verificables de alerta hidráulica o eléctrica
enum CodigoAlerta {
  ALERTA_NINGUNA        = 0,
  ALERTA_ANOMALIA_RIEGO = 1, // 20 s acumulados de bombeo activo sin subida >= +2%
  ALERTA_TIEMPO_MAXIMO  = 2, // Límite continuo (25 s) o total de ciclo (60 s)
  ALERTA_FALLO_SUELO    = 3  // Señal analógica inválida o desconectada en GPIO 34
};

// Estado de salud del sensor ambiental DHT22
enum EstadoSensorDHT {
  DHT_INICIANDO     = 0,
  DHT_OPERATIVO     = 1,
  DHT_RANGO_INUSUAL = 2,
  DHT_SIN_RESPUESTA = 3
};

#endif // CONFIGURACION_H
