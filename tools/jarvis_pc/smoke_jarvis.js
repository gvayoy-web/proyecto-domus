/* Smoke test de jarvis.html con DOM/TTS/Serial simulados.
   Uso: node tools/jarvis_pc/smoke_jarvis.js */
const fs = require("fs");
const path = require("path");
const vm = require("vm");

const HTML = path.join(__dirname, "jarvis.html");
const html = fs.readFileSync(HTML, "utf8");
const script = html.match(/<script>([\s\S]*?)<\/script>/)[1];
const hookSrc = script + `
;globalThis.__T = {
  handleLine, speak, hablarEstado, writeLine, st, applyAck, handleIr,
  setPort: (p) => { port = p; },
  getDiag: () => diagPending,
  setDiag: (v) => { diagPending = v; },
  CMD_HABLA, CMD_ACK_VOICE, SFX
};`;

/* ---------- stubs ---------- */
function makeClassList(el) {
  const set = new Set();
  return {
    add: (...c) => c.forEach(x => set.add(x)),
    remove: (...c) => c.forEach(x => set.delete(x)),
    toggle: (c, force) => {
      const on = force === undefined ? !set.has(c) : !!force;
      on ? set.add(c) : set.delete(c);
      return on;
    },
    contains: (c) => set.has(c),
    _set: set
  };
}
function makeEl(id) {
  const el = {
    id, textContent: "", className: "", style: {}, children: [],
    listeners: {},
    classList: null,
    addEventListener(type, fn) { (this.listeners[type] ||= []).push(fn); },
    appendChild(c) { this.children.push(c); return c; },
    prepend(c) { this.children.unshift(c); },
    querySelector() { return null; },
    remove() {},
    getContext() { return CTX; },
    width: 0, height: 0
  };
  el.classList = makeClassList(el);
  Object.defineProperty(el, "lastChild", {
    get() {
      const c = el.children[el.children.length - 1];
      if (!c) return null;
      return { remove: () => el.children.pop() };
    }
  });
  return el;
}
const CTX = new Proxy({}, {
  get(t, k) {
    if (k in t) return t[k];
    if (typeof k === "string" && ["clearRect","beginPath","moveTo","lineTo","stroke","arc","fill","fillText"].includes(k))
      return () => {};
    return t[k];
  },
  set(t, k, v) { t[k] = v; return true; }
});

const els = {};
const buttons = ["RIEGO_ON", "LUZ2_ON", "LUZ2_OFF", "TODO_OFF", "MODO_AUTO", "PARO", "REARMAR"]
  .map(cmd => { const e = makeEl("btn_" + cmd); e._cmd = cmd; return e; });
buttons.forEach(b => b.getAttribute = (k) => k === "data-cmd" ? b._cmd : null);
const diagButtons = ["DIAGNOSTICO", "PRUEBA", "RECUPERAR"]
  .map(cmd => { const e = makeEl("diag_" + cmd); e._cmd = cmd; return e; });
diagButtons.forEach(b => b.getAttribute = (k) => k === "data-diag" ? b._cmd : null);

const document = {
  getElementById: (id) => (els[id] ||= makeEl(id)),
  querySelectorAll: (sel) => sel === "[data-cmd]" ? buttons
    : sel === "[data-diag]" ? diagButtons : [],
  createElement: () => makeEl("dyn")
};

let timers = [], timerId = 1, cancelled = new Set();
const setTimeoutStub = (fn, ms) => { const id = timerId++; timers.push({ id, fn, ms }); return id; };
const clearTimeoutStub = (id) => { cancelled.add(id); };
function drain(max = 200) {
  let n = 0;
  while (timers.length && n++ < max) {
    const batch = timers; timers = [];
    for (const t of batch) if (!cancelled.has(t.id)) t.fn();
  }
}

const utterances = [];
const tts = { cancelCalls: 0, speaks: [] };
class Utterance {
  constructor(text) { this.text = text; this.lang = ""; this.rate = 1; this.voice = null; this.onend = null; this.onerror = null; }
}
const speechSynthesis = {
  speaking: false, pending: false,
  getVoices: () => [{ name: "Microsoft Sabina - Spanish (Mexico)", lang: "es-MX" }],
  cancel() { tts.cancelCalls++; this.speaking = false; this.pending = false; },
  speak(u) { this.speaking = true; tts.speaks.push(u); utterances.push(u); },
  resume() {}
};

