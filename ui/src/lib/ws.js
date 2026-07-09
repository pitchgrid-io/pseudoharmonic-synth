// WebSocket connection to JUCE backend
import { writable } from 'svelte/store';

export const connected = writable(false);
export const params = writable({
  stretch2: 2.0, stretch3: 3.0, stretch5: 5.0, stretch7: 7.0, stretch11: 11.0, stretch13: 13.0,
  decay: 2.0, release: 1.0, strikePos: 0.5, oddEven: 1.0,
  strike: 0.02, volume: 1.0, noiseMix: 0.0, detune: 1.0, relaxTime: 0.1,
  pitchBendRange: 2, mpeEnabled: true, mpeMasterBendRange: 2, mpePerNoteBendRange: 48,
  curvePartials: 16, logBaseline: 0.5, warp: 32,
  centreFocus: 0.0, ampTilt: 0.0, phaseSpread: 0.0, excitationMode: 0,
  filterType: 0, filterCutoff: 12000, filterReso: 0.1,
  delayTime: 0.3, delayFeedback: 0.3, delayMix: 0.0, reverbAmount: 0.0, reverbSize: 0.5,
  macro1: 0, macro2: 0, macro3: 0, macro4: 0, macro5: 0, macro6: 0, macro7: 0, macro8: 0,
  oscSendConsonance: false, oscSendSpectrum: true,
  showRatioLabels: true, followTuning: false, oscConnected: false,
  tuningMode: 'MPE', mtsMasterAvailable: false, mtsActive: false, mtsScaleName: ''
});
export const curveData = writable(null);
export const activeNotes = writable([]);
export const intervals = writable([]);
export const scaleDegrees = writable([]);
export const followTuningInfo = writable([]);
export const outputLevel = writable(0);

// Modulation state (from backend 'modstate' message).
export const modConfig = writable({ lfos: [], envs: [], routes: [] });
export const modSources = writable([]);   // available source names
export const modDests = writable([]);     // available destination names
export const presets = writable([]);      // preset names

let ws = null;
let reconnectTimer = null;

function connectWS(port) {
  const url = `ws://127.0.0.1:${port}`;

  try {
    ws = new WebSocket(url);

    ws.onopen = () => {
      connected.set(true);
      if (reconnectTimer) {
        clearInterval(reconnectTimer);
        reconnectTimer = null;
      }
    };

    ws.onclose = () => {
      connected.set(false);
      if (!reconnectTimer) {
        reconnectTimer = setInterval(() => connectWS(port), 2000);
      }
    };

    ws.onerror = () => {};

    ws.onmessage = (event) => {
      try {
        const msg = JSON.parse(event.data);
        if (msg.type === 'params') params.set(msg.data);
        else if (msg.type === 'curve') curveData.set(msg.data);
        else if (msg.type === 'notes') activeNotes.set(msg.data);
        else if (msg.type === 'intervals') intervals.set(msg.data);
        else if (msg.type === 'scaleDegrees') scaleDegrees.set(msg.data);
        else if (msg.type === 'followTuningInfo') followTuningInfo.set(msg.data);
        else if (msg.type === 'level') outputLevel.set(msg.value);
        else if (msg.type === 'modstate') {
          if (msg.data.config) modConfig.set(msg.data.config);
          if (msg.data.sources) modSources.set(msg.data.sources);
          if (msg.data.dests) modDests.set(msg.data.dests);
        }
        else if (msg.type === 'presets') presets.set(msg.data);
      } catch (e) {}
    };
  } catch (e) {
    if (!reconnectTimer) {
      reconnectTimer = setInterval(() => connectWS(port), 2000);
    }
  }
}

/**
 * Initialize the WS connection.
 * If running inside JUCE WebBrowserComponent, gets port via native function.
 * Otherwise falls back to URL query param (for standalone browser dev).
 */
export async function init() {
  if (window.__JUCE__) {
    // Running inside JUCE — get port via native bridge
    const { getNativeFunction } = await import('juce-framework-frontend');
    const uiMounted = getNativeFunction('uiMounted');
    const port = await uiMounted();
    connectWS(port);
  } else {
    // Standalone browser — get port from URL query param
    const urlParams = new URLSearchParams(window.location.search);
    const port = urlParams.get('wsPort') || '9100';
    connectWS(port);
  }
}

export function sendParam(id, value) {
  if (ws && ws.readyState === WebSocket.OPEN) {
    ws.send(JSON.stringify({ type: 'param', id, value }));
  }
  // Also update local store immediately
  params.update(p => ({ ...p, [id]: value }));
}

// Push the full modulation config to the backend (routes + LFO/env settings).
// Also updates the local store optimistically.
export function sendModConfig(config) {
  if (ws && ws.readyState === WebSocket.OPEN) {
    ws.send(JSON.stringify({ type: 'modConfig', data: config }));
  }
  modConfig.set(config);
}

// Send a generic typed command to the backend (preset save/load/list, etc.).
export function sendCommand(type, extra = {}) {
  if (ws && ws.readyState === WebSocket.OPEN) {
    ws.send(JSON.stringify({ type, ...extra }));
  }
}

// Sources that swing bipolar (-1..1); used to map a route's depth to a range.
export const BIPOLAR_SOURCES = new Set(['lfo1', 'lfo2', 'lfo3', 'pitch']);
