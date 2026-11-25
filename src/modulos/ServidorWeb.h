/**
 * ============================================================================
 * ARCHIVO     : ServidorWeb.h
 * PROYECTO    : SISTEMA DE RIEGO AUTOMÁTICO
 * ----------------------------------------------------------------------------
 * RESPONSABILIDAD DEL MÓDULO:
 *   Gestiona las comunicaciones inalámbricas y la supervisión remota en LAN:
 *     - Conexión y reconexión cooperativa a dos redes Wi-Fi configuradas.
 *     - Sincronización horaria NTP (UTC-6 El Salvador).
 *     - Consulta meteorológica externa a Open-Meteo inhibida durante cualquier
 *       ciclo activo de riego o pausa de infiltración.
 *     - Servidor HTTP local (puerto 80) y los 10 endpoints de la API REST JSON.
 *
 * COMPONENTES QUE GESTIONA:
 *   - Transceptor Wi-Fi interno del ESP32 (modo WIFI_STA).
 *   - WebServer HTTP en puerto 80 y cliente HTTPClient para Open-Meteo.
 *
 * COMUNICACIÓN CON OTROS MÓDULOS:
 *   Sirve INDEX_HTML desde DashboardWeb, serializa en "/api/data" el estado
 *   único de Sensores y ControlRiego, y delega todas las acciones POST a las
 *   funciones centralizadas de ControlRiego y Pantalla.
 * ============================================================================
 */

#ifndef SERVIDOR_WEB_H
#define SERVIDOR_WEB_H

#include "Configuracion.h"

// Estado de sincronización de reloj NTP (consultado por cabecera TFT y API JSON)
extern bool ntpSincronizado;

void iniciarRedYServidorWeb();
void configurarServidorWeb();
void atenderClientesWeb();
void gestionarWiFiYClima();
void consultarOpenMeteo();
bool solicitudControlAutorizada();

#endif // SERVIDOR_WEB_H
