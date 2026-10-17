"use strict";

const $ = (sel) => document.querySelector(sel);

const fileInput = $("#file");
const dropzone = $("#dropzone");
const stage = $("#stage");
const frame = $("#frame");
const preview = $("#preview");
const overlay = $("#overlay");
const output = $("#output");
const statusline = $("#statusline");
const copyBtn = $("#copy");
const segBtns = [...document.querySelectorAll(".seg-btn")];
const selectAllBtn = $("#select-all");
const selCount = $("#sel-count");
const scaleSel = $("#scale");
const sortSel = $("#sort");
const paraSel = $("#para");
const presetTrigger = $("#preset-trigger");
const presetCur = $("#preset-cur");
const presetMenu = $("#preset-menu");
const sidebar = $("#sidebar");
const sidebarScrim = $("#sidebar-scrim");
const sidebarOpenBtn = $("#sidebar-open");
const sidebarCloseBtn = $("#sidebar-close");
const pickBtn = $("#pick");
const clearBtn = $("#clear");
const boxesBtn = $("#toggle-boxes");
const imgToolbar = $("#img-toolbar");
const errorbar = $("#errorbar");
const errorText = $("#error-text");
const retryBtn = $("#retry");
const healthDot = $("#health-dot");
const healthText = $("#health-text");

let currentFile = null;   // original File, kept so scale changes re-encode from source
let currentDataUrl = null; // data URL of the normalized image, sent to /v1/ocr
let blocks = [];          // data[] of last result; .text reflects edits
let showBoxes = true;
let mode = "text";        // blocks | text
let selected = new Set(); // copy scope: indexes to emit; full after every result
let controller = null;    // AbortController of in-flight request

// ---------- health & models ----------

async function checkHealth() {
  try {
    const r = await fetch("v1/health");
    if (!r.ok) throw new Error();
    healthDot.className = "dot ok";
    healthText.textContent = "在线";
  } catch {
    healthDot.className = "dot err";
    healthText.textContent = "离线";
  }
}

async function loadModels() {
  try {
    const r = await fetch("v1/models");
    if (!r.ok) throw new Error();
    const j = await r.json();
    setPresets((j.data || []).map((m) => m.id), j.current);
  } catch {
    // keep the built-in presets in the menu
    setPresets(["medium", "small", "tiny"], "small");
  }
}

function setPresets(ids, current) {
  if (!ids.length) return;
  if (!current || !ids.includes(current)) current = ids[0];
  presetCur.textContent = `(${current})`;
  presetMenu.innerHTML = "";
  for (const id of ids) {
    const btn = document.createElement("button");
    btn.type = "button";
    btn.role = "menuitemradio";
    btn.dataset.preset = id;
    btn.setAttribute("aria-checked", String(id === current));
    if (id === current) btn.classList.add("current");
    btn.innerHTML =
      '<svg width="14" height="14" viewBox="0 0 14 14" aria-hidden="true"><polyline points="2,7.5 5.5,11 12,3.5" fill="none" stroke="currentColor" stroke-width="1.8" stroke-linecap="round" stroke-linejoin="round"/></svg>';
    btn.append(id);
    presetMenu.appendChild(btn);
  }
  presetMenu.dataset.current = current;
}

function setPresetOpen(open) {
  presetMenu.hidden = !open;
  presetTrigger.setAttribute("aria-expanded", String(open));
}

presetTrigger.addEventListener("click", (e) => {
  e.stopPropagation();
  setPresetOpen(presetMenu.hidden);
});

presetMenu.addEventListener("click", (e) => {
  const btn = e.target.closest("button[data-preset]");
  if (!btn) return;
  const id = btn.dataset.preset;
  setPresetOpen(false);
  if (id === presetMenu.dataset.current) return;
  setPresets([...presetMenu.querySelectorAll("button")].map((b) => b.dataset.preset), id);
  if (currentDataUrl) ocr(); // re-recognize with the newly picked preset
});

