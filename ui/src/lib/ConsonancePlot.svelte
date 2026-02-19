<script>
  import { curveData, intervals } from './ws.js';

  let canvas;
  let ctx;
  let w = 800, h = 300;

  $: if (canvas && $curveData) draw($curveData, $intervals);

  function draw(data, ivls) {
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
    for (let c = 0; c <= 1200; c += 100) {
      const x = (c / maxCents) * w;
      ctx.beginPath();
      ctx.moveTo(x, 0);
      ctx.lineTo(x, h);
      ctx.stroke();
    }

    // Label some intervals
    const labels = [
      [0, 'P1'], [100, 'm1'], [200, 'M1'], [300, 'm2'], [400, 'M2'],
      [500, 'm3'], [600, 'M3'], [700, 'P4'], [800, 'm5'], [900, 'M5'],
      [1000, 'm6'], [1100, 'M6'], [1200, 'P7']
    ];
    ctx.fillStyle = '#555570';
    ctx.font = '9px Inter, sans-serif';
    ctx.textAlign = 'center';
    for (const [c, lbl] of labels) {
      const x = (c / maxCents) * w;
      ctx.fillText(lbl, x, h - 4);
    }

    // Find max PL for normalization
    const maxPL = Math.max(...data.pl, 0.001);

    // Draw PL curve (subtle)
    drawCurve(data.pl, n, maxCents, maxPL, '#333355', 1);

    // Draw hull (subtle dashed)
    if (data.hull) {
      ctx.setLineDash([4, 4]);
      drawCurve(data.hull, n, maxCents, maxPL, '#444466', 0.8);
      ctx.setLineDash([]);
    }

    // Draw consonance curve (main feature)
    if (data.consonance) {
      drawCurveAbsolute(data.consonance, n, maxCents, 1.0, '#FFAB00', 2);
    }

    // Draw interval lines
    if (ivls && ivls.length > 0) {
      for (const iv of ivls) {
        const cents = iv.cents || iv;
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
      const y = h - 20 - (arr[i] / maxVal) * (h - 30);
      if (i === 0) ctx.moveTo(x, y);
      else ctx.lineTo(x, y);
    }
    ctx.stroke();
  }

  function drawCurveAbsolute(arr, n, maxCents, maxVal, color, lineWidth) {
    if (!arr || n === 0) return;
    ctx.strokeStyle = color;
    ctx.lineWidth = lineWidth;
    ctx.beginPath();
    for (let i = 0; i < n; i++) {
      const x = (i / n) * w;
      const y = h - 20 - (arr[i] / maxVal) * (h - 30);
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
        if ($curveData) draw($curveData, $intervals);
      }
    });
    ro.observe(el);
    return { destroy: () => ro.disconnect() };
  }
</script>

<div class="plot-container" use:onResize>
  <canvas bind:this={canvas} style="width:100%;height:100%"></canvas>
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
