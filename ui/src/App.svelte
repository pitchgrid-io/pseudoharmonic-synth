<script>
  import { onMount } from 'svelte';
  import { init, sendParam, params, activeNotes } from './lib/ws.js';
  import Knob from './lib/Knob.svelte';
  import ConsonancePlot from './lib/ConsonancePlot.svelte';

  onMount(() => init());

  let showSettings = false;

  function send(id) {
    return (val) => sendParam(id, val);
  }
</script>

<div class="synth-ui">
  <!-- Header -->
  <header>
    <div class="logo">
      <span class="logo-pseudo">Pseudo</span><span class="logo-harmonic">Harmonic</span>
    </div>
    <div class="active-notes">
      {#each $activeNotes as note}
        <span class="note-badge">
          {note.note} ({Math.round(note.freq)}Hz)
        </span>
      {/each}
    </div>
    <div class="settings-wrapper">
      <button class="settings-btn" on:click={() => showSettings = !showSettings}>
        <svg width="16" height="16" viewBox="0 0 16 16" fill="currentColor">
          <path d="M6.5.5a.5.5 0 0 0-.5.5v1.07a5.5 5.5 0 0 0-1.56.64L3.58 1.85a.5.5 0 0 0-.7 0l-.7.7a.5.5 0 0 0 0 .71l.86.86A5.5 5.5 0 0 0 2.4 5.7H1.3a.5.5 0 0 0-.5.5v1a.5.5 0 0 0 .5.5h1.1a5.5 5.5 0 0 0 .64 1.56l-.86.86a.5.5 0 0 0 0 .7l.7.71a.5.5 0 0 0 .71 0l.86-.86a5.5 5.5 0 0 0 1.56.64v1.1a.5.5 0 0 0 .5.5h1a.5.5 0 0 0 .5-.5v-1.1a5.5 5.5 0 0 0 1.56-.64l.86.86a.5.5 0 0 0 .7 0l.71-.7a.5.5 0 0 0 0-.71l-.86-.86a5.5 5.5 0 0 0 .64-1.56h1.1a.5.5 0 0 0 .5-.5v-1a.5.5 0 0 0-.5-.5h-1.1a5.5 5.5 0 0 0-.64-1.56l.86-.86a.5.5 0 0 0 0-.7l-.7-.71a.5.5 0 0 0-.71 0l-.86.86A5.5 5.5 0 0 0 8.5 2.07V1a.5.5 0 0 0-.5-.5h-1ZM7 5a2 2 0 1 1 0 4 2 2 0 0 1 0-4Z"/>
        </svg>
      </button>
      {#if showSettings}
        <div class="settings-panel">
          <div class="settings-row">
            <label>MPE</label>
            <input type="checkbox" checked={$params.mpeEnabled}
                   on:change={(e) => sendParam('mpeEnabled', e.target.checked ? 1 : 0)} />
          </div>
          {#if !$params.mpeEnabled}
            <div class="settings-row">
              <label>Pitch Bend Range</label>
              <select value={$params.pitchBendRange}
                      on:change={(e) => sendParam('pitchBendRange', Number(e.target.value))}>
                {#each [1,2,3,4,5,7,12,24,48] as v}
                  <option value={v} selected={$params.pitchBendRange === v}>{v} st</option>
                {/each}
              </select>
            </div>
          {:else}
            <div class="settings-row">
              <label>Master Bend</label>
              <select value={$params.mpeMasterBendRange}
                      on:change={(e) => sendParam('mpeMasterBendRange', Number(e.target.value))}>
                {#each [1,2,3,4,5,7,12,24] as v}
                  <option value={v} selected={$params.mpeMasterBendRange === v}>{v} st</option>
                {/each}
              </select>
            </div>
            <div class="settings-row">
              <label>Per-Note Bend</label>
              <select value={$params.mpePerNoteBendRange}
                      on:change={(e) => sendParam('mpePerNoteBendRange', Number(e.target.value))}>
                {#each [12,24,48,96] as v}
                  <option value={v} selected={$params.mpePerNoteBendRange === v}>{v} st</option>
                {/each}
              </select>
            </div>
          {/if}
        </div>
      {/if}
    </div>
  </header>

  <!-- Visualization -->
  <section class="viz-section">
    <ConsonancePlot />
  </section>

  <!-- Controls -->
  <section class="controls">
    <!-- Spectrum stretch (the core feature) -->
    <div class="control-group">
      <h3>Spectrum</h3>
      <div class="knob-row">
        <Knob label="2nd" value={$params.stretch2} min={1.9} max={2.1} step={0.001}
               onChange={send('stretch2')} />
        <Knob label="3rd" value={$params.stretch3} min={2.9} max={3.1} step={0.001}
               onChange={send('stretch3')} />
        <Knob label="5th" value={$params.stretch5} min={4.9} max={5.1} step={0.001}
               onChange={send('stretch5')} />
        <Knob label="7th" value={$params.stretch7} min={6.9} max={7.1} step={0.001}
               onChange={send('stretch7')} />
      </div>
    </div>

    <!-- Timbre -->
    <div class="control-group">
      <h3>Timbre</h3>
      <div class="knob-row">
        <Knob label="Strike Pos" value={$params.strikePos} min={0.01} max={1} step={0.01}
               onChange={send('strikePos')} />
        <Knob label="Odd/Even" value={$params.oddEven} min={0} max={1} step={0.01}
               onChange={send('oddEven')} />
        <Knob label="Noise" value={$params.noiseMix} min={0} max={1} step={0.01}
               onChange={send('noiseMix')} />
        <Knob label="Volume" value={$params.volume} min={0.001} max={0.1} step={0.001} log={true}
               onChange={send('volume')} />
      </div>
    </div>

    <!-- Envelope -->
    <div class="control-group">
      <h3>Envelope</h3>
      <div class="knob-row">
        <Knob label="Decay" value={$params.decay} min={0.01} max={20} step={0.01} log={true}
               onChange={send('decay')} />
        <Knob label="Release" value={$params.release} min={0.01} max={20} step={0.01} log={true}
               onChange={send('release')} />
        <Knob label="Detune" value={$params.detune} min={0.5} max={2} step={0.001} log={true}
               onChange={send('detune')} />
        <Knob label="Relax" value={$params.relaxTime} min={0.01} max={1} step={0.01} log={true}
               onChange={send('relaxTime')} />
      </div>
    </div>
  </section>
</div>

<style>
  .synth-ui {
    display: flex;
    flex-direction: column;
    height: 100vh;
    background: var(--bg-primary);
    padding: 12px;
    gap: 8px;
  }

  header {
    display: flex;
    align-items: center;
    gap: 16px;
    padding: 8px 16px;
    background: var(--bg-secondary);
    border-radius: 8px;
    border: 1px solid var(--border);
  }

  .logo {
    font-size: 18px;
    font-weight: 700;
    letter-spacing: -0.5px;
  }
  .logo-pseudo { color: var(--text-secondary); }
  .logo-harmonic { color: var(--accent-orange); }

  .active-notes {
    display: flex;
    gap: 4px;
    margin-left: auto;
  }
  .note-badge {
    background: var(--accent-blue);
    color: white;
    font-size: 10px;
    padding: 2px 6px;
    border-radius: 4px;
    font-weight: 600;
  }

  .viz-section {
    flex: 1;
    min-height: 0;
  }

  .controls {
    display: flex;
    gap: 12px;
    padding: 12px;
    background: var(--bg-secondary);
    border-radius: 8px;
    border: 1px solid var(--border);
  }

  .control-group {
    flex: 1;
  }

  .control-group h3 {
    font-size: 10px;
    text-transform: uppercase;
    letter-spacing: 1px;
    color: var(--text-secondary);
    margin-bottom: 8px;
    padding-left: 4px;
  }

  .knob-row {
    display: flex;
    gap: 4px;
    justify-content: center;
  }

  .settings-wrapper {
    position: relative;
  }
  .settings-btn {
    background: none;
    border: 1px solid var(--border);
    border-radius: 4px;
    color: var(--text-secondary);
    cursor: pointer;
    padding: 4px 6px;
    display: flex;
    align-items: center;
  }
  .settings-btn:hover {
    color: var(--text-primary);
    border-color: var(--text-secondary);
  }
  .settings-panel {
    position: absolute;
    top: 100%;
    right: 0;
    margin-top: 6px;
    background: var(--bg-panel);
    border: 1px solid var(--border);
    border-radius: 6px;
    padding: 10px 14px;
    z-index: 100;
    min-width: 200px;
    display: flex;
    flex-direction: column;
    gap: 8px;
  }
  .settings-row {
    display: flex;
    align-items: center;
    justify-content: space-between;
    gap: 12px;
  }
  .settings-row label {
    font-size: 11px;
    color: var(--text-secondary);
    white-space: nowrap;
  }
  .settings-row select {
    background: var(--bg-secondary);
    color: var(--text-primary);
    border: 1px solid var(--border);
    border-radius: 4px;
    padding: 3px 6px;
    font-size: 11px;
    cursor: pointer;
  }
</style>
