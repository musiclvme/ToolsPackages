import {
  DEFAULT_FS,
  generateSignal,
  processSignal,
  magnitudeSpectrumDb,
  firFrequencyResponse,
  getFilterTaps,
  algoNeedsBand,
  algoNeedsOrder,
  algoNeedsPreA,
  algoNeedsMa,
  decodeWav,
  normalize,
} from "./dsp.js";
import { drawTimeDomain, drawFrequencyDomain, drawFilterResponse } from "./charts.js";

const theme = {
  plotBg: "#0c1219",
  input: "#38bdf8",
  output: "#fbbf24",
  accent: "#2dd4bf",
  muted: "#94a3b8",
};

const ALGOS = [
  { id: "fir-highpass", label: "FIR 高通", hint: "窗函数 sinc，可调截止频率与阶数" },
  { id: "fir-lowpass", label: "FIR 低通", hint: "保留低频，抑制高频噪声" },
  { id: "fir-bandpass", label: "FIR 带通", hint: "只保留一段频带（语音常用）" },
  { id: "fir-bandstop", label: "FIR 带阻 / 陷波", hint: "抑制窄带干扰" },
  { id: "iir-highpass", label: "IIR 一阶高通", hint: "轻量递归滤波，相位非线性" },
  { id: "iir-lowpass", label: "IIR 一阶低通", hint: "平滑信号，计算量极小" },
  { id: "pre-emphasis", label: "预加重 (AMR)", hint: "y[n]=x[n]−a·x[n−1]，抬高高频" },
  { id: "de-emphasis", label: "去加重", hint: "预加重的逆过程" },
  { id: "moving-average", label: "滑动平均", hint: "最简单的低通平滑" },
  { id: "window-hamming", label: "Hamming 窗", hint: "帧处理前加窗，减频谱泄漏" },
  { id: "passthrough", label: "直通（对照）", hint: "不做处理，便于对比" },
];

const SIGNALS = [
  { id: "sine", label: "正弦波" },
  { id: "two-tone", label: "双音 (低频+高频)" },
  { id: "sine+noise", label: "正弦 + 噪声" },
  { id: "square", label: "方波" },
  { id: "chirp", label: "线性扫频 Chirp" },
  { id: "noise", label: "白噪声" },
  { id: "impulse", label: "冲激" },
  { id: "speech-like", label: "类语音 (共振峰)" },
  { id: "upload", label: "上传 WAV" },
];

const els = {
  signalType: document.getElementById("signalType"),
  algo: document.getElementById("algo"),
  algoHint: document.getElementById("algoHint"),
  fs: document.getElementById("fs"),
  duration: document.getElementById("duration"),
  freq: document.getElementById("freq"),
  freq2: document.getElementById("freq2"),
  cutoff: document.getElementById("cutoff"),
  cutoff2: document.getElementById("cutoff2"),
  order: document.getElementById("order"),
  preA: document.getElementById("preA"),
  maWin: document.getElementById("maWin"),
  wavFile: document.getElementById("wavFile"),
  runBtn: document.getElementById("runBtn"),
  playIn: document.getElementById("playIn"),
  playOut: document.getElementById("playOut"),
  timeCanvas: document.getElementById("timeCanvas"),
  freqCanvas: document.getElementById("freqCanvas"),
  respCanvas: document.getElementById("respCanvas"),
  meta: document.getElementById("meta"),
  bandRow: document.getElementById("bandRow"),
  orderRow: document.getElementById("orderRow"),
  preRow: document.getElementById("preRow"),
  maRow: document.getElementById("maRow"),
  uploadRow: document.getElementById("uploadRow"),
  freq2Row: document.getElementById("freq2Row"),
};

let uploaded = null; // { samples, sampleRate }
let last = { input: null, output: null, fs: DEFAULT_FS };
let audioCtx = null;

function fillSelect(sel, items) {
  sel.innerHTML = items.map((it) => `<option value="${it.id}">${it.label}</option>`).join("");
}

function num(el, fallback) {
  const v = parseFloat(el.value);
  return Number.isFinite(v) ? v : fallback;
}

function updateParamVisibility() {
  const algo = els.algo.value;
  const sig = els.signalType.value;
  els.bandRow.hidden = !algoNeedsBand(algo);
  els.orderRow.hidden = !algoNeedsOrder(algo);
  els.preRow.hidden = !algoNeedsPreA(algo);
  els.maRow.hidden = !algoNeedsMa(algo);
  els.uploadRow.hidden = sig !== "upload";
  els.freq2Row.hidden = !(sig === "two-tone" || sig === "chirp");
  const a = ALGOS.find((x) => x.id === algo);
  els.algoHint.textContent = a ? a.hint : "";
}

function gatherParams() {
  return {
    cutoff: num(els.cutoff, 300),
    cutoff2: num(els.cutoff2, 3000),
    order: Math.max(5, Math.round(num(els.order, 63)) | 1),
    preA: num(els.preA, 0.9),
    maWin: Math.max(1, Math.round(num(els.maWin, 5))),
  };
}

