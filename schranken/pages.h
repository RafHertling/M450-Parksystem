// Weboberflaeche der Schranken. Statt einer Kamera wird das Kennzeichen eingetippt.
#pragma once

static const char SCHRANKEN_HTML[] = R"rawliteral(<!doctype html>
<html lang="de">
<head>
<meta charset="utf-8">
<meta name="viewport" content="width=device-width, initial-scale=1">
<title>Parkhaus Schranken</title>
<style>
  :root {
    --blue: #0b3d91; --blue-dark: #082a66; --ink: #16202e; --muted: #5b6676;
    --line: #d9dee6; --bg: #eef1f6; --card: #ffffff;
    --ok: #17803d; --ok-bg: #e5f5ea; --err: #b42318; --err-bg: #fdeceb;
    --warn: #8a5a00; --warn-bg: #fff4d6; --amber: #f5b400;
  }
  * { box-sizing: border-box; }
  body { margin: 0; font-family: system-ui, -apple-system, "Segoe UI", Roboto, sans-serif;
         background: var(--bg); color: var(--ink); }
  header { background: var(--blue); color: #fff; display: flex; align-items: center; gap: 14px;
           padding: 12px 20px; flex-wrap: wrap; }
  .logo { width: 44px; height: 44px; border-radius: 8px; background: #fff; color: var(--blue);
          display: grid; place-items: center; font-weight: 800; font-size: 30px; flex: none; }
  header h1 { margin: 0; font-size: 20px; line-height: 1.1; }
  header small { display: block; opacity: .8; font-size: 13px; margin-top: 2px; }
  .spacer { flex: 1; }
  .chip { background: rgba(255,255,255,.14); border-radius: 999px; padding: 6px 12px; font-size: 13px; white-space: nowrap; }
  .chip b { font-variant-numeric: tabular-nums; }
  .dot::before { content: ""; display: inline-block; width: 9px; height: 9px; border-radius: 50%;
          margin-right: 7px; background: #b8c2d4; }
  .dot.ok::before { background: #5be08a; } .dot.bad::before { background: #ff8a80; }
  main { max-width: 1040px; margin: 20px auto; padding: 0 16px 40px; display: grid; gap: 18px;
         grid-template-columns: repeat(2, minmax(0, 1fr)); }
  @media (max-width: 800px) { main { grid-template-columns: minmax(0, 1fr); } }
  section { background: var(--card); border: 1px solid var(--line); border-radius: 12px; padding: 18px; }
  section h2 { margin: 0 0 4px; font-size: 18px; }
  section .sub { margin: 0 0 12px; color: var(--muted); font-size: 13px; }
  .full { grid-column: 1 / -1; }
  .gate { display: flex; align-items: center; gap: 12px; margin: 6px 0 14px; }
  .gate svg { width: 170px; height: 78px; flex: none; }
  .arm { transform-box: view-box; transform-origin: 38px 52px; transition: transform .6s ease-in-out; }
  .gate[data-state=open] .arm { transform: rotate(-78deg); }
  .state { font-weight: 700; padding: 3px 12px; border-radius: 999px; font-size: 14px; }
  .gate[data-state=open] .state { background: var(--ok-bg); color: var(--ok); }
  .gate[data-state=closed] .state { background: #e9edf5; color: var(--ink); }
  form.row { display: flex; gap: 10px; flex-wrap: wrap; }
  label.sr { position: absolute; left: -9999px; }
  input[type=text] { font: inherit; font-size: 18px; padding: 10px 12px; border: 2px solid var(--line);
          border-radius: 8px; min-width: 0; flex: 1 1 160px; text-transform: uppercase; background: #fff; }
  input::placeholder { text-transform: none; color: #8a94a6; }
  input:focus-visible, button:focus-visible { outline: 3px solid var(--amber); outline-offset: 1px; }
  button { font: inherit; font-weight: 600; padding: 10px 16px; border-radius: 8px; border: 0; cursor: pointer;
           background: var(--blue); color: #fff; }
  button:hover { background: var(--blue-dark); }
  button:disabled { opacity: .55; cursor: default; }
  button.ghost { background: #e9edf5; color: var(--ink); } button.ghost:hover { background: #dbe2ee; }
  .msg { margin-top: 12px; padding: 10px 12px; border-radius: 8px; font-size: 15px; min-height: 44px;
         background: #f3f5f9; color: var(--muted); }
  .msg.ok { background: var(--ok-bg); color: var(--ok); }
  .msg.err { background: var(--err-bg); color: var(--err); }
  .msg.warn { background: var(--warn-bg); color: var(--warn); }
  .msg .t { display: block; font-size: 12px; opacity: .75; margin-top: 3px; }
  input.host { text-transform: none; font-size: 16px; flex: 0 1 220px; }
  .hint { color: var(--muted); font-size: 13px; margin: 8px 0 0; }
</style>
</head>
<body>
<header>
  <div class="logo">P</div>
  <div>
    <h1>Schranken</h1>
    <small>Parkhaus – Pico 2 (Kamera nachgemacht)</small>
  </div>
  <div class="spacer"></div>
  <div class="chip">Zeit Pico: <b id="clock">–</b></div>
  <div class="chip dot" id="kasseChip">Kasse: unbekannt</div>
</header>

<main>
  <section id="entrySection">
    <h2>Einfahrt</h2>
    <p class="sub">Kamera an der Einfahrtsschranke</p>
    <div class="gate" id="entryGate" data-state="closed">
      <svg viewBox="0 0 170 78" aria-hidden="true">
        <defs>
          <pattern id="stripeE" width="16" height="16" patternUnits="userSpaceOnUse" patternTransform="rotate(45)">
            <rect width="8" height="16" fill="#d92d20"/><rect x="8" width="8" height="16" fill="#fff"/>
          </pattern>
        </defs>
        <rect x="0" y="66" width="170" height="6" fill="#c9d0dc"/>
        <rect class="arm" x="38" y="46" width="122" height="12" rx="3" fill="url(#stripeE)" stroke="#16202e" stroke-width="1.5"/>
        <rect x="26" y="30" width="24" height="38" rx="4" fill="#0b3d91"/>
        <circle cx="38" cy="52" r="5" fill="#f5b400"/>
      </svg>
      <span class="state" id="entryState">Schranke geschlossen</span>
    </div>
    <form class="row" id="entryForm" autocomplete="off">
      <label class="sr" for="entryPlate">Kennzeichen Einfahrt</label>
      <input type="text" id="entryPlate" placeholder="Kennzeichen, z. B. TI 2468" maxlength="40" autofocus>
      <button type="submit" id="entryBtn">Einfahrt auslösen</button>
    </form>
    <div class="msg" id="entryMsg" role="status" aria-live="polite">Noch keine Eingabe.</div>
  </section>

  <section id="exitSection">
    <h2>Ausfahrt</h2>
    <p class="sub">Kamera an der Ausfahrtsschranke</p>
    <div class="gate" id="exitGate" data-state="closed">
      <svg viewBox="0 0 170 78" aria-hidden="true">
        <defs>
          <pattern id="stripeX" width="16" height="16" patternUnits="userSpaceOnUse" patternTransform="rotate(45)">
            <rect width="8" height="16" fill="#d92d20"/><rect x="8" width="8" height="16" fill="#fff"/>
          </pattern>
        </defs>
        <rect x="0" y="66" width="170" height="6" fill="#c9d0dc"/>
        <rect class="arm" x="38" y="46" width="122" height="12" rx="3" fill="url(#stripeX)" stroke="#16202e" stroke-width="1.5"/>
        <rect x="26" y="30" width="24" height="38" rx="4" fill="#0b3d91"/>
        <circle cx="38" cy="52" r="5" fill="#f5b400"/>
      </svg>
      <span class="state" id="exitState">Schranke geschlossen</span>
    </div>
    <form class="row" id="exitForm" autocomplete="off">
      <label class="sr" for="exitPlate">Kennzeichen Ausfahrt</label>
      <input type="text" id="exitPlate" placeholder="Kennzeichen, z. B. TI 2468" maxlength="40">
      <button type="submit" id="exitBtn">Ausfahrt auslösen</button>
    </form>
    <div class="msg" id="exitMsg" role="status" aria-live="polite">Noch keine Eingabe.</div>
  </section>

  <section class="full">
    <h2>Verbindung zur Kasse</h2>
    <p class="sub">Die Schranken fragen bei jeder Ein- und Ausfahrt die Kasse (Pico 1). Antwortet sie nicht, bleiben beide Schranken zu.</p>
    <form class="row" id="cfgForm" autocomplete="off">
      <label class="sr" for="kasseHost">IP-Adresse der Kasse</label>
      <input type="text" class="host" id="kasseHost" placeholder="IP der Kasse, z. B. 192.168.1.50" maxlength="40">
      <button type="submit" class="ghost" id="cfgBtn">Speichern</button>
      <button type="button" class="ghost" id="pingBtn">Verbindung testen</button>
    </form>
    <div class="msg" id="cfgMsg" role="status" aria-live="polite">–</div>
  </section>
</main>

<script>
(function () {
  'use strict';
  var $ = function (id) { return document.getElementById(id); };
  var skew = 0, haveClock = false;

  function fmtClock(ms) {
    try { return new Date(ms).toLocaleTimeString('de-CH', { timeZone: 'Europe/Zurich', hour12: false }); }
    catch (e) { return new Date(ms).toISOString().substr(11, 8); }
  }
  setInterval(function () { if (haveClock) $('clock').textContent = fmtClock(Date.now() + skew); }, 1000);

  // Nur eine Anfrage gleichzeitig: wartet der Pico auf die Kasse, gehen ihm sonst die
  // Verbindungen aus und er bricht die laufende Anfrage ab.
  var busy = 0;
  function api(path, timeoutMs) {
    var ctl = new AbortController();
    var timer = setTimeout(function () { ctl.abort(); }, timeoutMs || 12000);
    busy++;
    return fetch(path, { method: 'POST', cache: 'no-store', signal: ctl.signal })
      .then(function (r) { clearTimeout(timer); busy--; return r.json(); })
      .catch(function (e) { clearTimeout(timer); busy--; throw e; });
  }
  function showMsg(el, kind, text, extra) {
    el.className = 'msg ' + kind;
    el.textContent = text;
    if (extra) {
      var t = document.createElement('span');
      t.className = 't';
      t.textContent = extra;
      el.appendChild(t);
    }
  }

  function setGate(gateId, stateId, open) {
    var g = $(gateId);
    g.setAttribute('data-state', open ? 'open' : 'closed');
    $(stateId).textContent = open ? 'Schranke offen' : 'Schranke geschlossen';
  }

  var kasseNames = { ok: 'Kasse: verbunden', error: 'Kasse: Verbindungsfehler', unknown: 'Kasse: unbekannt' };
  var pollFails = 0, polling = false;
  function poll() {
    if (busy > 0 || polling) return;
    polling = true;
    var ctl = new AbortController();
    setTimeout(function () { ctl.abort(); }, 5000);
    fetch('/api/state', { cache: 'no-store', signal: ctl.signal }).then(function (r) { return r.json(); }).then(function (s) {
      polling = false;
      pollFails = 0;
      setGate('entryGate', 'entryState', s.entryOpen);
      setGate('exitGate', 'exitState', s.exitOpen);
      skew = s.now * 1000 - Date.now();
      haveClock = true;
      $('clock').textContent = fmtClock(Date.now() + skew);
      var chip = $('kasseChip');
      chip.className = 'chip dot ' + (s.kasseState === 'ok' ? 'ok' : (s.kasseState === 'error' ? 'bad' : ''));
      chip.textContent = kasseNames[s.kasseState] + ' (' + s.kasse + ')';
      if (document.activeElement !== $('kasseHost') && !$('kasseHost').getAttribute('data-touched')) $('kasseHost').value = s.kasse;
    }).catch(function () {
      polling = false;
      pollFails++;
      if (pollFails >= 2) {
        var chip = $('kasseChip');
        chip.className = 'chip dot bad';
        chip.textContent = 'Schranken-Pico nicht erreichbar';
      }
    });
  }
  setInterval(poll, 500);
  poll();

  function wire(kind, formId, plateId, btnId, msgId, gateId, stateId) {
    $(formId).addEventListener('submit', function (ev) {
      ev.preventDefault();
      var btn = $(btnId), plate = $(plateId).value, t0 = Date.now();
      btn.disabled = true;
      showMsg($(msgId), '', 'Kennzeichen wird geprüft …');
      api('/api/' + kind + '?plate=' + encodeURIComponent(plate)).then(function (r) {
        var secs = ((Date.now() - t0) / 1000).toFixed(2).replace('.', ',') + ' s';
        var cls = r.ok ? 'ok' : (r.code === 'connection' ? 'warn' : 'err');
        var note = r.ok ? ' · Schranke geöffnet'
          : (r.barrier === 'open' ? ' · abgewiesen (Schranke ist noch vom vorherigen Fahrzeug offen)' : ' · Schranke bleibt geschlossen');
        showMsg($(msgId), cls, r.message, 'Antwort nach ' + secs + note);
        if (r.ok) setGate(gateId, stateId, true);
        btn.disabled = false;
        poll();
      }).catch(function () {
        showMsg($(msgId), 'warn', 'Verbindungsfehler: Der Schranken-Pico antwortet nicht.');
        btn.disabled = false;
      });
    });
  }
  wire('entry', 'entryForm', 'entryPlate', 'entryBtn', 'entryMsg', 'entryGate', 'entryState');
  wire('exit', 'exitForm', 'exitPlate', 'exitBtn', 'exitMsg', 'exitGate', 'exitState');

  $('kasseHost').addEventListener('input', function () { this.setAttribute('data-touched', '1'); });
  $('cfgForm').addEventListener('submit', function (ev) {
    ev.preventDefault();
    api('/api/config?kasse=' + encodeURIComponent($('kasseHost').value.trim()), 6000).then(function (r) {
      showMsg($('cfgMsg'), r.ok ? 'ok' : 'err', r.message);
      if (r.ok) $('kasseHost').removeAttribute('data-touched');
      poll();
    }).catch(function () { showMsg($('cfgMsg'), 'warn', 'Verbindungsfehler: Der Schranken-Pico antwortet nicht.'); });
  });
  $('pingBtn').addEventListener('click', function () {
    var b = this;
    b.disabled = true;
    showMsg($('cfgMsg'), '', 'Kasse wird gefragt …');
    api('/api/kasse', 15000).then(function (r) {
      showMsg($('cfgMsg'), r.ok ? 'ok' : 'warn', r.message);
      b.disabled = false;
      poll();
    }).catch(function () {
      showMsg($('cfgMsg'), 'warn', 'Verbindungsfehler: Der Schranken-Pico antwortet nicht.');
      b.disabled = false;
    });
  });
})();
</script>
</body>
</html>
)rawliteral";
