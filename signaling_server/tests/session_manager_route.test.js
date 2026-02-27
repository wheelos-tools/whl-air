const test = require('node:test');
const assert = require('node:assert/strict');

function createFakeWs() {
  return {
    OPEN: 1,
    readyState: 1,
    sent: [],
    send(payload) {
      this.sent.push(JSON.parse(payload));
    },
  };
}

function loadSessionManagerFresh() {
  const modulePath = require.resolve('../src/session_manager');
  delete require.cache[modulePath];
  return require('../src/session_manager');
}

test('routes envelope signaling payload and normalizes candidate m-line key', () => {
  const sessionManager = loadSessionManagerFresh();

  const sender = createFakeWs();
  const recipient = createFakeWs();

  sessionManager.addClient(sender, 'cockpit_a');
  sessionManager.addClient(recipient, 'vehicle_a');

  sessionManager.routeMessage(sender, {
    type: 'signaling',
    from: 'cockpit_a',
    to: 'vehicle_a',
    payload: {
      type: 'candidate',
      candidate: 'candidate:abc',
      sdpMid: '0',
      sdpMlineIndex: 4,
    },
  });

  assert.equal(recipient.sent.length, 1);
  assert.equal(recipient.sent[0].type, 'candidate');
  assert.equal(recipient.sent[0].from, 'cockpit_a');
  assert.equal(recipient.sent[0].to, 'vehicle_a');
  assert.equal(recipient.sent[0].candidate, 'candidate:abc');
  assert.equal(recipient.sent[0].sdpMlineIndex, 4);
  assert.equal(recipient.sent[0].sdpMLineIndex, 4);
});

test('routes direct candidate payload preserving compatibility keys', () => {
  const sessionManager = loadSessionManagerFresh();

  const sender = createFakeWs();
  const recipient = createFakeWs();

  sessionManager.addClient(sender, 'cockpit_b');
  sessionManager.addClient(recipient, 'vehicle_b');

  sessionManager.routeMessage(sender, {
    type: 'candidate',
    from: 'cockpit_b',
    to: 'vehicle_b',
    candidate: 'candidate:def',
    sdpMid: '1',
    sdpMlineIndex: 9,
  });

  assert.equal(recipient.sent.length, 1);
  assert.equal(recipient.sent[0].type, 'candidate');
  assert.equal(recipient.sent[0].from, 'cockpit_b');
  assert.equal(recipient.sent[0].to, 'vehicle_b');
  assert.equal(recipient.sent[0].candidate, 'candidate:def');
  assert.equal(recipient.sent[0].sdpMlineIndex, 9);
  assert.equal(recipient.sent[0].sdpMLineIndex, 9);
});
