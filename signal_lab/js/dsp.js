/**
 * Lightweight DSP primitives for the Signal Lab demo.
 * Sample rate defaults to 8 kHz — the AMR narrowband rate.
 */

export const DEFAULT_FS = 8000;

/** Generate common test signals. */
export function generateSignal(type, opts = {}) {
  const {
    fs = DEFAULT_FS,
    duration = 0.05,
    freq = 440,
    freq2 = 1200,
    amplitude = 0.8,
    noiseLevel = 0.15,
  } = opts;
  const n = Math.max(8, Math.floor(fs * duration));
  const x = new Float64Array(n);

  switch (type) {
    case "sine":
      for (let i = 0; i < n; i++) x[i] = amplitude * Math.sin((2 * Math.PI * freq * i) / fs);
      break;
    case "two-tone":
      for (let i = 0; i < n; i++) {
        x[i] =
          (amplitude / 2) * Math.sin((2 * Math.PI * freq * i) / fs) +
          (amplitude / 2) * Math.sin((2 * Math.PI * freq2 * i) / fs);
      }
      break;
    case "square": {
      const period = fs / Math.max(1, freq);
      for (let i = 0; i < n; i++) x[i] = amplitude * (i % period < period / 2 ? 1 : -1);
      break;
    }
    case "chirp": {
      const f0 = Math.max(20, freq);
      const f1 = Math.max(f0 + 10, freq2);
      for (let i = 0; i < n; i++) {
        const t = i / fs;
        const phase = 2 * Math.PI * (f0 * t + ((f1 - f0) * t * t) / (2 * duration));
        x[i] = amplitude * Math.sin(phase);
      }
      break;
    }
    case "noise":
      for (let i = 0; i < n; i++) x[i] = amplitude * (Math.random() * 2 - 1);
      break;
    case "impulse":
      x[0] = amplitude;
      break;
    case "speech-like": {
      // Rough voiced frame: F0 + formants (for AMR intuition)
      const f0 = 120;
      for (let i = 0; i < n; i++) {
        const t = i / fs;
        const glottal = Math.sin(2 * Math.PI * f0 * t);
        const f1 = Math.sin(2 * Math.PI * 700 * t) * 0.55;
        const f2 = Math.sin(2 * Math.PI * 1200 * t) * 0.35;
        const f3 = Math.sin(2 * Math.PI * 2500 * t) * 0.2;
        x[i] = amplitude * glottal * (0.4 + 0.6 * (f1 + f2 + f3));
      }
      break;
    }
    case "sine+noise":
      for (let i = 0; i < n; i++) {
        x[i] =
          amplitude * Math.sin((2 * Math.PI * freq * i) / fs) +
          noiseLevel * (Math.random() * 2 - 1);
      }
      break;
    default:
      for (let i = 0; i < n; i++) x[i] = amplitude * Math.sin((2 * Math.PI * freq * i) / fs);
  }
  return x;
}

/** Hamming window. */
export function hamming(n) {
  const w = new Float64Array(n);
  for (let i = 0; i < n; i++) w[i] = 0.54 - 0.46 * Math.cos((2 * Math.PI * i) / (n - 1 || 1));
  return w;
}

/** Apply window in-place copy. */
export function applyWindow(x, kind = "hamming") {
  const n = x.length;
  const y = new Float64Array(n);
  if (kind === "rectangular" || kind === "none") {
    y.set(x);
    return y;
  }
  const w = kind === "hann"
    ? Float64Array.from({ length: n }, (_, i) => 0.5 * (1 - Math.cos((2 * Math.PI * i) / (n - 1 || 1))))
    : hamming(n);
  for (let i = 0; i < n; i++) y[i] = x[i] * w[i];
  return y;
}

/**
 * Design a windowed-sinc FIR filter.
 * type: lowpass | highpass | bandpass | bandstop
 */
