// SPDX-License-Identifier: MPL-2.0
// Copyright (C) 2026 Charles Vestal (fm1-base's web/fm1panel.js: the layout, the parts, the lights)
// AFFINE: the same panel in the page's style: every colour a class (style.css .pn-*: the skin's), the labels in the
// pixel font, a ring on a knob while it turns.
//   const P = affPanel(svg)   the body, the knobs, the buttons, the keys and the screen's bezel (viewBox 0 0 1000 640)
//   P.knobs[label] { g, ind, ring, x, y, r, angle, role, label, name }; P.buttons[bit]; P.buttonsByLabel["OCT-"];
//   P.keys[k] (k 0..26); P.lights(b, k, db, dk)
"use strict";
const PANEL_BTN = { FX: 0, SEL: 1, ENV: 2, LFO: 3, EDIT: 4, GLO: 5, HOME: 6, SAVE: 7, ARP: 8, SEQ: 9, PLAY: 10, REC: 11, "OCT-": 12, "OCT+": 13 };
const PANEL_ENC = { SELECT: 0, ALGORITHM: 1, PRESETS: 2, KNOB1: 3, KNOB2: 4, KNOB3: 5, KNOB4: 6 };
const PANEL_WHITE_K = [0, 2, 4, 6, 7, 9, 11, 12, 14, 16, 18, 19, 21, 23, 24, 26];
const PANEL_BLACK_K = [1, 3, 5, 8, 10, 13, 15, 17, 20, 22, 25];
const PANEL_ROW1 = ["FX", "SEL", "ENV", "LFO", "EDIT", "GLO"], PANEL_ROW2 = ["HOME", "SAVE", "ARP", "SEQ", "PLAY", "REC"];

function affPanel(svg) {
  const NS = "http://www.w3.org/2000/svg";
  const BLACK_AFTER = [0, 1, 2, 4, 5, 7, 8, 9, 11, 12, 14];
  function el(tag, attrs, parent = svg) {
    const e = document.createElementNS(NS, tag);
    for (const [k, v] of Object.entries(attrs)) e.setAttribute(k, v);
    parent.appendChild(e);
    return e;
  }
  function text(parent, x, y, s, size, cls = "") {
    const t = el("text", { x, y, "text-anchor": "middle", "font-size": size, class: "pn-lbl " + cls, "pointer-events": "none" }, parent);
    t.textContent = s;
    return t;
  }
  el("rect", { x: 4, y: 4, width: 992, height: 632, rx: 44, class: "pn-body" });
  const knobs = {}, byRole = [];
  function knob(x, y, r, label, role, name) {
    const g = el("g", { class: "ctl knob" });
    el("circle", { cx: x, cy: y, r: r + 5, class: "pn-tray" }, g);
    const ring = el("circle", { cx: x, cy: y, r: r + 10, class: "pn-ring" }, g);
    el("circle", { cx: x, cy: y, r, class: "pn-knob" }, g);
    const ind = el("line", { x1: x, y1: y - r + 5, x2: x, y2: y - r + 15, class: "pn-ind" }, g);
    text(g, x, y - r - 14, label, 16);
    el("circle", { cx: x, cy: y, r: r + 14, fill: "transparent" }, g);
    const k = { g, ind, ring, x, y, r, angle: 0, role, label, name: name || label };
    knobs[label] = k;
    if (typeof role === "number") byRole[role] = k;
    return k;
  }
  knob(105, 100, 24, "MASTER", "master");
  knob(215, 100, 24, "SELECT", PANEL_ENC.SELECT);
  knob(105, 205, 24, "PRESETS", PANEL_ENC.PRESETS);
  knob(215, 205, 24, "ALGORITHM", PANEL_ENC.ALGORITHM);
  for (let i = 0; i < 4; i++) knob(610 + i * 104, 100, 22, `K${i + 1}`, PANEL_ENC[`KNOB${i + 1}`], `KNOB ${i + 1}`);

  const buttons = [], buttonsByLabel = {};
  function button(x, y, w, h, label, two) {
    const g = el("g", { class: "ctl" });
    el("rect", { x, y, width: w, height: h, rx: 6, class: "pn-btn" }, g);
    if (two) { text(g, x + w / 2, y + h / 2 - 2, label, 12, "btn"); text(g, x + w / 2, y + h / 2 + 12, two, 12, "btn"); }
    else text(g, x + w / 2, y + h / 2 + 6, label, label.length > 3 ? 12 : 16, "btn");
    const b = { g, bit: PANEL_BTN[label], label: two ? `${label} ${two}` : label, name: { "OCT-": "OCT minus", "OCT+": "OCT plus" }[label] || label };
    buttons[b.bit] = buttonsByLabel[label] = b;
  }
  el("rect", { x: 58, y: 258, width: 210, height: 58, rx: 14, class: "pn-tray" });
  button(72, 268, 82, 38, "OCT-"); button(170, 268, 82, 38, "OCT+");
  el("rect", { x: 300, y: 44, width: 236, height: 236, rx: 18, class: "pn-bezel" });
  el("rect", { x: 568, y: 160, width: 400, height: 140, rx: 18, class: "pn-tray" });
  for (let i = 0; i < 6; i++) button(582 + i * 63, 174, 54, 48, PANEL_ROW1[i]);
  for (let i = 0; i < 6; i++) button(582 + i * 63, 236, 54, 48, PANEL_ROW2[i], PANEL_ROW2[i] === "PLAY" ? "STOP" : undefined);

  const keys = [];
  (function keybed(x0, y0, w, h) {
    el("rect", { x: x0, y: y0, width: w, height: h, rx: 18, class: "pn-tray" });
    const pitch = (w - 24) / 16, kw = pitch * 0.78;
    BLACK_AFTER.forEach((a, k) => {
      const g = el("g", { class: "ctl" });
      el("rect", { x: x0 + 12 + (a + 1) * pitch - kw / 2, y: y0 + h * 0.06, width: kw, height: h * 0.33, rx: kw / 2, class: "pn-key black" }, g);
      keys[PANEL_BLACK_K[k]] = { g, name: `Black key ${k + 1}`, n: k };
    });
    for (let i = 0; i < 16; i++) {
      const g = el("g", { class: "ctl" });
      el("rect", { x: x0 + 12 + i * pitch + (pitch - kw) / 2, y: y0 + h * 0.43, width: kw, height: h * 0.52, rx: kw / 2, class: "pn-key white" }, g);
      keys[PANEL_WHITE_K[i]] = { g, name: `White key ${i + 1}`, n: i };
    }
  })(36, 340, 928, 270);

  function lights(b, k, db = 0, dk = 0) {
    for (const x of buttons) {
      if (!x) continue;
      const lit = !!(b & (1 << x.bit));
      x.g.classList.toggle("lit", lit);
      x.g.classList.toggle("dim", !lit && !!(db & (1 << x.bit)));
    }
    keys.forEach((x, i) => {
      const lit = !!(k & (1 << i));
      x.g.classList.toggle("lit", lit);
      x.g.classList.toggle("dim", !lit && !!(dk & (1 << i)));
    });
  }
  return { knobs, byRole, buttons, buttonsByLabel, keys, lights, el, text };
}
