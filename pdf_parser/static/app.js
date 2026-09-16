/* global pdfjsLib */

const fileInput = document.getElementById("fileInput");
const dropzone = document.getElementById("dropzone");
const workspace = document.getElementById("workspace");
const pagesEl = document.getElementById("pages");
const pageSpec = document.getElementById("pageSpec");
const password = document.getElementById("password");
const applySpec = document.getElementById("applySpec");
const selectAll = document.getElementById("selectAll");
const clearAll = document.getElementById("clearAll");
const exportBtn = document.getElementById("exportBtn");
const selectionSummary = document.getElementById("selectionSummary");
const fileMeta = document.getElementById("fileMeta");
const bookmarks = document.getElementById("bookmarks");
const bookmarkList = document.getElementById("bookmarkList");
const toast = document.getElementById("toast");

pdfjsLib.GlobalWorkerOptions.workerSrc =
  "https://cdnjs.cloudflare.com/ajax/libs/pdf.js/3.11.174/pdf.worker.min.js";

const state = {
  file: null,
  pageCount: 0,
  selected: new Set(),
};

function showToast(message) {
  toast.hidden = false;
  toast.textContent = message;
  window.clearTimeout(showToast.timer);
  showToast.timer = window.setTimeout(() => {
    toast.hidden = true;
  }, 3200);
}

function selectedPagesSorted() {
  return [...state.selected].sort((a, b) => a - b);
}

function updateSummary() {
  const count = state.selected.size;
  selectionSummary.textContent = count
    ? `已选择 ${count} / ${state.pageCount} 页`
    : `共 ${state.pageCount} 页，尚未选择`;
  exportBtn.disabled = count === 0 || !state.file;
  pageSpec.value = selectedPagesSorted().join(",");
  document.querySelectorAll(".page-card").forEach((card) => {
    const page = Number(card.dataset.page);
    card.classList.toggle("selected", state.selected.has(page));
  });
}

function togglePage(page, force) {
  const shouldSelect = force === undefined ? !state.selected.has(page) : force;
  if (shouldSelect) state.selected.add(page);
  else state.selected.delete(page);
  updateSummary();
}

function parseLocalSpec(spec, pageCount) {
  const tokens = spec.split(/[,\s]+/).filter(Boolean);
  const selected = [];
  const seen = new Set();
  const add = (page) => {
    if (page < 1 || page > pageCount) {
      throw new Error(`页码 ${page} 超出范围（共 ${pageCount} 页）`);
    }
    if (!seen.has(page)) {
      seen.add(page);
      selected.push(page);
    }
  };
  for (const token of tokens) {
    const lower = token.toLowerCase();
    if (lower === "odd") {
      for (let page = 1; page <= pageCount; page += 2) add(page);
      continue;
    }
    if (lower === "even") {
      for (let page = 2; page <= pageCount; page += 2) add(page);
      continue;
    }
    const range = token.match(/^(\d+)?-(\d+)?$/);
    if (range) {
      const start = range[1] ? Number(range[1]) : 1;
      const end = range[2] ? Number(range[2]) : pageCount;
      if (start > end) throw new Error(`无效范围 ${token}`);
      for (let page = start; page <= end; page += 1) add(page);
      continue;
    }
    if (/^\d+$/.test(token)) {
      add(Number(token));
      continue;
    }
    throw new Error(`无法识别页码：${token}`);
  }
  return selected;
}

async function renderPdf(file) {
  const bytes = await file.arrayBuffer();
  const pdf = await pdfjsLib.getDocument({ data: bytes }).promise;
  state.pageCount = pdf.numPages;
  state.selected = new Set();
  pagesEl.innerHTML = "";

  for (let pageNumber = 1; pageNumber <= pdf.numPages; pageNumber += 1) {
    const page = await pdf.getPage(pageNumber);
    const viewport = page.getViewport({ scale: 0.35 });
    const card = document.createElement("button");
    card.type = "button";
    card.className = "page-card";
    card.dataset.page = String(pageNumber);
    const canvas = document.createElement("canvas");
    canvas.width = viewport.width;
    canvas.height = viewport.height;
    const label = document.createElement("div");
    label.className = "label";
    label.innerHTML = `<span>第 ${pageNumber} 页</span><span class="check"></span>`;
    card.append(canvas, label);
    card.addEventListener("click", () => togglePage(pageNumber));
    pagesEl.append(card);
    await page.render({ canvasContext: canvas.getContext("2d"), viewport }).promise;
  }
  updateSummary();
}