export function designFir(type, fs, cutoff, order = 63, cutoff2 = null) {
  const M = Math.max(3, order | 1); // odd length
  const mid = (M - 1) / 2;
  const h = new Float64Array(M);
  const w = hamming(M);

  const sinc = (x) => (Math.abs(x) < 1e-12 ? 1 : Math.sin(Math.PI * x) / (Math.PI * x));

  if (type === "lowpass" || type === "highpass") {
    const fc = Math.min(0.49, Math.max(0.001, cutoff / fs));
    for (let n = 0; n < M; n++) {
      const k = n - mid;
      h[n] = 2 * fc * sinc(2 * fc * k) * w[n];
    }
    if (type === "highpass") {
      for (let n = 0; n < M; n++) h[n] = -h[n];
      h[mid] += 1;
    }
  } else {
    const f1 = Math.min(0.48, Math.max(0.001, cutoff / fs));
    const f2 = Math.min(0.49, Math.max(f1 + 0.01, (cutoff2 ?? cutoff * 2) / fs));
    for (let n = 0; n < M; n++) {
      const k = n - mid;
      const band = 2 * f2 * sinc(2 * f2 * k) - 2 * f1 * sinc(2 * f1 * k);
      h[n] = band * w[n];
    }
    if (type === "bandstop") {
      for (let n = 0; n < M; n++) h[n] = -h[n];
      h[mid] += 1;
    }
  }

  // Unity passband gain normalization for lowpass-like designs
  let sum = 0;
  for (let i = 0; i < M; i++) sum += h[i];
  if (type === "lowpass" && Math.abs(sum) > 1e-9) {
    for (let i = 0; i < M; i++) h[i] /= sum;
  } else if (type === "highpass") {
    let alt = 0;
    for (let i = 0; i < M; i++) alt += (i % 2 === 0 ? 1 : -1) * h[i];
    if (Math.abs(alt) > 1e-9) for (let i = 0; i < M; i++) h[i] /= alt;
  }

  return h;
}

/** FIR convolution (same length as input, zero-padded edges). */
export function firFilter(x, h) {
  const n = x.length;
  const m = h.length;
  const y = new Float64Array(n);
  const mid = Math.floor((m - 1) / 2);
  for (let i = 0; i < n; i++) {
    let acc = 0;
    for (let k = 0; k < m; k++) {
      const j = i + mid - k;
      if (j >= 0 && j < n) acc += h[k] * x[j];
    }
    y[i] = acc;
  }
  return y;
}

/** Simple 1st-order IIR high/low pass (bilinear). */
export function iirFirstOrder(x, type, fs, cutoff) {
  const rc = 1 / (2 * Math.PI * Math.max(1, cutoff));
  const dt = 1 / fs;
  const alpha = dt / (rc + dt);
  const y = new Float64Array(x.length);
  if (type === "lowpass") {
    y[0] = x[0];
    for (let i = 1; i < x.length; i++) y[i] = y[i - 1] + alpha * (x[i] - y[i - 1]);
  } else {
    // highpass
    y[0] = x[0];
    for (let i = 1; i < x.length; i++) y[i] = alpha * (y[i - 1] + x[i] - x[i - 1]);
  }
  return y;
}

/**
 * Speech pre-emphasis: y[n] = x[n] - a * x[n-1]
 * Used in AMR / CELP front-ends (typical a ≈ 0.9).
 */
export function preEmphasis(x, a = 0.9) {
  const y = new Float64Array(x.length);
  y[0] = x[0];
  for (let i = 1; i < x.length; i++) y[i] = x[i] - a * x[i - 1];
  return y;
}

export function deEmphasis(x, a = 0.9) {
  const y = new Float64Array(x.length);
  y[0] = x[0];
  for (let i = 1; i < x.length; i++) y[i] = x[i] + a * y[i - 1];
  return y;
}

/** Moving-average (boxcar) lowpass. */
export function movingAverage(x, win = 5) {
  const w = Math.max(1, win | 0);
  const y = new Float64Array(x.length);
  let sum = 0;
  for (let i = 0; i < x.length; i++) {
    sum += x[i];
    if (i >= w) sum -= x[i - w];
    y[i] = sum / Math.min(w, i + 1);
  }
  return y;
}

