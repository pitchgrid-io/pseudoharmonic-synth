<script>
  import { params, sendParam } from './ws.js';
  import Knob from './Knob.svelte';
  const macros = [1, 2, 3, 4, 5, 6, 7, 8];
  function send(id) { return (v) => sendParam(id, v); }
</script>

<div class="macro-panel">
  <h3>Macros</h3>
  <div class="knob-row">
    {#each macros as i}
      <Knob label={"M" + i} value={$params["macro" + i]} min={0} max={1} step={0.001}
            defaultValue={0} onChange={send("macro" + i)} />
    {/each}
  </div>
  <p class="hint">Macros drive assigned parameters via the mod matrix. Also controllable by host
     automation, MIDI CC 20–27, or OSC.</p>
</div>

<style>
  .macro-panel { padding: 12px 16px; }
  h3 { margin: 0 0 10px; font-size: 12px; letter-spacing: 0.08em; text-transform: uppercase; color: #aab; }
  .knob-row { display: flex; flex-wrap: wrap; gap: 14px; }
  .hint { margin-top: 12px; font-size: 11px; color: #778; max-width: 520px; }
</style>
