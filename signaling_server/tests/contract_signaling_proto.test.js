const fs = require('node:fs');
const path = require('node:path');
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

function parseContractCases(content) {
  const lines = content.split(/\r?\n/);
  const cases = [];
  let current = null;

  for (const rawLine of lines) {
    const line = rawLine.trim();
    if (!line || line.startsWith('#')) {
      continue;
    }
    if (line === '[case]') {
      if (current && Object.keys(current).length > 0) {
        cases.push(current);
      }
      current = {};
      continue;
    }
    const splitAt = line.indexOf('=');
    if (splitAt < 0 || !current) {
      continue;
    }
    const key = line.slice(0, splitAt).trim();
    const value = line.slice(splitAt + 1).trim();
    current[key] = value;
  }

  if (current && Object.keys(current).length > 0) {
    cases.push(current);
  }

  return cases;
}

const repoRoot = path.resolve(__dirname, '..', '..');
const contractCasesPath = path.join(repoRoot, 'contracts', 'signaling_envelope_contract_cases.txt');
const protoPath = path.join(repoRoot, 'proto', 'remote_command.proto');
const protoRulesPath = path.join(repoRoot, 'contracts', 'proto_contract_rules.txt');

test('contract: signaling envelope compatibility cases', () => {
  const sessionManager = loadSessionManagerFresh();
  const text = fs.readFileSync(contractCasesPath, 'utf8');
  const cases = parseContractCases(text);

  assert.ok(cases.length > 0, 'contract cases should not be empty');

  for (const item of cases) {
    const sender = createFakeWs();
    const recipient = createFakeWs();

    sessionManager.addClient(sender, item.from);
    sessionManager.addClient(recipient, item.to);

    const mline = Number(item.sdp_mline_index);
    const directMessage = {
      type: 'candidate',
      from: item.from,
      to: item.to,
      candidate: item.candidate,
      sdpMid: item.sdp_mid,
      sdpMlineIndex: mline,
    };

    const message = item.shape === 'envelope'
      ? {
        type: 'signaling',
        from: item.from,
        to: item.to,
        payload: directMessage,
      }
      : directMessage;

    sessionManager.routeMessage(sender, message);

    assert.equal(recipient.sent.length, 1, `${item.name}: routed once`);
    const routed = recipient.sent[0];
    assert.equal(routed.type, 'candidate', `${item.name}: type`);
    assert.equal(routed.from, item.from, `${item.name}: from`);
    assert.equal(routed.to, item.to, `${item.name}: to`);
    assert.equal(routed.candidate, item.candidate, `${item.name}: candidate`);
    assert.equal(routed.sdpMid, item.sdp_mid, `${item.name}: sdpMid`);
    assert.equal(routed.sdpMlineIndex, mline, `${item.name}: sdpMlineIndex`);
    assert.equal(routed.sdpMLineIndex, mline, `${item.name}: sdpMLineIndex compat`);
  }
});

test('contract: proto schema required snippets are present', () => {
  const protoContents = fs.readFileSync(protoPath, 'utf8');
  const rules = fs
    .readFileSync(protoRulesPath, 'utf8')
    .split(/\r?\n/)
    .map((line) => line.trim())
    .filter((line) => line && !line.startsWith('#'));

  assert.ok(rules.length > 0, 'proto contract rules should not be empty');

  for (const rule of rules) {
    assert.ok(protoContents.includes(rule), `missing proto contract rule: ${rule}`);
  }
});