function buildInput() {
  const type = els.signalType.value;
  if (type === "upload") {
    if (!uploaded) throw new Error("请先选择一个 WAV 文件");
    // Use up to 0.25 s for plotting responsiveness
    const maxN = Math.min(uploaded.samples.length, Math.floor(uploaded.sampleRate * 0.25));
    const slice = uploaded.samples.subarray(0, maxN);
    return { x: Float64Array.from(slice), fs: uploaded.sampleRate };
  }
  const fs = Math.max(1000, Math.round(num(els.fs, DEFAULT_FS)));
  const duration = Math.min(0.5, Math.max(0.01, num(els.duration, 0.05)));
  const x = generateSignal(type, {
    fs,
    duration,
    freq: num(els.freq, 440),
    freq2: num(els.freq2, 1200),
    amplitude: 0.8,
    noiseLevel: 0.2,
  });
  return { x, fs };
}

function run() {
  try {
    const { x, fs } = buildInput();
    const algo = els.algo.value;
    const params = gatherParams();
    const y = processSignal(x, fs, algo, params);

    const specIn = magnitudeSpectrumDb(x, fs);
    const specOut = magnitudeSpectrumDb(y, fs);

    drawTimeDomain(els.timeCanvas, x, y, fs, theme);
    drawFrequencyDomain(els.freqCanvas, specIn.freqs, specIn.mags, specOut.freqs, specOut.mags, theme, fs / 2);

    const taps = getFilterTaps(algo, fs, params);
    if (taps) {
      const resp = firFrequencyResponse(taps, fs, 1024);
      const marks = algoNeedsBand(algo) ? [params.cutoff, params.cutoff2] : [params.cutoff];
      drawFilterResponse(els.respCanvas, resp.freqs, resp.mags, theme, marks);
      els.respCanvas.closest(".plot-panel").hidden = false;
    } else {
      els.respCanvas.closest(".plot-panel").hidden = true;
    }

    last = { input: x, output: y, fs };
    const peak = (arr) => {
      let m = 0;
      for (let i = 0; i < arr.length; i++) m = Math.max(m, Math.abs(arr[i]));
      return m;
    };
    els.meta.textContent = `fs=${fs} Hz · N=${x.length} · 输入峰值=${peak(x).toFixed(3)} · 输出峰值=${peak(y).toFixed(3)} · 算法=${algo}`;
  } catch (err) {
    els.meta.textContent = `错误: ${err.message || err}`;
    console.error(err);
  }
}

async function playBuffer(samples, fs) {
  if (!samples || !samples.length) return;
  if (!audioCtx) audioCtx = new (window.AudioContext || window.webkitAudioContext)();
  if (audioCtx.state === "suspended") await audioCtx.resume();
  const norm = normalize(samples, 0.85);
  // Repeat short clips so they are audible
  const minLen = Math.ceil(fs * 0.35);
  const reps = Math.max(1, Math.ceil(minLen / norm.length));
  const buf = audioCtx.createBuffer(1, norm.length * reps, fs);
  const ch = buf.getChannelData(0);
  for (let r = 0; r < reps; r++) ch.set(norm, r * norm.length);
  const src = audioCtx.createBufferSource();
  src.buffer = buf;
  src.connect(audioCtx.destination);
  src.start();
}

function bind() {
  fillSelect(els.signalType, SIGNALS);
  fillSelect(els.algo, ALGOS);
  els.algo.value = "fir-highpass";
  els.signalType.value = "two-tone";
  els.fs.value = String(DEFAULT_FS);
  els.duration.value = "0.05";
  els.freq.value = "200";
  els.freq2.value = "2000";
  els.cutoff.value = "800";
  els.cutoff2.value = "2400";
  els.order.value = "63";
  els.preA.value = "0.9";
  els.maWin.value = "5";

  [
    els.signalType,
    els.algo,
    els.fs,
    els.duration,
    els.freq,
    els.freq2,
    els.cutoff,
    els.cutoff2,
    els.order,
    els.preA,
    els.maWin,
  ].forEach((el) => {
    el.addEventListener("input", () => {
      updateParamVisibility();
      run();
    });
    el.addEventListener("change", () => {
      updateParamVisibility();
      run();
    });
  });

  els.wavFile.addEventListener("change", async (e) => {
    const file = e.target.files?.[0];
    if (!file) return;
    try {
      const buf = await file.arrayBuffer();
      uploaded = decodeWav(buf);
      els.fs.value = String(uploaded.sampleRate);
      els.meta.textContent = `已加载 ${file.name} · ${uploaded.sampleRate} Hz · ${uploaded.samples.length} 采样`;
      run();
    } catch (err) {
      els.meta.textContent = `WAV 读取失败: ${err.message || err}`;
    }
  });

  els.runBtn.addEventListener("click", run);
  els.playIn.addEventListener("click", () => playBuffer(last.input, last.fs));
  els.playOut.addEventListener("click", () => playBuffer(last.output, last.fs));

  window.addEventListener("resize", () => {
    if (last.input) run();
  });

  updateParamVisibility();
  run();

  // Entrance: mark ready for CSS motion
  requestAnimationFrame(() => document.body.classList.add("ready"));
}

bind();
