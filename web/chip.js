// SPDX-License-Identifier: MPL-2.0
// AFFINE's site, 02 The chip: the PSX bus as its five stages in order (app/dsp/psx.c: CODEC, VERB, CRUSH, REDUX,
// SKIP), one open: its screen on the FM-1, one line, its settings heard. The sounds are tests/engine/psx_demo.c's
// renders through the firmware's own effects (a Cyberworm Amen at 175: 4 s dry, then 16 s with the knobs moving; the
// verbs four bars each). A scope draws what plays, in the skin's dither.
"use strict";
(() => {
  const A = window.affine;
  const chain = document.getElementById("chain");
  if (!chain || !A) return;
  const VERB_LEN = 16 * 60 / 175 + 1.5;            // (each preset: four bars at 175 and 1.5 s of tail)
  const VERBS = ["ROOM", "STUDIO S", "STUDIO M", "STUDIO L", "HALL", "HALF ECHO", "SPACE ECHO", "CHAOS", "DELAY"];
  const STAGES = [
    { id: "codec", name: "CODEC", tag: "GRIT · XA", screen: "psx-codec", cap: "EFFECTS › PSX › CODEC",
      line: "The sample grit of the era: GRIT, the sound chip's own ADPCM; XA, the CD's.",
      opts: [{ n: "GRIT", clip: "grit" }, { n: "XA", clip: "xa" }] },
    { id: "verb", name: "VERB", tag: "9 presets", screen: "psx-verb", cap: "EFFECTS › PSX › VERB",
      line: "The sound chip's reverbs: nine rooms from its documented registers, run at 22.05 kHz.",
      opts: VERBS.map((n, i) => ({ n, clip: "verbs", at: i * VERB_LEN, len: VERB_LEN })) },
    { id: "crush", name: "CRUSH", tag: "16 › 1 bit", screen: "psx-lofi", cap: "EFFECTS › PSX › LOFI",
      line: "The bits taken away, down to one. HOME's K2 and black key 10.", opts: [{ n: "CRUSH", clip: "crush" }] },
    { id: "redux", name: "REDUX", tag: "The rate", screen: "psx-lofi", cap: "EFFECTS › PSX › LOFI",
      line: "The sample rate taken down, each sample held. HOME's K3.", opts: [{ n: "REDUX", clip: "redux" }] },
    { id: "skip", name: "SKIP", tag: "A scratched disc", screen: "psx-skip", cap: "EFFECTS › PSX › SKIP",
      line: "The last moment again, as a disc that skips. Black key 8.", opts: [{ n: "SKIP", clip: "skip" }] },
  ];
  const $ = (id) => document.getElementById(id);
  let open = 1, opt = 0;

  STAGES.forEach((s, i) => {
    const li = document.createElement("li");
    li.innerHTML = `<button class="stage__go" type="button" aria-pressed="false"><i>${String(i + 1).padStart(2, "0")}</i><b>${s.name}</b><i>${s.tag}</i></button>`;
    li.querySelector("button").addEventListener("click", () => { pick(i, 0); playNow(); });
    chain.append(li);
  });
  function pick(i, o) {
    open = i; opt = o;
    const s = STAGES[i];
    [...chain.children].forEach((li, k) => { li.classList.toggle("on", k === i); li.querySelector("button").setAttribute("aria-pressed", k === i ? "true" : "false"); });
    $("chip-name").textContent = s.name;
    $("chip-tag").textContent = s.tag;
    $("chip-line").textContent = s.line;
    $("chip-cap").textContent = s.cap;
    A.draw($("chip-scr"), `screens/${s.screen}.png`);
    const box = $("chip-opts");
    box.innerHTML = "";
    if (s.opts.length > 1) s.opts.forEach((p, k) => {
      const b = document.createElement("button");
      b.type = "button";
      b.textContent = p.n;
      b.setAttribute("aria-pressed", k === o ? "true" : "false");
      b.addEventListener("click", () => { pick(open, k); playNow(); });
      box.append(b);
    });
    box.hidden = s.opts.length < 2;
    $("chip-hint").textContent = s.id === "verb" ? `${s.opts[o].n}: four bars, then its tail` : "4 s dry, then the stage, its knobs moving";
  }

  // ---------------------------------------------------------------- the sound
  let ac = null, an = null, src = null, playing = null;
  const bufs = new Map();
  function clip(name) {
    if (!bufs.has(name)) bufs.set(name, fetch(`sounds/${name}.m4a`).then((r) => { if (!r.ok) throw new Error(r.status); return r.arrayBuffer(); })
      .then((b) => ac.decodeAudioData(b)));
    return bufs.get(name);
  }
  function stopNow() {
    if (src) { try { src.stop(); } catch (e) {} src.onended = null; src = null; }
    playing = null;
    window.affineEmu?.duck(false);
    $("chip-play").textContent = "Play";
    $("scope-t").textContent = "Silent";
    $("scope-p").style.width = "0";
  }
  async function playNow() {
    if (!ac) {
      ac = new AudioContext();
      an = ac.createAnalyser();
      an.fftSize = 1024;
      an.connect(ac.destination);
    }
    await ac.resume();
    stopNow();
    const s = STAGES[open], p = s.opts[opt], me = {};
    playing = me;
    $("chip-play").textContent = "Loading";
    let buf;
    try { buf = await clip(p.clip); } catch (e) { $("chip-play").textContent = "Play"; $("scope-t").textContent = "Could not load the sound"; return; }
    if (playing !== me) return;
    src = ac.createBufferSource();
    src.buffer = buf;
    src.connect(an);
    const at = p.at || 0, len = p.len || buf.duration - at;
    src.start(0, at, len);
    me.t0 = ac.currentTime; me.len = len; me.name = p.n;
    src.onended = () => { if (playing === me) stopNow(); };
    window.affineEmu?.duck(true);                  // (the FM-1 above, if on, steps back while this plays)
    $("chip-play").textContent = "Stop";
    draw();
  }
  $("chip-play").addEventListener("click", () => (playing ? stopNow() : playNow()));

  // ---------------------------------------------------------------- the scope: what plays, in whole pixels
  const sc = $("scope"), g = sc.getContext("2d");
  let W = 0, H = 0, im = null, u32 = null, raf = 0;
  const pack = (c) => (255 << 24) | (c[2] << 16) | (c[1] << 8) | c[0];
  function size() {
    const r = sc.getBoundingClientRect();
    W = Math.max(120, Math.round(r.width / 3)); H = Math.max(40, Math.round(r.height / 3));
    sc.width = W; sc.height = H;
    im = g.createImageData(W, H); u32 = new Uint32Array(im.data.buffer);
  }
  new ResizeObserver(() => { size(); paint(null); }).observe(sc);
  const wave = new Float32Array(1024);
  function paint(data) {
    if (!u32) return;
    const s = A.SKINS[A.skin].c, R = A.R, bg = pack(s[7]), mid = H >> 1, hot = pack(s[4]), ink = pack(s[6]);
    const fill = R.lane.map(pack);
    u32.fill(bg);
    for (let x = 0; x < W; x += 2) u32[mid * W + x] = pack(s[0]);
    const pts = [];
    for (let x = 0; x < W; x++) {
      const v = data ? data[Math.floor(x * data.length / W)] : 0;
      pts.push(Math.max(1, Math.min(H - 2, Math.round(mid - v * 1.8 * (H * 0.46)))));
    }
    for (let x = 0; x < W; x++) {                     // under the line, toward the middle: the hot ramp, dithered
      const y = pts[x], a = Math.min(y, mid), b = Math.max(y, mid);
      for (let yy = a; yy <= b; yy++) {
        const t = Math.trunc(256 * (1 - Math.abs(yy - mid) / (H * 0.5)) * 0.7);
        u32[yy * W + x] = A.dith(fill, t, x, yy);
      }
    }
    for (let x = 0; x < W; x++) {
      const y0 = pts[Math.max(0, x - 1)], y1 = pts[x];
      for (let yy = Math.min(y0, y1); yy <= Math.max(y0, y1); yy++) u32[yy * W + x] = x % 7 ? hot : ink;
    }
    g.putImageData(im, 0, 0);
  }
  function draw() {
    if (raf) return;
    const step = () => {
      raf = 0;
      if (!playing || !src) { paint(null); return; }
      an.getFloatTimeDomainData(wave);
      paint(wave);
      const k = Math.min(1, (ac.currentTime - playing.t0) / playing.len);
      $("scope-p").style.width = `${k * 100}%`;
      const at = ac.currentTime - playing.t0;
      $("scope-t").textContent = STAGES[open].id === "verb" ? `${playing.name} · ${at.toFixed(1)} s` : (at < 4 ? `Dry · ${at.toFixed(1)} s` : `${playing.name} · ${at.toFixed(1)} s`);
      raf = requestAnimationFrame(step);
    };
    raf = requestAnimationFrame(step);
  }
  addEventListener("affine:skin", () => paint(null));
  pick(1, 0);
  size(); paint(null);
})();
