/**
 * Canvas plotters for time / frequency domain overlays.
 */

function dpr(canvas) {
  const ratio = window.devicePixelRatio || 1;
  const rect = canvas.getBoundingClientRect();
  const w = Math.max(1, Math.floor(rect.width * ratio));
  const h = Math.max(1, Math.floor(rect.height * ratio));
  if (canvas.width !== w || canvas.height !== h) {
    canvas.width = w;
    canvas.height = h;
  }
  const ctx = canvas.getContext("2d");
  ctx.setTransform(ratio, 0, 0, ratio, 0, 0);
  return { ctx, width: rect.width, height: rect.height };
}

function clear(ctx, w, h, bg) {
  ctx.fillStyle = bg;
  ctx.fillRect(0, 0, w, h);
}

function drawGrid(ctx, w, h, pad, opts = {}) {
  const { xTicks = 8, yTicks = 6, color = "rgba(148, 163, 184, 0.12)" } = opts;
  const iw = w - pad.l - pad.r;
  const ih = h - pad.t - pad.b;
  ctx.strokeStyle = color;
  ctx.lineWidth = 1;
  ctx.beginPath();
  for (let i = 0; i <= xTicks; i++) {
    const x = pad.l + (iw * i) / xTicks;
    ctx.moveTo(x, pad.t);
    ctx.lineTo(x, pad.t + ih);
  }
  for (let i = 0; i <= yTicks; i++) {
    const y = pad.t + (ih * i) / yTicks;
    ctx.moveTo(pad.l, y);
    ctx.lineTo(pad.l + iw, y);
  }
  ctx.stroke();
}

function drawAxesLabels(ctx, w, h, pad, xLabel, yLabel, theme) {
  ctx.fillStyle = theme.muted;
  ctx.font = "11px \"IBM Plex Mono\", ui-monospace, monospace";
  ctx.textAlign = "center";
  ctx.fillText(xLabel, pad.l + (w - pad.l - pad.r) / 2, h - 8);
  ctx.save();
  ctx.translate(12, pad.t + (h - pad.t - pad.b) / 2);
  ctx.rotate(-Math.PI / 2);
  ctx.fillText(yLabel, 0, 0);
  ctx.restore();
}

function downsample(xs, ys, maxPoints) {
  if (xs.length <= maxPoints) return { xs, ys };
  const step = Math.ceil(xs.length / maxPoints);
  const ox = [];
  const oy = [];
  for (let i = 0; i < xs.length; i += step) {
    ox.push(xs[i]);
    oy.push(ys[i]);
  }
  return { xs: ox, ys: oy };
}

function strokePolyline(ctx, points, color, width = 1.5) {
  if (points.length < 2) return;
  ctx.strokeStyle = color;
  ctx.lineWidth = width;
  ctx.lineJoin = "round";
  ctx.beginPath();
  ctx.moveTo(points[0].x, points[0].y);
  for (let i = 1; i < points.length; i++) ctx.lineTo(points[i].x, points[i].y);
  ctx.stroke();
}

/**
 * Time-domain overlay: input (cyan) vs output (amber).
 */
export function drawTimeDomain(canvas, input, output, fs, theme) {
  const { ctx, width: w, height: h } = dpr(canvas);
  const pad = { t: 18, r: 16, b: 36, l: 48 };
  clear(ctx, w, h, theme.plotBg);
  drawGrid(ctx, w, h, pad);

  const n = Math.min(input.length, output.length);
  const t = Array.from({ length: n }, (_, i) => (i / fs) * 1000);
  let maxAbs = 1e-6;
  for (let i = 0; i < n; i++) {
    maxAbs = Math.max(maxAbs, Math.abs(input[i]), Math.abs(output[i]));
  }
  const yMin = -maxAbs * 1.1;
  const yMax = maxAbs * 1.1;
  const tMax = t[n - 1] || 1;

  const iw = w - pad.l - pad.r;
  const ih = h - pad.t - pad.b;
  const mapX = (tv) => pad.l + (tv / tMax) * iw;
  const mapY = (yv) => pad.t + ((yMax - yv) / (yMax - yMin)) * ih;

  const di = downsample(t, Array.from(input.subarray(0, n)), 2000);
  const dout = downsample(t, Array.from(output.subarray(0, n)), 2000);

  strokePolyline(
    ctx,
    di.xs.map((tv, i) => ({ x: mapX(tv), y: mapY(di.ys[i]) })),
    theme.input,
    1.4
  );
  strokePolyline(
    ctx,
    dout.xs.map((tv, i) => ({ x: mapX(tv), y: mapY(dout.ys[i]) })),
    theme.output,
    1.6
  );

  // zero line
  ctx.strokeStyle = "rgba(148, 163, 184, 0.35)";
  ctx.setLineDash([4, 4]);
  ctx.beginPath();
  ctx.moveTo(pad.l, mapY(0));
  ctx.lineTo(pad.l + iw, mapY(0));
  ctx.stroke();
  ctx.setLineDash([]);

  // tick labels
  ctx.fillStyle = theme.muted;
  ctx.font = "10px \"IBM Plex Mono\", ui-monospace, monospace";
  ctx.textAlign = "center";
  for (let i = 0; i <= 4; i++) {
    const tv = (tMax * i) / 4;
    ctx.fillText(tv.toFixed(1), mapX(tv), h - 20);
  }
  ctx.textAlign = "right";
  for (let i = 0; i <= 4; i++) {
    const yv = yMax - ((yMax - yMin) * i) / 4;
    ctx.fillText(yv.toFixed(2), pad.l - 6, mapY(yv) + 3);
  }

  drawAxesLabels(ctx, w, h, pad, "时间 (ms)", "幅度", theme);
}

