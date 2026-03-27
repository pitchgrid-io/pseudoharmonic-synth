<script>
  import { onMount } from 'svelte';
  import { init, sendParam, params, activeNotes, outputLevel } from './lib/ws.js';
  import Knob from './lib/Knob.svelte';
  import ConsonancePlot from './lib/ConsonancePlot.svelte';

  onMount(() => init());

  let showSettings = false;

  function send(id) {
    return (val) => sendParam(id, val);
  }

  const stretchDev = 0.03;   // ±3% relative range
  const stretchSteps = 126;  // 127 distinct values
  function sr(prime) {
    const lo = prime * (1 - stretchDev);
    const hi = prime * (1 + stretchDev);
    return { min: lo, max: hi, step: (hi - lo) / stretchSteps };
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
          <div class="settings-divider"></div>
          <div class="settings-row">
            <label>Send Spectrum (OSC)</label>
            <input type="checkbox" checked={$params.oscSendSpectrum}
                   on:change={(e) => sendParam('oscSendSpectrum', e.target.checked ? 1 : 0)} />
          </div>
          <div class="settings-row">
            <label>Send Consonance (OSC)</label>
            <input type="checkbox" checked={$params.oscSendConsonance}
                   on:change={(e) => sendParam('oscSendConsonance', e.target.checked ? 1 : 0)} />
          </div>
          <div class="settings-divider"></div>
          <div class="settings-row">
            <label>Show Ratio Labels</label>
            <input type="checkbox" checked={$params.showRatioLabels}
                   on:change={(e) => sendParam('showRatioLabels', e.target.checked ? 1 : 0)} />
          </div>
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
        <Knob label="2nd" value={$params.stretch2} min={sr(2).min} max={sr(2).max} step={sr(2).step}
               onChange={send('stretch2')} />
        <Knob label="3rd" value={$params.stretch3} min={sr(3).min} max={sr(3).max} step={sr(3).step}
               onChange={send('stretch3')} />
        <Knob label="5th" value={$params.stretch5} min={sr(5).min} max={sr(5).max} step={sr(5).step}
               onChange={send('stretch5')} />
        <Knob label="7th" value={$params.stretch7} min={sr(7).min} max={sr(7).max} step={sr(7).step}
               onChange={send('stretch7')} />
        <Knob label="11th" value={$params.stretch11} min={sr(11).min} max={sr(11).max} step={sr(11).step}
               onChange={send('stretch11')} />
        <Knob label="13th" value={$params.stretch13} min={sr(13).min} max={sr(13).max} step={sr(13).step}
               onChange={send('stretch13')} />
        <Knob label="Warp" value={$params.warp} min={0} max={32} step={0.1}
               onChange={send('warp')} />
      </div>
    </div>

    <!-- Timbre -->
    <div class="control-group">
      <h3>Timbre</h3>
      <div class="knob-row">
        <Knob label="Strike" value={$params.strike} min={0} max={1} step={0.01}
               onChange={send('strike')} />
        <Knob label="Strike Pos" value={$params.strikePos} min={0.01} max={1} step={0.01}
               onChange={send('strikePos')} />
        <Knob label="Odd/Even" value={$params.oddEven} min={0} max={1} step={0.01}
               onChange={send('oddEven')} />
        <Knob label="Noise" value={$params.noiseMix} min={0} max={1} step={0.01}
               onChange={send('noiseMix')} />
      </div>
    </div>

    <!-- Envelope -->
    <div class="control-group">
      <h3>Envelope</h3>
      <div class="knob-row">
        <Knob label="Decay" value={$params.decay} min={0.01} max={20} step={0.01} log={true}
               onChange={send('decay')} />
        <Knob label="Sustain" value={$params.sustain} min={0} max={1} step={0.01}
               onChange={send('sustain')} />
        <Knob label="Release" value={$params.release} min={0.01} max={20} step={0.01} log={true}
               onChange={send('release')} />
        <Knob label="Onset Pitch" value={$params.detune} min={0.5} max={2} step={0.001} log={true}
               onChange={send('detune')} />
        <Knob label="Settle" value={$params.relaxTime} min={0.01} max={1} step={0.01} log={true}
               onChange={send('relaxTime')} />
        <Knob label="Volume" value={$params.volume} min={0.01} max={2} step={0.01} log={true}
               onChange={send('volume')} />
      </div>
    </div>

    <!-- Consonance -->
    <div class="control-group">
      <h3>Consonance</h3>
      <div class="knob-row">
        <Knob label="Partials" value={$params.curvePartials} min={1} max={32} step={0.1}
               onChange={send('curvePartials')} />
        <Knob label="Log Base" value={$params.logBaseline} min={0.1} max={2} step={0.01}
               onChange={send('logBaseline')} />
      </div>
    </div>
  </section>

  <!-- Output level meter -->
  <div class="level-meter">
    <div class="level-fill" style="height: {Math.min(100, $outputLevel * 100 / 0.5)}%"></div>
  </div>
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
    flex-wrap: wrap;
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
  .settings-divider {
    border-top: 1px solid var(--border);
    margin: 2px 0;
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

  .level-meter {
    position: fixed;
    right: 0;
    top: 0;
    bottom: 0;
    width: 4px;
    background: var(--bg-secondary);
  }
  .level-fill {
    position: absolute;
    bottom: 0;
    width: 100%;
    background: var(--accent-orange);
    transition: height 50ms ease-out;
  }
</style>
