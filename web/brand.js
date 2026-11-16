// SPDX-License-Identifier: MPL-2.0
// AFFINE's site: the header's isotype, HOME's gem (app/ui/aff.c gem_draw: six faces up, six down, vertices on whole
// pixels, the far faces first, each dithered in the skin's ramps) in a canvas of whole pixels. Still, it is the mark;
// a pointer on it (or the focus) throws it into a spin with a wobble, which slows to a gentle turn while it stays; let
// go, it finishes its turn and rests as it was. Nothing moves with reduced motion.
"use strict";
(() => {
  const A = window.affine, brand = document.querySelector(".brand"), cv = brand && brand.querySelector("canvas");
  if (!A || !cv) return;
  const W = cv.width, H = cv.height, g = cv.getContext("2d"), img = g.createImageData(W, H);
  const buf = new Uint32Array(img.data.buffer);
  const BAYER = [[0, 8, 2, 10], [12, 4, 14, 6], [3, 11, 1, 9], [15, 7, 13, 5]];
  const pack = (c) => (255 << 24) | (c[2] << 16) | (c[1] << 8) | c[0];
  const sinT = (t) => Math.sin(t * 2 * Math.PI);
  let hi = null, lo = null;
  const palette = (R) => { hi = R.gemHi.map(pack); lo = R.gemLo.map(pack); };
  const dith = (r, t, x, y) => {
    const v = t * 4;
    if (v <= 0) return r[0];
    if (v >= 1024) return r[4];
    const i = v >> 8;
    return r[i + (((v & 255) >> 4) > BAYER[y & 3][x & 3] ? 1 : 0)];
  };

  // the gem at turn ang (turns), tilted toward us by tilt (radians), r pixels, bright 1..
  function draw(ang, tilt, r, bright) {
    buf.fill(0);
    const ct = Math.cos(tilt), st = Math.sin(tilt), cx = W / 2 - 0.5, cy = H / 2 + r * 0.12;
    const P = [[0, -1.25, 0], [0, 1, 0]], vx = [], vy = [], vz = [];
    for (let k = 0; k < 6; k++) { const t = ang + k / 6; P.push([sinT(t + 0.25), 0, sinT(t)]); }
    P.forEach((p, k) => {
      const y2 = p[1] * ct - p[2] * st, z2 = p[1] * st + p[2] * ct, s = 1 / (1 + 0.25 * z2);
      vx[k] = Math.round(cx + p[0] * r * s); vy[k] = Math.round(cy + y2 * r * s); vz[k] = z2;
    });
    const faces = [];
    for (let k = 0; k < 6; k++) {
      const a = 2 + k, b = 2 + (k + 1) % 6, l = sinT(ang + k / 6 + 0.6);
      faces.push({ v: [0, a, b], l: 0.62 + 0.38 * l, r: hi }, { v: [1, b, a], l: 0.45 + 0.4 * l, r: lo });
    }
    faces.forEach((f) => { f.z = vz[f.v[0]] + vz[f.v[1]] + vz[f.v[2]]; });
    faces.sort((p, q) => q.z - p.z);
    for (const f of faces) {
      const [ax, bx, qx] = f.v.map((i) => vx[i]), [ay, by, qy] = f.v.map((i) => vy[i]);
      if ((bx - ax) * (qy - ay) - (by - ay) * (qx - ax) <= 0) continue;   // (facing away)
      const l = Math.trunc(f.l * bright * 256);
      for (let y = Math.max(0, Math.min(ay, by, qy)); y <= Math.min(H - 1, Math.max(ay, by, qy)); y++)
        for (let x = Math.max(0, Math.min(ax, bx, qx)); x <= Math.min(W - 1, Math.max(ax, bx, qx)); x++) {
          if ((bx - ax) * (y - ay) - (by - ay) * (x - ax) < 0) continue;
          if ((qx - bx) * (y - by) - (qy - by) * (x - bx) < 0) continue;
          if ((ax - qx) * (y - qy) - (ay - qy) * (x - qx) < 0) continue;
          buf[y * W + x] = dith(f.r, l, x, y);
        }
    }
    g.putImageData(img, 0, 0);
  }

  // the motion: a throw on the pointer, its spin easing to a slow turn; let go, the turn ends on a rest pose (the six
  // sides repeat every sixth of a turn), the wobble and the flash dying away
  const REST = 0, R0 = W * 0.4;
  let ang = REST, spin = 0, pulse = 0, wob = 0, over = false, raf = 0, last = 0;
  function frame(now) {
    raf = 0;
    const dt = last ? Math.min(0.05, (now - last) / 1000) : 1 / 60;
    last = now;
    if (over) {
      spin += (0.22 - spin) * Math.min(1, dt * 2.2);     // (to a slow turn while it stays)
    } else {
      const left = ((REST - ang) % (1 / 6) + 1 / 6) % (1 / 6);   // (to the next rest pose)
      spin = Math.max(0.12, Math.min(spin, left * 6));
      if (left < 0.004 && pulse < 0.02 && wob < 0.02) { ang = REST; spin = 0; pulse = 0; wob = 0; last = 0; draw(ang, 0.35, R0, 1); return; }
    }
    ang = (ang + spin * dt) % 1;
    pulse *= Math.pow(2, -dt / 0.12);
    wob *= Math.pow(2, -dt / 0.35);
    const tilt = 0.35 + wob * 0.5 * Math.sin(now / 90);   // (thrown: it rocks toward and away from us)
    draw(ang, tilt, R0 * (1 + 0.14 * pulse), 1 + 0.45 * pulse);
    raf = requestAnimationFrame(frame);
  }
  const go = () => { if (!raf) raf = requestAnimationFrame(frame); };
  function throwIt() {
    if (!A.motion) return;
    over = true;
    spin = Math.max(spin, 2.4);                          // (turns a second)
    pulse = 1;
    wob = 1;
    go();
  }
  const letGo = () => { over = false; if (A.motion) go(); };
  brand.addEventListener("pointerenter", throwIt);
  brand.addEventListener("pointerleave", letGo);
  brand.addEventListener("focus", throwIt);
  brand.addEventListener("blur", letGo);

  palette(A.R);
  draw(REST, 0.35, R0, 1);
  addEventListener("affine:skin", (e) => { palette(e.detail.ramps); if (!raf) draw(ang, 0.35, R0, 1); });
})();
