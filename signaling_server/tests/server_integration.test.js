const test = require('node:test');
const assert = require('node:assert/strict');
const { spawn } = require('node:child_process');
const net = require('node:net');
const path = require('node:path');
const jwt = require('jsonwebtoken');
const WebSocket = require('ws');

const SERVER_ROOT = path.resolve(__dirname, '..');

function getFreePort() {
  return new Promise((resolve, reject) => {
    const server = net.createServer();
    server.listen(0, '127.0.0.1', () => {
      const address = server.address();
      const port = address && typeof address === 'object' ? address.port : null;
      server.close((err) => {
        if (err) {
          reject(err);
          return;
        }
        resolve(port);
      });
    });
    server.on('error', reject);
  });
}

function waitForChildReady(child, matcher, timeoutMs) {
  return new Promise((resolve, reject) => {
    let settled = false;
    const stdout = [];
    const stderr = [];

    const timer = setTimeout(() => {
      if (!settled) {
        settled = true;
        reject(
          new Error(
            `server start timeout (${timeoutMs}ms)\nstdout:\n${stdout.join('')}\nstderr:\n${stderr.join('')}`
          )
        );
      }
    }, timeoutMs);

    child.stdout.on('data', (chunk) => {
      const text = chunk.toString();
      stdout.push(text);
      if (!settled && matcher.test(text)) {
        settled = true;
        clearTimeout(timer);
        resolve({ stdout: stdout.join(''), stderr: stderr.join('') });
      }
    });

    child.stderr.on('data', (chunk) => {
      stderr.push(chunk.toString());
    });

    child.on('exit', (code, signal) => {
      if (!settled) {
        settled = true;
        clearTimeout(timer);
        reject(
          new Error(
            `server exited early (code=${code}, signal=${signal})\nstdout:\n${stdout.join('')}\nstderr:\n${stderr.join('')}`
          )
        );
      }
    });
  });
}

async function startServer({ port, jwtSecret }) {
  const child = spawn('node', ['src/server.js'], {
    cwd: SERVER_ROOT,
    env: {
      ...process.env,
      PORT: String(port),
      JWT_SECRET: jwtSecret,
      SSL_ENABLED: 'false',
    },
    stdio: ['ignore', 'pipe', 'pipe'],
  });

  await waitForChildReady(child, /WebSocket server started on port/i, 5000);
  return child;
}

function stopServer(child) {
  return new Promise((resolve) => {
    const done = () => resolve();

    child.once('exit', done);
    child.kill('SIGTERM');

    setTimeout(() => {
      if (!child.killed) {
        child.kill('SIGKILL');
      }
      resolve();
    }, 3000);
  });
}

function createToken(clientId, jwtSecret) {
  return jwt.sign({ clientId }, jwtSecret, { expiresIn: '1h' });
}

function createWsClient(port, token) {
  const query = token ? `?token=${encodeURIComponent(token)}` : '';
  const ws = new WebSocket(`ws://127.0.0.1:${port}/${query}`);

  const queue = [];
  const waiters = [];
  let closed = false;

  ws.on('message', (data) => {
    let parsed;
    try {
      parsed = JSON.parse(data.toString());
    } catch {
      parsed = { type: 'invalid_json', raw: data.toString() };
    }

    const waiterIndex = waiters.findIndex((item) => item.predicate(parsed));
    if (waiterIndex >= 0) {
      const waiter = waiters.splice(waiterIndex, 1)[0];
      waiter.resolve(parsed);
      return;
    }

    queue.push(parsed);
  });

  ws.on('close', () => {
    closed = true;
    while (waiters.length > 0) {
      const waiter = waiters.shift();
      waiter.reject(new Error('websocket closed before expected message'));
    }
  });

  function waitOpen(timeoutMs = 3000) {
    return new Promise((resolve, reject) => {
      if (ws.readyState === ws.OPEN) {
        resolve();
        return;
      }

      const timer = setTimeout(() => {
        cleanup();
        reject(new Error('websocket open timeout'));
      }, timeoutMs);

      function cleanup() {
        clearTimeout(timer);
        ws.off('open', onOpen);
        ws.off('error', onError);
      }

      function onOpen() {
        cleanup();
        resolve();
      }

      function onError(err) {
        cleanup();
        reject(err);
      }

      ws.once('open', onOpen);
      ws.once('error', onError);
    });
  }

  function waitMessage(predicate, timeoutMs = 3000) {
    const fromQueueIndex = queue.findIndex(predicate);
    if (fromQueueIndex >= 0) {
      const [msg] = queue.splice(fromQueueIndex, 1);
      return Promise.resolve(msg);
    }

    return new Promise((resolve, reject) => {
      if (closed) {
        reject(new Error('websocket already closed'));
        return;
      }

      const timer = setTimeout(() => {
        const idx = waiters.findIndex((item) => item.resolve === resolve);
        if (idx >= 0) {
          waiters.splice(idx, 1);
        }
        reject(new Error('waitMessage timeout'));
      }, timeoutMs);

      waiters.push({
        predicate,
        resolve: (message) => {
          clearTimeout(timer);
          resolve(message);
        },
        reject: (err) => {
          clearTimeout(timer);
          reject(err);
        },
      });
    });
  }

  function waitClose(timeoutMs = 3000) {
    return new Promise((resolve, reject) => {
      if (ws.readyState === ws.CLOSED) {
        resolve();
        return;
      }

      const timer = setTimeout(() => reject(new Error('websocket close timeout')), timeoutMs);
      ws.once('close', () => {
        clearTimeout(timer);
        resolve();
      });
    });
  }

  return {
    ws,
    waitOpen,
    waitMessage,
    waitClose,
    close() {
      if (ws.readyState === ws.OPEN || ws.readyState === ws.CONNECTING) {
        ws.close();
      }
    },
  };
}

