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
      <svg class="logo-icon" viewBox="0 0 78 78" xmlns="http://www.w3.org/2000/svg">
        <rect style="fill:#000;stroke-width:0.243" width="50.195" height="69.989" x="9.398" y="4.138" ry="0.83"/>
        <rect style="fill:#000;stroke-width:1.322" width="20.135" height="68.171" x="53.536" y="5.712" ry="4.351"/>
        <path fill="#0D75FF" d="m 11.148453,2.4578779 c -6.3493773,0 -8.690575,2.3624512 -8.690575,8.6679791 v 56.050739 c 0,6.094157 2.6299237,8.538303 8.592338,8.538303 h 7.272345 c -0.769895,-0.500612 -1.49718,-1.057388 -2.131496,-1.738104 -1.768338,-1.860112 -1.780927,-4.593633 -1.726696,-6.995995 0.180126,-2.659931 -0.30699,-5.392484 0.458064,-7.990443 0.670147,-2.45465 2.967243,-3.902265 5.075498,-5.02937 2.58859,-1.262668 4.926361,-3.284486 7.818068,-3.784131 -0.784422,-1.651926 -0.807665,-3.640822 -0.765054,-5.446709 0.18981,-2.719965 -0.322485,-5.519332 0.476463,-8.17442 0.06004,-0.211091 0.136548,-0.412497 0.218864,-0.609063 -1.839033,-0.276935 -3.544424,-1.365309 -5.124888,-2.297785 -2.158611,-1.405978 -4.678444,-2.354915 -6.449688,-4.289586 -1.703453,-1.801045 -1.763495,-4.436768 -1.708295,-6.767475 0.179158,-2.718028 -0.305053,-5.506743 0.446442,-8.165705 0.677896,-2.496288 3.024381,-3.95068 5.165561,-5.09231 2.750318,-1.3517521 5.22173,-3.5565797 8.389437,-3.831578 0.114273,-0.00484 0.228547,-0.00581 0.341852,-0.00388 0.79798,0.016461 1.588212,0.2023755 2.33196,0.4880251 2.596338,1.2384605 5.062908,2.7451411 7.513984,4.2489169 1.051705,0.526758 2.007537,1.283972 2.729979,2.213544 1.03718,-1.33626 2.60215,-2.282293 4.075119,-3.0714612 0.02808,-0.013556 0.05617,-0.030018 0.08425,-0.044542 L 41.673143,2.4578779 Z m 34.354798,0 2.942065,5.2269038 c 1.716042,-1.0293069 3.457265,-2.0034205 5.489014,-2.1815884 0.900632,-0.035827 1.79739,0.1500871 2.637011,0.4705957 2.582781,1.2229676 5.031919,2.718997 7.465562,4.215026 1.919412,0.950875 3.548298,2.641533 4.030571,4.773738 0.390275,2.703504 0.211117,5.455423 0.197559,8.18023 0.06779,1.55316 -0.03874,3.199275 -0.644969,4.64108 0.561684,0.08231 1.113684,0.22852 1.640506,0.436705 2.226401,1.070944 4.351118,2.345232 6.464214,3.63017 l -0.0265,-20.722085 c -9.68e-4,-6.0146774 -2.313,-8.6362806 -8.779756,-8.644281 z M 28.72922,8.5669065 c -1.282191,-0.00774 -2.569223,0.8462974 -5.563582,2.5524485 -5.989688,3.412302 -5.564551,2.686075 -5.605225,9.579429 -0.03971,6.893355 -0.456127,6.162286 5.493856,9.643337 5.452214,3.189593 5.262403,3.418113 9.790742,0.814345 L 29.656,25.489952 c -0.327317,0.07248 -0.661278,0.110769 -0.996507,0.11426 -2.674223,0 -4.842107,-2.167622 -4.842107,-4.84152 0,-2.673896 2.167884,-4.841518 4.842107,-4.841518 2.02357,0.01594 3.823818,1.288428 4.513813,3.19056 l 6.599794,-0.0029 C 39.818616,14.092048 39.647205,14.332187 34.264718,11.182295 31.288758,9.4432213 30.01141,8.5736845 28.72922,8.5669065 Z m 25.467552,0 C 53.131508,8.5610953 52.00136,9.1846842 49.951211,10.36311 l 3.23356,5.745914 c 0.30649,-0.09325 0.621481,-0.155845 0.940337,-0.186882 2.674223,0 4.842109,2.167621 4.842109,4.841519 0,2.673897 -2.167886,4.841518 -4.842109,4.841518 -2.014267,-0.014 -3.809485,-1.273385 -4.508002,-3.16248 l -6.605604,0.0039 c -0.04454,4.989669 0.135579,4.753403 5.508382,7.897485 5.949983,3.481052 5.108424,3.476211 11.098111,0.06488 5.989688,-3.412302 5.564551,-2.686074 5.605225,-9.579428 C 65.26293,13.936181 65.679347,14.66725 59.729364,11.18523 56.754373,9.4441897 55.477993,8.5756212 54.195803,8.5678747 Z m -7.145983,3.4471615 c -3.863034,2.18643 -4.009266,2.522432 -4.01895,7.094762 l 6.582361,-0.0039 c 0.182024,-0.445393 0.429431,-0.861167 0.734065,-1.233618 z m -7.298994,10.432504 -6.562993,0.0029 c -0.167785,0.456846 -0.403502,0.885784 -0.699201,1.272352 l 3.254865,5.783679 c 3.845602,-2.176748 3.997644,-2.517591 4.007329,-7.058936 z m 1.565937,8.328381 c -1.015874,-0.0058 -2.074359,0.551934 -3.94535,1.623846 l 3.290697,5.846618 c 0.191767,-0.05064 0.386464,-0.08944 0.58299,-0.116196 2.674223,0 4.842108,2.167621 4.842108,4.841518 -0.02984,1.057077 -0.405014,2.075309 -1.068169,2.899101 l 3.29457,5.853397 c 4.290108,-2.427537 3.993772,-2.488541 4.029603,-8.68859 0.03971,-6.893354 0.456126,-6.162284 -5.493855,-9.643337 -2.974992,-1.74101 -4.251371,-2.610547 -5.533562,-2.617325 z m 25.467552,0 c -1.28219,-0.0077 -2.569223,0.847266 -5.56455,2.553418 -5.989688,3.412302 -5.564551,2.686074 -5.605225,9.579429 -0.03971,6.893354 -0.456127,6.162284 5.493856,9.644305 5.949983,3.481052 5.109393,3.476211 11.099081,0.06488 5.989687,-3.412302 5.564551,-2.686074 5.605224,-9.579428 0.03971,-6.893356 0.456126,-6.162286 -5.493856,-9.643338 -2.974991,-1.74101 -4.251371,-2.610547 -5.533561,-2.617325 z m -32.311387,3.27674 c -4.580635,2.584403 -4.288171,2.524368 -4.324971,8.855138 -0.03971,6.893354 -0.456127,6.162286 5.493856,9.644306 5.445434,3.185719 5.262403,3.418112 9.77331,0.824995 L 42.180596,47.62828 c -0.304042,0.09209 -0.616412,0.154036 -0.93259,0.184946 -2.674223,0 -4.842109,-2.167621 -4.842109,-4.841518 0.01562,-1.192898 0.471179,-2.33803 1.279285,-3.215737 z m 32.240692,4.078496 c 2.674223,0 4.842109,2.167621 4.842109,4.841519 0,2.673897 -2.167886,4.841518 -4.842109,4.841518 -2.674223,0 -4.842107,-2.167621 -4.842107,-4.841518 0,-2.673898 2.167884,-4.841519 4.842107,-4.841519 z M 28.72922,53.152453 c -1.282191,-0.0077 -2.569223,0.847267 -5.563582,2.553418 -5.989688,3.412302 -5.564551,2.686074 -5.605225,9.579429 -0.03971,6.893354 -0.456127,6.162285 5.493856,9.644305 5.42994,3.177005 5.261435,3.417144 9.732637,0.848235 l -3.199665,-5.685881 c -0.305316,0.06303 -0.616008,0.09644 -0.927748,0.09973 -2.674223,0 -4.842107,-2.16762 -4.842107,-4.841518 0,-2.673898 2.167884,-4.841518 4.842107,-4.841518 2.061091,0.0046 3.893372,1.313269 4.566108,3.261246 l 6.546531,-0.0029 c 0.04746,-5.084564 -0.102653,-4.832805 -5.508383,-7.995284 -2.974991,-1.741011 -4.252339,-2.610548 -5.534529,-2.617326 z m 25.467552,0 c -1.043959,-0.0058 -2.142149,0.588729 -4.11095,1.719707 l 3.259707,5.790457 c 0.255491,-0.07297 0.516554,-0.124786 0.780547,-0.154928 2.674223,0 4.842109,2.167622 4.842109,4.841519 0,2.673897 -2.167886,4.841519 -4.842109,4.841519 -1.991613,-0.0091 -3.774517,-1.23662 -4.493476,-3.093731 l -6.62013,0.0029 c -0.04261,4.923825 0.158822,4.69821 5.50935,7.827768 5.949983,3.481051 5.108425,3.47621 11.098113,0.06488 5.989687,-3.412303 5.564551,-2.686074 5.605224,-9.579429 0.03971,-6.893355 0.456126,-6.162285 -5.493856,-9.644306 C 56.75631,54.0278 55.47993,53.158263 54.19774,53.151485 Z m 21.527044,0.982829 c -0.469684,0.267251 -0.939369,0.53063 -1.394527,0.80466 -2.204128,1.144535 -4.27074,2.78581 -6.773142,3.200244 0.192716,0.381512 0.349601,0.783357 0.459032,1.205538 0.460001,2.698663 0.259537,5.460265 0.25179,8.187009 0.07554,2.1293 -0.07264,4.442578 -1.42939,6.195208 -0.670148,0.796913 -1.490401,1.426311 -2.361981,1.987927 l 2.46943,-0.02649 c 6.961976,-0.07469 8.804317,-2.285182 8.751323,-8.51181 z m -28.537449,2.388805 c -4.005391,2.262926 -4.145812,2.556323 -4.154528,7.240976 l 6.567835,-0.0029 c 0.194986,-0.528635 0.480763,-1.01918 0.844464,-1.449551 z m -7.434572,10.578719 -6.587204,0.0029 c -0.176366,0.452832 -0.419908,0.876532 -0.722442,1.256859 l 3.243243,5.763344 c 3.879497,-2.194176 4.054782,-2.522431 4.065434,-7.022138 z m 1.656001,6.572846 c -0.06294,0.08424 -0.124927,0.167516 -0.192716,0.248854 -0.648842,0.70783 -1.416802,1.274288 -2.223497,1.789426 h 4.797561 c -0.780548,-0.50836 -1.516548,-1.074818 -2.152801,-1.773933 -0.08135,-0.08618 -0.154947,-0.175264 -0.228547,-0.264347 z"/>
      </svg>
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
    display: flex;
    align-items: center;
    gap: 8px;
    font-size: 18px;
    font-weight: 700;
    letter-spacing: -0.5px;
  }
  .logo-icon {
    width: 28px;
    height: 28px;
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