// close the preset menu on outside click or Escape
document.addEventListener("click", (e) => {
  if (!presetMenu.hidden && !e.target.closest(".brand")) setPresetOpen(false);
});
document.addEventListener("keydown", (e) => {
  if (e.key === "Escape" && !presetMenu.hidden) setPresetOpen(false);
});

// ---------- sidebar ----------

function setSidebar(open) {
  sidebarScrim.hidden = !open;
  sidebarOpenBtn.setAttribute("aria-expanded", String(open));
  if (open) {
    sidebar.hidden = false;
    requestAnimationFrame(() => sidebar.classList.add("open"));
  } else {
    sidebar.classList.remove("open");
    sidebar.hidden = true; // no exit animation; transform 0.2s only matters on open
  }
}

sidebarOpenBtn.addEventListener("click", () => setSidebar(true));
sidebarCloseBtn.addEventListener("click", () => setSidebar(false));
sidebarScrim.addEventListener("click", () => setSidebar(false));
document.addEventListener("keydown", (e) => {
  if (e.key === "Escape" && !sidebar.hidden) setSidebar(false);
});

// ---------- image input ----------

dropzone.addEventListener("click", () => fileInput.click());
pickBtn.addEventListener("click", () => fileInput.click());
fileInput.addEventListener("change", () => {
  if (fileInput.files[0]) loadFile(fileInput.files[0]);
  fileInput.value = "";
});

["dragover", "dragenter"].forEach((ev) =>
  document.addEventListener(ev, (e) => {
    e.preventDefault();
    document.body.classList.add("dragging");
  })
);
["dragleave", "drop"].forEach((ev) =>
  document.addEventListener(ev, (e) => {
    e.preventDefault();
    document.body.classList.remove("dragging");
  })
);
document.addEventListener("drop", (e) => {
  const f = e.dataTransfer?.files?.[0];
  if (f && f.type.startsWith("image/")) loadFile(f);
});

document.addEventListener("paste", (e) => {
  if (document.activeElement?.closest("#output")) return; // paste in edit mode: text only
  const item = [...(e.clipboardData?.items || [])].find((i) =>
    i.type.startsWith("image/")
  );
  if (item) loadFile(item.getAsFile());
});

async function loadFile(file) {
  try {
    currentFile = file;
    currentDataUrl = await normalize(file);
    // new image invalidates the previous overlay: clear boxes before the new
    // src loads, so the old result can't be drawn on the new image
    blocks = [];
    overlay.innerHTML = "";
    preview.src = currentDataUrl;
    dropzone.hidden = true;
    stage.hidden = false;
    imgToolbar.hidden = false;
    await ocr();
  } catch (err) {
    showError("图片处理失败：" + err.message);
  }
}

const webpOk = (() => {
  try {
    return document.createElement("canvas").toDataURL("image/webp").startsWith("data:image/webp");
  } catch {
    return false;
  }
})();

async function normalize(file) {
  // imageOrientation: 'from-image' bakes EXIF rotation into pixels,
  // so preview and engine see the same orientation
  const bmp = await createImageBitmap(file, { imageOrientation: "from-image" });
  const longEdge = Number(scaleSel.value);
  const max = longEdge || 4096;
  const scale = Math.min(1, max / Math.max(bmp.width, bmp.height));
  const w = Math.round(bmp.width * scale);
  const h = Math.round(bmp.height * scale);
  const canvas = document.createElement("canvas");
  canvas.width = w;
  canvas.height = h;
  canvas.getContext("2d").drawImage(bmp, 0, 0, w, h);
  bmp.close();
  const type = webpOk ? "image/webp" : "image/jpeg";
  return canvas.toDataURL(type, 0.9);
}

scaleSel.addEventListener("change", () => {
  if (currentFile) loadFile(currentFile);
});

clearBtn.addEventListener("click", () => {
  currentFile = null;
  currentDataUrl = null;
  preview.removeAttribute("src");
  overlay.innerHTML = "";
  stage.hidden = true;
  dropzone.hidden = false;
  imgToolbar.hidden = true;
  // results on the right are intentionally kept
});

