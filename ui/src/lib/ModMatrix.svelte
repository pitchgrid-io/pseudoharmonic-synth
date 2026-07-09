<script>
  import { modConfig, modSources, modDests, sendModConfig, BIPOLAR_SOURCES } from './ws.js';

  const shapes = ['Sine', 'Triangle', 'Ramp Up', 'Ramp Down', 'Square', 'Pulse', 'S&H', 'Hann'];
  const curves = [['linear', 'Linear'], ['s', 'S'], ['squared', 'Exp'], ['sqrt', 'Log'], ['2step', '2-Step'], ['3step', '3-Step']];

  const clone = (c) => JSON.parse(JSON.stringify(c));
  const push = (c) => sendModConfig(c);
  const emptyComp = () => ({ en: false, src: 'none', mode: 0, xfer: 'linear', min: 0, max: 1, mult: 1 });

  function makeRoute() {
    const src = $modSources[0] || 'lfo1';
    const bip = BIPOLAR_SOURCES.has(src);
    return {
      en: true, dest: $modDests[0] || 'volume', combine: 0, op: 0, anc: 0, ancAmt: 1,
      pers: 0, interp: 0.01,
      w: { en: true, src, mode: 0, xfer: 'linear', min: bip ? -1 : 0, max: 1, mult: 1 },
      x: emptyComp(), y: emptyComp(), z: emptyComp()
    };
  }

  const depthOf = (r) => r.w.max;
  function setDepth(r, d) {
    r.w.max = d;
    r.w.min = BIPOLAR_SOURCES.has(r.w.src) ? -Math.abs(d) : 0;
  }

  function addRoute() { const c = clone($modConfig); c.routes.push(makeRoute()); push(c); }
  function removeRoute(i) { const c = clone($modConfig); c.routes.splice(i, 1); push(c); }
  function setSource(i, v) { const c = clone($modConfig); c.routes[i].w.src = v; setDepth(c.routes[i], depthOf(c.routes[i])); push(c); }
  function setDest(i, v) { const c = clone($modConfig); c.routes[i].dest = v; push(c); }
  function setRouteDepth(i, v) { const c = clone($modConfig); setDepth(c.routes[i], +v); push(c); }
  function setCurve(i, v) { const c = clone($modConfig); c.routes[i].w.xfer = v; push(c); }
  function toggleRoute(i) { const c = clone($modConfig); c.routes[i].en = !c.routes[i].en; push(c); }

  function setLfo(i, field, v) { const c = clone($modConfig); c.lfos[i][field] = v; push(c); }
  function setEnv(i, field, v) { const c = clone($modConfig); c.envs[i][field] = v; push(c); }
</script>

<div class="mod-matrix">
  <!-- LFOs -->
  <div class="mod-group">
    <h3>LFOs</h3>
    {#each ($modConfig.lfos || []) as lfo, i}
      <div class="gen-row">
        <span class="gen-label">LFO {i + 1}</span>
        <label>Rate
          <input type="number" min="0.01" max="30" step="0.01" value={lfo.rate}
                 on:change={(e) => setLfo(i, 'rate', +e.target.value)} /> Hz
        </label>
        <label>Shape
          <select value={lfo.shape} on:change={(e) => setLfo(i, 'shape', +e.target.value)}>
            {#each shapes as s, si}<option value={si}>{s}</option>{/each}
          </select>
        </label>
        <label class="chk"><input type="checkbox" checked={lfo.retrig}
                on:change={(e) => setLfo(i, 'retrig', e.target.checked)} /> Retrig</label>
      </div>
    {/each}
  </div>

  <!-- Envelopes -->
  <div class="mod-group">
    <h3>Envelopes</h3>
    {#each ($modConfig.envs || []) as env, i}
      <div class="gen-row">
        <span class="gen-label">Env {i + 1}</span>
        {#each [['attack','A'],['decay','D'],['sustain','S'],['release','R']] as [f, lbl]}
          <label>{lbl}
            <input type="number" min="0" max={f === 'sustain' ? 1 : 20} step="0.01" value={env[f]}
                   on:change={(e) => setEnv(i, f, +e.target.value)} />
          </label>
        {/each}
      </div>
    {/each}
  </div>

  <!-- Routes -->
  <div class="mod-group">
    <div class="routes-head">
      <h3>Mod Matrix</h3>
      <button class="add-btn" on:click={addRoute}>+ Add route</button>
    </div>
    {#if ($modConfig.routes || []).length === 0}
      <p class="hint">No routes yet. Add a route to modulate a parameter from a source
        (LFO, envelope, macro, or expression).</p>
    {/if}
    {#each ($modConfig.routes || []) as route, i}
      <div class="route-row" class:disabled={!route.en}>
        <input type="checkbox" checked={route.en} on:change={() => toggleRoute(i)} title="Enable" />
        <select value={route.w.src} on:change={(e) => setSource(i, e.target.value)}>
          {#each $modSources as s}<option value={s}>{s}</option>{/each}
        </select>
        <span class="arrow">→</span>
        <select value={route.dest} on:change={(e) => setDest(i, e.target.value)}>
          {#each $modDests as d}<option value={d}>{d}</option>{/each}
        </select>
        <label class="depth">depth
          <input type="number" step="0.01" value={depthOf(route)}
                 on:change={(e) => setRouteDepth(i, e.target.value)} />
        </label>
        <select value={route.w.xfer} on:change={(e) => setCurve(i, e.target.value)}>
          {#each curves as [val, lbl]}<option value={val}>{lbl}</option>{/each}
        </select>
        <button class="rm-btn" on:click={() => removeRoute(i)} title="Remove">✕</button>
      </div>
    {/each}
  </div>
</div>

<style>
  .mod-matrix { display: flex; flex-direction: column; gap: 20px; padding: 12px 16px; }
  h3 { margin: 0 0 10px; font-size: 12px; letter-spacing: 0.08em; text-transform: uppercase; color: #aab; }
  .gen-row { display: flex; align-items: center; gap: 14px; margin-bottom: 8px; font-size: 12px; color: #bcd; }
  .gen-label { width: 52px; color: #89a; font-weight: 600; }
  label { display: inline-flex; align-items: center; gap: 5px; color: #99a; }
  input[type="number"] { width: 62px; background: #1b1d26; color: #dde; border: 1px solid #333; border-radius: 5px; padding: 4px 6px; }
  select { background: #1b1d26; color: #dde; border: 1px solid #333; border-radius: 5px; padding: 4px 6px; }
  .routes-head { display: flex; align-items: center; justify-content: space-between; }
  .add-btn { background: #2a3350; color: #cde; border: 1px solid #3a4a6a; border-radius: 6px; padding: 5px 12px; cursor: pointer; }
  .add-btn:hover { background: #33406a; }
  .route-row { display: flex; align-items: center; gap: 10px; padding: 6px 0; border-bottom: 1px solid #23252e; }
  .route-row.disabled { opacity: 0.45; }
  .arrow { color: #667; }
  .depth input { width: 70px; }
  .rm-btn { margin-left: auto; background: transparent; color: #a55; border: none; cursor: pointer; font-size: 14px; }
  .rm-btn:hover { color: #f66; }
  .hint { font-size: 11px; color: #778; }
</style>
