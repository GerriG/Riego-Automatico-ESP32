/**
 * ============================================================================
 * ARCHIVO     : DashboardWeb.cpp
 * PROYECTO    : SISTEMA DE RIEGO AUTOMÁTICO
 * SUBTÍTULO   : Sistema de control y supervisión mediante ESP32
 * ----------------------------------------------------------------------------
 * RESPONSABILIDAD DEL MÓDULO:
 *   Contiene íntegramente la aplicación web estática (HTML5, hojas de estilo
 *   CSS3 responsive, iconografía vectorial SVG y cliente JavaScript de
 *   telemetría y control) almacenada en memoria Flash (PROGMEM).
 *
 * MANTENIMIENTO:
 *   Al residir en un archivo independiente, permite editar la presentación
 *   visual o los textos de la interfaz web sin recorrer ni modificar la lógica
 *   C++ de sensores, máquina de estados ni controladores de hardware.
 * ============================================================================
 */

#include "DashboardWeb.h"
const char INDEX_HTML[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html lang="es">
<head>
<meta charset="UTF-8">
<meta name="viewport" content="width=device-width, initial-scale=1.0">
<title>Sistema de riego automático — Control y supervisión mediante ESP32</title>
<style>
:root {
  --bg-deep: #060e1a;
  --bg-mid: #0b192c;
  --panel: rgba(14, 29, 49, 0.88);
  --surface: rgba(7, 17, 31, 0.58);
  --border: rgba(56, 189, 248, 0.20);
  --border-strong: rgba(56, 189, 248, 0.40);
  --text: #f1f5f9;
  --muted: #94a3b8;
  --water: #0ea5e9;
  --cyan: #22d3ee;
  --emerald: #10b981;
  --amber: #f59e0b;
  --red: #ef4444;
  --shadow: 0 14px 34px rgba(2, 8, 20, 0.42);
}
* { box-sizing: border-box; }
body {
  margin: 0; padding: 18px 16px 32px; min-height: 100vh;
  font-family: system-ui, -apple-system, 'Segoe UI', Roboto, Ubuntu, sans-serif;
  color: var(--text);
  background:
    radial-gradient(circle at 12% 0%, rgba(14, 165, 233, 0.18) 0%, transparent 46%),
    radial-gradient(circle at 88% 8%, rgba(16, 185, 129, 0.12) 0%, transparent 42%),
    linear-gradient(180deg, var(--bg-mid) 0%, var(--bg-deep) 100%);
}
.shell { max-width: 1200px; margin: 0 auto; display: flex; flex-direction: column; gap: 16px; }
.topbar {
  background: var(--panel); border: 1px solid var(--border-strong); border-radius: 20px;
  padding: 18px 22px; display: flex; justify-content: space-between; align-items: center;
  flex-wrap: wrap; gap: 14px; box-shadow: var(--shadow); backdrop-filter: blur(12px);
}
.identity { display: flex; align-items: center; gap: 14px; }
.emblem {
  width: 48px; height: 48px; border-radius: 14px; flex-shrink: 0;
  background: linear-gradient(135deg, rgba(14, 165, 233, 0.26), rgba(34, 211, 238, 0.12));
  border: 1px solid rgba(56, 189, 248, 0.48); display: grid; place-items: center;
  box-shadow: inset 0 0 16px rgba(34, 211, 238, 0.16);
}
.identity h1 { margin: 0; font-size: 1.28rem; font-weight: 800; letter-spacing: 0.2px; color: #fff; }
.identity p { margin: 3px 0 0; font-size: 0.82rem; color: var(--cyan); font-weight: 600; letter-spacing: 0.3px; }
.telemetry-strip { display: flex; flex-wrap: wrap; gap: 8px; align-items: center; }
.pill {
  display: inline-flex; align-items: center; gap: 7px; padding: 7px 12px; border-radius: 999px;
  background: var(--surface); border: 1px solid rgba(148, 163, 184, 0.18);
  font-size: 0.79rem; font-weight: 600; color: #e2e8f0;
}
.pill-mcu { border-color: rgba(56, 189, 248, 0.38); color: var(--cyan); background: rgba(14, 165, 233, 0.12); }
.dot { width: 9px; height: 9px; border-radius: 50%; background: var(--emerald); box-shadow: 0 0 8px rgba(16, 185, 129, 0.7); }
.sync-dot { width: 8px; height: 8px; border-radius: 50%; background: var(--cyan); transition: transform 0.22s ease; }
.alert-banner {
  display: none; align-items: center; justify-content: space-between; flex-wrap: wrap; gap: 14px;
  padding: 15px 20px; border-radius: 16px; color: #fee2e2;
  background: linear-gradient(90deg, rgba(239, 68, 68, 0.24), rgba(153, 27, 27, 0.22));
  border: 1px solid rgba(248, 113, 113, 0.75); box-shadow: 0 10px 26px rgba(239, 68, 68, 0.18);
}
.alert-banner strong { display: block; font-size: 0.95rem; margin-bottom: 3px; color: #fff; }
.toast {
  display: none; align-items: center; justify-content: space-between; gap: 10px;
  padding: 12px 16px; border-radius: 13px; font-size: 0.87rem; font-weight: 600;
  border: 1px solid var(--cyan); background: rgba(14, 165, 233, 0.18); color: #e0f2fe;
}
.hero-grid { display: grid; grid-template-columns: 1.32fr 1fr; gap: 16px; }
.env-grid { display: grid; grid-template-columns: repeat(3, 1fr); gap: 14px; }
.split-grid { display: grid; grid-template-columns: 1fr 1.18fr; gap: 16px; }
.diag-grid { display: grid; grid-template-columns: repeat(3, 1fr); gap: 12px; }
@media (max-width: 960px) {
  .hero-grid, .split-grid { grid-template-columns: 1fr; }
  .env-grid, .diag-grid { grid-template-columns: 1fr 1fr; }
}
@media (max-width: 640px) {
  body { padding: 12px 10px 26px; }
  .topbar { padding: 15px; }
  .env-grid, .diag-grid { grid-template-columns: 1fr; }
  .btn-row, .thresh-grid { grid-template-columns: 1fr !important; }
}
.card {
  background: var(--panel); border: 1px solid var(--border); border-radius: 20px;
  padding: 20px; box-shadow: var(--shadow); display: flex; flex-direction: column; gap: 14px;
}
.card-hero {
  border-color: var(--border-strong);
  background: linear-gradient(165deg, rgba(16, 36, 62, 0.94) 0%, rgba(10, 23, 40, 0.92) 100%);
}
.section-head {
  display: flex; justify-content: space-between; align-items: center; flex-wrap: wrap; gap: 8px;
  padding-bottom: 10px; border-bottom: 1px solid rgba(148, 163, 184, 0.12);
}
.section-title {
  margin: 0; display: flex; align-items: center; gap: 8px; font-size: 0.85rem;
  font-weight: 700; text-transform: uppercase; letter-spacing: 0.9px; color: var(--cyan);
}
.section-badge {
  font-size: 0.75rem; font-weight: 600; padding: 4px 10px; border-radius: 999px;
  background: var(--surface); border: 1px solid rgba(148, 163, 184, 0.18); color: var(--muted);
}
.moisture-body { display: flex; align-items: center; gap: 24px; flex-wrap: wrap; }
.gauge-box { position: relative; width: 168px; height: 168px; flex-shrink: 0; margin: 0 auto; }
.gauge-box svg { width: 168px; height: 168px; transform: rotate(-90deg); }
.gauge-center {
  position: absolute; inset: 0; display: flex; flex-direction: column;
  align-items: center; justify-content: center; text-align: center;
}
.gauge-pct { font-size: 2.55rem; font-weight: 800; line-height: 1; letter-spacing: -0.5px; }
.gauge-caption { font-size: 0.74rem; color: var(--muted); margin-top: 6px; font-weight: 600; }
.moisture-info { flex: 1; min-width: 240px; display: flex; flex-direction: column; gap: 11px; }
.state-row { display: flex; align-items: center; gap: 10px; flex-wrap: wrap; }
.status-chip {
  display: inline-flex; align-items: center; padding: 5px 13px; border-radius: 999px;
  font-size: 0.83rem; font-weight: 700; background: rgba(16, 185, 129, 0.18);
  color: #6ee7b7; border: 1px solid rgba(16, 185, 129, 0.35);
}
.decision-box {
  padding: 11px 14px; border-radius: 13px; background: var(--surface);
  border-left: 4px solid var(--water); display: flex; flex-direction: column; gap: 3px;
}
.decision-box strong { font-size: 0.88rem; color: #fff; }
.decision-box span { font-size: 0.8rem; color: var(--muted); line-height: 1.35; }
.bar-wrap { display: flex; flex-direction: column; gap: 6px; }
.bar-track {
  position: relative; height: 14px; border-radius: 999px;
  background: rgba(15, 23, 42, 0.85); border: 1px solid rgba(148, 163, 184, 0.2); overflow: visible;
}
.bar-fill {
  height: 100%; width: 0%; border-radius: 999px;
  background: linear-gradient(90deg, var(--water), var(--emerald)); transition: width 0.35s ease;
}
.bar-marker {
  position: absolute; top: -4px; bottom: -4px; width: 3px; border-radius: 2px;
  transform: translateX(-50%); pointer-events: none;
}
.marker-inf { background: var(--amber); box-shadow: 0 0 6px rgba(245, 158, 11, 0.8); }
.marker-sup { background: #fff; box-shadow: 0 0 6px rgba(255, 255, 255, 0.8); }
.bar-scale { display: flex; justify-content: space-between; font-size: 0.72rem; color: var(--muted); }
.thresh-grid { display: grid; grid-template-columns: repeat(3, 1fr); gap: 9px; }
.mini-stat {
  background: var(--surface); border: 1px solid rgba(148, 163, 184, 0.12);
  border-radius: 12px; padding: 9px 11px;
}
.mini-stat span { display: block; font-size: 0.72rem; color: var(--muted); margin-bottom: 2px; }
.mini-stat strong { font-size: 0.96rem; color: var(--text); }
.kv-list { display: flex; flex-direction: column; gap: 8px; }
.kv {
  display: flex; justify-content: space-between; align-items: center; gap: 12px;
  padding: 9px 12px; border-radius: 11px; background: var(--surface);
  border: 1px solid rgba(148, 163, 184, 0.1); font-size: 0.86rem;
}
.kv span { color: var(--muted); }
.kv strong { text-align: right; color: var(--text); }
.btn-row { display: grid; grid-template-columns: 1fr 1fr; gap: 10px; margin-top: auto; }
button {
  padding: 12px 14px; border-radius: 12px; border: 1px solid rgba(148, 163, 184, 0.24);
  background: rgba(30, 41, 59, 0.75); color: var(--text); font-family: inherit;
  font-weight: 700; font-size: 0.84rem; cursor: pointer;
  transition: transform 0.12s ease, background 0.16s ease, border-color 0.16s ease;
}
button:hover:not(:disabled) { background: rgba(51, 65, 85, 0.9); border-color: rgba(56, 189, 248, 0.45); }
button:active:not(:disabled) { transform: scale(0.99); }
button:disabled { opacity: 0.45; cursor: not-allowed; }
.btn-water { background: linear-gradient(135deg, var(--water), #0284c7); color: #fff; border-color: transparent; }
.btn-water:hover:not(:disabled) { background: linear-gradient(135deg, #38bdf8, var(--water)); }
.btn-amber { background: rgba(245, 158, 11, 0.18); color: #fde68a; border-color: rgba(245, 158, 11, 0.48); }
.btn-red { background: linear-gradient(135deg, var(--red), #dc2626); color: #fff; border-color: transparent; }
.subcard {
  background: var(--surface); border: 1px solid rgba(148, 163, 184, 0.14);
  border-radius: 15px; padding: 15px 16px; display: flex; flex-direction: column; gap: 9px;
}
.subcard-head { display: flex; justify-content: space-between; align-items: center; font-size: 0.8rem; font-weight: 700; color: #cbd5e1; }
.metric-big { font-size: 1.95rem; font-weight: 800; line-height: 1.1; color: #fff; }
.explain { margin: 0; font-size: 0.8rem; color: var(--muted); line-height: 1.4; }
.cfg-stack { display: flex; flex-direction: column; gap: 11px; }
.cfg-group { display: flex; flex-direction: column; gap: 5px; }
.cfg-label { font-size: 0.79rem; color: #cbd5e1; font-weight: 600; }
.seg { display: flex; gap: 7px; flex-wrap: wrap; }
.seg button { flex: 1; min-width: 82px; padding: 9px 10px; font-size: 0.79rem; background: var(--surface); }
.seg button.active {
  background: linear-gradient(135deg, var(--water), var(--cyan));
  color: #041322; border-color: transparent; box-shadow: 0 4px 14px rgba(14, 165, 233, 0.28);
}
.footer-bar { display: flex; justify-content: space-between; align-items: center; flex-wrap: wrap; gap: 10px; padding-top: 6px; }
.footer-note { font-size: 0.77rem; color: var(--muted); }
</style>
</head>
<body>
<div class="shell">
  <!-- A. ENCABEZADO -->
  <header class="topbar">
    <div class="identity">
      <div class="emblem" aria-hidden="true">
        <svg width="26" height="26" viewBox="0 0 24 24" fill="none" stroke="#22d3ee" stroke-width="1.9" stroke-linecap="round" stroke-linejoin="round">
          <rect x="6" y="6" width="12" height="12" rx="2.5" stroke="#38bdf8"/>
          <path d="M9 2v3M15 2v3M9 19v3M15 19v3M2 9h3M2 15h3M19 9h3M19 15h3" stroke="#64748b"/>
          <path d="M12 8.6c0 0-2.7 3.1-2.7 4.9a2.7 2.7 0 0 0 5.4 0c0-1.8-2.7-4.9-2.7-4.9z" fill="rgba(34,211,238,0.25)" stroke="#22d3ee"/>
        </svg>
      </div>
      <div>
        <h1>Sistema de riego automático</h1>
        <p>Sistema de control y supervisión mediante ESP32</p>
      </div>
    </div>
    <div class="telemetry-strip">
      <span class="pill pill-mcu" id="mcuPill">ESP32 DevKit V1</span>
      <span class="pill"><span class="dot" id="connDot"></span><span id="connTxt">Conectando...</span></span>
      <span class="pill" id="ipPill">IP local: --</span>
      <span class="pill"><span class="sync-dot" id="syncDot"></span><span id="updPill">Sincronizando...</span></span>
    </div>
  </header>

  <div class="alert-banner" id="alertBanner">
    <div>
      <strong>Protección hidráulica activada — Bomba bloqueada por seguridad</strong>
      <span id="alertMsgTxt">--</span>
    </div>
    <button class="btn-red" onclick="resetAlert()">Restablecer protección</button>
  </div>

  <div class="toast" id="toastBox"></div>

  <!-- B y C. PANEL PRINCIPAL DE HUMEDAD Y PANEL DE CONTROL -->
  <section class="hero-grid">
    <article class="card card-hero">
      <div class="section-head">
        <h2 class="section-title">Nivel de humedad del sustrato</h2>
        <span class="section-badge" id="calibBadge">Índice relativo calibrado (ADC 12 bits)</span>
      </div>
      <div class="moisture-body">
        <div class="gauge-box">
          <svg viewBox="0 0 128 128">
            <circle cx="64" cy="64" r="52" stroke="rgba(148,163,184,0.14)" stroke-width="11" fill="none"/>
            <circle id="gaugeArc" cx="64" cy="64" r="52" stroke="#0ea5e9" stroke-width="11"
                    stroke-linecap="round" fill="none" stroke-dasharray="326.73" stroke-dashoffset="326.73"/>
          </svg>
          <div class="gauge-center">
            <div class="gauge-pct" id="soilPctVal">--%</div>
            <div class="gauge-caption" id="soilAdcSub">Lectura ADC: --</div>
          </div>
        </div>
        <div class="moisture-info">
          <div class="state-row">
            <span class="status-chip" id="soilStateChip">Verificando sensor...</span>
          </div>
          <div class="decision-box" id="needWaterBox">
            <strong id="needWaterTitle">Evaluando necesidad de riego...</strong>
            <span id="needWaterDesc">Esperando telemetría del microcontrolador ESP32.</span>
          </div>
          <div class="bar-wrap">
            <div class="bar-track">
              <div class="bar-fill" id="soilBarFill"></div>
              <div class="bar-marker marker-inf" id="markInf" title="Umbral inferior de activación"></div>
              <div class="bar-marker marker-sup" id="markSup" title="Umbral superior de finalización"></div>
            </div>
            <div class="bar-scale">
              <span>0% Seco</span>
              <span id="scaleMarks">Activar / Finalizar</span>
              <span>100% Saturado</span>
            </div>
          </div>
          <div class="thresh-grid">
            <div class="mini-stat"><span>Umbral inferior (activar)</span><strong id="uInfVal">&lt; --%</strong></div>
            <div class="mini-stat"><span>Umbral superior (finalizar)</span><strong id="uSupVal">&ge; --%</strong></div>
            <div class="mini-stat"><span>Referencia activa</span><strong id="dynNote">--</strong></div>
          </div>
        </div>
      </div>
    </article>

    <article class="card">
      <div class="section-head">
        <h2 class="section-title">Panel de control hidráulico</h2>
        <span class="section-badge" id="sysStateBadge">EN ESPERA</span>
      </div>
      <div class="kv-list">
        <div class="kv"><span>Modo de operación</span><strong id="modeStateTxt">--</strong></div>
        <div class="kv"><span>Estado de la bomba (PWM 5000 Hz)</span><strong id="pumpStateTxt">--</strong></div>
        <div class="kv"><span>Estado de infiltración / ciclo</span><strong id="phaseTxt">--</strong></div>
        <div class="kv"><span>Estado de las protecciones</span><strong id="protStateTxt">--</strong></div>
        <div class="kv"><span>Dry-Run (acumulado entre pulsos, máx. 20 s)</span><strong id="dryRunTxt">--</strong></div>
        <div class="kv"><span>Bombeo continuo sin pausa (máx. 25 s)</span><strong id="contLimitTxt">--</strong></div>
      </div>
      <div class="btn-row">
        <button id="btnPumpToggle" class="btn-water" onclick="handleManualPump()">Encender bomba manual</button>
        <button id="btnModeToggle" onclick="handleModeSwitch()">Cambiar modo de operación</button>
      </div>
    </article>
  </section>

  <!-- D. PANEL AMBIENTAL -->
  <section class="card">
    <div class="section-head">
      <h2 class="section-title">Monitoreo ambiental local (sensor DHT22)</h2>
      <span class="section-badge" id="dhtStateBadge">Estado DHT22: --</span>
    </div>
    <div class="env-grid">
      <div class="subcard">
        <div class="subcard-head">
          <span>Temperatura ambiental</span>
          <svg width="18" height="18" viewBox="0 0 24 24" fill="none" stroke="#f59e0b" stroke-width="2"><path d="M14 14.76V3.5a2.5 2.5 0 0 0-5 0v11.26a4.5 4.5 0 1 0 5 0z"/></svg>
        </div>
        <div class="metric-big" id="tempVal">--</div>
        <div class="kv"><span>Índice de calor</span><strong id="hiVal">--</strong></div>
        <p class="explain" id="tempExplain">Muestreo digital en GPIO 13 cada 2.5 segundos.</p>
      </div>
      <div class="subcard">
        <div class="subcard-head">
          <span>Humedad relativa del aire</span>
          <svg width="18" height="18" viewBox="0 0 24 24" fill="none" stroke="#0ea5e9" stroke-width="2"><path d="M12 2.7c0 0-7 7.8-7 12.3a7 7 0 0 0 14 0c0-4.5-7-12.3-7-12.3z"/></svg>
        </div>
        <div class="metric-big" id="humVal">--</div>
        <div class="kv"><span>Rango operativo</span><strong>0 a 100% HR</strong></div>
        <p class="explain">Porcentaje de vapor de agua presente en el aire del entorno.</p>
      </div>
      <div class="subcard">
        <div class="subcard-head">
          <span>Déficit de presión de vapor (VPD)</span>
          <svg width="18" height="18" viewBox="0 0 24 24" fill="none" stroke="#22d3ee" stroke-width="2"><path d="M3 8h13a3 3 0 1 0-3-3M3 12h15a3 3 0 1 1-3 3M3 16h9"/></svg>
        </div>
        <div class="metric-big" id="vpdVal">--</div>
        <div class="kv"><span>Demanda de evaporación</span><strong id="vpdTag">--</strong></div>
        <p class="explain" id="vpdExplain">Indica la capacidad de secado del aire según la ecuación de Tetens.</p>
      </div>
    </div>
  </section>

  <!-- E y F. COMPENSACIÓN CLIMÁTICA Y CONFIGURACIÓN -->
  <section class="split-grid">
    <article class="card">
      <div class="section-head">
        <h2 class="section-title">Sistema Adaptativo de Compensación Climática basado en reglas</h2>
      </div>
      <p class="explain">Ajusta de forma determinista el umbral inferior entre 20% y 75% según temperatura, VPD y estado meteorológico. No emplea inteligencia artificial entrenada y solo riega cuando el sustrato lo requiere.</p>
      <div class="kv-list">
        <div class="kv"><span>Umbral base configurado</span><strong id="csBase">--</strong></div>
        <div class="kv"><span>Compensación calculada (reglas)</span><strong id="csCalcDelta">--</strong></div>
        <div class="kv"><span>Compensación efectiva aplicada</span><strong id="csDelta">--</strong></div>
        <div class="kv"><span>Umbral dinámico resultante</span><strong id="csDyn">--</strong></div>
        <div class="kv"><span>Motivo de la compensación</span><strong id="csFactors">--</strong></div>
        <div class="kv"><span>Estado de Open-Meteo</span><strong id="csWeather">--</strong></div>
        <div class="kv"><span>Disponibilidad meteorológica externa</span><strong id="csWeatherAge">--</strong></div>
      </div>
      <div class="decision-box" style="border-left-color:var(--amber)">
        <span id="csSummary" style="color:#fde68a;font-weight:600">--</span>
      </div>
    </article>

    <article class="card">
      <div class="section-head">
        <h2 class="section-title">Configuración de parámetros (memoria NVS)</h2>
        <span class="section-badge">Guardado persistente</span>
      </div>
      <div class="cfg-stack">
        <div class="cfg-group">
          <span class="cfg-label">Modo de operación del sistema</span>
          <div class="seg">
            <button id="modeAutoBtn" onclick="setCfg('mode',1)">Automático (lazo cerrado)</button>
            <button id="modeManBtn" onclick="setCfg('mode',0)">Manual (supervisado)</button>
          </div>
        </div>
        <div class="cfg-group">
          <span class="cfg-label">Umbral base de activación de humedad</span>
          <div class="seg">
            <button id="base30" onclick="setCfg('base',30)">30% (Bajo)</button>
            <button id="base40" onclick="setCfg('base',40)">40% (Estándar)</button>
            <button id="base55" onclick="setCfg('base',55)">55% (Alto)</button>
          </div>
        </div>
        <div class="cfg-group">
          <span class="cfg-label">Histéresis (margen adicional para finalizar el riego)</span>
          <div class="seg">
            <button id="hyst5" onclick="setCfg('hyst',5)">+5%</button>
            <button id="hyst10" onclick="setCfg('hyst',10)">+10%</button>
            <button id="hyst15" onclick="setCfg('hyst',15)">+15%</button>
          </div>
        </div>
        <div class="cfg-group">
          <span class="cfg-label">Potencia de bomba (control PWM a 5000 Hz)</span>
          <div class="seg">
            <button id="pwm165" onclick="setCfg('pwm',165)">Suave (65%)</button>
            <button id="pwm215" onclick="setCfg('pwm',215)">Normal (84%)</button>
            <button id="pwm255" onclick="setCfg('pwm',255)">Máxima (100%)</button>
          </div>
        </div>
        <div class="cfg-group">
          <span class="cfg-label">Riego por pulsos (6 s encendido / 10 s infiltración)</span>
          <div class="seg">
            <button id="pulseOnBtn" onclick="setCfg('pulse',1)">Pulsos activados</button>
            <button id="pulseOffBtn" onclick="setCfg('pulse',0)">Bombeo continuo</button>
          </div>
        </div>
        <div class="cfg-group">
          <span class="cfg-label">Compensación climática, unidad térmica y pantalla TFT</span>
          <div class="seg">
            <button id="optClima" onclick="sendAction('/api/cfg/clima','Compensación climática actualizada.')">Comp. climática</button>
            <button id="optUnit" onclick="sendAction('/api/cfg/unit','Unidad de temperatura actualizada.')">Unidad °C / °F</button>
            <button id="optBright" onclick="cycleBrightness()">Brillo TFT</button>
          </div>
        </div>
      </div>
    </article>
  </section>

  <!-- G. DIAGNÓSTICO -->
  <section class="card">
    <div class="section-head">
      <h2 class="section-title">Diagnóstico del sistema embebido</h2>
      <span class="section-badge" id="diagServerBadge">Servidor HTTP activo</span>
    </div>
    <div class="diag-grid">
      <div class="kv-list">
        <div class="kv"><span>Sensor capacitivo (GPIO 34)</span><strong id="diagSoil">--</strong></div>
        <div class="kv"><span>Sensor DHT22 (GPIO 13)</span><strong id="diagDht">--</strong></div>
        <div class="kv"><span>Alertas de seguridad</span><strong id="diagAlert">--</strong></div>
      </div>
      <div class="kv-list">
        <div class="kv"><span>Conexión Wi-Fi</span><strong id="diagWifi">--</strong></div>
        <div class="kv"><span>Estado del servidor</span><strong id="diagServer">--</strong></div>
      </div>
      <div class="kv-list">
        <div class="kv"><span>Tiempo de funcionamiento</span><strong id="diagUptime">--</strong></div>
        <div class="kv"><span>Memoria RAM disponible</span><strong id="diagHeap">--</strong></div>
      </div>
    </div>
    <div class="footer-bar">
      <span class="footer-note">El control y la supervisión local (TFT y Dashboard Web en LAN) siguen funcionando sin Internet; Open-Meteo requiere conectividad externa.</span>
      <button onclick="sendAction('/api/tft/recover','Pantalla física TFT reinicializada correctamente.')">Reiniciar pantalla física TFT</button>
    </div>
  </section>
</div>

<script>
let state = null;
let fetching = false;
let cmdBusy = false;
let lastOkMs = 0;

function showToast(msg, isError = false) {
  const box = document.getElementById('toastBox');
  box.style.display = 'flex';
  box.style.borderColor = isError ? 'var(--red)' : 'var(--cyan)';
  box.style.background = isError ? 'rgba(239, 68, 68, 0.20)' : 'rgba(14, 165, 233, 0.18)';
  box.style.color = isError ? '#fee2e2' : '#e0f2fe';
  box.textContent = msg;
  clearTimeout(window._toastTimer);
  window._toastTimer = setTimeout(() => { box.style.display = 'none'; }, 4500);
}

async function sendAction(url, okMsg) {
  if (cmdBusy) return;
  cmdBusy = true;
  try {
    const ctrl = new AbortController();
    const tid = setTimeout(() => ctrl.abort(), 3500);
    const res = await fetch(url, {
      method: 'POST',
      headers: { 'X-Control-Local': 'ESP32' },
      signal: ctrl.signal
    });
    clearTimeout(tid);
    const txt = (await res.text()).trim();
    if (!res.ok) {
      showToast(txt || 'Acción rechazada por las protecciones del ESP32.', true);
    } else {
      const msgFinal = (txt && txt !== 'OK') ? txt : (okMsg || 'Operación aplicada correctamente.');
      showToast(msgFinal, false);
    }
    await pollTelemetry();
  } catch (e) {
    showToast('Sin respuesta del ESP32. Verifique la conexión Wi-Fi local.', true);
  } finally {
    cmdBusy = false;
  }
}

function resetAlert() {
  sendAction('/api/alert/reset', 'Protección restablecida correctamente. La bomba permanece apagada.');
}

function setCfg(param, val) {
  const valid = {
    mode: [0, 1],
    base: [30, 40, 55],
    hyst: [5, 10, 15],
    pwm: [165, 215, 255],
    pulse: [0, 1],
    bright: [90, 165, 230]
  };
  if (!valid[param] || !valid[param].includes(val)) {
    showToast('Parámetro o valor fuera del catálogo permitido.', true);
    return;
  }
  if (param === 'mode' && state) {
    if (state.alertCode !== 0) {
      showToast('Restablezca primero la protección activa antes de cambiar el modo.', true);
      return;
    }
    if (val === 1 && !state.soilValid) {
      showToast('No se puede activar el modo automático sin lectura válida del sensor en GPIO 34.', true);
      return;
    }
  }
  sendAction(`/api/cfg/set?param=${param}&val=${val}`, 'Parámetro actualizado y guardado en memoria NVS.');
}

function cycleBrightness() {
  if (!state) return;
  const next = state.brightPwm <= 100 ? 165 : (state.brightPwm <= 180 ? 230 : 90);
  setCfg('bright', next);
}

function handleManualPump() {
  if (!state) return;
  if (state.alertCode !== 0) {
    showToast('Bomba bloqueada por alerta activa. Revise el sistema y restablezca la protección.', true);
    return;
  }
  if (!state.soilValid && !state.pumpOn) {
    showToast('Acción rechazada: el sensor capacitivo (GPIO 34) no presenta lectura válida.', true);
    return;
  }
  if (!state.pumpOn) {
    if (!confirm('¿Desea pasar a modo MANUAL y encender la bomba ahora?\nProtecciones activas:\n• Dry-Run: corte a los 20 s acumulados de bombeo activo sin subida de humedad (+2%).\n• Límite continuo: corte a los 25 s ininterrumpidos de funcionamiento.')) return;
    sendAction('/api/pump/toggle', 'Bomba encendida en modo manual supervisado.');
  } else {
    sendAction('/api/pump/toggle', 'Bomba apagada correctamente.');
  }
}

function handleModeSwitch() {
  if (!state) return;
  if (state.alertCode !== 0) {
    showToast('Restablezca primero la protección activa antes de cambiar de modo.', true);
    return;
  }
  sendAction('/api/pump/mode', state.autoMode ? 'Sistema cambiado a modo MANUAL.' : 'Sistema cambiado a modo AUTOMÁTICO.');
}

function fmtSeconds(s) {
  const h = String(Math.floor(s / 3600)).padStart(2, '0');
  const m = String(Math.floor((s % 3600) / 60)).padStart(2, '0');
  const sec = String(s % 60).padStart(2, '0');
  return `${h}:${m}:${sec}`;
}

async function pollTelemetry() {
  if (fetching) return;
  fetching = true;
  try {
    const ctrl = new AbortController();
    const tid = setTimeout(() => ctrl.abort(), 2500);
    const res = await fetch('/api/data', { signal: ctrl.signal, cache: 'no-store' });
    clearTimeout(tid);
    if (!res.ok) throw new Error('HTTP ' + res.status);
    const d = await res.json();
    if (typeof d.soilPct !== 'number') throw new Error('JSON inválido');
    state = d;
    lastOkMs = Date.now();

    // A. Encabezado
    document.getElementById('mcuPill').textContent = d.deviceId || 'ESP32 DevKit V1';
    document.getElementById('connDot').style.background = 'var(--emerald)';
    document.getElementById('connTxt').textContent = `En línea (${d.ssid})`;
    document.getElementById('ipPill').textContent = `IP local: ${d.ip}`;
    document.getElementById('updPill').textContent = `Actualizado: ${d.clock}`;
    const sDot = document.getElementById('syncDot');
    sDot.style.transform = 'scale(1.3)';
    setTimeout(() => { sDot.style.transform = 'scale(1)'; }, 220);

    // Banner de alerta
    const banner = document.getElementById('alertBanner');
    if (d.alertCode !== 0) {
      banner.style.display = 'flex';
      document.getElementById('alertMsgTxt').textContent = d.alertMsg;
    } else {
      banner.style.display = 'none';
    }

    // B. Panel principal de humedad
    const arc = document.getElementById('gaugeArc');
    const pct = d.soilValid ? Math.max(0, Math.min(100, d.soilPct)) : 0;
    arc.style.strokeDashoffset = (326.73 * (1 - pct / 100)).toFixed(1);
    const colSoil = !d.soilValid ? 'var(--red)' : (pct < d.uInf ? 'var(--amber)' : 'var(--emerald)');
    arc.style.stroke = colSoil;

    const pctEl = document.getElementById('soilPctVal');
    pctEl.textContent = d.soilValid ? `${pct}%` : '---';
    pctEl.style.color = colSoil;
    document.getElementById('soilAdcSub').textContent = d.soilValid
      ? `ADC: ${d.soilAdc} (${d.soilVolt.toFixed(2)} V)`
      : 'Sensor sin lectura válida';

    const chip = document.getElementById('soilStateChip');
    chip.textContent = d.substrateText;
    chip.style.color = colSoil;
    chip.style.borderColor = !d.soilValid ? 'rgba(239,68,68,0.45)' : (pct < d.uInf ? 'rgba(245,158,11,0.45)' : 'rgba(16,185,129,0.4)');
    chip.style.background = !d.soilValid ? 'rgba(239,68,68,0.16)' : (pct < d.uInf ? 'rgba(245,158,11,0.16)' : 'rgba(16,185,129,0.16)');

    const nwBox = document.getElementById('needWaterBox');
    const nwTitle = document.getElementById('needWaterTitle');
    const nwDesc = document.getElementById('needWaterDesc');
    if (!d.soilValid) {
      nwBox.style.borderLeftColor = 'var(--red)';
      nwTitle.textContent = '¿Necesita activar el riego?: No disponible (fallo de sensor)';
      nwDesc.textContent = 'El riego permanece bloqueado hasta restablecer lecturas válidas en el puerto GPIO 34.';
    } else if (d.pumpOn || d.soaking) {
      nwBox.style.borderLeftColor = 'var(--cyan)';
      nwTitle.textContent = 'Proceso de riego activo hasta alcanzar el umbral superior';
      nwDesc.textContent = `Suministrando agua de forma controlada hasta llegar al menos a ${d.uSup}% de humedad.`;
    } else if (pct < d.uInf) {
      nwBox.style.borderLeftColor = 'var(--amber)';
      nwTitle.textContent = '¿Necesita activar el riego?: SÍ';
      nwDesc.textContent = d.autoMode
        ? `La humedad (${pct}%) está bajo el umbral (< ${d.uInf}%). El control automático iniciará el suministro.`
        : `La humedad (${pct}%) está bajo el umbral (< ${d.uInf}%). Active el modo automático o riegue manualmente.`;
    } else {
      nwBox.style.borderLeftColor = 'var(--emerald)';
      nwTitle.textContent = '¿Necesita activar el riego?: NO';
      nwDesc.textContent = `El sustrato conserva humedad suficiente (${pct}% ≥ ${d.uInf}%). La bomba permanece en espera.`;
    }

    const bar = document.getElementById('soilBarFill');
    bar.style.width = `${pct}%`;
    bar.style.background = !d.soilValid ? 'var(--red)' : (pct < d.uInf ? 'var(--amber)' : 'linear-gradient(90deg, var(--water), var(--emerald))');
    document.getElementById('markInf').style.left = `${Math.max(0, Math.min(100, d.uInf))}%`;
    document.getElementById('markSup').style.left = `${Math.max(0, Math.min(100, d.uSup))}%`;
    document.getElementById('scaleMarks').textContent = `Activar < ${d.uInf}% · Finalizar ≥ ${d.uSup}%`;

    document.getElementById('uInfVal').textContent = `< ${d.uInf}%`;
    document.getElementById('uSupVal').textContent = `≥ ${d.uSup}%`;
    document.getElementById('dynNote').textContent = d.climaSmart
      ? `Dinámica (${d.climaDelta >= 0 ? '+' : ''}${d.climaDelta}% aplic.)`
      : `Base fija (${d.baseTh}%)`;

    // C. Panel de control
    document.getElementById('sysStateBadge').textContent = d.stateText;
    document.getElementById('modeStateTxt').textContent = d.autoMode ? 'Automático (lazo cerrado)' : 'Manual (supervisado)';
    const pumpEl = document.getElementById('pumpStateTxt');
    pumpEl.textContent = d.pumpOn ? `ENCENDIDA (${d.pwmPct}% de potencia)` : 'APAGADA';
    pumpEl.style.color = d.pumpOn ? 'var(--emerald)' : 'var(--muted)';

    if (d.soaking) {
      document.getElementById('phaseTxt').textContent = `En infiltración (${d.phaseSec} s restantes · bomba en pausa)`;
    } else if (d.pumpOn && d.autoMode && d.pulseMode) {
      document.getElementById('phaseTxt').textContent = `Pulso de bombeo (${d.phaseSec} s restantes | ciclo ${d.cycleSec} s)`;
    } else if (d.pumpOn) {
      document.getElementById('phaseTxt').textContent = `Bombeo continuo (${d.phaseSec} s / límite 25 s)`;
    } else {
      document.getElementById('phaseTxt').textContent = d.pulseMode ? 'En espera (pulsos 6 s ON / 10 s infiltración)' : 'En espera (modo continuo)';
    }

    const protEl = document.getElementById('protStateTxt');
    protEl.textContent = d.alertCode === 0 ? 'Operativas (vigilancia activa)' : 'PROTECCIÓN ACTIVADA (bloqueo seguro)';
    protEl.style.color = d.alertCode === 0 ? 'var(--emerald)' : 'var(--red)';
    document.getElementById('dryRunTxt').textContent = `${d.dryRunSec} s / 20 s acumulados (exige +2% humedad)`;
    document.getElementById('contLimitTxt').textContent = `${d.contSec} s / 25 s ininterrumpidos`;

    const bp = document.getElementById('btnPumpToggle');
    bp.textContent = d.pumpOn ? 'Apagar bomba ahora' : 'Encender bomba manual';
    bp.className = d.pumpOn ? 'btn-red' : 'btn-water';
    bp.disabled = (d.alertCode !== 0) || (!d.soilValid && !d.pumpOn);

    const bm = document.getElementById('btnModeToggle');
    bm.textContent = d.autoMode ? 'Cambiar a modo MANUAL' : 'Cambiar a modo AUTOMÁTICO';
    bm.className = d.autoMode ? 'btn-amber' : '';
    bm.disabled = (d.alertCode !== 0) || (!d.autoMode && !d.soilValid);

    // D. Panel ambiental
    const unit = d.useF ? '°F' : '°C';
    document.getElementById('dhtStateBadge').textContent = d.dhtValid
      ? `DHT22: ${d.dhtState} (hace ${d.dhtAgeSec} s)`
      : `DHT22: ${d.dhtState}`;

    if (d.dhtValid) {
      const tDisp = d.useF ? (d.tempC * 1.8 + 32) : d.tempC;
      const hiDisp = d.useF ? (d.heatIdxC * 1.8 + 32) : d.heatIdxC;
      document.getElementById('tempVal').textContent = `${tDisp.toFixed(1)} ${unit}`;
      document.getElementById('hiVal').textContent = d.hiApplicable ? `${hiDisp.toFixed(1)} ${unit}` : 'No aplica (<27 °C o <40% HR)';
      document.getElementById('humVal').textContent = `${d.humPct.toFixed(1)} %`;
      document.getElementById('vpdVal').textContent = `${d.vpdKpa.toFixed(2)} kPa`;
      document.getElementById('vpdTag').textContent = d.vpdLabel;
      document.getElementById('tempExplain').textContent = `Última lectura válida recibida hace ${d.dhtAgeSec} s (GPIO 13).`;
    } else {
      document.getElementById('tempVal').textContent = 'No disponible';
      document.getElementById('hiVal').textContent = 'No disponible';
      document.getElementById('humVal').textContent = 'No disponible';
      document.getElementById('vpdVal').textContent = 'No disponible';
      document.getElementById('vpdTag').textContent = 'Sin lectura';
      document.getElementById('tempExplain').textContent = 'Sin respuesta reciente del DHT22; el control de riego continúa operando con el sensor de suelo.';
    }
    document.getElementById('vpdExplain').textContent = d.vpdExplain;

    // E. Compensación climática basada en reglas
    document.getElementById('csBase').textContent = `${d.baseTh}%`;
    document.getElementById('csCalcDelta').textContent = `${d.climaCalcDelta >= 0 ? '+' : ''}${d.climaCalcDelta}%`;
    document.getElementById('csDelta').textContent = d.climaClamped
      ? `${d.climaDelta >= 0 ? '+' : ''}${d.climaDelta}% (acotada al rango 20%–75%)`
      : `${d.climaDelta >= 0 ? '+' : ''}${d.climaDelta}%`;
    document.getElementById('csDyn').textContent = `Activar < ${d.uInf}% | Finalizar ≥ ${d.uSup}%`;
    document.getElementById('csFactors').textContent = d.climaFactors;
    document.getElementById('csWeather').textContent = d.weatherText;
    document.getElementById('csWeatherAge').textContent = d.weatherValid
      ? `Vigente (recibido hace ${d.weatherAgeMin} min con Internet)`
      : 'Sin conexión externa vigente (Open-Meteo requiere Internet; opera con sensores locales/base)';
    document.getElementById('csSummary').textContent = d.envSummary;

    // F. Configuración
    document.getElementById('modeAutoBtn').className = d.autoMode ? 'active' : '';
    document.getElementById('modeManBtn').className = !d.autoMode ? 'active' : '';
    [30, 40, 55].forEach(v => document.getElementById('base' + v).className = (d.baseTh === v ? 'active' : ''));
    [5, 10, 15].forEach(v => document.getElementById('hyst' + v).className = (d.hystPct === v ? 'active' : ''));
    [165, 215, 255].forEach(v => document.getElementById('pwm' + v).className = (d.pwmRaw === v ? 'active' : ''));
    document.getElementById('pulseOnBtn').className = d.pulseMode ? 'active' : '';
    document.getElementById('pulseOffBtn').className = !d.pulseMode ? 'active' : '';

    const cBtn = document.getElementById('optClima');
    cBtn.textContent = d.climaSmart ? 'Comp. climática: Activa' : 'Comp. climática: Inactiva';
    cBtn.className = d.climaSmart ? 'active' : '';

    document.getElementById('optUnit').textContent = `Unidad: ${unit}`;
    const bTxt = d.brightPwm <= 100 ? 'Bajo' : (d.brightPwm <= 180 ? 'Medio' : 'Alto');
    document.getElementById('optBright').textContent = `Brillo TFT: ${bTxt}`;

    // G. Diagnóstico
    document.getElementById('diagSoil').textContent = d.soilValid
      ? `Operativo (ADC ${d.soilAdc} · ${d.soilVolt.toFixed(2)} V)`
      : `Fallo o desconectado (ADC ${d.soilAdc})`;
    document.getElementById('diagDht').textContent = d.dhtValid
      ? `${d.dhtState} (hace ${d.dhtAgeSec} s)`
      : `${d.dhtState} (verificar puerto D13)`;
    document.getElementById('diagWifi').textContent = `${d.ssid} (${d.rssi} dBm) · IP ${d.ip}`;
    document.getElementById('diagServer').textContent = 'Servidor HTTP puerto 80 activo · API JSON OK';
    document.getElementById('diagAlert').textContent = d.alertMsg;
    document.getElementById('diagUptime').textContent = fmtSeconds(d.uptimeSec);
    document.getElementById('diagHeap').textContent = `${Math.round(d.freeHeap / 1024)} KB libres`;
  } catch (err) {
    const lostSec = lastOkMs ? Math.round((Date.now() - lastOkMs) / 1000) : 0;
    document.getElementById('connDot').style.background = 'var(--red)';
    document.getElementById('connTxt').textContent = lostSec ? `Reconectando (${lostSec} s)...` : 'Sin conexión con ESP32';
  } finally {
    fetching = false;
  }
}

setInterval(pollTelemetry, 1000);
pollTelemetry();
</script>
</body>
</html>
)rawliteral";
