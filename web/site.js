// SPDX-License-Identifier: MPL-2.0
// AFFINE's site: the skins (the firmware's five palettes, app/ui/common.c SKIN_RGB), the dither patterns drawn in
// them, the FM-1's screens recoloured to the skin, and the page's motion. Everything else listens for "affine:skin".
"use strict";
(() => {
  // each: line, dim, gray (labels), mid (amounts: cool), hi (what plays: hot), block (cards), white (text), ground
  const SKINS = [
    { name: "POLYGON", c: [[40, 36, 84], [84, 80, 130], [120, 118, 150], [70, 214, 200], [255, 92, 40], [24, 22, 52], [236, 230, 214], [6, 8, 20]] },
    { name: "MEMCARD", c: [[40, 60, 130], [80, 100, 170], [150, 168, 210], [120, 230, 255], [255, 214, 80], [20, 34, 96], [240, 240, 232], [8, 14, 48]] },
    { name: "DISC", c: [[40, 40, 48], [70, 72, 80], [110, 112, 120], [255, 120, 200], [60, 255, 140], [22, 22, 26], [232, 232, 226], [8, 8, 10]] },
    { name: "BIOS", c: [[60, 66, 84], [100, 108, 130], [160, 168, 186], [255, 180, 60], [140, 200, 255], [36, 40, 52], [245, 247, 250], [16, 18, 24]] },
    { name: "AMBER", c: [[60, 36, 10], [110, 70, 20], [170, 110, 40], [255, 170, 60], [255, 210, 120], [30, 18, 6], [255, 236, 200], [10, 6, 2]] },
  ];
  const VARS = ["--line", "--dim", "--gray", "--cool", "--hot", "--card", "--ink", "--bg"];
  const BAYER = [[0, 8, 2, 10], [12, 4, 14, 6], [3, 11, 1, 9], [15, 7, 13, 5]];
  const hex = (c) => "#" + c.map((v) => v.toString(16).padStart(2, "0")).join("");
  const rgba = (c, a) => `rgba(${c[0]},${c[1]},${c[2]},${a})`;
  const motion = !matchMedia("(prefers-reduced-motion: reduce)").matches;

  // the firmware's ramps (app/ui/aff.c aff_init): five colours from a to b, the dither picks between neighbours
  const ramp = (a, b) => [0, 1, 2, 3, 4].map((i) => [0, 1, 2].map((k) => Math.trunc(a[k] + (b[k] - a[k]) * i / 4)));
  const mix = (a, b, pct) => [0, 1, 2].map((k) => Math.trunc(a[k] + (b[k] - a[k]) * pct / 100));
  function ramps(c) {
    return {
      sky: ramp(c[7], c[5]), floor0: ramp(c[7], c[0]), floor1: ramp(c[7], mix(c[0], c[1], 50)), lane: ramp(c[7], c[4]),
      lane2: ramp(c[7], mix(c[4], c[6], 40)), gemHi: ramp(mix(c[4], c[7], 55), mix(c[4], c[6], 45)),
      gemLo: ramp(mix(c[4], c[7], 85), mix(c[4], c[7], 20)), bar: ramp(c[5], c[3]), chrome: ramp(c[1], c[6]), stripe: ramp(c[5], c[4]),
    };
  }
  function dith(r, t, x, y) {
    const v = t * 4;
    if (v <= 0) return r[0];
    if (v >= 1024) return r[4];
    const i = v >> 8;
    return r[i + (((v & 255) >> 4) > BAYER[y & 3][x & 3] ? 1 : 0)];
  }

  // a pattern drawn pixel by pixel: f(x, y) -> [r, g, b, a] or null (clear)
  function pattern(w, h, f) {
    const cv = document.createElement("canvas");
    cv.width = w; cv.height = h;
    const g = cv.getContext("2d"), im = g.createImageData(w, h);
    for (let y = 0; y < h; y++) for (let x = 0; x < w; x++) {
      const p = f(x, y), o = (y * w + x) * 4;
      if (p) { im.data[o] = p[0]; im.data[o + 1] = p[1]; im.data[o + 2] = p[2]; im.data[o + 3] = p[3] ?? 255; }
    }
    g.putImageData(im, 0, 0);
    return `url(${cv.toDataURL()})`;
  }
  // the 1-bit edges the blocks wipe in through: a mask, opaque left of a dithered edge (scaled 300 %, slid across)
  const MASKS = {
    "--m-wipe": pattern(96, 4, (x, y) => (x < 32 || (x < 64 && (x - 32) * 16 / 32 < 16 - BAYER[y & 3][x & 3])) ? [0, 0, 0] : null),
    "--m-scan": pattern(4, 96, (x, y) => (y < 32 || (y < 64 && (y - 32) * 16 / 32 < 16 - BAYER[y & 3][x & 3])) ? [0, 0, 0] : null),
  };

  let cur = 0, R = ramps(SKINS[0].c);
  function apply(i, save) {
    cur = ((i % SKINS.length) + SKINS.length) % SKINS.length;
    const s = SKINS[cur], c = s.c, root = document.documentElement.style;
    R = ramps(c);
    c.forEach((v, k) => root.setProperty(VARS[k], hex(v)));
    // the dither patterns, at 2 CSS px a pixel (drawn 2x2 and shown at 4 px)
    const two = (f) => (x, y) => f(x >> 1, y >> 1);
    root.setProperty("--d-hot", pattern(4, 4, two((x, y) => ((x + y) & 1) ? c[4] : null)));
    root.setProperty("--d-hot2", pattern(4, 4, two((x, y) => (x & 1) && (y & 1) ? c[4] : null)));
    root.setProperty("--d-dots", pattern(4, 4, two((x, y) => ((x + y) & 1) ? c[0] : null)));
    // bands: 4 px wide, 32 tall, from the colour to nothing (the section's top, the start button's shadow)
    const band = (col, down) => pattern(2, 16, (x, y) => {
      const t = down ? 15 - y : y;
      return t > BAYER[y & 3][x & 3] ? col : null;
    });
    root.setProperty("--g-down", band(c[0], true));
    root.setProperty("--g-hot", band(c[4], true));
    root.setProperty("--g-up", band(c[4], false));
    // the chrome for the big numbers: the logo's letters (a bright band a third down, darker at the foot)
    root.setProperty("--chrome", pattern(2, 28, (x, y) => {
      const v = y / 28, t = v < 0.12 ? 200 : v < 0.38 ? 256 : v < 0.45 ? 150 : 210 - Math.trunc(v * 120);
      return dith(R.chrome, t, x, y);
    }));
    for (const [k, v] of Object.entries(MASKS)) root.setProperty(k, v);
    document.querySelector('meta[name="theme-color"]')?.setAttribute("content", hex(c[7]));
    const btn = document.getElementById("skin");
    if (btn) {
      document.getElementById("skin-n").textContent = s.name;
      btn.setAttribute("aria-label", `The skin: ${s.name}. Next skin`);
      btn.querySelector("i").innerHTML = [4, 3, 6, 1].map((k) => `<b style="background:${hex(c[k])}"></b>`).join("");
    }
    if (save) try { localStorage.setItem("affine.skin", String(cur)); } catch (e) { /* private window */ }
    recolorAll();
    dispatchEvent(new CustomEvent("affine:skin", { detail: { index: cur, skin: s, ramps: R } }));
  }

  // ---------------------------------------------------------------- the FM-1's screens, in the skin
  // The simulator draws them in POLYGON: every colour on them is one of POLYGON's eight or a step of its ramps, each
  // through the FM-1's 16-bit colour. Each colour found maps to the same place in the skin's.
  const q565 = (c) => [c[0] >> 3 << 3, c[1] >> 2 << 2, c[2] >> 3 << 3];
  function places(c) {
    const r = ramps(c), out = c.slice();
    for (const k of Object.keys(r)) out.push(...r[k]);
    return out.map(q565);
  }
  const SRC = places(SKINS[0].c);
  const imgs = new Map();
  function load(src) {
    if (!imgs.has(src)) imgs.set(src, new Promise((res, rej) => { const im = new Image(); im.onload = () => res(im); im.onerror = rej; im.src = src; }));
    return imgs.get(src);
  }
  async function draw(cv, src, anim) {
    anim = anim || (src && src !== cv.dataset.src && cv.dataset.shown);   // (a new picture on a screen in view)
    if (src) cv.dataset.src = src;
    src = cv.dataset.src;
    if (!src) return;
    mos.delete(cv);                                    // (a mosaic under way gives up: this drawing wins)
    let im;
    try { im = await load(src); } catch (e) { return; }
    if (cv.dataset.src !== src) return;               // (another picture asked for meanwhile)
    const g = cv.getContext("2d", { willReadFrequently: true });
    g.drawImage(im, 0, 0, cv.width, cv.height);
    if (cur === 0) { if (anim) mosaic(cv); return; }   // (POLYGON: the pictures' own colours)
    const DST = places(SKINS[cur].c), d = g.getImageData(0, 0, cv.width, cv.height), p = d.data, memo = new Map();
    for (let o = 0; o < p.length; o += 4) {
      const key = (p[o] << 16) | (p[o + 1] << 8) | p[o + 2];
      let m = memo.get(key);
      if (!m) {
        let best = 0, bd = 1e9;
        for (let k = 0; k < SRC.length; k++) {
          const s = SRC[k], dd = (s[0] - p[o]) ** 2 + (s[1] - p[o + 1]) ** 2 + (s[2] - p[o + 2]) ** 2;
          if (dd < bd) { bd = dd; best = k; }
        }
        m = bd < 400 ? DST[best] : [p[o], p[o + 1], p[o + 2]];   // (a colour of LOAF's own, the waves': kept)
        memo.set(key, m);
      }
      p[o] = m[0]; p[o + 1] = m[1]; p[o + 2] = m[2];
    }
    g.putImageData(d, 0, 0);
    if (anim) mosaic(cv);
  }
  // a screen resolves out of a mosaic, big blocks down to its pixels, in four steps (~250 ms; after Libraries.dev's
  // image reveal, in 2D): as a screen arrives, and as it changes to another picture
  const mos = new WeakMap();
  function mosaic(cv) {
    if (!motion || !cv.closest(".scr")) return;
    const w = cv.width, h = cv.height, src = document.createElement("canvas"), tmp = document.createElement("canvas");
    src.width = w; src.height = h; src.getContext("2d").drawImage(cv, 0, 0);
    const g = cv.getContext("2d"), token = {};
    mos.set(cv, token);
    const blocks = [24, 12, 6, 3];
    let i = 0;
    const frame = () => {
      if (mos.get(cv) !== token) return;
      g.imageSmoothingEnabled = false;
      if (i === blocks.length) { g.drawImage(src, 0, 0); mos.delete(cv); return; }
      const b = blocks[i++], sw = Math.ceil(w / b), sh = Math.ceil(h / b);
      tmp.width = sw; tmp.height = sh;
      tmp.getContext("2d").drawImage(src, 0, 0, sw, sh);
      g.drawImage(tmp, 0, 0, sw, sh, 0, 0, sw * b, sh * b);
      setTimeout(frame, 62);
    };
    frame();
  }
  function recolorAll() { document.querySelectorAll("canvas[data-src]").forEach((cv) => draw(cv)); }

  // ---------------------------------------------------------------- motion: reveals, counts, the section in view
  function reveals() {
    if (!motion || !("IntersectionObserver" in window)) return;
    document.documentElement.classList.add("reveal");
    const io = new IntersectionObserver((es) => {
      for (const e of es) {
        if (!e.isIntersecting) continue;
        const el = e.target;
        el.classList.add("in");
        io.unobserve(el);
        if (el.matches(".wipe")) setTimeout(() => el.classList.add("done"), 1400);
        if (el.matches(".scan")) el.querySelectorAll("canvas[data-src]").forEach((cv) => { cv.dataset.shown = "1"; draw(cv, undefined, true); });
        el.querySelectorAll?.("[data-count]").forEach(count);
        if (el.matches("[data-count]")) count(el);
      }
    }, { rootMargin: "0px 0px -12% 0px" });
    document.querySelectorAll(".sec, .rv-up, .wipe, .scan").forEach((el) => io.observe(el));
  }
  // a number counts up when it arrives, stepped (the FM-1's own values jump, they don't glide)
  function count(el) {
    if (el.dataset.counted) return;
    el.dataset.counted = "1";
    const to = parseFloat(el.dataset.count), dp = (el.dataset.count.split(".")[1] || "").length, t0 = performance.now(), dur = 700;
    const step = (now) => {
      const k = Math.min(1, (now - t0) / dur), e = 1 - (1 - k) ** 3, v = Math.round(to * e * 10 ** dp / 1) / 10 ** dp;
      el.textContent = Math.floor(k * 12) === 12 ? el.dataset.count : v.toFixed(dp);
      if (k < 1) requestAnimationFrame(step);
    };
    requestAnimationFrame(step);
  }
  // the section in view: its link lit, and the subnav's one bar slides under it (style.css .subnav__ink)
  function navSpy() {
    const links = [...document.querySelectorAll(".subnav a[href^='#']")], ink = document.querySelector(".subnav__ink");
    const map = new Map(links.map((a) => [a.getAttribute("href").slice(1), a]));
    let on = null;
    const place = () => { if (!ink || !on) return; ink.style.setProperty("--x", `${on.offsetLeft}px`); ink.style.setProperty("--w", `${on.offsetWidth}px`); };
    const io = new IntersectionObserver((es) => {
      for (const e of es) if (e.isIntersecting && map.has(e.target.id)) {
        links.forEach((a) => a.classList.remove("on"));
        on = map.get(e.target.id);
        on.classList.add("on");
        place();
      }
    }, { rootMargin: "-40% 0px -55% 0px" });
    map.forEach((a, id) => { const s = document.getElementById(id); if (s) io.observe(s); });
    addEventListener("resize", place);
    document.fonts?.ready.then(place);
  }

  window.affine = { SKINS, ramps, dith, mix, hex, rgba, apply, draw, motion, get skin() { return cur; }, get R() { return R; } };
  document.addEventListener("DOMContentLoaded", () => {
    document.getElementById("skin")?.addEventListener("click", () => apply(cur + 1, true));
    apply(+(document.documentElement.dataset.skin || 0), false);
    reveals();
    navSpy();
  });
})();
