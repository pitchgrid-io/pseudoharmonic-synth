<script>
  import { onMount } from 'svelte';
  import { init, sendParam, params, activeNotes, outputLevel, followTuningInfo } from './lib/ws.js';
  import Knob from './lib/Knob.svelte';
  import ConsonancePlot from './lib/ConsonancePlot.svelte';
  import MacroPanel from './lib/MacroPanel.svelte';
  import ModMatrix from './lib/ModMatrix.svelte';
  import FxPanel from './lib/FxPanel.svelte';
  import PresetPanel from './lib/PresetPanel.svelte';
  import logoSrc from '../../assets/logo_ph.svg';

  onMount(() => init());

  let showSettings = false;
  let showModeMenu = false;
  let view = 'play';          // 'play' | 'mod' | 'fx' | 'presets'
  const tabs = [['play', 'Play'], ['mod', 'Modulation'], ['fx', 'Filter / FX'], ['presets', 'Presets']];

  function send(id) {
    return (val) => sendParam(id, val);
  }

  function selectMode(mode) {
    if (mode === 'MIDI') {
      sendParam('mpeEnabled', 0);
      sendParam('mtsOverride', 1);
    } else if (mode === 'MPE') {
      sendParam('mpeEnabled', 1);
      sendParam('mtsOverride', 1);
    } else if (mode === 'MTS') {
      sendParam('mtsOverride', 0);
    }
    // Keep the panel open so the user can see the mode-specific settings
    // flip (bend range / master bend / per-note bend) after the mode change.
  }

  function handleDocClick(e) {
    if (!e.target.closest('.mode-wrapper')) showModeMenu = false;
    if (!e.target.closest('.settings-wrapper')) showSettings = false;
  }

  const stretchDev = 0.03;   // ±3% relative range
  const stretchSteps = 126;  // 127 distinct values
  function sr(prime) {
    const lo = prime * (1 - stretchDev);
    const hi = prime * (1 + stretchDev);
    return { min: lo, max: hi, step: (hi - lo) / stretchSteps };
  }
</script>

<svelte:window on:click={handleDocClick} />

