<script>
  import { onMount } from 'svelte';
  import { connect, sendParam, params, connected, activeNotes } from './lib/ws.js';
  import Knob from './lib/Knob.svelte';
  import ConsonancePlot from './lib/ConsonancePlot.svelte';

  onMount(() => connect());

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
    <div class="status" class:connected={$connected}>
      {$connected ? '● Connected' : '○ Disconnected'}
    </div>
    <div class="active-notes">
      {#each $activeNotes as note}
        <span class="note-badge">
          {note.note || note}
        </span>
      {/each}
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
        <Knob label="Strike" value={$params.strikePos} min={0.01} max={1} step={0.01}
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

  .status {
    font-size: 11px;
    color: var(--accent-red);
  }
  .status.connected {
    color: #4caf50;
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
</style>
