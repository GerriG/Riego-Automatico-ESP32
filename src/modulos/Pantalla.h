/**
 * ============================================================================
 * ARCHIVO     : Pantalla.h
 * PROYECTO    : SISTEMA DE RIEGO AUTOMÁTICO
 * ----------------------------------------------------------------------------
 * RESPONSABILIDAD DEL MÓDULO:
 *   Gestiona la interfaz gráfica local de 320x240 píxeles sobre la pantalla
 *   TFT ILI9341 (bus VSPI), el control PWM de retroiluminación (GPIO 21), la
 *   lectura táctil resistiva XPT2046 en bus HSPI independiente y la navegación
 *   sin parpadeo entre las 4 pestañas operativas:
 *     0. RIEGO (SISTEMA RIEGO)
 *     1. AMBIENTE
 *     2. AJUSTES (7 controles individuales sin scroll)
 *     3. DIAGN. (DIAGNOSTICO + reinicio en caliente de pantalla)
 *
 * COMPONENTES QUE GESTIONA:
 *   - Pantalla TFT ILI9341 (CS=5, DC=22, RST=17, LED=21, VSPI).
 *   - Digitalizador táctil XPT2046 (CLK=32, CS=33, DIN=25, DO=26, IRQ=27, HSPI).
 *
 * COMUNICACIÓN CON OTROS MÓDULOS:
 *   Lee el estado único de Sensores, ControlRiego y ServidorWeb, y delega toda
 *   acción de mando (bomba manual, modo, rearme y NVS) al módulo ControlRiego.
 * ============================================================================
 */

#ifndef PANTALLA_H
#define PANTALLA_H

#include "Configuracion.h"

// Estado compartido de navegación y solicitud de refresco visual sin parpadeo
extern PestanaTFT pestanaActual;
extern bool       forzarRedibujoTFT;

// Inicialización de hardware gráfico, brillo PWM y panel táctil
void inicializarPantallaTFT();
void aplicarBrilloPantalla(uint8_t duty);
void reiniciarPantallaTFT();
void mostrarAvisoTFT(const char* texto);

// Renderizado estructurado y actualización selectiva en memoria GRAM
void dibujarMarcoFijoTFT();
void dibujarBarraPestanasTFT();
void cambiarPestanaTFT(PestanaTFT nueva);
void dibujarBotonAjusteTFT(int idx);
void dibujarEstructuraPestanaActual();
void actualizarCabeceraTFT(bool forzar);
void actualizarDatosPantallaTFT(bool forzar);

// Adquisición y despacho de eventos táctiles
void atenderPantallaTactil();
void procesarToqueTFT(int16_t x, int16_t y);

#endif // PANTALLA_H
