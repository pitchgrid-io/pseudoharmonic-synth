// WebSocket connection to JUCE backend
import { writable } from 'svelte/store';

export const connected = writable(false);
export const params = writable({
  stretch2: 2.0, stretch3: 3.0, stretch5: 5.0, stretch7: 7.0,
  decay: 2.0, release: 1.0, strikePos: 0.5, oddEven: 1.0,
  strike: 0.02, volume: 1.0, noiseMix: 0.0, detune: 1.0, relaxTime: 0.1,
  pitchBendRange: 2, mpeEnabled: false, mpeMasterBendRange: 2, mpePerNoteBendRange: 48,
  curvePartials: 16, logBaseline: 0.5, warp: 32,
  oscSendConsonance: false, oscSendSpectrum: true,
  showRatioLabels: false
});
export const curveData = writable(null);
export const activeNotes = writable([]);
export const intervals = writable([]);
export const scaleDegrees = writable([]);
export const outputLevel = writable(0);

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
        else if (msg.type === 'level') outputLevel.set(msg.value);
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
