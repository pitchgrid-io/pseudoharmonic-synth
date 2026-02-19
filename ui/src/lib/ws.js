// WebSocket connection to JUCE backend
import { writable } from 'svelte/store';

export const connected = writable(false);
export const params = writable({
  stretch2: 2.0, stretch3: 3.0, stretch5: 5.0, stretch7: 7.0,
  decay: 2.0, release: 1.0, strikePos: 0.5, oddEven: 1.0,
  volume: 0.02, noiseMix: 0.0, detune: 1.0, relaxTime: 0.1
});
export const curveData = writable(null);
export const activeNotes = writable([]);
export const intervals = writable([]);

let ws = null;
let reconnectTimer = null;

export function getWSPort() {
  const urlParams = new URLSearchParams(window.location.search);
  return urlParams.get('wsPort') || '9100';
}

export function connect() {
  const port = getWSPort();
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
        reconnectTimer = setInterval(() => connect(), 2000);
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
      } catch (e) {}
    };
  } catch (e) {
    if (!reconnectTimer) {
      reconnectTimer = setInterval(() => connect(), 2000);
    }
  }
}

export function sendParam(id, value) {
  if (ws && ws.readyState === WebSocket.OPEN) {
    ws.send(JSON.stringify({ type: 'param', id, value }));
  }
  // Also update local store immediately
  params.update(p => ({ ...p, [id]: value }));
}
