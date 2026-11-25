/**
 * ============================================================================
 * ARCHIVO     : DashboardWeb.h
 * PROYECTO    : SISTEMA DE RIEGO AUTOMÁTICO
 * ----------------------------------------------------------------------------
 * RESPONSABILIDAD DEL MÓDULO:
 *   Declara la cadena constante en memoria de programa (PROGMEM) que aloja el
 *   código completo HTML5, CSS3, gráficos vectoriales SVG y lógica JavaScript
 *   del Dashboard Web de supervisión y control mediante ESP32.
 *
 * COMUNICACIÓN CON OTROS MÓDULOS:
 *   Consumido exclusivamente por ServidorWeb.cpp para servir la ruta raíz
 *   "GET /" desde la memoria Flash sin ocupar RAM dinámica ni requerir CDNs,
 *   sistemas de archivos externos (SPIFFS/LittleFS) ni salida a Internet.
 * ============================================================================
 */

#ifndef DASHBOARD_WEB_H
#define DASHBOARD_WEB_H

#include <Arduino.h>

extern const char INDEX_HTML[] PROGMEM;

#endif // DASHBOARD_WEB_H
