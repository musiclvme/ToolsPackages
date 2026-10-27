/**
 * Node smoke test for DSP core (no browser).
 * Run: node js/smoke-test.mjs
 */
import {
  generateSignal,
  processSignal,
  magnitudeSpectrumDb,
  designFir,
  firFrequencyResponse,
  nextPow2,
} from "./dsp.js";

function assert(cond, msg) {
  if (!cond) throw new Error(msg);
}

const fs = 8000;
const x = generateSignal("two-tone", { fs, duration: 0.05, freq: 200, freq2: 2000 });
assert(x.length === 400, `expected 400 samples, got ${x.length}`);

const yHp = processSignal(x, fs, "fir-highpass", { cutoff: 800, order: 63 });
assert(yHp.length === x.length, "HPF length mismatch");

const specIn = magnitudeSpectrumDb(x, fs);
const specOut = magnitudeSpectrumDb(yHp, fs);
assert(specIn.freqs.length === specOut.mags.length, "spectrum size");

// Find bins near 200 Hz and 2000 Hz
const near = (freqs, f) => {
  let best = 0;
  let bestD = Infinity;
  for (let i = 0; i < freqs.length; i++) {
    const d = Math.abs(freqs[i] - f);
    if (d < bestD) {
      bestD = d;
      best = i;
    }
  }
  return best;
};
const iLow = near(specIn.freqs, 200);
const iHigh = near(specIn.freqs, 2000);
const attenLow = specOut.mags[iLow] - specIn.mags[iLow];
const attenHigh = specOut.mags[iHigh] - specIn.mags[iHigh];
assert(attenLow < -10, `HPF should attenuate 200 Hz, got ${attenLow.toFixed(1)} dB`);
assert(attenHigh > -6, `HPF should keep 2000 Hz, got ${attenHigh.toFixed(1)} dB`);

const yPre = processSignal(x, fs, "pre-emphasis", { preA: 0.9 });
assert(yPre.length === x.length, "pre-emphasis length");

const h = designFir("lowpass", fs, 1000, 63);
const resp = firFrequencyResponse(h, fs, 512);
assert(resp.freqs.length === nextPow2(512) / 2, "FIR response length");

const yMa = processSignal(x, fs, "moving-average", { maWin: 5 });
assert(Number.isFinite(yMa[10]), "MA output finite");

console.log("smoke-test OK");
console.log(`  HPF Δ@200Hz=${attenLow.toFixed(1)} dB, Δ@2000Hz=${attenHigh.toFixed(1)} dB`);
