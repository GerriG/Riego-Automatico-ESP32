# Sistema de riego automático

**Sistema de control y supervisión mediante ESP32**  
Proyecto final de la asignatura **Sistemas Digitales** — Ingeniería en Sistemas y Computación (**Universidad Tecnológica de El Salvador, UTEC**).

Sistema digital embebido de control automático en lazo cerrado basado en un microcontrolador **ESP32 DevKit V1**, organizado en módulos C/C++ independientes para separar con claridad la adquisición de sensores (`ADC1` y `DHT22`), el algoritmo de control hidráulico con histéresis, riego por pulsos, modulación **PWM (`5000 Hz`)**, *Soft Start* y protección `Dry-Run` acumulada sobre etapa **MOSFET `LR7843`**, la interfaz táctil local (`ILI9341` + `XPT2046` en buses SPI segregados), el servidor HTTP/REST local y el código HTML/CSS/JS del Dashboard Web en memoria Flash (`PROGMEM`).

---

## 1. Arquitectura Modular de Archivos del Proyecto

En la raíz del directorio se mantienen únicamente el programa principal `.ino` y este `README.md`, mientras que todas las dependencias y documentos complementarios se agrupan en `src/modulos/` (ubicada dentro de `src/` conforme a la especificación de Arduino IDE para compilación recursiva automática de archivos `.cpp`):