/** Next power of two. */
export function nextPow2(n) {
  let p = 1;
  while (p < n) p <<= 1;
  return p;
}

/** In-place radix-2 Cooley–Tukey FFT on interleaved [re,im,...] or separate arrays. */
export function fft(re, im) {
  const n = re.length;
  if (n !== im.length || (n & (n - 1)) !== 0) {
    throw new Error("FFT length must be power of 2");
  }
  // bit reverse
  for (let i = 1, j = 0; i < n; i++) {
    let bit = n >> 1;
    for (; j & bit; bit >>= 1) j ^= bit;
    j ^= bit;
    if (i < j) {
      [re[i], re[j]] = [re[j], re[i]];
      [im[i], im[j]] = [im[j], im[i]];
    }
  }
  for (let len = 2; len <= n; len <<= 1) {
    const ang = (-2 * Math.PI) / len;
    const wlenRe = Math.cos(ang);
    const wlenIm = Math.sin(ang);
    for (let i = 0; i < n; i += len) {
      let wr = 1;
      let wi = 0;
      for (let j = 0; j < len / 2; j++) {
        const uRe = re[i + j];
        const uIm = im[i + j];
        const vRe = re[i + j + len / 2] * wr - im[i + j + len / 2] * wi;
        const vIm = re[i + j + len / 2] * wi + im[i + j + len / 2] * wr;
        re[i + j] = uRe + vRe;
        im[i + j] = uIm + vIm;
        re[i + j + len / 2] = uRe - vRe;
        im[i + j + len / 2] = uIm - vIm;
        const nwr = wr * wlenRe - wi * wlenIm;
        wi = wr * wlenIm + wi * wlenRe;
        wr = nwr;
      }
    }
  }
}

/** Magnitude spectrum in dB (one-sided), with optional zero-pad. */
export function magnitudeSpectrumDb(x, fs, padTo = null) {
  const n0 = x.length;
  const n = padTo ? nextPow2(Math.max(n0, padTo)) : nextPow2(n0);
  const re = new Float64Array(n);
  const im = new Float64Array(n);
  const win = hamming(n0);
  for (let i = 0; i < n0; i++) re[i] = x[i] * win[i];
  fft(re, im);
  const half = n / 2;
  const mags = new Float64Array(half);
  const freqs = new Float64Array(half);
  const scale = 1 / n0;
  for (let k = 0; k < half; k++) {
    const mag = Math.hypot(re[k], im[k]) * scale * (k === 0 ? 1 : 2);
    mags[k] = 20 * Math.log10(Math.max(mag, 1e-12));
    freqs[k] = (k * fs) / n;
  }
  return { freqs, mags, nfft: n };
}

/** Frequency response of FIR taps (dB). */
export function firFrequencyResponse(h, fs, nfft = 512) {
  const n = nextPow2(Math.max(nfft, h.length));
  const re = new Float64Array(n);
  const im = new Float64Array(n);
  re.set(h);
  fft(re, im);
  const half = n / 2;
  const mags = new Float64Array(half);
  const freqs = new Float64Array(half);
  for (let k = 0; k < half; k++) {
    mags[k] = 20 * Math.log10(Math.max(Math.hypot(re[k], im[k]), 1e-12));
    freqs[k] = (k * fs) / n;
  }
  return { freqs, mags };
}

/** Peak-normalize to [-1, 1]. */
export function normalize(x, peak = 0.95) {
  let m = 0;
  for (let i = 0; i < x.length; i++) m = Math.max(m, Math.abs(x[i]));
  const y = new Float64Array(x.length);
  if (m < 1e-12) return y;
  const g = peak / m;
  for (let i = 0; i < x.length; i++) y[i] = x[i] * g;
  return y;
}