<div class="synth-ui">
  <!-- Header -->
  <header>
    <div class="logo">
      <img src={logoSrc} alt="PseudoHarmonic" class="logo-img" />
    </div>
    <div class="mode-wrapper">
      <button
        class="mode-btn"
        class:mode-midi={$params.tuningMode === 'MIDI'}
        class:mode-mpe={$params.tuningMode === 'MPE'}
        class:mode-mts={$params.tuningMode === 'MTS'}
        aria-haspopup="menu"
        aria-expanded={showModeMenu}
        title="MIDI mode"
        on:click={() => showModeMenu = !showModeMenu}
      >
        <span class="mode-label">{$params.tuningMode}</span>
        {#if $params.tuningMode === 'MTS' && $params.mtsScaleName}
          <span class="mts-scale">— {$params.mtsScaleName}</span>
        {/if}
        <svg class="mode-caret" width="8" height="8" viewBox="0 0 10 10" fill="currentColor">
          <path d="M2 4l3 3 3-3z"/>
        </svg>
      </button>
      {#if showModeMenu}
        <div class="mode-panel" role="menu" on:click|stopPropagation>
          <button
            class="mode-item"
            class:checked={$params.tuningMode === 'MPE'}
            role="menuitemradio"
            aria-checked={$params.tuningMode === 'MPE'}
            on:click={() => selectMode('MPE')}
          >
            <span class="mode-check">{$params.tuningMode === 'MPE' ? '✓' : ''}</span>
            <span>MPE</span>
          </button>
          {#if $params.mtsMasterAvailable}
            <button
              class="mode-item"
              class:checked={$params.tuningMode === 'MTS'}
              role="menuitemradio"
              aria-checked={$params.tuningMode === 'MTS'}
              on:click={() => selectMode('MTS')}
            >
              <span class="mode-check">{$params.tuningMode === 'MTS' ? '✓' : ''}</span>
              <span>MTS{#if $params.mtsScaleName} — {$params.mtsScaleName}{/if}</span>
            </button>
          {/if}
          <button
            class="mode-item"
            class:checked={$params.tuningMode === 'MIDI'}
            role="menuitemradio"
            aria-checked={$params.tuningMode === 'MIDI'}
            on:click={() => selectMode('MIDI')}
          >
            <span class="mode-check">{$params.tuningMode === 'MIDI' ? '✓' : ''}</span>
            <span>MIDI</span>
          </button>
          {#if $params.tuningMode === 'MIDI'}
            <div class="mode-divider"></div>
            <div class="mode-setting">
              <span class="mode-setting-label">Pitch Bend Range</span>
              <select
                value={$params.pitchBendRange}
                on:change={(e) => sendParam('pitchBendRange', Number(e.target.value))}
              >
                {#each [1,2,3,4,5,7,12,24,48] as v}
                  <option value={v} selected={$params.pitchBendRange === v}>{v} st</option>
                {/each}
              </select>
            </div>
          {:else if $params.tuningMode === 'MPE'}
            <div class="mode-divider"></div>
            <div class="mode-setting">
              <span class="mode-setting-label">Master Bend</span>
              <select
                value={$params.mpeMasterBendRange}
                on:change={(e) => sendParam('mpeMasterBendRange', Number(e.target.value))}
              >
                {#each [1,2,3,4,5,7,12,24] as v}
                  <option value={v} selected={$params.mpeMasterBendRange === v}>{v} st</option>
                {/each}
              </select>
            </div>
            <div class="mode-setting">
              <span class="mode-setting-label">Per-Note Bend</span>
              <select
                value={$params.mpePerNoteBendRange}
                on:change={(e) => sendParam('mpePerNoteBendRange', Number(e.target.value))}
              >
                {#each [12,24,48,96] as v}
                  <option value={v} selected={$params.mpePerNoteBendRange === v}>{v} st</option>
                {/each}
              </select>
            </div>
          {/if}
        </div>
      {/if}
    </div>
    <div class="active-notes">
      {#each $activeNotes as note}
        <span class="note-badge">
          {note.note} ({Math.round(note.freq)}Hz)
        </span>
      {/each}
    </div>
    {#if $params.oscConnected}
    <button
      class="follow-tuning-btn"
      class:active={$params.followTuning}
      on:click={() => sendParam('followTuning', $params.followTuning ? 0 : 1)}
      title="Follow Tuning"
    >
      <svg width="14" height="14" viewBox="0 0 16 16" fill="currentColor">
        <path d="M4.5 1v5.268A1.5 1.5 0 0 0 4 7.5v1A1.5 1.5 0 0 0 5.5 10h.028a3.5 3.5 0 0 0 4.944 0H10.5A1.5 1.5 0 0 0 12 8.5v-1a1.5 1.5 0 0 0-.5-1.118V1h-2v5h-3V1h-2zm1 7.5a.5.5 0 0 1 .5-.5h4a.5.5 0 0 1 .5.5v1a.5.5 0 0 1-.5.5H6a.5.5 0 0 1-.5-.5v-1zM8 13a2.5 2.5 0 0 1-2.45-2h4.9A2.5 2.5 0 0 1 8 13z"/>
      </svg>
      <span class="follow-label">Follow Tuning</span>
    </button>
    {/if}
    <div class="settings-wrapper">
      <button class="settings-btn" on:click={() => showSettings = !showSettings}>
        <svg width="16" height="16" viewBox="0 0 16 16" fill="currentColor">
          <path d="M6.5.5a.5.5 0 0 0-.5.5v1.07a5.5 5.5 0 0 0-1.56.64L3.58 1.85a.5.5 0 0 0-.7 0l-.7.7a.5.5 0 0 0 0 .71l.86.86A5.5 5.5 0 0 0 2.4 5.7H1.3a.5.5 0 0 0-.5.5v1a.5.5 0 0 0 .5.5h1.1a5.5 5.5 0 0 0 .64 1.56l-.86.86a.5.5 0 0 0 0 .7l.7.71a.5.5 0 0 0 .71 0l.86-.86a5.5 5.5 0 0 0 1.56.64v1.1a.5.5 0 0 0 .5.5h1a.5.5 0 0 0 .5-.5v-1.1a5.5 5.5 0 0 0 1.56-.64l.86.86a.5.5 0 0 0 .7 0l.71-.7a.5.5 0 0 0 0-.71l-.86-.86a5.5 5.5 0 0 0 .64-1.56h1.1a.5.5 0 0 0 .5-.5v-1a.5.5 0 0 0-.5-.5h-1.1a5.5 5.5 0 0 0-.64-1.56l.86-.86a.5.5 0 0 0 0-.7l-.7-.71a.5.5 0 0 0-.71 0l-.86.86A5.5 5.5 0 0 0 8.5 2.07V1a.5.5 0 0 0-.5-.5h-1ZM7 5a2 2 0 1 1 0 4 2 2 0 0 1 0-4Z"/>
        </svg>
      </button>
      {#if showSettings}
        <div class="settings-panel">
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
          {#if $params.buildTimestamp}
            <div class="settings-divider"></div>
            <div class="settings-row build-info">
              <label>Build</label>
              <span>{$params.buildTimestamp} UTC</span>
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

  <!-- Tab bar -->
  <nav class="tab-bar">
    {#each tabs as [id, label]}
      <button class="tab" class:active={view === id} on:click={() => view = id}>{label}</button>
    {/each}
  </nav>

  {#if view === 'mod'}
    <MacroPanel />
    <ModMatrix />
  {/if}

  {#if view === 'fx'}
    <FxPanel />
  {/if}

  {#if view === 'presets'}
    <PresetPanel />
  {/if}

  {#if view === 'play'}
  {#if $params.followTuning && $followTuningInfo.length > 0}
  <section class="follow-tuning-info">
    <table>
      <thead>
        <tr>
          <th>Prime</th>
          <th>Ratio</th>
          <th>Degree</th>
          <th>Note Pitch</th>
          <th>Deviation</th>
          <th>Adjusted</th>
        </tr>
      </thead>
      <tbody>
        {#each $followTuningInfo as entry}
        <tr class:clamped={entry.clamped} class:no-solution={entry.noSolution}>
          <td>{entry.prime}</td>
          {#if entry.noSolution}
            <td colspan="4" class="no-solution-text">--</td>
          {:else}
            <td class="ratio">{entry.chosenRatio}</td>
            <td>{entry.scaleDegree}</td>
            <td>{entry.notePitch.toFixed(2)} ct</td>
            <td>{(entry.deviation * 1200).toFixed(2)} ct</td>
          {/if}
          <td>{entry.adjustedVal.toFixed(5)}</td>
        </tr>
        {/each}
      </tbody>
    </table>
  </section>
  {/if}

  <!-- Controls -->
  <section class="controls">
    <!-- Spectrum stretch (the core feature) -->
    <div class="control-group">
      <h3>Spectrum</h3>
      <div class="knob-row">
        <Knob label="2nd" value={$params.stretch2} min={sr(2).min} max={sr(2).max} step={sr(2).step} defaultValue={2}
               onChange={send('stretch2')} />
        <Knob label="3rd" value={$params.stretch3} min={sr(3).min} max={sr(3).max} step={sr(3).step} defaultValue={3}
               onChange={send('stretch3')} />
        <Knob label="5th" value={$params.stretch5} min={sr(5).min} max={sr(5).max} step={sr(5).step} defaultValue={5}
               onChange={send('stretch5')} />
        <Knob label="7th" value={$params.stretch7} min={sr(7).min} max={sr(7).max} step={sr(7).step} defaultValue={7}
               onChange={send('stretch7')} />
        <Knob label="11th" value={$params.stretch11} min={sr(11).min} max={sr(11).max} step={sr(11).step} defaultValue={11}
               onChange={send('stretch11')} />
        <Knob label="13th" value={$params.stretch13} min={sr(13).min} max={sr(13).max} step={sr(13).step} defaultValue={13}
               onChange={send('stretch13')} />
        <Knob label="Warp" value={$params.warp} min={0} max={32} step={0.1} defaultValue={32}
               onChange={send('warp')} />
      </div>
    </div>

    <!-- Timbre -->
    <div class="control-group">
      <h3>Timbre</h3>
      <div class="knob-row">
        <Knob label="Strike" value={$params.strike} min={0} max={1} step={0.01} defaultValue={0.2}
               onChange={send('strike')} />
        <Knob label="Strike Pos" value={$params.strikePos} min={0.01} max={1} step={0.01} defaultValue={0.5}
               onChange={send('strikePos')} />
        <Knob label="Odd/Even" value={$params.oddEven} min={0} max={1} step={0.01} defaultValue={1}
               onChange={send('oddEven')} />
        <Knob label="Noise" value={$params.noiseMix} min={0} max={1} step={0.01} defaultValue={0}
               onChange={send('noiseMix')} />
      </div>
    </div>

    <!-- Envelope -->
    <div class="control-group">
      <h3>Envelope</h3>
      <div class="knob-row">
        <Knob label="Decay" value={$params.decay} min={0.01} max={20} step={0.01} log={true} defaultValue={2}
               onChange={send('decay')} />
        <Knob label="Sustain" value={$params.sustain} min={0} max={1} step={0.01} defaultValue={0}
               onChange={send('sustain')} />
        <Knob label="Release" value={$params.release} min={0.01} max={20} step={0.01} log={true} defaultValue={1}
               onChange={send('release')} />
        <Knob label="Onset Pitch" value={$params.detune} min={0.5} max={2} step={0.001} log={true} defaultValue={1}
               onChange={send('detune')} />
        <Knob label="Settle" value={$params.relaxTime} min={0.01} max={1} step={0.01} log={true} defaultValue={0.1}
               onChange={send('relaxTime')} />
        <Knob label="Volume" value={$params.volume} min={0.01} max={2} step={0.01} log={true} defaultValue={1}
               onChange={send('volume')} />
      </div>
    </div>

    <!-- Consonance -->
    <div class="control-group">
      <h3>Consonance</h3>
      <div class="knob-row">
        <Knob label="Partials" value={$params.curvePartials} min={1} max={32} step={0.1} defaultValue={16}
               onChange={send('curvePartials')} />
        <Knob label="Log Base" value={$params.logBaseline} min={0.1} max={2} step={0.01} defaultValue={0.5}
               onChange={send('logBaseline')} />
      </div>
    </div>
  </section>
  {/if}

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
    display: flex;
    align-items: center;
  }
  .logo-img {
    height: 32px;
    width: auto;
    display: block;
  }

  .mode-wrapper {
    position: relative;
  }
  .mode-btn {
    display: inline-flex;
    align-items: center;
    gap: 6px;
    font-size: 10px;
    font-weight: 700;
    letter-spacing: 0.6px;
    padding: 4px 8px;
    border-radius: 4px;
    border: 1px solid var(--border);
    background: transparent;
    color: var(--text-secondary);
    text-transform: uppercase;
    cursor: pointer;
    user-select: none;
  }
  .mode-btn:hover {
    border-color: var(--text-secondary);
  }
  .mode-btn.mode-midi {
    color: var(--text-secondary);
  }
  .mode-btn.mode-mpe {
    color: #0D75FF;
    border-color: #0D75FF;
    background: rgba(13, 117, 255, 0.1);
  }
  .mode-btn.mode-mts {
    color: #FF8A1F;
    border-color: #FF8A1F;
    background: rgba(255, 138, 31, 0.12);
  }
  .mode-btn .mts-scale {
    font-weight: 500;
    text-transform: none;
    letter-spacing: 0;
    opacity: 0.85;
    max-width: 160px;
    overflow: hidden;
    text-overflow: ellipsis;
    white-space: nowrap;
  }
  .mode-btn .mode-caret {
    opacity: 0.7;
  }
  .mode-panel {
    position: absolute;
    top: 100%;
    left: 0;
    margin-top: 6px;
    background: var(--bg-panel);
    border: 1px solid var(--border);
    border-radius: 6px;
    padding: 4px;
    z-index: 100;
    min-width: 160px;
    display: flex;
    flex-direction: column;
    gap: 2px;
  }
  .mode-item {
    display: flex;
    align-items: center;
    gap: 8px;
    padding: 5px 8px;
    background: transparent;
    border: none;
    color: var(--text-primary);
    font-size: 11px;
    text-align: left;
    cursor: pointer;
    border-radius: 4px;
  }
  .mode-item:hover {
    background: var(--bg-secondary);
  }
  .mode-item.checked {
    color: #0D75FF;
    font-weight: 600;
  }
  .mode-item .mode-check {
    width: 10px;
    display: inline-block;
    text-align: center;
  }
  .mode-divider {
    border-top: 1px solid var(--border);
    margin: 4px 2px;
  }
  .mode-setting {
    display: flex;
    align-items: center;
    justify-content: space-between;
    gap: 12px;
    padding: 4px 8px;
  }
  .mode-setting-label {
    font-size: 11px;
    color: var(--text-secondary);
    white-space: nowrap;
  }
  .mode-setting select {
    background: var(--bg-secondary);
    color: var(--text-primary);
    border: 1px solid var(--border);
    border-radius: 4px;
    padding: 3px 6px;
    font-size: 11px;
    cursor: pointer;
  }

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

  .follow-tuning-btn {
    background: none;
    border: 1px solid var(--border);
    border-radius: 4px;
    color: var(--text-secondary);
    cursor: pointer;
    padding: 4px 8px;
    display: flex;
    align-items: center;
    gap: 4px;
    font-size: 11px;
    margin-right: 6px;
    white-space: nowrap;
  }
  .follow-tuning-btn:hover {
    color: var(--text-primary);
    border-color: var(--text-secondary);
  }
  .follow-tuning-btn.active {
    color: #0D75FF;
    border-color: #0D75FF;
    background: rgba(13, 117, 255, 0.1);
  }
  .follow-label {
    line-height: 1;
  }
  .tab-bar {
    display: flex;
    gap: 4px;
    padding: 0 16px;
    margin: 6px 0 4px;
    border-bottom: 1px solid #23252e;
  }
  .tab {
    background: transparent;
    color: #889;
    border: none;
    border-bottom: 2px solid transparent;
    padding: 8px 16px;
    cursor: pointer;
    font-size: 12px;
    letter-spacing: 0.04em;
    text-transform: uppercase;
  }
  .tab:hover { color: #cde; }
  .tab.active {
    color: #FFAB00;
    border-bottom-color: #FFAB00;
  }
  .follow-tuning-info {
    padding: 0 16px 4px;
  }
  .follow-tuning-info table {
    width: 100%;
    border-collapse: collapse;
    font-size: 10px;
    font-family: 'Inter', monospace;
  }
  .follow-tuning-info th {
    color: var(--text-secondary);
    font-weight: 500;
    text-align: left;
    padding: 2px 8px;
    border-bottom: 1px solid var(--border);
  }
  .follow-tuning-info td {
    color: var(--text-primary);
    padding: 2px 8px;
  }
  .follow-tuning-info .ratio {
    color: #0D75FF;
    font-weight: 600;
  }
  .follow-tuning-info tr.clamped td {
    color: #FF4444;
  }
  .follow-tuning-info tr.clamped .ratio {
    color: #FF4444;
  }
  .follow-tuning-info tr.no-solution td {
    color: var(--text-secondary);
    opacity: 0.5;
  }
  .follow-tuning-info .no-solution-text {
    text-align: center;
    font-style: italic;
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
  .build-info span {
    font-size: 10px;
    color: var(--text-secondary);
    opacity: 0.85;
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
