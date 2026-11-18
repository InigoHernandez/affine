// SPDX-License-Identifier: MPL-2.0
// AFFINE's manual (manual.html), after MONO2K's: one page at a time (the address's #name), its breadcrumb and its
// neighbours; the FM-1's screens drawn in the skin's colours as a page opens (site.js A.draw, the mosaic); the FM-1
// drawn on the first page (panel.js); the change log from changelog.js (CHANGELOG.md's), its kinds as filters.
"use strict";
(() => {
  const A = window.affine;
  const $ = (s, r = document) => r.querySelector(s);
  const $$ = (s, r = document) => [...r.querySelectorAll(s)];
  const arts = $$(".doc article"), side = $("#side"), pager = $("#pager");
  if (!arts.length) return;
  document.body.classList.remove("noscript-all");
  const TITLE = document.title;

  // each page: its breadcrumb (Manual / the group / the page)
  arts.forEach((a) => {
    const p = document.createElement("p");
    p.className = "crumb label";
    for (const t of ["Manual", a.dataset.group, $("h1", a).textContent]) p.append(Object.assign(document.createElement("span"), { textContent: t }));
    a.prepend(p);
  });

  // the FM-1 on the first page, HOME on its screen
  const isvg = $("#intro-svg");
  if (isvg && typeof affPanel === "function") affPanel(isvg);

  function screens(a) {
    if (!A) return;
    $$("canvas[data-src]", a).forEach((cv) => {
      if (cv.dataset.shown) return;
      cv.dataset.shown = "1";
      A.draw(cv, undefined, true);
    });
  }
  const link = (a, label) => {
    const x = document.createElement("a");
    x.href = `#${a.id}`;
    x.innerHTML = `<small class="label"></small><b class="title"></b>`;
    x.firstChild.textContent = label;
    x.lastChild.textContent = $("h1", a).textContent;
    return x;
  };
  function show(id, scroll) {
    let i = arts.findIndex((a) => a.id === id);
    if (i < 0) i = 0;
    const a = arts[i];
    arts.forEach((x) => { x.hidden = x !== a; });
    $$("a", side).forEach((x) => x.getAttribute("href") === `#${a.id}` ? x.setAttribute("aria-current", "page") : x.removeAttribute("aria-current"));
    $$(".subnav a[aria-current], .subnav a[data-page]").forEach((x) => x.removeAttribute("aria-current"));
    const sub = a.id === "changelog" ? $(".subnav a[data-page='changelog']") : $(".subnav a[href='manual.html']");
    sub?.setAttribute("aria-current", "page");
    pager.replaceChildren();
    if (i > 0) pager.append(Object.assign(link(arts[i - 1], "Previous"), { className: "pager__prev" }));
    if (i < arts.length - 1) pager.append(Object.assign(link(arts[i + 1], "Next"), { className: "pager__next" }));
    document.title = i ? `${$("h1", a).textContent} · ${TITLE}` : TITLE;
    screens(a);
    if (matchMedia("(max-width: 860px)").matches) side.open = false;
    if (scroll) scrollTo({ top: 0, behavior: "auto" });
  }
  addEventListener("hashchange", () => show(location.hash.slice(1), true));
  // the site's FM-1 screens follow the skin; the panel's colours are CSS
  show(location.hash.slice(1), false);
  if (!location.hash && matchMedia("(max-width: 860px)").matches) side.open = false;

  // ------------------------------------------------------------------------------------------ the change log
  const log = $("[data-changelog]"), CH = window.AFF_CHANGES, KINDS = window.AFF_KINDS;
  if (log && CH && KINDS) {
    // a line's text: `code`, <https://…> and [words](https://…) become code and links; everything else is text
    const rich = (t) => {
      const f = document.createDocumentFragment();
      const re = /`([^`]+)`|<(https?:[^>\s]+)>|\[([^\]]+)\]\((https?:[^)\s]+)\)/g;
      let at = 0, m;
      while ((m = re.exec(t))) {
        if (m.index > at) f.append(t.slice(at, m.index));
        if (m[1] !== undefined) f.append(Object.assign(document.createElement("code"), { textContent: m[1] }));
        else {
          const x = Object.assign(document.createElement("a"), { href: m[2] || m[4], textContent: m[2] ? m[2].replace(/^https?:\/\//, "") : m[3] });
          if (!x.href.startsWith(location.origin)) { x.target = "_blank"; x.rel = "noopener"; }
          f.append(x);
        }
        at = re.lastIndex;
      }
      if (at < t.length) f.append(t.slice(at));
      return f;
    };
    const when = (d) => new Date(`${d}T12:00:00`).toLocaleDateString("en-GB", { day: "numeric", month: "long", year: "numeric" });
    const count = {};
    CH.forEach((u) => u.items.forEach(([k]) => { count[k] = (count[k] || 0) + 1; }));
    const kinds = document.createElement("div");
    kinds.className = "chips log__kinds";
    kinds.setAttribute("role", "group");
    kinds.setAttribute("aria-label", "Kinds of change");
    const chip = (k, label, n) => {
      const b = document.createElement("button");
      b.type = "button";
      b.dataset.kind = k;
      b.setAttribute("aria-pressed", String(k === "all"));
      b.append(label, Object.assign(document.createElement("small"), { textContent: n }));
      return b;
    };
    kinds.append(chip("all", "All", CH.reduce((n, u) => n + u.items.length, 0)), ...Object.keys(KINDS).filter((k) => count[k]).map((k) => chip(k, KINDS[k], count[k])));
    const ups = CH.map((u) => {
      const s = document.createElement("section");
      s.className = "log__up";
      const h = document.createElement("header");
      h.append(Object.assign(document.createElement("time"), { dateTime: u.date, textContent: when(u.date), className: "label" }));
      if (u.title) h.append(Object.assign(document.createElement("h2"), { className: "title", textContent: u.title }));
      const ul = document.createElement("ul");
      u.items.forEach(([k, t]) => {
        const li = document.createElement("li");
        li.dataset.kind = k;
        li.append(Object.assign(document.createElement("span"), { className: "badge", textContent: KINDS[k] || k }));
        const p = document.createElement("p");
        p.append(rich(t));
        li.append(p);
        ul.append(li);
      });
      s.append(h, ul);
      return s;
    });
    log.replaceChildren(kinds, ...ups);
    kinds.addEventListener("click", (e) => {
      const b = e.target.closest("button");
      if (!b) return;
      const kind = b.dataset.kind;
      $$("button", kinds).forEach((x) => x.setAttribute("aria-pressed", String(x === b)));
      ups.forEach((s) => {
        let any = false;
        $$("li", s).forEach((li) => { const on = kind === "all" || li.dataset.kind === kind; li.hidden = !on; any ||= on; });
        s.hidden = !any;
      });
    });
  }
})();
