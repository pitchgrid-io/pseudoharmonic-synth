<script>
  import { onMount } from 'svelte';
  import { presets, sendCommand } from './ws.js';
  let name = '';
  let selected = '';

  onMount(() => sendCommand('presetList'));

  function load(n) { selected = n; sendCommand('presetLoad', { name: n }); }
  function save() { if (name.trim()) { sendCommand('presetSave', { name: name.trim() }); } }
  function del(n) { sendCommand('presetDelete', { name: n }); if (selected === n) selected = ''; }
</script>

<div class="preset-panel">
  <div class="save-row">
    <input type="text" placeholder="Preset name…" bind:value={name}
           on:keydown={(e) => e.key === 'Enter' && save()} />
    <button class="save-btn" on:click={save} disabled={!name.trim()}>Save</button>
  </div>

  <div class="list">
    {#if $presets.length === 0}
      <p class="hint">No presets yet.</p>
    {/if}
    {#each $presets as p}
      <div class="preset-row" class:active={selected === p}>
        <button class="load" on:click={() => load(p)}>{p}</button>
        <button class="del" title="Delete" on:click={() => del(p)}>✕</button>
      </div>
    {/each}
  </div>
</div>

<style>
  .preset-panel { padding: 12px 16px; max-width: 460px; }
  .save-row { display: flex; gap: 8px; margin-bottom: 14px; }
  input[type="text"] { flex: 1; background: #1b1d26; color: #dde; border: 1px solid #333; border-radius: 6px; padding: 7px 10px; }
  .save-btn { background: #2a3350; color: #cde; border: 1px solid #3a4a6a; border-radius: 6px; padding: 7px 16px; cursor: pointer; }
  .save-btn:disabled { opacity: 0.4; cursor: default; }
  .list { display: flex; flex-direction: column; gap: 2px; }
  .preset-row { display: flex; align-items: center; gap: 8px; border-radius: 6px; }
  .preset-row.active { background: rgba(255, 171, 0, 0.12); }
  .load { flex: 1; text-align: left; background: transparent; color: #cde; border: none; padding: 8px 10px; cursor: pointer; font-size: 13px; }
  .load:hover { color: #fff; }
  .preset-row.active .load { color: #FFAB00; }
  .del { background: transparent; color: #a55; border: none; cursor: pointer; padding: 6px 10px; }
  .del:hover { color: #f66; }
  .hint { font-size: 11px; color: #778; }
</style>
