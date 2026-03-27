<script>
  import { curveData, intervals, scaleDegrees, params } from './ws.js';

  let canvas;
  let ctx;
  let w = 800, h = 300;
  let mouseX = -1, mouseY = -1;

  $: if (canvas && $curveData) draw($curveData, $intervals, $scaleDegrees);

  function handleMouseMove(e) {
    const rect = canvas.getBoundingClientRect();
    mouseX = e.clientX - rect.left;
    mouseY = e.clientY - rect.top;
    if ($curveData) draw($curveData, $intervals, $scaleDegrees);
  }

  function handleMouseLeave() {
    mouseX = -1;
    mouseY = -1;
    if ($curveData) draw($curveData, $intervals, $scaleDegrees);
  }

  function draw(data, ivls, degs) {
    if (!canvas) return;
    ctx = canvas.getContext('2d');
    const dpr = window.devicePixelRatio || 1;
    canvas.width = w * dpr;
    canvas.height = h * dpr;
    ctx.scale(dpr, dpr);

    // Clear
    ctx.fillStyle = '#0a0a0f';
    ctx.fillRect(0, 0, w, h);

    if (!data || !data.pl) return;

    const n = data.resolution || data.pl.length;
    const maxCents = data.maxCents || 1200;

    // Draw grid
    ctx.strokeStyle = '#1a1a2a';
    ctx.lineWidth = 0.5;
    for (let c = 0; c <= maxCents; c += 100) {
      const x = (c / maxCents) * w;
      ctx.beginPath();
      ctx.moveTo(x, 0);
      ctx.lineTo(x, h);
      ctx.stroke();
    }

    // Label x-axis in cents
    ctx.fillStyle = '#8888aa';
    ctx.font = '9px Inter, sans-serif';
    ctx.textAlign = 'center';
    for (let c = 0; c <= maxCents; c += 200) {
      const x = (c / maxCents) * w;
      ctx.fillText(c + 'ct', x, h - 4);
    }

    // Find max for normalization (hull >= PL, so use hull if available)
    const maxPL = Math.max(...(data.hull || data.pl), 0.001);

    // Draw PL curve
    drawCurve(data.pl, n, maxCents, maxPL, '#6666aa', 1);

    // Draw hull (flat-topped PL) as dashed line
    if (data.hull) {
      drawDashedCurve(data.hull, n, maxCents, maxPL, '#cc4444', 1);
    }

    // Draw consonance curve
    if (data.consonance) {
      drawFilledCurve(data.consonance, n, maxCents, 1.0, '#FFAB00', 1.5);
    }

    // Draw peak ratio labels
    const peaks = data.peakLabels;
    if (peaks && peaks.length > 0) {
      const showAll = $params.showRatioLabels;
      const baseline = h - 20;
      const curveH = h - 30;
      let lastDrawnX = -Infinity;

      for (const peak of peaks) {
        const px = (peak.cents / maxCents) * w;
        const py = baseline - (peak.consonance / 1.0) * curveH;
        const label = peak.labels.join(', ');

        // Check hover proximity (10px radius)
        const dx = mouseX - px;
        const dy = mouseY - py;
        const dist = Math.sqrt(dx * dx + dy * dy);
        const isHovered = (mouseX >= 0 && dist < 10);

        if (isHovered || showAll) {
          ctx.fillStyle = '#FFAB00';
          ctx.font = 'bold 10px Inter, sans-serif';
          ctx.textAlign = 'center';
          ctx.fillText(label, px, py - 8);

          // Dot at peak tip
          ctx.beginPath();
          ctx.arc(px, py, 2, 0, Math.PI * 2);
          ctx.fill();

          if (showAll) lastDrawnX = px;
        }
      }
    }

    // Draw scale degree lines and tuning nodes (from OSC tuning)
    if (degs && degs.length > 0) {
      const plotTop = 20;
      const plotBot = h - 15;
      const plotH = plotBot - plotTop;

      // Dashed vertical lines and labels
      for (const deg of degs) {
        const x = (deg.cents / maxCents) * w;

        ctx.strokeStyle = 'rgba(200,200,210,0.6)';
        ctx.lineWidth = 1;
        ctx.setLineDash([4, 4]);
        ctx.beginPath();
        ctx.moveTo(x, plotTop);
        ctx.lineTo(x, plotBot);
        ctx.stroke();
        ctx.setLineDash([]);

        // Label above
        ctx.fillStyle = 'rgba(220,220,230,0.9)';
        ctx.font = '11px Inter, sans-serif';
        ctx.textAlign = 'center';
        ctx.fillText(deg.label, x, 14);
      }

      // Strip y values are already normalized to [0, 1) by squeezed_t in scalatrix
      const tyToY = (ty) => plotBot - ty * plotH;

      // Connect consecutive in-scale nodes with solid lines (skip non-scale nodes)
      ctx.strokeStyle = 'rgba(200,200,210,0.7)';
      ctx.lineWidth = 2;
      ctx.beginPath();
      let first = true;
      for (const deg of degs) {
        if (!deg.inScale) continue;
        const nx = (deg.cents / maxCents) * w;
        const ny = tyToY(deg.ty);
        if (first) { ctx.moveTo(nx, ny); first = false; }
        else ctx.lineTo(nx, ny);
      }
      ctx.stroke();

      // Draw node circles (all nodes, in-scale slightly larger)
      for (const deg of degs) {
        const nx = (deg.cents / maxCents) * w;
        const ny = tyToY(deg.ty);
        ctx.fillStyle = 'rgba(220,220,230,0.9)';
        ctx.beginPath();
        ctx.arc(nx, ny, 3, 0, Math.PI * 2);
        ctx.fill();
      }
    }

    // Draw interval lines
    if (ivls && ivls.length > 0) {
      for (const iv of ivls) {
        const cents = iv.cents || iv;
        if (cents < 0 || cents > maxCents) continue;
        const cons = iv.consonance || 0;
        const x = (cents / maxCents) * w;

        // Vertical line colored by consonance
        const r = Math.round(255 * (1 - cons));
        const g = Math.round(200 * cons);
        const b = Math.round(100 + 155 * cons);
        ctx.strokeStyle = `rgb(${r},${g},${b})`;
        ctx.lineWidth = 2;
        ctx.beginPath();
        ctx.moveTo(x, 0);
        ctx.lineTo(x, h - 15);
        ctx.stroke();

        // Consonance value at top
        ctx.fillStyle = `rgb(${r},${g},${b})`;
        ctx.font = 'bold 11px Inter, sans-serif';
        ctx.textAlign = 'center';
        ctx.fillText(cons.toFixed(2), x, 14);
      }
    }
  }

  function drawCurve(arr, n, maxCents, maxVal, color, lineWidth) {
    if (!arr || n === 0) return;
    ctx.strokeStyle = color;
    ctx.lineWidth = lineWidth;
    ctx.beginPath();
    for (let i = 0; i < n; i++) {
      const x = (i / n) * w;
      const v = Math.max(0, Math.min(arr[i], maxVal));
      const y = h - 20 - (v / maxVal) * (h - 30);
      if (i === 0) ctx.moveTo(x, y);
      else ctx.lineTo(x, y);
    }
    ctx.stroke();
  }

  function drawDashedCurve(arr, n, maxCents, maxVal, color, lineWidth) {
    if (!arr || n === 0) return;
    ctx.strokeStyle = color;
    ctx.lineWidth = lineWidth;
    ctx.setLineDash([4, 4]);
    ctx.beginPath();
    for (let i = 0; i < n; i++) {
      const x = (i / n) * w;
      const v = Math.max(0, Math.min(arr[i], maxVal));
      const y = h - 20 - (v / maxVal) * (h - 30);
      if (i === 0) ctx.moveTo(x, y);
      else ctx.lineTo(x, y);
    }
    ctx.stroke();
    ctx.setLineDash([]);
  }

  function drawFilledCurve(arr, n, maxCents, maxVal, color, lineWidth) {
    if (!arr || n === 0) return;
    const baseline = h - 20;
    ctx.fillStyle = color + '30';
    ctx.beginPath();
    ctx.moveTo(0, baseline);
    for (let i = 0; i < n; i++) {
      const x = (i / n) * w;
      const y = baseline - (arr[i] / maxVal) * (h - 30);
      ctx.lineTo(x, y);
    }
    ctx.lineTo(((n - 1) / n) * w, baseline);
    ctx.closePath();
    ctx.fill();

    ctx.strokeStyle = color;
    ctx.lineWidth = lineWidth;
    ctx.beginPath();
    for (let i = 0; i < n; i++) {
      const x = (i / n) * w;
      const y = baseline - (arr[i] / maxVal) * (h - 30);
      if (i === 0) ctx.moveTo(x, y);
      else ctx.lineTo(x, y);
    }
    ctx.stroke();
  }

  function onResize(el) {
    const ro = new ResizeObserver(entries => {
      for (const entry of entries) {
        w = entry.contentRect.width;
        h = entry.contentRect.height;
        if ($curveData) draw($curveData, $intervals, $scaleDegrees);
      }
    });
    ro.observe(el);
    return { destroy: () => ro.disconnect() };
  }
</script>

<div class="plot-container" use:onResize>
  <canvas bind:this={canvas} style="width:100%;height:100%;cursor:crosshair"
          on:mousemove={handleMouseMove} on:mouseleave={handleMouseLeave}></canvas>
</div>

<style>
  .plot-container {
    width: 100%;
    height: 100%;
    min-height: 200px;
    background: var(--bg-primary);
    border: 1px solid var(--border);
    border-radius: 8px;
    overflow: hidden;
  }
</style>