// ---------- ocr ----------

function layoutOptions() {
  return { sort: sortSel.value, para: Number(paraSel.value) };
}

async function postJSON(url, body, signal) {
  const r = await fetch(url, {
    method: "POST",
    headers: { "Content-Type": "application/json" },
    body: JSON.stringify(body),
    signal,
  });
  const j = await r.json().catch(() => null);
  if (!j) throw new Error("服务器返回了非 JSON 响应");
  if (j.code === 100 || j.code === 101) {
    renderResult(j);
  } else {
    showError(j.message || `识别失败（错误 ${j.code}）`, true);
    setStatus("");
  }
}

async function ocr() {
  if (!currentDataUrl) return;
  if (controller) controller.abort();
  controller = new AbortController();
  setLoading(true, "识别中…");
  hideError();
  try {
    await postJSON(
      "v1/ocr",
      { model: presetMenu.dataset.current, image: currentDataUrl, ...layoutOptions() },
      controller.signal
    );
  } catch (err) {
    if (err.name === "AbortError") return;
    showError("请求失败：" + err.message, true);
    setStatus("");
    checkHealth();
  } finally {
    setLoading(false);
  }
}

// Re-run only the layout stage on the blocks we already have: no re-OCR.
async function reprocess() {
  if (!blocks.length) {
    if (currentDataUrl) return ocr();
    return;
  }
  commitEdits(); // pending text edits must go with the blocks
  if (controller) controller.abort();
  controller = new AbortController();
  setLoading(true, "重排中…");
  hideError();
  try {
    await postJSON(
      "v1/reprocess",
      { blocks, ...layoutOptions() },
      controller.signal
    );
  } catch (err) {
    if (err.name === "AbortError") return;
    showError("请求失败：" + err.message, true);
    setStatus("");
    checkHealth();
  } finally {
    setLoading(false);
  }
}

sortSel.addEventListener("change", () => reprocess());
paraSel.addEventListener("change", () => reprocess());

// ---------- modes ----------

function setMode(next) {
  if (mode === "text" && next !== "text") commitEdits();
  mode = next;
  for (const btn of segBtns) {
    const on = btn.dataset.mode === next;
    btn.classList.toggle("active", on);
    btn.setAttribute("aria-checked", String(on));
  }
  output.classList.toggle("mode-blocks", next === "blocks");
  output.classList.toggle("mode-text", next === "text");
  renderView();
}

for (const btn of segBtns)
  btn.addEventListener("click", () => setMode(btn.dataset.mode));

selectAllBtn.addEventListener("click", () => {
  const all = blocks.length > 0 && selected.size === blocks.length;
  selected.clear();
  if (!all) blocks.forEach((_, i) => selected.add(i));
  syncSelection();
});

// Text mode edits are committed on every mode switch, before copy and
// before reprocess, so blocks[].text stays the source of truth.
function commitEdits() {
  if (mode !== "text") return;
  for (const el of output.querySelectorAll(".block")) {
    const i = Number(el.dataset.idx);
    if (blocks[i]) blocks[i].text = el.innerText;
  }
}

output.addEventListener("paste", (e) => {
  if (mode !== "text" || !e.target.closest?.(".block")) return;
  e.preventDefault();
  document.execCommand("insertText", false, e.clipboardData.getData("text/plain"));
});

// ---------- copy: exactly the selected blocks ----------

function collectText() {
  commitEdits();
  return blocks
    .filter((_, i) => selected.has(i))
    // The engine's per-block "end" separators carry the paragraph structure.
    .map((b) => b.text + (b.end ?? "\n"))
    .join("");
}