| Archivo | Responsabilidad dentro del proyecto |
| :--- | :--- |
| [Proyecto_Riego_Smart_UI_Web.ino](file:///c:/Users/gerri/Documents/Arduino/Proyecto_Riego_Smart_UI_Web/Proyecto_Riego_Smart_UI_Web.ino) | Punto de entrada en la raíz (`setup()` y `loop()`) y planificador cooperativo general mediante `millis()`. *(Conserva el nombre de la carpeta principal tal como exige Arduino IDE).* |
| [Configuracion.h](file:///c:/Users/gerri/Documents/Arduino/Proyecto_Riego_Smart_UI_Web/src/modulos/Configuracion.h) | Mapa definitivo de pines `GPIO`, parámetros `PWM` (`5000 Hz`, `8 bits`), calibración del sensor capacitivo y panel táctil, tiempos hidráulicos, paleta `RGB565` y enumeraciones de estado. |
| [ControlRiego.h](file:///c:/Users/gerri/Documents/Arduino/Proyecto_Riego_Smart_UI_Web/src/modulos/ControlRiego.h) / [ControlRiego.cpp](file:///c:/Users/gerri/Documents/Arduino/Proyecto_Riego_Smart_UI_Web/src/modulos/ControlRiego.cpp) | Máquina finita de 4 estados, evaluación de umbrales, histéresis, riego por pulsos (`6 s ON / 10 s pausa`), control manual centralizado, *Soft Start* PWM, contador acumulativo `Dry-Run` (`20 s`), límite continuo (`25 s`), rearme verificado de alertas y persistencia `NVS` (`Preferences`). |
| [Sensores.h](file:///c:/Users/gerri/Documents/Arduino/Proyecto_Riego_Smart_UI_Web/src/modulos/Sensores.h) / [Sensores.cpp](file:///c:/Users/gerri/Documents/Arduino/Proyecto_Riego_Smart_UI_Web/src/modulos/Sensores.cpp) | Adquisición, filtrado y validación eléctrica del sensor capacitivo (`GPIO 34`) y del `DHT22` (`GPIO 13`), cálculo de `VPD` e índice de calor, y Sistema Adaptativo de Compensación Climática basado en reglas (distinguiendo compensación calculada y efectiva aplicada). |
| [Pantalla.h](file:///c:/Users/gerri/Documents/Arduino/Proyecto_Riego_Smart_UI_Web/src/modulos/Pantalla.h) / [Pantalla.cpp](file:///c:/Users/gerri/Documents/Arduino/Proyecto_Riego_Smart_UI_Web/src/modulos/Pantalla.cpp) | Controlador gráfico `ILI9341` (`VSPI`), digitalizador táctil `XPT2046` (`HSPI`), brillo PWM, reinicio en caliente (`GPIO 17`) y renderizado sin parpadeo de las 4 pestañas locales (`SISTEMA RIEGO`, `AMBIENTE`, `AJUSTES`, `DIAGN.`). |
| [ServidorWeb.h](file:///c:/Users/gerri/Documents/Arduino/Proyecto_Riego_Smart_UI_Web/src/modulos/ServidorWeb.h) / [ServidorWeb.cpp](file:///c:/Users/gerri/Documents/Arduino/Proyecto_Riego_Smart_UI_Web/src/modulos/ServidorWeb.cpp) | Conexión Wi-Fi redundante, sincronización `NTP`, cliente `Open-Meteo` (`setConnectTimeout` + `setTimeout`, inhibido durante riego e infiltración), servidor HTTP local (`puerto 80`) y los 10 endpoints de la API REST JSON. |
| [DashboardWeb.h](file:///c:/Users/gerri/Documents/Arduino/Proyecto_Riego_Smart_UI_Web/src/modulos/DashboardWeb.h) / [DashboardWeb.cpp](file:///c:/Users/gerri/Documents/Arduino/Proyecto_Riego_Smart_UI_Web/src/modulos/DashboardWeb.cpp) | Código íntegro HTML5, CSS3 *responsive*, iconos SVG y lógica JavaScript del Dashboard Web almacenado en `PROGMEM`, editable sin tocar la lógica C++ de Arduino. |
| [credenciales.example.h](file:///c:/Users/gerri/Documents/Arduino/Proyecto_Riego_Smart_UI_Web/src/modulos/credenciales.example.h) | Plantilla limpia sin contraseñas reales para configurar `SSID_1`/`PASS_1` y `SSID_2`/`PASS_2` en `src/modulos/credenciales.h` (excluido por [.gitignore](file:///c:/Users/gerri/Documents/Arduino/Proyecto_Riego_Smart_UI_Web/.gitignore)). |
| [GUIA_ESTUDIO_SISTEMA_DE_RIEGO_AUTOMATICO.md](file:///c:/Users/gerri/Documents/Arduino/Proyecto_Riego_Smart_UI_Web/src/modulos/GUIA_ESTUDIO_SISTEMA_DE_RIEGO_AUTOMATICO.md) | Guía técnica educativa para la defensa de los 6 integrantes, referenciada directamente a cada módulo `.h/.cpp`. |
| [PRUEBAS_VALIDACION_ESP32.md](file:///c:/Users/gerri/Documents/Arduino/Proyecto_Riego_Smart_UI_Web/src/modulos/PRUEBAS_VALIDACION_ESP32.md) | Protocolo de 25 pruebas diferenciando validación compilada, simulada y física pendiente. |

---

## 2. Entorno de Compilación y Bibliotecas Verificadas

Compilado y verificado en **Arduino IDE 2.x / `arduino-cli`** para **`ESP32 Dev Module`** (`FQBN: esp32:esp32:esp32`):

| Paquete / Biblioteca | Versión verificada | Módulos donde interviene |
| :--- | :---: | :--- |
| **Núcleo `esp32:esp32`** | `2.0.17` *(compatible con `3.x`)* | Todos (`ADC1`, `LEDC PWM`, `SPI`, `WiFi`, `WebServer`, `HTTPClient`, `Preferences`) |
| `Adafruit GFX Library` + `Adafruit ILI9341` | `1.12.6` / `1.6.4` | `Pantalla.cpp` (bus `VSPI` `320 × 240`) |
| `XPT2046_Touchscreen` | `1.4` | `Pantalla.cpp` (bus `HSPI` independiente) |
| `DHT sensor library` + `Adafruit Unified Sensor` | `1.4.7` / `1.1.15` | `Sensores.cpp` (`GPIO 13`) |
| `ArduinoJson` | `7.4.3` | `ServidorWeb.cpp` (`/api/data` y respuesta de `Open-Meteo`) |

---

## 3. Mapa Definitivo de Conexiones de Hardware (ESP32 + Placa de Expansión)

| Subsistema | Señal del módulo | Pin ESP32 | Módulo de software / Notas |
| :--- | :--- | :---: | :--- |
| **Pantalla TFT `ILI9341` (`VSPI`)** | `CS`, `DC`, `RST`, `LED` | `GPIO 5`, `22`, `17`, `21` | `Pantalla.cpp` · `VCC` a `5V` interno; brillo PWM en `Canal 0` |
| **Bus SPI de video (`VSPI`)** | `MOSI`, `MISO`, `SCK` | `GPIO 23`, `19`, `18` | `Pantalla.cpp` · Líneas nativas de hardware `VSPI` |
| **Panel táctil `XPT2046` (`HSPI`)** | `T_CLK`, `T_CS`, `T_DIN`, `T_DO`, `T_IRQ` | `GPIO 32`, `33`, `25`, `26`, `27` | `Pantalla.cpp` · Bus `HSPI` separado sin colisión con video |
| **Sensor capacitivo de suelo v2.0** | `AOUT` (`Signal`) | `GPIO 34` (`ADC1_CH6`) | `Sensores.cpp` · `3.3V` y `GND`; compatible con Wi-Fi activo |
| **Sensor ambiental `DHT22`** | `DAT` (`Out`) | `GPIO 13` | `Sensores.cpp` · `3.3V` y `GND` con *pull-up* |
| **Módulo MOSFET `LR7843`** | `PWM` / `GND` lógico | `GPIO 4` / `GND` | `ControlRiego.cpp` · `Canal 1` (`5000 Hz`, `8 bits`) + microbomba `5V` y diodo *Flyback* |

---

## 4. Puesta en Marcha y Operación

1. **Credenciales privadas:** si no existe `src/modulos/credenciales.h`, copie `src/modulos/credenciales.example.h` como `src/modulos/credenciales.h` e ingrese sus dos redes Wi-Fi locales.
2. **Apertura en Arduino IDE:** abra `Proyecto_Riego_Smart_UI_Web.ino` en la raíz; Arduino IDE compilará automáticamente de forma recursiva todos los módulos `.h` y `.cpp` dentro de `src/modulos/`. Seleccione **`ESP32 Dev Module`** y pulse **Subir**.
3. **Conectividad local frente a Internet externo:** el control de riego y la supervisión local (pantalla TFT y Dashboard Web en `http://<IP_LOCAL_ESP32>/` desde `PROGMEM`) siguen funcionando sin conexión a Internet; únicamente la consulta meteorológica a `Open-Meteo` requiere conectividad externa, y se inhibe siempre que exista un ciclo de riego o pausa de infiltración en curso.
4. **Seguridad honesta en red local:** los endpoints `POST` verifican subred privada `RFC 1918`, catálogo estricto de parámetros, compuerta única `puedeActivarBomba()` en `ControlRiego.cpp` y cabecera `X-Control-Local: ESP32` (mitigación básica contra *CSRF* accidental en LAN; al enviarse desde JavaScript es visible en el navegador y no equivale a autenticación criptográfica).
