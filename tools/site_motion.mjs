// SPDX-License-Identifier: MPL-2.0
// tools/site_motion.mjs: a GIF of a region while actions run (Pillow assembles it; PYTHON= a Python with Pillow).
// e.g. node tools/site_motion.mjs http://127.0.0.1:8781/ chip.gif 1440 ".chip" '[{"js":"document.querySelectorAll(\".stage__go\")[4].click()","ms":900}]'
// node tools/site_motion.mjs URL OUT.gif WIDTH SELECTOR_TO_FRAME ACTIONS_JSON
// frames of a region while actions run (each: {js, ms}), assembled into a GIF by Pillow
import { spawn, execFileSync } from "node:child_process";
import { writeFileSync, mkdtempSync } from "node:fs";
import { tmpdir } from "node:os";
import { join } from "node:path";

const [url, out, width, selArg, actionsJson] = process.argv.slice(2);
const [sel, extra = "0"] = selArg.split("+");   // (".playbar+320": the region 320 px taller, for what opens below)
const actions = JSON.parse(actionsJson);
const CHROME = "/Applications/Google Chrome.app/Contents/MacOS/Google Chrome";
const port = 9800 + Math.floor(Math.random() * 150);
const prof = mkdtempSync(join(tmpdir(), "cdpm-")), frames = mkdtempSync(join(tmpdir(), "frames-"));
const ch = spawn(CHROME, ["--headless=new", "--disable-gpu", "--hide-scrollbars", `--remote-debugging-port=${port}`, `--user-data-dir=${prof}`, "about:blank"], { stdio: "ignore" });
const sleep = (ms) => new Promise((r) => setTimeout(r, ms));
let ws, id = 0;
const pending = new Map();
const send = (method, params = {}) => { const i = ++id; ws.send(JSON.stringify({ id: i, method, params })); return new Promise((res, rej) => pending.set(i, { res, rej })); };
const ev = async (e) => { const r = await send("Runtime.evaluate", { expression: e, awaitPromise: true, returnByValue: true }); if (r.exceptionDetails) throw new Error(JSON.stringify(r.exceptionDetails).slice(0, 300)); return r.result.value; };
try {
  let t;
  for (let k = 0; k < 50 && !t; k++) { await sleep(150); try { t = (await (await fetch(`http://127.0.0.1:${port}/json/list`)).json()).find((x) => x.type === "page"); } catch {} }
  ws = new WebSocket(t.webSocketDebuggerUrl);
  await new Promise((r) => ws.addEventListener("open", r));
  ws.addEventListener("message", (m) => { const d = JSON.parse(m.data); if (d.id && pending.has(d.id)) { const p = pending.get(d.id); pending.delete(d.id); d.error ? p.rej(new Error(d.error.message)) : p.res(d.result); } });
  await send("Page.enable");
  await send("Emulation.setDeviceMetricsOverride", { width: +width, height: 900, deviceScaleFactor: 1, mobile: +width < 768 });
  await send("Page.navigate", { url });
  await sleep(1800);
  await ev(`(async () => { document.documentElement.style.scrollBehavior = "auto"; const H = document.documentElement.scrollHeight;
    for (let y = 0; y < H; y += 300) { scrollTo(0, y); await new Promise((r) => setTimeout(r, 50)); } await new Promise((r) => setTimeout(r, 1500)); })()`);
  await ev(`(() => { const el = document.querySelector(${JSON.stringify(sel)}); scrollTo(0, el.getBoundingClientRect().top + scrollY - 60); })()`);
  await sleep(500);
  let n = 0;
  const grab = async () => {
    const r = await ev(`(() => { const b = document.querySelector(${JSON.stringify(sel)}).getBoundingClientRect(); return { x: b.left + scrollX, y: b.top + scrollY, w: b.width, h: b.height }; })()`);
    const s = await send("Page.captureScreenshot", { format: "png", clip: { x: Math.max(0, r.x - 8), y: Math.max(0, r.y - 8), width: r.w + 16, height: r.h + 16 + +extra, scale: 1 } });
    writeFileSync(join(frames, `f${String(n++).padStart(4, "0")}.png`), Buffer.from(s.data, "base64"));
  };
  for (let k = 0; k < 6; k++) await grab();
  for (const a of actions) {
    await ev(a.js);
    const t0 = Date.now();
    while (Date.now() - t0 < a.ms) await grab();
  }
  for (let k = 0; k < 10; k++) await grab();
  execFileSync(process.env.PYTHON || "python3", ["-c", `
import glob
from PIL import Image
fs = sorted(glob.glob(${JSON.stringify(frames)} + "/f*.png"))
ims = [Image.open(f).convert("RGB") for f in fs]
w = max(i.size[0] for i in ims); h = max(i.size[1] for i in ims)
ims = [i.resize((w, h)) if i.size != (w, h) else i for i in ims]
sc = min(1.0, 720 / w)
if sc < 1: ims = [i.resize((int(w * sc), int(h * sc)), Image.LANCZOS) for i in ims]
ims = [i.quantize(colors=128, method=Image.Quantize.MEDIANCUT) for i in ims]
ims[0].save(${JSON.stringify(out)}, save_all=True, append_images=ims[1:], duration=45, loop=0, optimize=True)
print(len(ims), "frames", ims[0].size)
`], { stdio: "inherit" });
} finally { ws?.close(); ch.kill(); }