copyBtn.addEventListener("click", async () => {
  const text = collectText();
  try {
    await navigator.clipboard.writeText(text);
  } catch {
    const ta = document.createElement("textarea");
    ta.value = text;
    document.body.appendChild(ta);
    ta.select();
    document.execCommand("copy");
    ta.remove();
  }
  copyBtn.textContent = "已复制";
  setTimeout(() => (copyBtn.textContent = "复制"), 1200);
});

retryBtn.addEventListener("click", () => ocr());

boxesBtn.addEventListener("click", () => {
  showBoxes = !showBoxes;
  boxesBtn.classList.toggle("active", showBoxes);
  boxesBtn.setAttribute("aria-pressed", String(showBoxes));
  overlay.style.display = showBoxes ? "" : "none";
});

// ---------- rendering ----------

function renderResult(j) {
  blocks = j.data || [];
  // every result defines a new copy scope: start from "copy everything";
  // pruning noise is a deliberate act the user then performs
  selected = new Set(blocks.map((_, i) => i));
  // boxes must exist before renderView: its syncSelection stamps .sel onto
  // the polygons, and freshly drawn ones would otherwise stay gray
  drawBoxes();
  renderView();
  const has = blocks.length > 0;
  selectAllBtn.hidden = !has;
  for (const btn of segBtns) btn.disabled = !has;
  setStatus(formatStatus(j));
}

// Rebuild the visible view from blocks[]. Both modes render the same data;
// text mode shows exactly what copy produces, blocks mode one row per box.
function renderView() {
  output.innerHTML = "";
  if (!blocks.length) {
    output.innerHTML = '<p class="empty-hint">未识别到文字</p>';
    syncSelection();
    return;
  }
  if (mode === "blocks") renderBlocksView();
  else renderTextView();
  syncSelection();
}

function renderBlocksView() {
  blocks.forEach((b, i) => {
    const div = document.createElement("div");
    div.className = "block";
    div.dataset.idx = String(i);
    div.textContent = b.text;
    output.appendChild(div);
  });
}

function renderTextView() {
  // Group blocks into paragraphs by end === "\n"; spans render inline so the
  // flow equals the copy output. end === " " becomes a real space text node:
  // CSS cannot produce it, and native selection copy must include it.
  let para = document.createElement("div");
  para.className = "para";
  blocks.forEach((b, i) => {
    const span = document.createElement("span");
    span.className = "block";
    span.contentEditable = "true";
    span.spellcheck = false;
    span.dataset.idx = String(i);
    span.textContent = b.text;
    para.appendChild(span);
    if (b.end === " ") para.appendChild(document.createTextNode(" "));
    if (b.end === "\n") {
      output.appendChild(para);
      para = document.createElement("div");
      para.className = "para";
    }
  });
  if (para.childNodes.length) output.appendChild(para);
}

function drawBoxes() {
  overlay.innerHTML = "";
  if (!preview.naturalWidth) return;
  // frame is the single source of truth for display size: its aspect ratio
  // comes from the image, the img fills it (object-fit:contain), and the
  // overlay is inset 0 of the same box — so boxes can never drift off image.
  // set as soon as the image loads, before any OCR result, so a freshly
  // picked image is correctly sized immediately instead of snapping late
  frame.style.aspectRatio = `${preview.naturalWidth} / ${preview.naturalHeight}`;
  if (!blocks.length) return;
  overlay.setAttribute("viewBox", `0 0 ${preview.naturalWidth} ${preview.naturalHeight}`);
  overlay.setAttribute("preserveAspectRatio", "xMidYMid meet");
  blocks.forEach((b, i) => {
    const poly = document.createElementNS("http://www.w3.org/2000/svg", "polygon");
    poly.setAttribute("points", b.box.map((p) => p.join(",")).join(" "));
    poly.dataset.idx = i;
    // carry the current copy scope: boxes can be redrawn without a render
    // (e.g. the preview reloads on re-encode) and must not flash gray
    if (selected.has(i)) poly.classList.add("sel");
    overlay.appendChild(poly);
  });
}

// ---------- box <-> text two-way highlight ----------

