// SPDX-License-Identifier: MPL-2.0
// tools/site_shot.mjs: the site in headless Chrome over the DevTools protocol (no packages: Node 22+ and Chrome).
// Serve web/ first (.claude/launch.json affine-web, http://127.0.0.1:8781: not "localhost", which may be ::1).
// node tools/site_shot.mjs URL OUTPREFIX WIDTH[,WIDTH...] [--js=SCRIPT] [--wait=MS] [--clip=y0:y1]
// headless Chrome over the DevTools protocol: a full-page shot at each width (device metrics emulated, so a phone's
// 390 px is 390), after scrolling the whole page so every reveal has run; --js runs before the shot.
import { spawn } from "node:child_process";
import { writeFileSync, mkdtempSync } from "node:fs";
import { tmpdir } from "node:os";
import { join } from "node:path";

const [url, out, widthsArg, ...rest] = process.argv.slice(2);
const opt = Object.fromEntries(rest.map((a) => a.replace(/^--/, "").split(/=(.*)/s).slice(0, 2)));
const CHROME = "/Applications/Google Chrome.app/Contents/MacOS/Google Chrome";
const port = 9300 + Math.floor(Math.random() * 500);
const prof = mkdtempSync(join(tmpdir(), "cdp-"));
const ch = spawn(CHROME, ["--headless=new", "--disable-gpu", "--hide-scrollbars", `--remote-debugging-port=${port}`, `--user-data-dir=${prof}`, "about:blank"], { stdio: "ignore" });
const sleep = (ms) => new Promise((r) => setTimeout(r, ms));
let ws, id = 0;
const pending = new Map(), errors = [];   // (console errors, exceptions, failed loads: printed per width)
function send(method, params = {}) {
  const i = ++id;
  ws.send(JSON.stringify({ id: i, method, params }));
  return new Promise((res, rej) => pending.set(i, { res, rej }));
}
async function evaluate(expr) {
  const r = await send("Runtime.evaluate", { expression: expr, awaitPromise: true, returnByValue: true });
  if (r.exceptionDetails) throw new Error(JSON.stringify(r.exceptionDetails).slice(0, 400));
  return r.result.value;
}
try {
  let target;
  for (let k = 0; k < 50 && !target; k++) {
    await sleep(150);
    try { target = (await (await fetch(`http://127.0.0.1:${port}/json/list`)).json()).find((t) => t.type === "page"); } catch {}
  }
  ws = new WebSocket(target.webSocketDebuggerUrl);
  await new Promise((r) => ws.addEventListener("open", r));
  ws.addEventListener("message", (m) => {
    const d = JSON.parse(m.data);
    if (d.id && pending.has(d.id)) { const p = pending.get(d.id); pending.delete(d.id); d.error ? p.rej(new Error(d.error.message)) : p.res(d.result); }
    if (d.method === "Runtime.exceptionThrown") errors.push(d.params.exceptionDetails.exception?.description || d.params.exceptionDetails.text);
    if (d.method === "Runtime.consoleAPICalled" && d.params.type === "error") errors.push(d.params.args.map((a) => a.value ?? a.description).join(" "));
    if (d.method === "Log.entryAdded" && d.params.entry.level === "error") errors.push(`${d.params.entry.text} ${d.params.entry.url || ""}`);
  });
  await send("Page.enable"); await send("Runtime.enable"); await send("Log.enable");
  for (const w of widthsArg.split(",").map(Number)) {
    const mobile = w < 768;
    await send("Emulation.setDeviceMetricsOverride", { width: w, height: 900, deviceScaleFactor: 1, mobile });
    errors.length = 0;
    await send("Page.navigate", { url });
    await sleep(1800);
    // walk the page so every reveal runs, then back to the top
    await evaluate(`(async () => { document.documentElement.style.scrollBehavior = "auto"; const H = document.documentElement.scrollHeight;
      for (let y = 0; y < H; y += 300) { scrollTo(0, y); await new Promise((r) => setTimeout(r, 60)); } await new Promise((r) => setTimeout(r, 1600)); scrollTo(0, 0); return H; })()`);
    if (opt.js) console.log(`${w}: js ->`, JSON.stringify(await evaluate(opt.js)));
    await sleep(+(opt.wait || 400));
    const H = await evaluate("document.documentElement.scrollHeight");
    const W = await evaluate("document.documentElement.scrollWidth");
    let clip = { x: 0, y: 0, width: w, height: H, scale: 1 };
    if (opt.clip) { const [a, b] = opt.clip.split(":").map(Number); clip = { x: 0, y: a, width: w, height: Math.min(H, b) - a, scale: 1 }; }
    const shot = await send("Page.captureScreenshot", { format: "png", captureBeyondViewport: true, clip });
    writeFileSync(`${out}-${w}.png`, Buffer.from(shot.data, "base64"));
    console.log(`${w}: ${out}-${w}.png  height ${H}  scrollWidth ${W}${W > w ? "  OVERFLOW" : ""}  errors ${errors.length}`);
    for (const e of errors) console.log(`  error: ${String(e).slice(0, 200)}`);
  }
} finally {
  ws?.close(); ch.kill();
}