async function loadInfo(file) {
  const form = new FormData();
  form.append("pdf", file);
  if (password.value) form.append("password", password.value);
  const response = await fetch("/api/info", { method: "POST", body: form });
  const payload = await response.json();
  if (!payload.ok) throw new Error(payload.error || "无法读取 PDF");

  fileMeta.hidden = false;
  fileMeta.innerHTML = `<strong>${file.name}</strong> · ${payload.pageCount} 页${
    payload.title ? ` · ${payload.title}` : ""
  }`;

  bookmarkList.innerHTML = "";
  if (payload.bookmarks && payload.bookmarks.length) {
    bookmarks.hidden = false;
    payload.bookmarks.forEach((item) => {
      const li = document.createElement("li");
      const button = document.createElement("button");
      button.type = "button";
      const pageLabel = item.page ? `p.${item.page}` : "p.?";
      button.textContent = `${"  ".repeat(item.depth)}${item.title} (${pageLabel})`;
      button.style.paddingLeft = `${8 + item.depth * 12}px`;
      button.addEventListener("click", () => {
        if (item.page) togglePage(item.page, true);
      });
      li.append(button);
      bookmarkList.append(li);
    });
  } else {
    bookmarks.hidden = true;
  }
}

async function openFile(file) {
  if (!file) return;
  if (!file.name.toLowerCase().endsWith(".pdf")) {
    showToast("请选择 PDF 文件");
    return;
  }
  state.file = file;
  workspace.hidden = false;
  dropzone.classList.add("has-file");
  try {
    await renderPdf(file);
    await loadInfo(file);
  } catch (error) {
    showToast(error.message || String(error));
  }
}

async function exportSelected() {
  if (!state.file || state.selected.size === 0) return;
  const form = new FormData();
  form.append("pdf", state.file);
  form.append("pages", selectedPagesSorted().join(","));
  if (password.value) form.append("password", password.value);
  exportBtn.disabled = true;
  try {
    const response = await fetch("/api/export", { method: "POST", body: form });
    if (!response.ok) {
      const payload = await response.json().catch(() => ({}));
      throw new Error(payload.error || "导出失败");
    }
    const blob = await response.blob();
    const url = URL.createObjectURL(blob);
    const link = document.createElement("a");
    const stem = state.file.name.replace(/\.pdf$/i, "");
    link.href = url;
    link.download = `${stem}.pages.pdf`;
    document.body.append(link);
    link.click();
    link.remove();
    URL.revokeObjectURL(url);
    showToast(`已导出 ${state.selected.size} 页`);
  } catch (error) {
    showToast(error.message || String(error));
  } finally {
    updateSummary();
  }
}

dropzone.addEventListener("dragover", (event) => {
  event.preventDefault();
  dropzone.classList.add("dragover");
});
dropzone.addEventListener("dragleave", () => dropzone.classList.remove("dragover"));
dropzone.addEventListener("drop", (event) => {
  event.preventDefault();
  dropzone.classList.remove("dragover");
  openFile(event.dataTransfer.files[0]);
});
fileInput.addEventListener("change", () => openFile(fileInput.files[0]));
applySpec.addEventListener("click", () => {
  try {
    const pages = parseLocalSpec(pageSpec.value, state.pageCount);
    state.selected = new Set(pages);
    updateSummary();
  } catch (error) {
    showToast(error.message || String(error));
  }
});
selectAll.addEventListener("click", () => {
  state.selected = new Set(Array.from({ length: state.pageCount }, (_, i) => i + 1));
  updateSummary();
});
clearAll.addEventListener("click", () => {
  state.selected.clear();
  updateSummary();
});
exportBtn.addEventListener("click", exportSelected);
