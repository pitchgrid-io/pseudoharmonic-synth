<script>
  export let value = 0.5;
  export let min = 0;
  export let max = 1;
  export let step = 0.001;
  export let label = '';
  export let unit = '';
  export let log = false;
  export let onChange = () => {};

  let dragging = false;
  let startY = 0;
  let startValue = 0;

  function toNorm(v) {
    if (log) return Math.log(v / min) / Math.log(max / min);
    return (v - min) / (max - min);
  }

  function fromNorm(n) {
    n = Math.max(0, Math.min(1, n));
    if (log) return min * Math.pow(max / min, n);
    return min + n * (max - min);
  }

  $: norm = toNorm(value);
  $: angle = -135 + norm * 270;
  $: displayVal = value < 10 ? value.toFixed(3) : value.toFixed(1);

  let dragStyle = null;

  function onPointerDown(e) {
    dragging = true;
    startY = e.clientY;
    startValue = norm;
    // Inject global style to suppress all hover/pointer effects during drag
    dragStyle = document.createElement('style');
    dragStyle.textContent = '*, *::before, *::after { pointer-events: none !important; cursor: ns-resize !important; user-select: none !important; -webkit-user-select: none !important; }';
    document.head.appendChild(dragStyle);
    // Window-level listeners work regardless of pointer-events CSS and window boundaries
    window.addEventListener('pointermove', onPointerMove);
    window.addEventListener('pointerup', onPointerUp);
  }

  function onPointerMove(e) {
    if (!dragging) return;
    const delta = (startY - e.clientY) / 200;
    const newNorm = Math.max(0, Math.min(1, startValue + delta));
    value = fromNorm(newNorm);
    onChange(value);
  }

  function onPointerUp() {
    dragging = false;
    window.removeEventListener('pointermove', onPointerMove);
    window.removeEventListener('pointerup', onPointerUp);
    if (dragStyle) { dragStyle.remove(); dragStyle = null; }
  }
</script>

<div class="knob-container" on:pointerdown={onPointerDown}>
  <svg viewBox="0 0 60 60" class="knob-svg">
    <!-- Track arc -->
    <circle cx="30" cy="30" r="24" fill="none" stroke="var(--knob-track)" stroke-width="3"
            stroke-dasharray="113 40" stroke-dashoffset="0"
            transform="rotate(135, 30, 30)" />
    <!-- Value arc -->
    <circle cx="30" cy="30" r="24" fill="none" stroke="var(--accent-orange)" stroke-width="3"
            stroke-dasharray="{norm * 113} {153 - norm * 113}" stroke-dashoffset="0"
            transform="rotate(135, 30, 30)" />
    <!-- Knob body -->
    <circle cx="30" cy="30" r="18" fill="var(--bg-knob)" stroke="var(--border)" stroke-width="1" />
    <!-- Indicator line -->
    <line x1="30" y1="30" x2="30" y2="15"
          stroke="var(--accent-orange)" stroke-width="2" stroke-linecap="round"
          transform="rotate({angle}, 30, 30)" />
  </svg>
  <div class="knob-label">{label}</div>
  <div class="knob-value">{displayVal}{unit}</div>
</div>

<style>
  .knob-container {
    display: flex;
    flex-direction: column;
    align-items: center;
    cursor: ns-resize;
    user-select: none;
    touch-action: none;
    width: 72px;
  }
  .knob-svg {
    width: 56px;
    height: 56px;
  }
  .knob-label {
    font-size: 10px;
    color: var(--text-secondary);
    text-transform: uppercase;
    letter-spacing: 0.5px;
    margin-top: 2px;
  }
  .knob-value {
    font-size: 10px;
    color: var(--text-primary);
    font-variant-numeric: tabular-nums;
  }
</style>
