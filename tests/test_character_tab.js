// Run from the project root:  node tests/test_character_tab.js
// Tests the JavaScript logic in character_tab.html in isolation:
//   - norm() key normalisation matches what C++ NormalizePersonaName produces
//   - updatePersonaImage() stores the URL under the right key
//   - The full roundtrip: name → norm(name) key → updatePersonaImage → renderGrid lookup

'use strict';

const fs   = require('fs');
const path = require('path');
const vm   = require('vm');

// ── Minimal DOM mock ──────────────────────────────────────────────────────────

const _els = {};
function mockEl() {
    return { innerHTML: '', focus() {}, select() {}, value: '', disabled: false };
}

global.document = {
    getElementById:    (id)  => { if (!_els[id]) _els[id] = mockEl(); return _els[id]; },
    querySelectorAll:  ()    => [],
    // Do NOT fire DOMContentLoaded — it calls send({action:'ready'}) which
    // would try to postMessage before the test sets up assertions.
    addEventListener:  ()    => {},
};
global.window   = global;
global.confirm  = () => false;
// Silence the webkit bridge — we don't want real messages sent.
global.window.webkit = { messageHandlers: { characters: { postMessage: () => {} } } };

// ── Load the HTML and eval the <script> block ─────────────────────────────────

const htmlPath = path.join(__dirname, '..', 'src', 'html', 'character_tab.html');
const html     = fs.readFileSync(htmlPath, 'utf8');

const scriptMatch = html.match(/<script>([\s\S]*?)<\/script>/);
if (!scriptMatch) { console.error('ERROR: no <script> block found'); process.exit(1); }

// Evaluate in the current context so var/function declarations become globals.
vm.runInThisContext(scriptMatch[1]);

// ── Helpers ───────────────────────────────────────────────────────────────────

let failures = 0;

function check(label, cond, extra) {
    if (cond) {
        console.log('PASS [' + label + ']');
    } else {
        console.error('FAIL [' + label + ']' + (extra ? ': ' + extra : ''));
        failures++;
    }
}

// ── norm() tests ──────────────────────────────────────────────────────────────
// These cases must produce the same result as C++ NormalizePersonaName.

check('norm-basic-ascii',     norm('Albert Einstein')        === 'albert_einstein');
check('norm-multi-word',      norm('Edgar Allan Poe')        === 'edgar_allan_poe');
check('norm-extra-spaces',    norm('Albert  Einstein')       === 'albert_einstein');
check('norm-trailing-space',  norm('Darwin ').slice(-1)      !== '_');
check('norm-leading-space',   norm(' Darwin').charAt(0)      !== '_');
check('norm-chinese-mixed',
    norm('蒋勋 Taiwanese Intellectual') === '蒋勋_taiwanese_intellectual',
    'got: ' + norm('蒋勋 Taiwanese Intellectual'));
check('norm-chinese-simple',
    norm('陈文茜 Taiwanese Journalist') === '陈文茜_taiwanese_journalist',
    'got: ' + norm('陈文茜 Taiwanese Journalist'));
check('norm-pure-chinese',    norm('李白') === '李白', 'got: ' + norm('李白'));
check('norm-dash-ascii',
    norm('Richard - Feynman') === 'richard_feynman',
    'got: ' + norm('Richard - Feynman'));
check('norm-dash-chinese',
    norm('陈文茜 - Taiwanese Journalist') === '陈文茜_taiwanese_journalist',
    'got: ' + norm('陈文茜 - Taiwanese Journalist'));

// ── updatePersonaImage() round-trip ──────────────────────────────────────────

// Start with a known character set.
setCharacters({
    cats:    { Science: ['Albert Einstein'], Literature: ['Edgar Allan Poe'] },
    imgs:    {},
    checked: [],
    descs:   {}
});

check('imgs-initially-empty-einstein', !_imgs['albert_einstein']);

const FAKE_URL = 'data:image/jpeg;base64,/9j/abc123==';
updatePersonaImage('albert_einstein', FAKE_URL);
check('update-stores-url',        _imgs['albert_einstein'] === FAKE_URL);
check('update-does-not-corrupt',  !_imgs['edgar_allan_poe']);

// ── Chinese name round-trip ───────────────────────────────────────────────────
// Simulates what C++ DoUploadImage does:
//   key = NormalizePersonaName(name)  →  sent via RunScript as the first arg
//   JS renderGrid() looks up _imgs[norm(displayName)]
// The two must match.

const CHINESE_NAME = '蒋勋 Taiwanese Intellectual';
const CPP_KEY      = '蒋勋_taiwanese_intellectual'; // what C++ NormalizePersonaName produces
const JS_KEY       = norm(CHINESE_NAME);            // what renderGrid() will look up

check('cpp-js-key-match', CPP_KEY === JS_KEY,
    'C++ key: ' + CPP_KEY + '  JS key: ' + JS_KEY);

setCharacters({
    cats:    { Taiwan: [CHINESE_NAME] },
    imgs:    {},
    checked: [],
    descs:   {}
});

updatePersonaImage(CPP_KEY, FAKE_URL);
check('chinese-url-retrievable', _imgs[JS_KEY] === FAKE_URL);

// ── Summary ───────────────────────────────────────────────────────────────────

if (failures === 0) {
    console.log('\nALL PASSED');
    process.exit(0);
} else {
    console.error('\n' + failures + ' FAILURE(S)');
    process.exit(1);
}