let rafCb = null;
const sandbox = {
  document, console,
  navigator: {},                       // sin serial → desconectado
  speechSynthesis, SpeechSynthesisUtterance: Utterance,
  TextEncoder: globalThis.TextEncoder,
  innerWidth: 800, innerHeight: 600,
  addEventListener() {}, removeEventListener() {},
  requestAnimationFrame(cb) { rafCb = cb; return 1; },
  setInterval: () => 1, clearInterval: () => {},
  setTimeout: setTimeoutStub, clearTimeout: clearTimeoutStub,
  alert() {}
};
vm.createContext(sandbox);
sandbox.window = sandbox;   // window === globalThis como en el navegador
vm.runInContext(hookSrc, sandbox, { filename: "jarvis_extracted.js" });
const T = sandbox.__T;

/* ---------- mini test runner ---------- */
let pass = 0, fail = 0;
function ok(cond, name) {
  if (cond) { pass++; }
  else { fail++; console.log("  FAIL: " + name); }
}
function drainVoices() { drain(); }

/* T1: TTS básico — cancel + speak diferido, HUD visible y se oculta en onend */
(async () => {
T.speak("Hola mundo");
ok(els.subtitle.textContent === "Hola mundo", "T1 subtitulo");
ok(els.hud.classList.contains("on"), "T1 hud on");
ok(tts.cancelCalls >= 1, "T1 cancel antes de speak (bug Chrome)");
drainVoices();
ok(tts.speaks.length === 1 && tts.speaks[0].text === "Hola mundo", "T1 utterance emitida tras drain");
tts.speaks[0].onend && tts.speaks[0].onend();
ok(!els.hud.classList.contains("on"), "T1 hud off en onend");

/* T2: habla A y B rápido → solo la última (gen) llega a speak() */
const n2 = tts.speaks.length;
T.speak("primera frase");
T.speak("segunda frase");
drainVoices();
ok(tts.speaks.length === n2 + 1 && tts.speaks[n2].text === "segunda frase", "T2 solo la frase más reciente");

/* T3: writeLine sin puerto → false + aviso audible, y quiet=true no habla */
let r = await T.writeLine("LUZ2_ON");
ok(r === false, "T3 writeLine devuelve false");
ok(/Sin conexi/.test(els.subtitle.textContent), "T3 aviso de conexión en subtítulo");
drainVoices();
ok(tts.speaks.some(u => /Sin conexi/.test(u.text)), "T3 aviso hablado");
const nAfterWarn = tts.speaks.length;
r = await T.writeLine("ESTADO", true);
ok(r === false, "T3 quiet no habla");
drainVoices();
ok(tts.speaks.length === nAfterWarn, "T3 poll silencioso");

/* T4: botón LUZ2_ON desconectado → chip optimista + aviso */
const bPorcheOn = buttons.find(b => b._cmd === "LUZ2_ON");
ok(bPorcheOn.listeners.click && bPorcheOn.listeners.click.length === 1, "T4 click registrado");
await bPorcheOn.listeners.click[0]();
ok(T.st.Porche === true, "T4 chip Porche optimista ON");
ok(/Sin conexi/.test(els.subtitle.textContent), "T4 botón sin puerto avisa");
ok(bPorcheOn.classList.contains("tap"), "T4 efecto tap");
drainVoices();

/* T5: ESTADO actualiza chips */
T.handleLine("ESTADO;Bomba=0;Casa=1;Porche=0;Spare=1;HUM=1800;HUM_PCT=72;PRESENCIA=1;TEMP_C=-1;HUM_AIRE_PCT=-1;LUZ_PCT=67;MIC=1;SD=0;EMERGENCIA=0;MODO_SEGURO=0;");
ok(T.st.Casa === true && T.st.Porche === false && T.st.Spare === true && T.st.Bomba === false, "T5 estados desde ESTADO");
ok(els.cCasa.classList.contains("on") && !els.cPorche.classList.contains("on") && els.cSpare.classList.contains("on"), "T5 chips DOM");
ok(els.soilPct.textContent === "72%", "T5 humedad %");
ok(els.luzPct.textContent === "67", "T5 luz %");

/* T6: ACKs individuales y masivos */
T.handleLine("ACK;LUZ2_ON;1");
ok(T.st.Porche === true, "T6 ACK LUZ2_ON");
T.handleLine("ACK;LUZ2_OFF;0");
ok(T.st.Porche === false, "T6 ACK LUZ2_OFF");
T.handleLine("ACK;LUZ1_ON;1");
T.handleLine("ACK;TODO_OFF;0");
ok(T.st.Casa === false && T.st.Spare === false && T.st.Bomba === false, "T6 TODO_OFF apaga todo");
T.handleLine("ACK;IR;MODO_AUTO");
ok(T.st.auto === true, "T6 IR MODO_AUTO");
T.handleLine("ACK;MODO_MANUAL;AUTO");
ok(T.st.auto === false, "T6 MODO_MANUAL");
T.handleLine("ACK;IR;PARPADEO;1");
ok(true, "T6 PARPADEO no rompe");

/* T6b: formatos exactos del firmware flasheado (ACK;CMD sin valor) */
drainVoices();
const n6b = tts.speaks.length;
T.handleLine("ACK;MODO_AUTO");
ok(T.st.auto === true, "T6b ACK;MODO_AUTO (2 partes)");
drainVoices();
ok(tts.speaks.length > n6b, "T6b MODO_AUTO habla");
T.handleLine("ACK;MODO_MANUAL");
ok(T.st.auto === false, "T6b ACK;MODO_MANUAL (2 partes)");
T.handleLine("ACK;LUZ2_ON;1");
ok(T.st.Porche === true, "T6b ACK;LUZ2_ON;1 formato serial real");

/* T7: REARMAR limpia seguro y habla */
T.handleLine("EVENTO;PARO_EMERGENCIA");
ok(T.st.seguro === true, "T7 PARO activa seguro");
ok(els.cSeguro.classList.contains("on"), "T7 chip PARO");
drainVoices();
const n7 = tts.speaks.length;
T.handleLine("ACK;REARMAR;SEGURO");
ok(T.st.seguro === false, "T7 REARME limpia seguro");
drainVoices();
ok(tts.speaks.length > n7 && /rearmado/i.test(tts.speaks[tts.speaks.length - 1].text), "T7 rearme hablado");

/* T8: NACK ruido filtrado vs NACK real hablado */
drainVoices();
const n8 = tts.speaks.length;
T.handleLine("NACK;RIEGO_ON;limite_de_frecuencia");
T.handleLine("NACK;LUZ1_ON;no_reconocido");
drainVoices();
ok(tts.speaks.length === n8, "T8 ruido NACK no habla");
T.handleLine("NACK;RIEGO_ON;bloqueado_por_paro");
drainVoices();
ok(tts.speaks.length === n8 + 1, "T8 NACK real habla");

/* T9: IR — tecla habla; REP=1 no spamea */
drainVoices();
const n9 = tts.speaks.length;
T.handleLine("IR;PROTO=0x20;CMD=0x18;TECLA=2;REP=0");
drainVoices();
ok(tts.speaks.length === n9 + 1 && /porche/i.test(tts.speaks[tts.speaks.length - 1].text), "T9 IR tecla 2 habla porche");
T.handleLine("IR;PROTO=0x20;CMD=0x18;TECLA=2;REP=1");
drainVoices();
ok(tts.speaks.length === n9 + 1, "T9 REP=1 no duplica voz");
ok(els.lastIR.textContent.includes("2"), "T9 último IR");

/* T10: ruido de arranque del ESP32 no rompe */
T.handleLine("ets rst:0x1 (POWERON_RESET),boot:0x13 (SPI_FAST_FLASH_BOOT)");
T.handleLine("");
T.handleLine("DIAGNOSTICO;IR=ON;BOMBA_ETAPA=GPIO_DIRECTO;ADC_LDR=1295;ADC_SUELRO=1800;SD=OK");
ok(true, "T10 ruido/DIAG sin excepción");

/* T11: nodo sin puerto avisa */
drainVoices();
const n11 = tts.speaks.length;
T.hablarEstado();
drainVoices();
ok(tts.speaks.length === n11 + 1 && /Sin conexi/.test(tts.speaks[tts.speaks.length - 1].text), "T11 nodo sin conexión avisa");

/* T12: nodo con puerto → frase de estado */
T.setPort({ writable: { getWriter: () => ({ write: async () => {}, releaseLock: () => {} }) }, readable: null });
drainVoices();
T.hablarEstado();
drainVoices();
const last12 = tts.speaks[tts.speaks.length - 1];
ok(last12 && /riego/.test(last12.text) && /luces/.test(last12.text) && /modo/.test(last12.text), "T12 nodo habla estado");
T.setPort(null);

/* T13: CMD_ACK_VOICE evita doble voz en PARO/MODO */
drainVoices();
const n13 = tts.speaks.length;
const bParo = buttons.find(b => b._cmd === "PARO");
await bParo.listeners.click[0]();
drainVoices();
const nuevas13 = tts.speaks.slice(n13);
ok(!nuevas13.some(u => u.text === T.CMD_HABLA.PARO), "T13 PARO no habla frase local (habla el ACK/EVENTO)");
ok(nuevas13.length <= 1 && (nuevas13.length === 0 || /Sin conexi/.test(nuevas13[0].text)), "T13 como mucho el aviso de conexión");
ok(T.st.seguro === true, "T13 PARO activa seguro optimista");

/* T14: rAF/tick corre sin errores */
if (rafCb) { try { rafCb(16); ok(true, "T14 tick"); } catch (e) { ok(false, "T14 tick: " + e.message); } }

/* T15: botón de diagnóstico sin puerto → aviso hablado + pendiente limpiada */
const bDiag = diagButtons.find(b => b._cmd === "DIAGNOSTICO");
const bRec = diagButtons.find(b => b._cmd === "RECUPERAR");
ok(bRec.listeners.click && bRec.listeners.click.length === 1, "T15 click data-diag registrado");
drainVoices();
await bRec.listeners.click[0]();
drainVoices();
ok(/Sin conexi/.test(els.subtitle.textContent), "T15 diag sin puerto avisa");
ok(T.getDiag() === null, "T15 pendiente limpiado si falla el envío");

/* T16: DIAGNOSTICO con puerto → respuesta hablada y pendiente consumida */
T.setPort({ writable: { getWriter: () => ({ write: async () => {}, releaseLock: () => {} }) }, readable: null });
drainVoices();
const n16 = tts.speaks.length;
await bDiag.listeners.click[0]();
ok(T.getDiag() === "DIAGNOSTICO", "T16 pendiente armado tras click");
T.handleLine("DIAGNOSTICO;MODO_SEGURO=OFF;MOTIVO_SEGURO=ninguno;WATCHDOG=ON;ERRORES_TOTAL=0;ULTIMO_ERROR=ninguno;SD_PRUEBA=1;ADC_SUELRO=1800;ADC_LDR=900;BOMBA_ETAPA=GPIO_DIRECTO;CAL_SECO=4000;CAL_HUMEDO=3000;CAL_OSCURO=3800;CAL_CLARO=600;");
drainVoices();
ok(T.getDiag() === null, "T16 pendiente consumido");
const u16 = tts.speaks.slice(n16).map(u => u.text).join(" | ");
ok(/Diagn.stico completo/.test(u16) && /watchdog activo/i.test(u16) && /Cero errores/.test(u16), "T16 diagnóstico hablado resume watchdog/errores");
ok(/Suelo al 56 por ciento/.test(u16), "T16 diagnóstico habla % de suelo");

/* T17: botón Estado habla el estado (pending ESTADO) */
drainVoices();
const n17 = tts.speaks.length;
await els.btnEstado.listeners.click[0]();
ok(T.getDiag() === "ESTADO", "T17 Estado arma pendiente");
T.handleLine("ESTADO;Bomba=0;Casa=1;Porche=0;Spare=0;HUM=1800;HUM_PCT=54;PRESENCIA=1;TEMP_C=-1;HUM_AIRE_PCT=-1;LUZ_PCT=40;MIC=ON;SD=ON;EMERGENCIA=OFF;MODO_SEGURO=OFF;");
drainVoices();
ok(T.getDiag() === null, "T17 pendiente consumido por ESTADO");
ok(tts.speaks.length > n17 && /riego/.test(tts.speaks[tts.speaks.length - 1].text), "T17 Estado habla");

/* T18: PRUEBA guiada → al FIN habla las acciones traducidas */
drainVoices();
const bPrueba = diagButtons.find(b => b._cmd === "PRUEBA");
await bPrueba.listeners.click[0]();
T.handleLine("PRUEBA;INICIO;COPIA_HASTA_FIN");
T.handleLine("PRUEBA;ACCIONES=TAPA_LDR,PULSA_MODO,PRUEBA_IR,PULSA_STOP");
drainVoices();
const n18 = tts.speaks.length;
T.handleLine("PRUEBA;FIN");
drainVoices();
ok(tts.speaks.length > n18 && /tapa el sensor de luz/.test(tts.speaks[tts.speaks.length - 1].text), "T18 PRUEBA;FIN habla acciones");

/* T19: respuestas MIC y PINTEST del equipo hablan su resultado */
T.setDiag("MIC");
drainVoices();
const n19 = tts.speaks.length;
T.handleLine("MIC;ON");
drainVoices();
ok(tts.speaks.length > n19 && /Micr.fono habilitado/.test(tts.speaks[tts.speaks.length - 1].text), "T19 MIC habla");
T.setDiag("PINTEST");
drainVoices();
const n19b = tts.speaks.length;
T.handleLine("PINTEST;GPIO=4;CRUDO=0;PULLUP=4095;PULLDOWN=0;CONECTADO");
T.handleLine("PINTEST;FIN");
drainVoices();
ok(tts.speaks.length > n19b && /Barrido de pines/.test(tts.speaks[tts.speaks.length - 1].text), "T19 PINTEST;FIN habla");

/* T20: SD_PRUEBA → encolada habla y el resultado llega por el ESTADO posterior */
drainVoices();
const n20 = tts.speaks.length;
T.handleLine("ACK;SD_PRUEBA;ENCOLADA");
drainVoices();
ok(tts.speaks.length > n20 && /Probando la tarjeta/.test(tts.speaks[tts.speaks.length - 1].text), "T20 encolada habla");
ok(T.getDiag() === "SD", "T20 pendiente SD armado");
T.handleLine("ESTADO;Bomba=0;Casa=0;Porche=0;Spare=0;HUM=1800;HUM_PCT=54;PRESENCIA=1;TEMP_C=-1;HUM_AIRE_PCT=-1;LUZ_PCT=40;MIC=ON;SD=ON;EMERGENCIA=OFF;MODO_SEGURO=OFF;");
drainVoices();
ok(T.getDiag() === null, "T20 SD consumido");
ok(/microSD está montada/.test(tts.speaks[tts.speaks.length - 1].text), "T20 resultado SD hablado");

/* T21: NACK con motivo conocido habla frase específica */
drainVoices();
const n21 = tts.speaks.length;
T.handleLine("NACK;RECUPERAR;paro_emergencia");
drainVoices();
ok(tts.speaks.length === n21 + 1 && /paro de emergencia/i.test(tts.speaks[tts.speaks.length - 1].text), "T21 NACK específico hablado");

/* T22: SFX existe y es invocable sin Web Audio (no-op en el harness) */
try {
  T.SFX.prewarm(); T.SFX.blip(); T.SFX.blipDiag(); T.SFX.ack(); T.SFX.error(); T.SFX.chime(); T.SFX.alarm();
  ok(true, "T22 SFX no-op sin AudioContext");
} catch (e) { ok(false, "T22 SFX lanzó: " + e.message); }
T.setPort(null);

console.log(`\n${pass} passed, ${fail} failed`);
process.exit(fail ? 1 : 0);
})().catch(e => { console.error("EXCEPCIÓN:", e); process.exit(1); });