test('signaling server end-to-end: auth, routing, leave, disconnect, errors', async () => {
  const port = await getFreePort();
  const jwtSecret = `whl-air-test-secret-${Date.now()}`;

  const server = await startServer({ port, jwtSecret });

  let cockpit;
  let vehicle;
  let invalid;

  try {
    cockpit = createWsClient(port, createToken('cockpit_e2e', jwtSecret));
    vehicle = createWsClient(port, createToken('vehicle_e2e', jwtSecret));

    await Promise.all([cockpit.waitOpen(), vehicle.waitOpen()]);

    const cockpitConnected = await cockpit.waitMessage((m) => m.type === 'connected');
    const vehicleConnected = await vehicle.waitMessage((m) => m.type === 'connected');

    assert.equal(cockpitConnected.clientId, 'cockpit_e2e');
    assert.equal(vehicleConnected.clientId, 'vehicle_e2e');

    cockpit.ws.send(
      JSON.stringify({
        type: 'join',
        from: 'cockpit_e2e',
        data: { targetVehicleId: 'vehicle_e2e' },
      })
    );

    const joinRequest = await vehicle.waitMessage((m) => m.type === 'join_request');
    assert.equal(joinRequest.from, 'cockpit_e2e');
    assert.equal(joinRequest.to, 'vehicle_e2e');

    cockpit.ws.send(
      JSON.stringify({
        type: 'offer',
        from: 'cockpit_e2e',
        to: 'vehicle_e2e',
        sdp: 'v=0-offer',
      })
    );

    const offer = await vehicle.waitMessage((m) => m.type === 'offer');
    assert.equal(offer.sdp, 'v=0-offer');

    vehicle.ws.send(
      JSON.stringify({
        type: 'answer',
        from: 'vehicle_e2e',
        to: 'cockpit_e2e',
        sdp: 'v=0-answer',
      })
    );

    const answer = await cockpit.waitMessage((m) => m.type === 'answer');
    assert.equal(answer.sdp, 'v=0-answer');

    cockpit.ws.send(
      JSON.stringify({
        type: 'candidate',
        from: 'cockpit_e2e',
        to: 'vehicle_e2e',
        candidate: 'candidate:xyz',
        sdpMid: '0',
        sdpMlineIndex: 7,
      })
    );

    const candidate = await vehicle.waitMessage((m) => m.type === 'candidate');
    assert.equal(candidate.candidate, 'candidate:xyz');
    assert.equal(candidate.sdpMlineIndex, 7);
    assert.equal(candidate.sdpMLineIndex, 7);

    cockpit.ws.send(
      JSON.stringify({
        type: 'offer',
        from: 'cockpit_e2e',
        to: 'vehicle_missing',
        sdp: 'offline-offer',
      })
    );

    const offlineError = await cockpit.waitMessage(
      (m) => m.type === 'error' && /not found|offline/i.test(m.message || '')
    );
    assert.match(offlineError.message, /not found|offline/i);

    cockpit.ws.send('{bad json');
    const parseError = await cockpit.waitMessage(
      (m) => m.type === 'error' && /Failed to parse message/i.test(m.message || '')
    );
    assert.match(parseError.message, /Failed to parse message/i);

    cockpit.ws.send(
      JSON.stringify({
        type: 'leave',
        from: 'cockpit_e2e',
        reason: 'manual-leave',
      })
    );

    const leaveMessage = await vehicle.waitMessage((m) => m.type === 'leave');
    assert.equal(leaveMessage.from, 'cockpit_e2e');
    assert.equal(leaveMessage.reason, 'manual-leave');

    cockpit.ws.send(
      JSON.stringify({
        type: 'join',
        from: 'cockpit_e2e',
        data: { targetVehicleId: 'vehicle_e2e' },
      })
    );

    await vehicle.waitMessage((m) => m.type === 'join_request');
    vehicle.close();

    const disconnectLeave = await cockpit.waitMessage(
      (m) => m.type === 'leave' && m.from === 'vehicle_e2e'
    );
    assert.match(disconnectLeave.reason, /Peer disconnected/i);

    invalid = createWsClient(port, null);
    await invalid.waitOpen();

    const authError = await invalid.waitMessage(
      (m) => m.type === 'error' && /Authentication failed/i.test(m.message || '')
    );
    assert.match(authError.message, /Authentication failed/i);
    await invalid.waitClose();
  } finally {
    if (cockpit) {
      cockpit.close();
    }
    if (vehicle) {
      vehicle.close();
    }
    if (invalid) {
      invalid.close();
    }
    await stopServer(server);
  }
});