function setHighlight(idx) {
  output.querySelectorAll(".block.hl").forEach((el) => el.classList.remove("hl"));
  overlay.querySelectorAll("polygon.hl").forEach((el) => el.classList.remove("hl"));
  if (idx < 0) return;
  output.querySelector(`.block[data-idx="${idx}"]`)?.classList.add("hl");
  overlay.querySelector(`polygon[data-idx="${idx}"]`)?.classList.add("hl");
}

function blockIdxFrom(el) {
  const b = el?.closest?.(".block");
  return b ? Number(b.dataset.idx) : -1;
}

output.addEventListener("mouseover", (e) => {
  const i = blockIdxFrom(e.target);
  if (i >= 0) setHighlight(i);
});
output.addEventListener("mouseleave", () => setHighlight(-1));

overlay.addEventListener("mouseover", (e) => {
  const idx = e.target.dataset?.idx;
  if (idx !== undefined) setHighlight(Number(idx));
});
overlay.addEventListener("mouseleave", () => setHighlight(-1));

// text pane: blocks mode click toggles selection; text mode leaves the
// click to the native caret / editor.
output.addEventListener("click", (e) => {
  if (mode !== "blocks") return;
  const i = blockIdxFrom(e.target);
  if (i >= 0) toggleSelect(i);
});

// image pane: a click always toggles selection in both modes; scroll the
// text pane only when the block became selected (deselecting a noise box
// must not yank the view around, e.g. after 全选 plus manual pruning).
overlay.addEventListener("click", (e) => {
  const idx = e.target.dataset?.idx;
  if (idx === undefined) return;
  const i = Number(idx);
  const becameSelected = !selected.has(i);
  toggleSelect(i);
  if (becameSelected || mode === "text") scrollToBlock(i);
});

function scrollToBlock(i) {
  output
    .querySelector(`.block[data-idx="${i}"]`)
    ?.scrollIntoView({ block: "nearest", behavior: "smooth" });
}

function toggleSelect(i) {
  if (selected.has(i)) selected.delete(i);
  else selected.add(i);
  syncSelection();
}

function syncSelection() {
  output.querySelectorAll(".block").forEach((el) =>
    el.classList.toggle("sel", selected.has(Number(el.dataset.idx)))
  );
  overlay.querySelectorAll("polygon").forEach((p) =>
    p.classList.toggle("sel", selected.has(Number(p.dataset.idx)))
  );
  selectAllBtn.textContent = selected.size > 0 && selected.size === blocks.length ? "取消" : "全选";
  selCount.textContent = `${selected.size} / ${blocks.length}`;
  selCount.hidden = blocks.length === 0;
  copyBtn.disabled = selected.size === 0;
}

preview.addEventListener("load", drawBoxes);

// ---------- status / errors ----------

function setLoading(loading, label = "识别中…") {
  statusline.innerHTML = loading ? '<span class="spin"></span>' + label : statusline.innerHTML;
  if (loading) {
    copyBtn.disabled = true;
    for (const btn of segBtns) btn.disabled = true;
    output.innerHTML = "";
  }
}

function setStatus(html) {
  statusline.innerHTML = html;
}

function formatStatus(j) {
  const parts = [];
  if (j.time) {
    const t = j.time;
    parts.push(`det ${t.det?.toFixed(0) ?? "?"} · cls ${t.cls?.toFixed(0) ?? "?"} · rec ${t.rec?.toFixed(0) ?? "?"} · total ${t.total?.toFixed(0) ?? "?"}ms`);
  }
  parts.push(`${blocks.length} 段`);
  const rec = j.meta?.models?.rec;
  if (rec) parts.push(rec.split("/").pop().replace(/\.onnx$/, ""));
  return parts.join("  ·  ");
}

function showError(msg, retryable = false) {
  errorText.textContent = msg;
  retryBtn.hidden = !retryable;
  errorbar.hidden = false;
}

function hideError() {
  errorbar.hidden = true;
}

checkHealth();
loadModels();
