<script>
  import { params, sendParam } from './ws.js';
  import Knob from './Knob.svelte';
  function send(id) { return (v) => sendParam(id, v); }
  const filterTypes = ['Off', 'Low Pass', 'High Pass', 'Band Pass', 'Notch'];
</script>

<div class="fx-panel">
  <div class="fx-group">
    <h3>Filter</h3>
    <div class="row">
      <label class="sel">
        <span>Type</span>
        <select value={$params.filterType} on:change={(e) => sendParam('filterType', +e.target.value)}>
          {#each filterTypes as t, i}<option value={i}>{t}</option>{/each}
        </select>
      </label>
      <Knob label="Cutoff" value={$params.filterCutoff} min={20} max={20000} step={1} log={true}
            defaultValue={12000} unit="Hz" onChange={send('filterCutoff')} />
      <Knob label="Reso" value={$params.filterReso} min={0} max={1} step={0.001}
            defaultValue={0.1} onChange={send('filterReso')} />
    </div>
  </div>

  <div class="fx-group">
    <h3>Delay</h3>
    <div class="row">
      <Knob label="Time" value={$params.delayTime} min={0.01} max={2} step={0.001} log={true}
            defaultValue={0.3} unit="s" onChange={send('delayTime')} />
      <Knob label="Feedback" value={$params.delayFeedback} min={0} max={0.95} step={0.001}
            defaultValue={0.3} onChange={send('delayFeedback')} />
      <Knob label="Mix" value={$params.delayMix} min={0} max={1} step={0.001}
            defaultValue={0} onChange={send('delayMix')} />
    </div>
  </div>

  <div class="fx-group">
    <h3>Reverb</h3>
    <div class="row">
      <Knob label="Amount" value={$params.reverbAmount} min={0} max={1} step={0.001}
            defaultValue={0} onChange={send('reverbAmount')} />
      <Knob label="Size" value={$params.reverbSize} min={0} max={1} step={0.001}
            defaultValue={0.5} onChange={send('reverbSize')} />
    </div>
  </div>
</div>

<style>
  .fx-panel { display: flex; flex-wrap: wrap; gap: 24px; padding: 12px 16px; }
  .fx-group h3 { margin: 0 0 10px; font-size: 12px; letter-spacing: 0.08em; text-transform: uppercase; color: #aab; }
  .row { display: flex; align-items: flex-end; gap: 16px; }
  .sel { display: flex; flex-direction: column; gap: 4px; font-size: 11px; color: #aab; }
  select { background: #1b1d26; color: #dde; border: 1px solid #333; border-radius: 6px; padding: 5px 8px; }
</style>
