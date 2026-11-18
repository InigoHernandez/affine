// SPDX-License-Identifier: MPL-2.0
// AFFINE's site, 03 Perform: the eleven black keys in their two rows, as the firmware lays them out (app/app.c
// BK_LAY_AFF, the row AFFINE; BKC_T, the row PUNCH: each combo several functions held together), each with the FM-1's
// screen while it is held (the simulator's: tests/scenarios/pads, affkeys, sitepunch) and one line; a PUNCH key also
// shows what it holds. Held here while the FM-1 above is on, the key is held there too (play.js window.affineEmu), on
// the same row: the switch here sets the FM-1's (FX + OCT), and the FM-1's FX + OCT sets the switch.
"use strict";
(() => {
  const A = window.affine, box = document.getElementById("bks");
  if (!box || !A) return;
  const ROWS = [
    { n: "AFFINE", how: "FX + OCT−", shot: "row-a", line: "Eleven effects, one a key.", keys: [
      { s: "R8", n: "ROLL 1/8", shot: "k-roll8", line: "The slice repeated in eighths while held: the gem's copies orbit." },
      { s: "R16", n: "ROLL 1/16", shot: "k-roll16", line: "Sixteenths: the classic jungle roll." },
      { s: "R32", n: "ROLL 1/32", shot: "k-roll32", line: "Thirty-seconds: a buzz, the copies tighter." },
      { s: "REV", n: "REVERSE", shot: "k-reverse", line: "The break backwards from where it is." },
      { s: "HLF", n: "HALF SPEED", shot: "k-half", line: "Down an octave, half the tempo, in time." },
      { s: "VNL", n: "VINYL BREAK", shot: "k-vinyl", line: "The turntable stopped by hand; let go and it spins back up." },
      { s: "FRZ", n: "FREEZE", shot: "k-freeze", line: "A sliver held and looped: the gem shimmers." },
      { s: "SKP", n: "SKIP", shot: "k-skip", line: "The disc skips: the last moment again, torn lines across." },
      { s: "VRB", n: "VERB THROW", shot: "k-verb", line: "Everything into the chip's reverb, and its tail kept." },
      { s: "CRS", n: "CRUSH", shot: "k-crush", line: "The bits gone at once: the floor in blocks." },
      { s: "FLT", n: "FILTER", shot: "k-filter", line: "A sweep while held, the floor's fog closing in." },
    ] },
    { n: "PUNCH", how: "FX + OCT+", shot: "row-b", line: "Eleven combos: two or three effects at once.", keys: [
      { s: "STU", n: "STUTTER", shot: "pk-stutter", mix: ["R16", "FLT"], line: "Sixteenths, thirty-seconds after a beat, rising into a high-pass." },
      { s: "BUZ", n: "BUZZ", shot: "pk-buzz", mix: ["R32", "CRS"], line: "A thirty-second roll through four bits." },
      { s: "RVB", n: "REV VERB", shot: "pk-revverb", mix: ["REV", "VRB"], line: "The break backwards into the chip's reverb." },
      { s: "SLO", n: "SLOW MO", shot: "pk-slowmo", mix: ["HLF", "FLT"], line: "Half speed an octave down, behind a low-pass." },
      { s: "TPS", n: "TAPE STOP", shot: "pk-tapestop", mix: ["VNL", "VRB"], line: "The turntable stopped over two beats, its tail in the reverb." },
      { s: "GLT", n: "GLITCH", shot: "pk-glitch", mix: ["SKP", "RND"], line: "The disc skips, and half the steps jump elsewhere." },
      { s: "WSH", n: "WASH", shot: "pk-wash", mix: ["FRZ", "VRB", "FLT"], line: "A sliver frozen, low-passed into the reverb." },
      { s: "CHP", n: "CHOP", shot: "pk-chop", mix: ["GAT", "CRS"], line: "Gated in sixteenths, through four bits." },
      { s: "ECH", n: "DUB ECHO", shot: "pk-dubecho", mix: ["R8", "DUB"], line: "Eighths rolled into a dotted-eighth echo." },
      { s: "SHT", n: "SHATTER", shot: "pk-shatter", mix: ["RND", "CRS", "SKP"], line: "Every step jumps and repitches, crushed and skipping." },
      { s: "DRP", n: "DROP OUT", shot: "pk-dropout", mix: ["KIL", "FLT"], line: "BOOM silenced, the break thinned to a high-pass." },
    ] },
  ];
  const ON_A = new Set(ROWS[0].keys.map((k) => k.s));   // (a PUNCH key's parts that are AFFINE keys of their own)
  const HOME = { n: "HOME", shot: "home", line: "The break's sixteen slices as a floor; the one playing lit." };
  const $ = (id) => document.getElementById(id);
  const seg = $("rows"), radios = [...seg.querySelectorAll("[data-row]")];
  let row = 0, switched = false, held = -1, back = 0, emuRow = -1, pend = 0;
  const idle = () => (switched ? { ...ROWS[row], n: ROWS[row].n } : HOME);
  function mix(k) {                          // what a PUNCH key holds, a badge each; else how to reach this row
    const el = $("perf-mix");
    el.replaceChildren();
    if (k >= 0 && ROWS[row].keys[k].mix) {
      ROWS[row].keys[k].mix.forEach((t, j) => {
        if (j) el.append(Object.assign(document.createElement("span"), { className: "perf__plus", textContent: "+" }));
        el.append(Object.assign(document.createElement("span"), { className: `badge${ON_A.has(t) ? " badge--cool" : ""}`, textContent: t, style: `--i:${j}` }));
      });
      el.setAttribute("aria-label", `It holds ${ROWS[row].keys[k].mix.join(", ")}`);
    } else {
      el.append(Object.assign(document.createElement("span"), { className: "label perf__how", textContent: `On the FM-1: ${ROWS[row].how}` }));
      el.removeAttribute("aria-label");
    }
  }
  function show(k) {
    const d = k < 0 ? idle() : ROWS[row].keys[k];
    $("perf-name").textContent = d.n;
    $("perf-key").textContent = k < 0 ? "Hold a key" : `Black key ${k + 1}${window.affineEmu?.on ? " · on the FM-1 too" : ""}`;
    $("perf-line").textContent = d.line;
    $("perf-cap").textContent = k < 0 ? (switched ? `HOME, ROW ${"AB"[row]}` : "HOME") : `HOME, ${d.n} held`;
    mix(k);
    A.draw($("perf-scr"), `screens/${d.shot}.png`);
  }
  function label(b, i, anim) {
    const k = ROWS[row].keys[i];
    b.setAttribute("aria-label", `Black key ${i + 1}: ${k.n}`);
    b.firstChild.textContent = k.s;
    if (anim) { b.classList.remove("swap"); void b.offsetWidth; b.classList.add("swap"); }
  }
  function setRow(r, fromEmu) {
    if (fromEmu) emuRow = r;
    if (r === row) return;
    row = r;
    switched = true;
    seg.style.setProperty("--on", r);
    radios.forEach((b, i) => { b.setAttribute("aria-checked", i === r ? "true" : "false"); b.tabIndex = i === r ? 0 : -1; });
    box.dataset.row = r;
    btns.forEach((b, i) => label(b, i, A.motion));
    if (held >= 0) show(held); else show(-1);
    if (!fromEmu && window.affineEmu?.row(r)) emuRow = r;
  }
  const btns = ROWS[0].keys.map((k, i) => {
    const b = document.createElement("button");
    b.type = "button";
    b.className = "bk";
    b.style.setProperty("--i", i);
    b.setAttribute("aria-pressed", "false");
    b.innerHTML = `<span></span><small>${i === 10 ? "-" : (i + 1) % 10}</small>`;
    label(b, i, false);
    const down = (e) => {
      e.preventDefault();
      if (held === i) return;
      if (held >= 0) up();
      held = i;
      b.setAttribute("aria-pressed", "true");
      if (e.pointerId !== undefined) try { b.setPointerCapture(e.pointerId); } catch {}
      const emu = window.affineEmu;
      if (emu?.on && emuRow !== row) {       // (the FM-1's row unknown or another: set it first, then the key)
        emu.row(row);
        emuRow = row;
        pend = setTimeout(() => { pend = 0; if (held === i) emu.blackKey(i, true); }, 260);
      } else emu?.blackKey(i, true);
      show(i);
    };
    const up = () => {
      if (held !== i) return;
      held = -1;
      b.setAttribute("aria-pressed", "false");
      if (pend) { clearTimeout(pend); pend = 0; } else window.affineEmu?.blackKey(i, false);
      clearTimeout(back);
      back = setTimeout(() => { if (held < 0) show(-1); }, 1600);   // (the screen lingers, then HOME again)
    };
    b.addEventListener("pointerdown", down);
    b.addEventListener("pointerup", up);
    b.addEventListener("pointercancel", up);
    b.addEventListener("lostpointercapture", up);
    b.addEventListener("pointerenter", () => { if (held < 0) { clearTimeout(back); show(i); } });
    b.addEventListener("keydown", (e) => { if ((e.key === " " || e.key === "Enter") && !e.repeat) down(e); });
    b.addEventListener("keyup", (e) => { if (e.key === " " || e.key === "Enter") up(); });
    b.addEventListener("blur", up);
    b.addEventListener("animationend", () => b.classList.remove("swap"));
    box.append(b);
    return b;
  });
  box.addEventListener("pointerleave", () => { if (held < 0) { clearTimeout(back); back = setTimeout(() => show(-1), 900); } });
  // the row switch: a radio group (click, or the arrow keys)
  radios.forEach((b, i) => {
    b.addEventListener("click", () => setRow(i));
    b.addEventListener("keydown", (e) => {
      if (!["ArrowLeft", "ArrowRight", "ArrowUp", "ArrowDown"].includes(e.key)) return;
      e.preventDefault();
      const r = row ? 0 : 1;
      setRow(r);
      radios[r].focus();
    });
  });
  mix(-1);
  // the FM-1 above: its keys light the same keys here; its FX + OCT moves the switch
  addEventListener("affine:keys", (e) => {
    const m = e.detail;
    PANEL_BLACK_K.forEach((bit, i) => { if (held !== i) btns[i].setAttribute("aria-pressed", m & (1 << bit) ? "true" : "false"); });
  });
  addEventListener("affine:row", (e) => setRow(e.detail, true));
})();
