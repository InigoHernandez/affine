// SPDX-License-Identifier: MPL-2.0
// AFFINE's hero: the FM-1's title screen (app/ui/aff.c aff_title) drawn wide, pixel by pixel in the skin's ramps:
// the floor of sixteen lanes in one-point perspective, the low-poly gem turning above it, the wordmark (speed stripes
// into chrome letters). It plays the splash once (the floor rushes, the gem flies in, the stripes land, the letters are
// revealed, a wave crosses the lanes), then the floor runs as the Amen plays at 170: a lane lit a step, the gem kicked
// on the beat, a glint across the chrome. A tap on the floor plays that lane. 30 frames a second, as the era drew;
// stopped when out of view; one still frame with reduced motion.
"use strict";
(() => {
  const A = window.affine;
  const box = document.getElementById("scene-box"), cv = document.getElementById("scene");
  if (!box || !cv || !A) return;
  const g = cv.getContext("2d");
  const BAYER = [[0, 8, 2, 10], [12, 4, 14, 6], [3, 11, 1, 9], [15, 7, 13, 5]];
  let W = 0, H = 0, PX = 3, img = null, buf = null, HOR = 0, VX = 0, LANE = 0, ZK = 0;
  let P = null;                                     // the ramps as packed pixels, and the eight colours

  const pack = (c) => (255 << 24) | (c[2] << 16) | (c[1] << 8) | c[0];   // (ImageData's bytes, little-endian)
  function palette(R, c) {
    const out = {};
    for (const [k, r] of Object.entries(R)) out[k] = r.map(pack);
    out.c = c.map(pack);
    return out;
  }
  const dith = (r, t, x, y) => {
    const v = t * 4;
    if (v <= 0) return r[0];
    if (v >= 1024) return r[4];
    const i = v >> 8;
    return r[i + (((v & 255) >> 4) > BAYER[y & 3][x & 3] ? 1 : 0)];
  };
  const clamp01 = (t) => t < 0 ? 0 : t > 1 ? 1 : t;
  const ease = (t) => { t = clamp01(t); return 1 - (1 - t) ** 3; };
  const sinT = (t) => Math.sin(t * 2 * Math.PI);    // (brk_sin_turns: a sine of turns)

  // ---------------------------------------------------------------- the size: whole pixels, PX CSS px each
  function size() {
    const w = box.clientWidth || 600;
    PX = w > 1100 ? 3 : 2;
    W = Math.max(160, Math.round(w / PX));
    H = Math.round(W * (w > 700 ? 0.36 : 0.62));
    cv.width = W; cv.height = H;
    cv.style.height = `${H * w / W}px`;
    img = g.createImageData(W, H);
    buf = new Uint32Array(img.data.buffer);
    HOR = Math.round(H * 0.6);
    VX = W / 2;
    LANE = W * 1.25 / 16;                           // (the lanes 1.25 screens wide at the near edge, as the FM-1's)
    ZK = H - HOR;                                    // (depth 1 at the bottom row)
  }

  // ---------------------------------------------------------------- the state
  const glow = new Float32Array(16);
  let scroll = 0, ang = 0, pulse = 0, slice = -1, step = -1, t0 = 0, last = 0, tapLane = -1, tapUntil = 0;
  const BPM = 170, STEP = 60000 / BPM / 4;
  // the Amen, chopped: its sixteen slices in order, then the edits a jungle track makes of them
  const PAT = [0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15,
               0, 1, 2, 3, 4, 5, 6, 7, 0, 1, 10, 11, 12, 13, 6, 15,
               0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 12, 14, 15,
               0, 1, 4, 5, 2, 3, 6, 7, 8, 9, 2, 3, 12, 13, 14, 14];

  // ---------------------------------------------------------------- the floor
  function floor() {
    const c = P.c;
    for (let y = HOR + 1; y < H; y++) {
      const d = y - HOR + 0.5, z = ZK / d, tile = z / 0.55 + scroll;
      const row = Math.floor(tile), depth = clamp01((z - 1) / 12);
      const lit = Math.trunc(clamp01(1 - depth * 0.45) * 200) + 40;
      let wx = (0 - VX + 0.5) * z, o = y * W;
      for (let x = 0; x < W; x++, wx += z) {
        const u = wx / LANE + 8;
        if (u < 0 || u >= 16) { buf[o + x] = c[7]; continue; }
        const lane = Math.floor(u), fr = u - lane, odd = (lane + row) & 1;
        let col;
        if (fr < 0.045 * z || fr > 1 - 0.045 * z) col = dith(P.floor1, lit * 3 >> 2, x, y);
        else if (lane === slice) col = dith(odd ? P.lane : P.lane2, lit + 40, x, y);
        else if (glow[lane] > 0.2) col = dith(P.lane, Math.trunc(lit * (0.1 + 0.4 * glow[lane])), x, y);
        else col = dith(odd ? P.floor1 : P.floor0, odd ? lit : lit * 3 >> 2, x, y);
        buf[o + x] = col;
      }
    }
  }

  // ---------------------------------------------------------------- the gem (aff.c gem_draw): six faces up, six down
  function gem(cx, cy, r, a, bright, ghost) {
    const vx = [], vy = [], vz = [], P3 = [[0, -1.25, 0], [0, 1, 0]];
    for (let k = 0; k < 6; k++) { const t = a + k / 6; P3.push([sinT(t + 0.25), 0, sinT(t)]); }
    for (let k = 0; k < 8; k++) {
      const p = P3[k], y2 = p[1] * 0.9394 - p[2] * 0.3429, z2 = p[1] * 0.3429 + p[2] * 0.9394, s = 1 / (1 + 0.25 * z2);
      vx[k] = Math.round(cx + p[0] * r * s); vy[k] = Math.round(cy + y2 * r * s); vz[k] = z2;   // (whole pixels: the jitter)
    }
    const faces = [];
    for (let k = 0; k < 6; k++) {
      const a1 = 2 + k, b1 = 2 + (k + 1) % 6;
      faces.push({ v: [0, a1, b1], l: 0.62 + 0.38 * sinT(a + k / 6 + 0.6), hi: true });
      faces.push({ v: [1, b1, a1], l: 0.45 + 0.4 * sinT(a + k / 6 + 0.6), hi: false });
    }
    faces.forEach((f) => { f.z = vz[f.v[0]] + vz[f.v[1]] + vz[f.v[2]]; });
    faces.sort((p, q) => q.z - p.z);                 // far first: the painter's way (no depth buffer then either)
    for (const f of faces) {
      const ax = vx[f.v[0]], ay = vy[f.v[0]], bx = vx[f.v[1]], by = vy[f.v[1]], qx = vx[f.v[2]], qy = vy[f.v[2]];
      if ((bx - ax) * (qy - ay) - (by - ay) * (qx - ax) <= 0) continue;
      let l = Math.trunc(f.l * bright * 256);
      if (ghost) l >>= 1;
      const rp = f.hi ? P.gemHi : P.gemLo;
      const x0 = Math.max(0, Math.min(ax, bx, qx)), x1 = Math.min(W - 1, Math.max(ax, bx, qx));
      const y0 = Math.max(0, Math.min(ay, by, qy)), y1 = Math.min(H - 1, Math.max(ay, by, qy));
      for (let y = y0; y <= y1; y++) for (let x = x0; x <= x1; x++) {
        if ((bx - ax) * (y - ay) - (by - ay) * (x - ax) < 0) continue;
        if ((qx - bx) * (y - by) - (qy - by) * (x - bx) < 0) continue;
        if ((ax - qx) * (y - qy) - (ay - qy) * (x - qx) < 0) continue;
        if (ghost && ((x + y) & 1)) continue;
        buf[y * W + x] = dith(rp, l, x, y);
      }
    }
  }

  // ---------------------------------------------------------------- the wordmark (aff.c LG_Q, aff_logo)
  const LG_H = 20, LG_W = 125, SHEAR = 0.18, NSTRIPES = 4;
  const LG_Q = [[0, 0, 4, 0, 16, 20, 12, 20], [7, 0, 11, 0, 23, 20, 19, 20], [14, 0, 18, 0, 30, 20, 26, 20],
    [21, 0, 25, 0, 37, 20, 33, 20], [28, 0, 32, 0, 44, 20, 40, 20], [40, 16, 52, 16, 52, 20, 42.4, 20],
    [48, 0, 52, 0, 52, 16, 48, 16], [34.4, 6, 48, 6, 48, 10, 36.8, 10], [55, 0, 59, 0, 59, 20, 55, 20],
    [59, 16, 67, 16, 67, 20, 59, 20], [59, 8, 64, 8, 64, 12, 59, 12], [69, 0, 73, 0, 73, 20, 69, 20],
    [73, 16, 81, 16, 81, 20, 73, 20], [73, 8, 78, 8, 78, 12, 73, 12], [83, 0, 87, 0, 87, 20, 83, 20],
    [90, 0, 94, 0, 94, 20, 90, 20], [102, 0, 106, 0, 106, 20, 102, 20], [101, 0, 106, 0, 95, 20, 90, 20],
    [109, 0, 113, 0, 113, 20, 109, 20], [113, 16, 121, 16, 121, 20, 113, 20], [113, 8, 119, 8, 119, 12, 113, 12],
    [113, 0, 121, 0, 121, 4, 113, 4]];
  function logo(ox, oy, sc, reveal, slide, lit, glint) {
    for (let q = 0; q < LG_Q.length; q++) {
      const stripe = q < NSTRIPES, dx = stripe ? slide[q] : 0;
      if (!stripe && LG_Q[q][0] > reveal) continue;
      const X = [], Y = [];
      for (let k = 0; k < 4; k++) {
        const ux = LG_Q[q][2 * k] + dx, uy = LG_Q[q][2 * k + 1];
        X[k] = ox + (ux + uy * SHEAR) * sc; Y[k] = oy + (LG_H - uy) * sc;
      }
      const x0 = Math.max(0, Math.floor(Math.min(...X))), x1 = Math.min(W - 1, Math.floor(Math.max(...X)));
      const y0 = Math.max(0, Math.floor(Math.min(...Y))), y1 = Math.min(H - 1, Math.floor(Math.max(...Y)));
      for (let y = y0; y <= y1; y++) for (let x = x0; x <= x1; x++) {
        const px = x + 0.5, py = y + 0.5;
        let inside = true;
        for (let k = 0; k < 4 && inside; k++) {
          const m = (k + 1) & 3;
          if ((X[m] - X[k]) * (py - Y[k]) - (Y[m] - Y[k]) * (px - X[k]) > 0) inside = false;
        }
        if (!inside) continue;
        const ux = (px - ox) / sc - (LG_H - (py - oy) / sc) * SHEAR;
        if (stripe) {                               // dimmer the further back, brighter up; lit as it lands
          const t = 96 + q * 40 + ((py - oy) < 6 * sc ? 24 : 0) + Math.trunc(lit[q] * 120);
          buf[y * W + x] = dith(P.stripe, t, x, y);
          continue;
        }
        const v = (py - oy) / (LG_H * sc);
        const t = v < 0.12 ? 200 : v < 0.38 ? 256 : v < 0.45 ? 150 : 210 - Math.trunc(v * 120);
        if (reveal - ux < 2) buf[y * W + x] = P.c[4];
        else if (glint >= 0 && Math.abs(ux - glint) < 2.5) buf[y * W + x] = P.c[6];
        else buf[y * W + x] = dith(P.chrome, t, x, y);
      }
    }
  }

  // ---------------------------------------------------------------- a frame, at el ms since the splash began
  function frame(now, el) {
    const dt = last ? Math.min(100, now - last) : 33;
    last = now;
    const e = el, c = P.c;
    // the time: the splash's rush, then the Amen's steps
    const f = clamp01(e / 900), warp = 1 + 40 * (1 - f) * (1 - f);
    if (e > 1600) {
      const s = Math.floor((e - 1600) / STEP);
      if (s !== step) {
        step = s;
        slice = PAT[s % PAT.length];
        if (now < tapUntil) slice = tapLane;
        glow[slice] = 1;
        pulse = s % 4 === 0 ? 1 : Math.max(pulse, 0.45);
      }
      scroll = (e - 1600) / STEP;                    // (a row a step)
    } else {
      scroll += dt / STEP * warp * 0.25;
      slice = -1;
    }
    const gsc = 0.12 + 0.78 * ease(e / 850) + 0.1 * clamp01((e - 900) / 400);
    ang += e < 900 ? 0.09 * (1 - f) : dt / 7000;
    if (e >= 850 && e < 900) pulse = 1;
    if (e >= 1000 && e < 1450) {                     // the logo lands: a wave across the lanes, the middle out
      const reach = (e - 1000) / 450 * 8.5;
      for (let i = 0; i < 16; i++) { const dl = Math.abs(i - 7.5); if (dl <= reach && dl > reach - 1.2) glow[i] = 1; }
    }
    for (let i = 0; i < 16; i++) glow[i] *= Math.pow(2, -dt / 50);   // (a 50 ms half-life: a short warm trail)
    pulse *= Math.pow(2, -dt / 90);

    // the sky, the horizon
    for (let y = 0; y < HOR; y++) {
      const k = Math.trunc(y * 256 / HOR), o = y * W;
      for (let x = 0; x < W; x++) buf[o + x] = dith(P.sky, k, x, y);
    }
    for (let x = 0; x < W; x++) buf[HOR * W + x] = c[1];
    // the rush: streaks out of the vanishing point, longer the faster
    if (e < 1100) {
      const w = 1 - clamp01(e / 1100), len = (30 + 160 * w) * W / 240;
      let sd = (0x2545F491 ^ Math.imul(Math.floor(e / 33), 2654435761)) >>> 0;
      for (let i = 0; i < 40; i++) {
        sd = (Math.imul(sd, 1664525) + 1013904223) >>> 0;
        const an = (sd >>> 8) / 16777216 * 0.5;
        sd = (Math.imul(sd, 1664525) + 1013904223) >>> 0;
        const r0 = 6 + (sd >>> 24) * 0.5 * W / 240, ca = sinT(an + 0.25), sa = sinT(an);
        for (let k = 0; k < len * w + 4; k += 2) {
          const px = Math.round(VX + ca * (r0 + k)), py = Math.round(HOR - sa * (r0 + k) * 0.7);
          if (px >= 0 && px < W && py >= 0 && py < HOR) buf[py * W + px] = k > len * w * 0.6 ? c[6] : c[1];
        }
      }
    }
    floor();
    // the wordmark's size and place, the gem under it
    const sc = Math.min(W * (W < 260 ? 0.86 : 0.64) / (LG_W + LG_H * SHEAR), HOR * 0.42 / LG_H), lw = (LG_W + LG_H * SHEAR) * sc;
    const ox = Math.round((W - lw) / 2), oy = Math.round(HOR * 0.1), lb = oy + LG_H * sc, room = HOR - lb;
    const r = Math.max(4, Math.min(room * 0.4, W * 0.08)) * gsc * (1 + 0.12 * pulse);
    const gy = HOR + (lb + room * 0.54 - HOR) * ease(e / 850);   // (up from the horizon to under the letters)
    if (gsc > 0.02) gem(W * 0.5, gy, r, ang, 1 + 0.35 * pulse, false);
    // the stripes slide in, the letters revealed, the glint
    const slide = [], lit = [];
    for (let i = 0; i < NSTRIPES; i++) {
      const s0 = 500 + 110 * i, u = (e - s0) / 220;
      slide[i] = e < s0 ? -400 : -36 * (1 - ease(u));
      lit[i] = u >= 0 && u < 1.6 ? 1 - u / 1.6 : 0;
    }
    const reveal = e < 1000 ? -1 : 26 + (LG_W - 20) * ease((e - 1000) / 350) + (e > 1350 ? 40 : 0);
    let glint = -1;
    if (e > 1600) { const gl = (e - 1600) % 2600; if (gl < 450) glint = 26 + (LG_W - 20) * gl / 450; }
    logo(ox, oy, sc, reveal, slide, lit, glint);
    g.putImageData(img, 0, 0);
  }

  // ---------------------------------------------------------------- running it
  let running = false, visible = true, raf = 0, lastDraw = 0;
  function loop(now) {
    raf = 0;
    if (!running) return;
    if (now - lastDraw >= 1000 / 30 - 2) { lastDraw = now; frame(now, now - t0); }
    raf = requestAnimationFrame(loop);
  }
  function go() {
    if (running || !A.motion) return;
    running = true;
    raf = requestAnimationFrame(loop);
  }
  function stop() { running = false; if (raf) cancelAnimationFrame(raf); raf = 0; }
  function still() { last = 0; step = -1; frame(performance.now(), 4600); }

  addEventListener("affine:skin", (ev) => {
    P = palette(ev.detail.ramps, ev.detail.skin.c);
    if (!A.motion || !running) still();
  });
  P = palette(A.R, A.SKINS[A.skin].c);
  size();
  new ResizeObserver(() => { const w = W; size(); if (w !== W && (!A.motion || !running)) still(); }).observe(box);
  t0 = performance.now();
  if (A.motion) {
    new IntersectionObserver((es) => { visible = es[0].isIntersecting; if (visible && !document.hidden) go(); else stop(); }).observe(box);
    document.addEventListener("visibilitychange", () => { if (document.hidden) stop(); else if (visible) go(); });
    go();
    setTimeout(() => box.classList.add("ready"), 1800);
  } else {
    still();
    box.classList.add("ready");
  }
  // a tap on the floor plays that lane (the next steps), and kicks the gem
  box.addEventListener("pointerdown", (e) => {
    const rc = cv.getBoundingClientRect(), x = (e.clientX - rc.left) / rc.width * W, y = (e.clientY - rc.top) / rc.height * H;
    const zz = ZK / Math.max(1, y - HOR + 0.5), u = (x - VX) * (y > HOR ? zz : ZK / 1) / LANE + 8;
    const lane = Math.max(0, Math.min(15, Math.floor(u)));
    tapLane = lane; tapUntil = performance.now() + STEP * 3;
    glow[lane] = 1; pulse = 1; slice = lane;
    if (!A.motion) { still(); }
  });
})();