/**
 * Frequency-domain magnitude (dB) overlay.
 */
export function drawFrequencyDomain(canvas, freqsIn, magsIn, freqsOut, magsOut, theme, fMax = null) {
  const { ctx, width: w, height: h } = dpr(canvas);
  const pad = { t: 18, r: 16, b: 36, l: 48 };
  clear(ctx, w, h, theme.plotBg);
  drawGrid(ctx, w, h, pad);

  const limit = fMax ?? freqsIn[freqsIn.length - 1] ?? 4000;
  let iMax = freqsIn.length - 1;
  while (iMax > 0 && freqsIn[iMax] > limit) iMax--;

  const sliceInF = freqsIn.subarray(0, iMax + 1);
  const sliceInM = magsIn.subarray(0, iMax + 1);
  const sliceOutF = freqsOut.subarray(0, Math.min(iMax + 1, freqsOut.length));
  const sliceOutM = magsOut.subarray(0, Math.min(iMax + 1, magsOut.length));

  let yMax = -Infinity;
  let yMin = Infinity;
  for (let i = 0; i < sliceInM.length; i++) {
    yMax = Math.max(yMax, sliceInM[i], sliceOutM[i] ?? -120);
    yMin = Math.min(yMin, sliceInM[i], sliceOutM[i] ?? -120);
  }
  yMax = Math.min(20, yMax + 5);
  yMin = Math.max(-100, yMin - 5);
  if (yMax - yMin < 20) {
    yMax += 10;
    yMin -= 10;
  }

  const iw = w - pad.l - pad.r;
  const ih = h - pad.t - pad.b;
  const mapX = (f) => pad.l + (f / limit) * iw;
  const mapY = (db) => pad.t + ((yMax - db) / (yMax - yMin)) * ih;

  const din = downsample(Array.from(sliceInF), Array.from(sliceInM), 1500);
  const dout = downsample(Array.from(sliceOutF), Array.from(sliceOutM), 1500);

  strokePolyline(
    ctx,
    din.xs.map((f, i) => ({ x: mapX(f), y: mapY(din.ys[i]) })),
    theme.input,
    1.4
  );
  strokePolyline(
    ctx,
    dout.xs.map((f, i) => ({ x: mapX(f), y: mapY(dout.ys[i]) })),
    theme.output,
    1.6
  );

  ctx.fillStyle = theme.muted;
  ctx.font = "10px \"IBM Plex Mono\", ui-monospace, monospace";
  ctx.textAlign = "center";
  for (let i = 0; i <= 4; i++) {
    const f = (limit * i) / 4;
    ctx.fillText(Math.round(f).toString(), mapX(f), h - 20);
  }
  ctx.textAlign = "right";
  for (let i = 0; i <= 4; i++) {
    const db = yMax - ((yMax - yMin) * i) / 4;
    ctx.fillText(db.toFixed(0), pad.l - 6, mapY(db) + 3);
  }

  drawAxesLabels(ctx, w, h, pad, "频率 (Hz)", "幅度 (dB)", theme);
}

/** Filter magnitude response. */
export function drawFilterResponse(canvas, freqs, mags, theme, cutoffMarks = []) {
  const { ctx, width: w, height: h } = dpr(canvas);
  const pad = { t: 18, r: 16, b: 36, l: 48 };
  clear(ctx, w, h, theme.plotBg);
  drawGrid(ctx, w, h, pad);

  const limit = freqs[freqs.length - 1] || 4000;
  let yMax = -Infinity;
  let yMin = Infinity;
  for (let i = 0; i < mags.length; i++) {
    yMax = Math.max(yMax, mags[i]);
    yMin = Math.min(yMin, mags[i]);
  }
  yMax = Math.min(10, yMax + 3);
  yMin = Math.max(-80, yMin - 5);

  const iw = w - pad.l - pad.r;
  const ih = h - pad.t - pad.b;
  const mapX = (f) => pad.l + (f / limit) * iw;
  const mapY = (db) => pad.t + ((yMax - db) / (yMax - yMin)) * ih;

  const d = downsample(Array.from(freqs), Array.from(mags), 1500);
  strokePolyline(
    ctx,
    d.xs.map((f, i) => ({ x: mapX(f), y: mapY(d.ys[i]) })),
    theme.accent,
    1.8
  );

  ctx.setLineDash([3, 3]);
  ctx.strokeStyle = "rgba(251, 191, 36, 0.55)";
  for (const fc of cutoffMarks) {
    if (fc <= 0 || fc >= limit) continue;
    ctx.beginPath();
    ctx.moveTo(mapX(fc), pad.t);
    ctx.lineTo(mapX(fc), pad.t + ih);
    ctx.stroke();
  }
  ctx.setLineDash([]);

  ctx.fillStyle = theme.muted;
  ctx.font = "10px \"IBM Plex Mono\", ui-monospace, monospace";
  ctx.textAlign = "center";
  for (let i = 0; i <= 4; i++) {
    const f = (limit * i) / 4;
    ctx.fillText(Math.round(f).toString(), mapX(f), h - 20);
  }
  ctx.textAlign = "right";
  for (let i = 0; i <= 4; i++) {
    const db = yMax - ((yMax - yMin) * i) / 4;
    ctx.fillText(db.toFixed(0), pad.l - 6, mapY(db) + 3);
  }

  drawAxesLabels(ctx, w, h, pad, "频率 (Hz)", "|H(f)| (dB)", theme);
}