/** Decode mono PCM WAV (16-bit or float) to Float64Array + sample rate. */
export function decodeWav(arrayBuffer) {
  const view = new DataView(arrayBuffer);
  if (view.getUint32(0, false) !== 0x52494646) throw new Error("不是有效的 WAV 文件");
  if (view.getUint32(8, false) !== 0x57415645) throw new Error("不是有效的 WAV 文件");

  let offset = 12;
  let fmt = null;
  let dataOffset = 0;
  let dataSize = 0;

  while (offset + 8 <= view.byteLength) {
    const id = view.getUint32(offset, false);
    const size = view.getUint32(offset + 4, true);
    offset += 8;
    if (id === 0x666d7420) {
      fmt = {
        audioFormat: view.getUint16(offset, true),
        numChannels: view.getUint16(offset + 2, true),
        sampleRate: view.getUint32(offset + 4, true),
        bitsPerSample: view.getUint16(offset + 14, true),
      };
    } else if (id === 0x64617461) {
      dataOffset = offset;
      dataSize = size;
    }
    offset += size + (size & 1);
  }
  if (!fmt || !dataSize) throw new Error("WAV 缺少 fmt/data 块");

  const { sampleRate, numChannels, bitsPerSample, audioFormat } = fmt;
  const samples = Math.floor(dataSize / (bitsPerSample / 8) / numChannels);
  const x = new Float64Array(samples);

  if (audioFormat === 1 && bitsPerSample === 16) {
    for (let i = 0; i < samples; i++) {
      let s = 0;
      for (let c = 0; c < numChannels; c++) {
        s += view.getInt16(dataOffset + (i * numChannels + c) * 2, true);
      }
      x[i] = s / numChannels / 32768;
    }
  } else if (audioFormat === 3 && bitsPerSample === 32) {
    for (let i = 0; i < samples; i++) {
      let s = 0;
      for (let c = 0; c < numChannels; c++) {
        s += view.getFloat32(dataOffset + (i * numChannels + c) * 4, true);
      }
      x[i] = s / numChannels;
    }
  } else {
    throw new Error(`暂不支持的 WAV 格式: format=${audioFormat}, bits=${bitsPerSample}`);
  }
  return { samples: x, sampleRate };
}

/** Run selected algorithm pipeline. */
export function processSignal(x, fs, algo, params) {
  const {
    cutoff = 300,
    cutoff2 = 3000,
    order = 63,
    preA = 0.9,
    maWin = 5,
  } = params;

  switch (algo) {
    case "fir-highpass":
      return firFilter(x, designFir("highpass", fs, cutoff, order));
    case "fir-lowpass":
      return firFilter(x, designFir("lowpass", fs, cutoff, order));
    case "fir-bandpass":
      return firFilter(x, designFir("bandpass", fs, cutoff, order, cutoff2));
    case "fir-bandstop":
      return firFilter(x, designFir("bandstop", fs, cutoff, order, cutoff2));
    case "iir-highpass":
      return iirFirstOrder(x, "highpass", fs, cutoff);
    case "iir-lowpass":
      return iirFirstOrder(x, "lowpass", fs, cutoff);
    case "pre-emphasis":
      return preEmphasis(x, preA);
    case "de-emphasis":
      return deEmphasis(x, preA);
    case "moving-average":
      return movingAverage(x, maWin);
    case "window-hamming":
      return applyWindow(x, "hamming");
    case "passthrough":
      return Float64Array.from(x);
    default:
      return firFilter(x, designFir("highpass", fs, cutoff, order));
  }
}

export function algoNeedsBand(algo) {
  return algo === "fir-bandpass" || algo === "fir-bandstop";
}

export function algoNeedsOrder(algo) {
  return algo.startsWith("fir-");
}

export function algoNeedsPreA(algo) {
  return algo === "pre-emphasis" || algo === "de-emphasis";
}

export function algoNeedsMa(algo) {
  return algo === "moving-average";
}

export function getFilterTaps(algo, fs, params) {
  if (!algo.startsWith("fir-")) return null;
  const type = algo.replace("fir-", "");
  return designFir(type, fs, params.cutoff, params.order, params.cutoff2);
}
