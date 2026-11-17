// SPDX-License-Identifier: MPL-2.0
// Copyright (C) 2026 Charles Vestal (fm1-base's web/emu/index.html: the controls, the input, the screen, switching on)
// AFFINE's FM-1 in the page: emu/app.wasm (the firmware, built by fm1-base's tools/build.py --web) in emu/worklet.js,
// on panel.js's drawing. Each control has an invisible target over it (touches, the keyboard, a screen reader); the
// status line says what the control under the pointer does in AFFINE; the computer's keys play it while the panel has
// the focus. window.affineEmu lets the page's other parts (perform.js) press its keys.
"use strict";
(() => {
  const $ = (id) => document.getElementById(id);
  const svg = $("panel");
  if (!svg) return;
  const BTN = PANEL_BTN, WHITE_K = PANEL_WHITE_K, BLACK_K = PANEL_BLACK_K;
  const STORE_KEY = "affine.web.store";
  const panel = affPanel(svg), knobs = panel.knobs, buttons = panel.buttonsByLabel, keys = panel.keys;
  const device = $("device"), controls = $("controls"), status = $("status");

  // ---------------------------------------------------------------- what each control does (the status line)
  const BK_NAMES = ["ROLL 1/8", "ROLL 1/16", "ROLL 1/32", "REVERSE", "HALF SPEED", "VINYL BREAK", "FREEZE", "SKIP", "VERB THROW", "CRUSH", "FILTER"];
  const HELP = {
    MASTER: "Volume.", SELECT: "The tempo, on every page.", ALGORITHM: "The break: the next one, or the one before.",
    PRESETS: "The pattern, P1 to P64; while playing, it comes in at the loop's end.", "KNOB 1": "On HOME: FILTER, low-pass left, high-pass right.", "KNOB 2": "On HOME: CRUSH, the bits down.",
    "KNOB 3": "On HOME: REDUX, the rate down.", "KNOB 4": "On HOME: VERB, the chip's reverb.",
    FX: "FX: PLAY, CHAIN, SLOT, PSX, SENDS; pressed again, the next.", SEL: "Tapped: the page's help. Held with another control: its second job.",
    EDIT: "EDIT: the BREAK, then BOOM, SNARE, HAT, CLAP.", GLO: "GLO: MIDI, SETUP, SCREEN, RESET.", HOME: "HOME: PERFORM, then SETTINGS. SEL + HOME, then HOME: INIT.",
    SAVE: "Save the pattern and the settings. SEL + SAVE: undo.", ARP: "ARP: the GENERATOR.", SEQ: "SEQ: STEPS, DRUMS, SETTINGS. Held, then SEQ: clear the pattern.",
    PLAY: "Play and stop the break.", REC: "Record what the keys play.", "OCT minus": "OCT−: the item before, or the pitch down.", "OCT plus": "OCT+: the next item, or the pitch up.",
    ENV: "ENV: TRACKS, PLAY, DROP, BUILD.", LFO: "LFO: LEVELS, MASTER, DYN.",
  };
  const helpOf = (c) => c.n !== undefined && c.name.startsWith("Black") ? [`Key ${c.n + 1}`, `${BK_NAMES[c.n]}, while held.`]
    : c.n !== undefined ? [`Key ${c.n + 1}`, `Slice ${c.n + 1} of the break.`] : [c.label || c.name, HELP[c.name] || ""];
  let stTimer = 0;
  function say(tag, text) {
    status.innerHTML = "";
    if (tag) { const b = document.createElement("b"); b.textContent = tag; status.append(b); }
    status.append(document.createTextNode(text));
    status.classList.remove("flip"); void status.offsetWidth; status.classList.add("flip");
    clearTimeout(stTimer);
  }

  // ---------------------------------------------------------------- the controls' targets
  function group(label) {
    const d = document.createElement("div");
    d.setAttribute("role", "group");
    d.setAttribute("aria-label", label);
    controls.appendChild(d);
    return d;
  }
  function hit(c, parent, attrs, cls) {
    const b = c.g.getBBox(), d = document.createElement("div");
    d.className = "hit" + (cls ? " " + cls : "");
    Object.assign(d.style, { left: `${b.x / 10}%`, top: `${b.y / 6.4}%`, width: `${b.width / 10}%`, height: `${b.height / 6.4}%` });
    d.tabIndex = 0;
    for (const [k, v] of Object.entries(attrs)) if (v != null) d.setAttribute(k, v);
    d.addEventListener("touchstart", (e) => e.preventDefault(), { passive: false });
    d.addEventListener("pointerdown", () => d.focus({ preventScroll: true }));
    d.addEventListener("pointerenter", () => { const [t, s] = helpOf(c); say(t, s); });
    d.addEventListener("focus", () => { const [t, s] = helpOf(c); say(t, s); });
    parent.appendChild(d);
    c.hit = d;
  }
  const gKnobs = group("Knobs"), gButtons = group("Buttons"), gBlack = group("Black keys"), gWhite = group("White keys");
  let master = 2800;
  const masterPct = () => Math.round(master * 100 / 4096);
  for (const k of Object.values(knobs)) {
    if (k.role === "master")
      hit(k, gKnobs, { role: "slider", "aria-label": "MASTER", "aria-orientation": "vertical", "aria-valuemin": 0, "aria-valuemax": 100,
                       "aria-valuenow": masterPct(), "aria-valuetext": `${masterPct()} percent` }, "knob");
    else hit(k, gKnobs, { role: "button", "aria-label": k.name, "aria-description": "Turn it with the arrow keys" }, "knob");
  }
  for (const lbl of ["OCT-", "OCT+", ...PANEL_ROW1, ...PANEL_ROW2]) hit(buttons[lbl], gButtons, { role: "button", "aria-label": buttons[lbl].name, "aria-pressed": "false" });
  BLACK_K.forEach((n) => hit(keys[n], gBlack, { role: "button", "aria-label": keys[n].name, "aria-pressed": "false" }));
  WHITE_K.forEach((n) => hit(keys[n], gWhite, { role: "button", "aria-label": keys[n].name, "aria-pressed": "false" }));

  // ---------------------------------------------------------------- state and input
  let node = null, gain = null, btnMask = 0, keyMask = 0, latched = 0, keyLatched = 0;
  const btnHeldBy = new Map(), keyHeldBy = new Map();
  const send = (m) => { if (node) node.port.postMessage(m); };
  function syncButtons() {
    let m = latched;
    for (const b of btnHeldBy.values()) m |= 1 << b;
    if (m !== btnMask) { btnMask = m; send({ type: "buttons", mask: m }); }
    for (const b of Object.values(buttons)) {
      b.g.classList.toggle("held", !!(m & (1 << b.bit)));
      b.g.classList.toggle("latched", !!(latched & (1 << b.bit)));
      b.hit.setAttribute("aria-pressed", m & (1 << b.bit) ? "true" : "false");
    }
  }
  function syncKeys() {
    let m = keyLatched;
    for (const k of keyHeldBy.values()) m |= 1 << k;
    if (m !== keyMask) { keyMask = m; send({ type: "keys", mask: m }); }
    keys.forEach((k, i) => {
      k.g.classList.toggle("held", !!(m & (1 << i)));
      k.g.classList.toggle("latched", !!(keyLatched & (1 << i)));
      k.hit.setAttribute("aria-pressed", m & (1 << i) ? "true" : "false");
    });
    dispatchEvent(new CustomEvent("affine:keys", { detail: m }));
  }
  for (const b of Object.values(buttons)) {
    b.hit.addEventListener("contextmenu", (e) => e.preventDefault());
    b.hit.addEventListener("pointerdown", (e) => {
      e.preventDefault();
      if (!node) return;
      if (e.shiftKey || e.button === 2) { latched ^= 1 << b.bit; syncButtons(); return; }
      if (latched & (1 << b.bit)) { latched &= ~(1 << b.bit); syncButtons(); return; }
      b.hit.setPointerCapture(e.pointerId);
      btnHeldBy.set("p" + e.pointerId, b.bit);
      syncButtons();
    });
  }
  keys.forEach((k, i) => {
    k.hit.addEventListener("contextmenu", (e) => e.preventDefault());
    k.hit.addEventListener("pointerdown", (e) => {
      e.preventDefault();
      if (!node) return;
      if (e.shiftKey || e.button === 2) { keyLatched ^= 1 << i; syncKeys(); return; }
      if (keyLatched & (1 << i)) { keyLatched &= ~(1 << i); syncKeys(); return; }
      k.hit.setPointerCapture(e.pointerId);
      keyHeldBy.set("p" + e.pointerId, i);
      syncKeys();
    });
    if (WHITE_K.includes(i)) k.hit.addEventListener("pointermove", (e) => {   // a finger sliding along the white keys
      const id = "p" + e.pointerId;
      if (!keyHeldBy.has(id)) return;
      for (const w of WHITE_K) {
        const r = keys[w].hit.getBoundingClientRect();
        if (e.clientX >= r.left && e.clientX < r.right && e.clientY >= r.top - 40 && e.clientY < r.bottom + 40) {
          if (keyHeldBy.get(id) !== w) { keyHeldBy.set(id, w); syncKeys(); }
          return;
        }
      }
    });
  });
  function pointerUp(e) {
    const id = "p" + e.pointerId;
    if (btnHeldBy.delete(id)) syncButtons();
    if (keyHeldBy.delete(id)) syncKeys();
  }
  device.addEventListener("pointerup", pointerUp);
  device.addEventListener("pointercancel", pointerUp);
  device.addEventListener("lostpointercapture", pointerUp);

  const turnT = new Map();
  function drawKnob(k) { k.ind.setAttribute("transform", `rotate(${k.angle} ${k.x} ${k.y})`); }
  function turn(k, n) {
    if (!n || !node) return;
    if (k.role === "master") {
      master = Math.max(0, Math.min(4096, master + n * 64));
      k.angle = -135 + 270 * master / 4096;
      k.hit.setAttribute("aria-valuenow", masterPct());
      k.hit.setAttribute("aria-valuetext", `${masterPct()} percent`);
      send({ type: "master", value: master });
    } else {
      k.angle = (k.angle + n * 18) % 360;
      send({ type: "enc", role: k.role, n });
    }
    drawKnob(k);
    k.g.classList.add("turning");
    clearTimeout(turnT.get(k));
    turnT.set(k, setTimeout(() => k.g.classList.remove("turning"), 250));
  }
  for (const k of Object.values(knobs)) {
    if (k.role === "master") { k.angle = -135 + 270 * master / 4096; drawKnob(k); }
    let drag = null, wheel = 0;
    k.hit.addEventListener("pointerdown", (e) => { e.preventDefault(); k.hit.setPointerCapture(e.pointerId); drag = { y: e.clientY, acc: 0 }; });
    k.hit.addEventListener("pointermove", (e) => {
      if (!drag) return;
      drag.acc += drag.y - e.clientY;
      drag.y = e.clientY;
      const step = k.role === "master" ? 3 : 7, n = Math.trunc(drag.acc / step);
      if (n) { drag.acc -= n * step; turn(k, n); }
    });
    const end = () => { drag = null; };
    k.hit.addEventListener("pointerup", end);
    k.hit.addEventListener("pointercancel", end);
    k.hit.addEventListener("wheel", (e) => {
      if (!node) return;
      e.preventDefault();
      const dy = e.deltaMode === 1 ? e.deltaY * 33 : e.deltaY;
      if (Math.abs(dy) >= 50) { turn(k, dy < 0 ? 1 : -1); wheel = 0; return; }
      wheel -= dy;
      const n = Math.trunc(wheel / 25);
      if (n) { wheel -= n * 25; turn(k, n); }
    }, { passive: false });
    k.hit.addEventListener("keydown", (e) => {
      if (e.metaKey || e.ctrlKey || e.altKey) return;
      const big = k.role === "master" ? 8 : 5;
      const n = { ArrowUp: 1, ArrowRight: 1, ArrowDown: -1, ArrowLeft: -1, PageUp: big, PageDown: -big }[e.key];
      if (!n) return;
      turn(k, n);
      e.preventDefault(); e.stopPropagation();
    });
  }

  // a press from the keyboard lasts at least 50 ms: the firmware reads the panel every 16 ms
  const downAt = new Map();
  function holdDown(id, kind, v) {
    downAt.set(id, performance.now());
    if (kind === "btn") { btnHeldBy.set(id, v); syncButtons(); } else { keyHeldBy.set(id, v); syncKeys(); }
  }
  function letGo(id) {
    if (!btnHeldBy.has(id) && !keyHeldBy.has(id)) return;
    const at = downAt.get(id), go = () => {
      if (downAt.get(id) !== at) return;
      downAt.delete(id);
      if (btnHeldBy.delete(id)) syncButtons();
      if (keyHeldBy.delete(id)) syncKeys();
    };
    const wait = 50 - (performance.now() - (at ?? 0));
    if (wait > 0) setTimeout(go, wait); else go();
  }
  for (const [c, kind] of [...Object.values(buttons).map((b) => [b, "btn"]), ...keys.map((k, i) => [k, i])]) {
    c.hit.addEventListener("keydown", (e) => {
      if ((e.key !== " " && e.key !== "Enter") || e.metaKey || e.ctrlKey || e.altKey) return;
      e.preventDefault(); e.stopPropagation();
      if (e.repeat || !node) return;
      if (kind === "btn") holdDown("f" + e.key, "btn", c.bit); else holdDown("f" + e.key, "key", kind);
    });
    c.hit.addEventListener("keyup", (e) => letGo("f" + e.key));
  }

  // the computer's keys, while the panel has the focus
  const KEYMAP = {};
  "qwertyui".split("").forEach((c, i) => KEYMAP["Key" + c.toUpperCase()] = { key: WHITE_K[i] });
  "asdfghjk".split("").forEach((c, i) => KEYMAP["Key" + c.toUpperCase()] = { key: WHITE_K[8 + i] });
  "1234567890".split("").forEach((c, i) => KEYMAP["Digit" + c] = { key: BLACK_K[i] });
  KEYMAP.Minus = { key: BLACK_K[10] };
  Object.assign(KEYMAP, { Space: { btn: BTN.PLAY }, KeyP: { btn: BTN.PLAY }, KeyO: { btn: BTN.REC }, Escape: { btn: BTN.HOME },
                          KeyZ: { btn: BTN["OCT-"] }, KeyX: { btn: BTN["OCT+"] } });
  const TURNS = { ArrowUp: ["SELECT", 1], ArrowDown: ["SELECT", -1], BracketRight: ["ALGORITHM", 1], BracketLeft: ["ALGORITHM", -1],
                  Quote: ["PRESETS", 1], Semicolon: ["PRESETS", -1] };
  device.addEventListener("keydown", (e) => {
    if (!node || e.metaKey || e.ctrlKey || e.altKey) return;
    if (e.target.classList.contains("knob") && /^Arrow|^Page/.test(e.key)) return;   // (a knob with the focus turns itself)
    const t = TURNS[e.code];
    if (t) { turn(knobs[t[0]], t[1]); e.preventDefault(); return; }
    const m = KEYMAP[e.code];
    if (!m) return;
    if ((e.code === "Space" || e.code === "Enter") && e.target !== device && !e.target.classList.contains("knob")) return;   // (a focused control's own)
    e.preventDefault();
    if (e.repeat) return;
    if (m.key !== undefined) holdDown("k" + e.code, "key", m.key); else holdDown("k" + e.code, "btn", m.btn);
  });
  addEventListener("keyup", (e) => letGo("k" + e.code));
  function letGoKeys() {
    for (const id of [...btnHeldBy.keys()]) if (id[0] === "k" || id[0] === "f") btnHeldBy.delete(id);
    for (const id of [...keyHeldBy.keys()]) if (id[0] === "k" || id[0] === "f") keyHeldBy.delete(id);
    syncButtons(); syncKeys();
  }
  addEventListener("blur", letGoKeys);
  device.addEventListener("focusout", (e) => { if (!device.contains(e.relatedTarget)) letGoKeys(); });

  // ---------------------------------------------------------------- the screen and the lights
  // the LCD's 240 x 240 drawn enlarged by a whole number (every pixel the same size), then shown
  const screenEl = $("lcd"), ctx = screenEl.getContext("2d");
  const lcd = document.createElement("canvas");
  lcd.width = lcd.height = 240;
  const lctx = lcd.getContext("2d"), img = lctx.createImageData(240, 240);
  function blit() { ctx.imageSmoothingEnabled = false; ctx.drawImage(lcd, 0, 0, screenEl.width, screenEl.height); }
  function fit() {
    const px = screenEl.getBoundingClientRect().width * (devicePixelRatio || 1);
    const n = 240 * Math.max(1, Math.ceil(px / 240 - 0.001));
    if (screenEl.width !== n) { screenEl.width = screenEl.height = n; blit(); }
  }
  new ResizeObserver(fit).observe(screenEl);
  // on a phone the panel scrolls sideways in its box: start with the screen in the middle
  const box = $("devbox");
  if (box && box.scrollWidth > box.clientWidth) box.scrollLeft = (box.scrollWidth * 0.418) - box.clientWidth / 2;
  function drawScreen(fb) {
    const d = img.data;
    for (let i = 0; i < 240 * 240; i++) {
      const v = fb[i], p = ((v >> 8) | (v << 8)) & 0xFFFF;
      d[i * 4] = ((p >> 11) & 31) * 255 / 31;
      d[i * 4 + 1] = ((p >> 5) & 63) * 255 / 63;
      d[i * 4 + 2] = (p & 31) * 255 / 31;
      d[i * 4 + 3] = 255;
    }
    lctx.putImageData(img, 0, 0);
    blit();
  }
  // before Start: the firmware's own HOME, as the simulator drew it, in the skin
  window.affine && window.affine.draw && (async () => {
    const pre = document.createElement("canvas");
    pre.width = pre.height = 240;
    const paint = async () => { if (node) return; await window.affine.draw(pre, "screens/home.png"); lctx.drawImage(pre, 0, 0); blit(); };
    addEventListener("affine:skin", paint);
    paint();
  })();

  // ---------------------------------------------------------------- the store (this browser's saves)
  function loadStore() {
    try {
      const raw = JSON.parse(localStorage.getItem(STORE_KEY) || "{}"), out = {};
      for (const [o, b64] of Object.entries(raw)) out[o] = Uint8Array.from(atob(b64), (c) => c.charCodeAt(0)).buffer;
      return out;
    } catch { return {}; }
  }
  function saveStore(store) {
    try {
      const raw = {};
      for (const [o, u8] of Object.entries(store)) { let s = ""; for (const c of u8) s += String.fromCharCode(c); raw[o] = btoa(s); }
      localStorage.setItem(STORE_KEY, JSON.stringify(raw));
    } catch { /* storage unavailable: this session only */ }
  }

  // ---------------------------------------------------------------- switching on
  const start = $("start"), cpu = $("cpu");
  async function switchOn() {
    start.disabled = true;
    start.querySelector("span").textContent = "Starting";
    say("", "Starting…");
    try {
      const ac = new AudioContext({ sampleRate: 44100, latencyHint: "interactive" });
      const [wasm] = await Promise.all([
        fetch("emu/app.wasm").then((r) => { if (!r.ok) throw new Error(`app.wasm: ${r.status}`); return r.arrayBuffer(); }),
        ac.audioWorklet.addModule("emu/app_worklet.js").catch(() => {}).then(() => ac.audioWorklet.addModule("emu/worklet.js"))]);
      node = new AudioWorkletNode(ac, "fm1", { numberOfInputs: 0, numberOfOutputs: 1, outputChannelCount: [2] });
      gain = ac.createGain();
      gain.gain.value = $("snd").getAttribute("aria-pressed") === "true" ? 0 : 1;
      node.connect(gain).connect(ac.destination);
      const ready = new Promise((res) => {
        node.port.onmessage = (e) => {
          const m = e.data;
          if (m.type === "ready") res();
          else if (m.type === "frame") {
            if (m.fb) drawScreen(m.fb);
            panel.lights(m.buttons, m.keys, m.dimButtons, m.dimKeys);
            if (m.load !== undefined) cpu.textContent = `CPU ${m.load}%`;
            if (m.store) saveStore(m.store);
          }
        };
      });
      const part = await import("./emu/app.js").catch(() => null);
      if (part && part.attach) part.attach({ node, ac, status: (t) => say("", t) });
      node.port.postMessage({ type: "load", wasm, store: loadStore(), master }, [wasm]);
      await ready;
      await ac.resume();
      start.classList.add("gone");
      controls.inert = false;
      device.tabIndex = 0;
      syncButtons(); syncKeys();
      device.focus({ preventScroll: true });
      say("On", ac.sampleRate === 44100 ? "Space plays the break. The keys chop it." : `Space plays the break (audio at ${ac.sampleRate} Hz).`);
    } catch (err) {
      node = null;
      start.disabled = false;
      start.querySelector("span").textContent = "Start";
      say("Off", "Could not start: " + (err && err.message || err) + ". Chrome, Edge, Firefox and Safari should all work.");
    }
  }
  start.addEventListener("click", switchOn);
  device.addEventListener("pointerdown", () => { if (node) device.focus({ preventScroll: true }); });
  $("snd").addEventListener("click", () => {
    const on = $("snd").getAttribute("aria-pressed") !== "true";
    $("snd").setAttribute("aria-pressed", on ? "true" : "false");
    $("snd").textContent = on ? "Muted" : "Mute";
    if (gain) gain.gain.value = level();
  });
  // a drawer opens as an accordion (style.css .drawer): shown, then .open; closed, it hides once its row has closed
  const setDrawer = (d, open) => {
    if (open) { d.hidden = false; void d.offsetHeight; d.classList.add("open"); return; }
    if (d.hidden) return;
    d.classList.remove("open");
    const end = () => { if (!d.classList.contains("open")) d.hidden = true; };
    d.addEventListener("transitionend", end, { once: true });
    setTimeout(end, 400);                             // (no transition with reduced motion: hide all the same)
  };
  const drawer = (btn, id) => $(btn).addEventListener("click", () => {
    const open = $(btn).getAttribute("aria-expanded") !== "true";
    for (const [b, d] of [["keys-tog", "keymap"], ["reset", "reset-ask"]]) if (d !== id) { $(b).setAttribute("aria-expanded", "false"); setDrawer($(d), false); }
    $(btn).setAttribute("aria-expanded", open ? "true" : "false");
    setDrawer($(id), open);
  });
  drawer("keys-tog", "keymap");
  drawer("reset", "reset-ask");
  $("reset-no").addEventListener("click", () => { $("reset").setAttribute("aria-expanded", "false"); setDrawer($("reset-ask"), false); });
  $("reset-yes").addEventListener("click", () => { try { localStorage.removeItem(STORE_KEY); } catch {} location.reload(); });

  // the page's other parts: a black key held from outside the panel (perform.js), the panel's state
  let ducked = false;
  const level = () => ($("snd").getAttribute("aria-pressed") === "true" ? 0 : ducked ? 0.12 : 1);
  window.affineEmu = {
    get on() { return !!node; },
    duck(on) { ducked = on; if (gain) gain.gain.setTargetAtTime(level(), gain.context.currentTime, 0.05); },
    blackKey(n, down) {
      if (!node) return false;
      const id = "x" + n;
      if (down) holdDown(id, "key", BLACK_K[n]); else letGo(id);
      return true;
    },
  };
})();
