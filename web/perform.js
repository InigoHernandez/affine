// SPDX-License-Identifier: MPL-2.0
// AFFINE's site, 03 Perform: the eleven black keys as AFFINE lays them out (app/app.c BK_LAY_AFF), each with the
// FM-1's screen while it is held (the simulator's) and one line. Held here while the FM-1 above is on, the key is
// held there too (play.js window.affineEmu): the screen above and the sound follow.
"use strict";
(() => {
  const A = window.affine, box = document.getElementById("bks");
  if (!box || !A) return;
  const KEYS = [
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
  ];
  const HOME = { n: "HOME", shot: "home", line: "The break's sixteen slices as a floor; the one playing lit." };
  const $ = (id) => document.getElementById(id);
  let held = -1;
  function show(k) {
    const d = k < 0 ? HOME : KEYS[k];
    $("perf-name").textContent = d.n;
    $("perf-key").textContent = k < 0 ? "Hold a key" : `Black key ${k + 1}${window.affineEmu?.on ? " · on the FM-1 too" : ""}`;
    $("perf-line").textContent = d.line;
    $("perf-cap").textContent = k < 0 ? "HOME" : `HOME, ${d.n} held`;
    A.draw($("perf-scr"), `screens/${d.shot}.png`);
  }
  const btns = KEYS.map((k, i) => {
    const b = document.createElement("button");
    b.type = "button";
    b.className = "bk";
    b.setAttribute("aria-pressed", "false");
    b.setAttribute("aria-label", `Black key ${i + 1}: ${k.n}`);
    b.innerHTML = `<span>${k.s}</span><small>${i === 10 ? "-" : (i + 1) % 10}</small>`;
    const down = (e) => {
      e.preventDefault();
      if (held === i) return;
      if (held >= 0) up();
      held = i;
      b.setAttribute("aria-pressed", "true");
      if (e.pointerId !== undefined) b.setPointerCapture(e.pointerId);
      window.affineEmu?.blackKey(i, true);
      show(i);
    };
    const up = () => {
      if (held !== i) return;
      held = -1;
      b.setAttribute("aria-pressed", "false");
      window.affineEmu?.blackKey(i, false);
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
    box.append(b);
    return b;
  });
  let back = 0;
  box.addEventListener("pointerleave", () => { if (held < 0) { clearTimeout(back); back = setTimeout(() => show(-1), 900); } });
  // the FM-1 above played from its own keys: the same key lights here
  addEventListener("affine:keys", (e) => {
    const m = e.detail;
    PANEL_BLACK_K.forEach((bit, i) => { if (held !== i) btns[i].setAttribute("aria-pressed", m & (1 << bit) ? "true" : "false"); });
  });
})();
