// pages.h - Weboberflaeche der Kasse. Dieselbe Seite dient als
//   Kasse   (http://<IP-Kasse>/)        und als
//   Automat (http://<IP-Kasse>/automat)  - der Automat ist die gleiche Oberflaeche im zweiten Fenster.
#pragma once

static const char KASSE_HTML[] = R"rawliteral(<!doctype html>
<html lang="de">
<head>
<meta charset="utf-8">
<meta name="viewport" content="width=device-width, initial-scale=1">
<title>Parkhaus Kasse</title>
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
  .chip { background: rgba(255,255,255,.14); border-radius: 999px; padding: 6px 12px; font-size: 13px;
          white-space: nowrap; }
  .chip b { font-variant-numeric: tabular-nums; }
  #conn.ok::before, #conn.bad::before { content: ""; display: inline-block; width: 9px; height: 9px;
          border-radius: 50%; margin-right: 7px; background: #5be08a; }
  #conn.bad::before { background: #ff8a80; }
  main { max-width: 1040px; margin: 20px auto; padding: 0 16px 40px; display: grid; gap: 18px;
         grid-template-columns: minmax(0, 1fr); }
  body.kasse main { grid-template-columns: minmax(0, 1.05fr) minmax(0, 1fr); }
  @media (max-width: 860px) { body.kasse main { grid-template-columns: minmax(0, 1fr); } }
  section { background: var(--card); border: 1px solid var(--line); border-radius: 12px; padding: 18px; min-width: 0; }
  .tablewrap { overflow-x: auto; }
  td.num, td.nowrap { white-space: nowrap; }
  @media (max-width: 520px) {
    th, td { padding: 6px 5px; font-size: 13px; }
    th:nth-child(3), td:nth-child(3) { display: none; }  /* Minuten-Spalte auf dem Handy ausblenden */
    section { padding: 14px; }
  }
  section h2 { margin: 0 0 12px; font-size: 16px; }
  .full { grid-column: 1 / -1; }
  form.row { display: flex; gap: 10px; flex-wrap: wrap; }
  label.sr { position: absolute; left: -9999px; }
  input[type=text], input[type=number] { font: inherit; font-size: 18px; padding: 10px 12px; border: 2px solid var(--line);
          border-radius: 8px; min-width: 0; flex: 1 1 180px; text-transform: uppercase; background: #fff; }
  input[type=number] { text-transform: none; }
  input::placeholder { text-transform: none; color: #8a94a6; }
  input:focus-visible, button:focus-visible { outline: 3px solid var(--amber); outline-offset: 1px; }
  button { font: inherit; font-weight: 600; padding: 10px 16px; border-radius: 8px; border: 0; cursor: pointer;
           background: var(--blue); color: #fff; }
  button:hover { background: var(--blue-dark); }
  button:disabled { opacity: .5; cursor: default; }
  button.pay { background: var(--ok); } button.pay:hover { background: #12662f; }
  button.ghost { background: #e9edf5; color: var(--ink); } button.ghost:hover { background: #dbe2ee; }
  .msg { margin-top: 12px; padding: 10px 12px; border-radius: 8px; font-size: 15px; display: none; }
  .msg.show { display: block; }
  .msg.ok { background: var(--ok-bg); color: var(--ok); }
  .msg.err { background: var(--err-bg); color: var(--err); }
  .msg.warn { background: var(--warn-bg); color: var(--warn); }
  .card { margin-top: 14px; border: 1px solid var(--line); border-radius: 10px; padding: 14px; display: none; }
  .card.show { display: block; }
  .card .plate { display: inline-block; font-weight: 800; font-size: 24px; letter-spacing: 1px; padding: 2px 12px;
          border: 3px solid var(--ink); border-radius: 8px; background: #fff; }
  .card dl { display: grid; grid-template-columns: auto 1fr; gap: 6px 16px; margin: 12px 0; }
  .card dt { color: var(--muted); } .card dd { margin: 0; font-variant-numeric: tabular-nums; }
  .amount { font-size: 30px; font-weight: 800; font-variant-numeric: tabular-nums; }
  .badge { display: inline-block; padding: 2px 10px; border-radius: 999px; font-size: 13px; font-weight: 700; }
  .badge.paid { background: var(--ok-bg); color: var(--ok); }
  .badge.open { background: var(--warn-bg); color: var(--warn); }
  table { width: 100%; border-collapse: collapse; font-size: 14px; }
  th, td { text-align: left; padding: 7px 8px; border-bottom: 1px solid var(--line); font-variant-numeric: tabular-nums; }
  th { color: var(--muted); font-weight: 600; font-size: 12px; text-transform: uppercase; letter-spacing: .04em; }
  td.num, th.num { text-align: right; }
  .empty { color: var(--muted); padding: 10px 0; }
  .hint { color: var(--muted); font-size: 13px; margin: 8px 0 0; }
  .test { border: 2px dashed var(--amber); background: #fffaf0; }
  .test h2::before { content: "TEST "; background: var(--amber); color: #3b2a00; border-radius: 4px; padding: 1px 6px;
          font-size: 12px; margin-right: 6px; }
  body.automat main { max-width: 640px; }
  body.automat .amount { font-size: 42px; }
</style>
</head>
<body>
<header>
  <div class="logo">P</div>
  <div>
    <h1 id="title">Kasse</h1>
    <small id="subtitle">Parkhaus – Pico 1</small>
  </div>
  <div class="spacer"></div>
  <div class="chip">Zeit Pico: <b id="clock">–</b></div>
  <div class="chip" id="conn">Verbinde …</div>
</header>

<main>
  <section id="paySection">
    <h2 id="payTitle">Gebühr abfragen und bezahlen</h2>
    <form class="row" id="feeForm" autocomplete="off">
      <label class="sr" for="plate">Kennzeichen</label>
      <input type="text" id="plate" placeholder="Kennzeichen, z. B. TI 2468" maxlength="40" autofocus>
      <button type="submit" id="btnFee">Gebühr abfragen</button>
    </form>
    <div class="msg" id="msg" role="status" aria-live="polite"></div>
    <div class="card" id="card" aria-live="polite">
      <span class="plate" id="cPlate"></span>
      <dl>
        <dt>Einfahrt</dt><dd id="cEntry"></dd>
        <dt>Aufenthalt</dt><dd id="cMin"></dd>
        <dt>Status</dt><dd id="cStatus"></dd>
        <dt id="cFeeLabel">Gebühr</dt><dd class="amount" id="cFee"></dd>
        <dt>Bezahlter Betrag</dt><dd id="cPaid"></dd>
        <dt>Offener Betrag</dt><dd id="cOpen"></dd>
      </dl>
      <button type="button" class="pay" id="btnPay">Zahlung bestätigen</button>
    </div>
  </section>

  <section id="listSection">
    <h2>Fahrzeuge im Parkhaus <span id="count" class="badge open" style="background:#e9edf5;color:#16202e">0</span></h2>
    <div id="listWrap"></div>
    <p class="hint">Die Liste aktualisiert sich alle 2 Sekunden. Der Automat (<a href="/automat" id="autoLink">/automat</a>) ist dieselbe Oberfläche in einem zweiten Fenster.</p>
  </section>

  <section id="testSection" class="full test" style="display:none">
    <h2>Testfeld (nur für Tests)</h2>
    <form class="row" id="tfForm" autocomplete="off">
      <label class="sr" for="tfPlate">Kennzeichen</label>
      <input type="text" id="tfPlate" placeholder="Kennzeichen" maxlength="40">
      <label class="sr" for="tfMinutes">Minuten</label>
      <input type="number" id="tfMinutes" placeholder="Minuten" min="0" max="99999" step="1">
      <button type="submit" id="tfBtn" class="ghost">Aufenthalt setzen</button>
    </form>
    <div class="msg" id="tfMsg" role="status" aria-live="polite"></div>
    <p class="hint">Setzt die Einfahrtszeit auf «jetzt minus Minuten». Danach innert 30 Sekunden abfragen, sonst läuft die Zeit weiter und die Gebühr kann springen. Nach der Abnahme wird das Testfeld entfernt (TESTFIELD_ENABLED in config.h).</p>
  </section>
</main>

<script>
(function () {
  'use strict';
  var MODE = location.pathname.indexOf('/automat') === 0 ? 'automat' : 'kasse';
  var $ = function (id) { return document.getElementById(id); };
  document.body.className = MODE;
  if (MODE === 'automat') {
    document.title = 'Parkhaus Automat';
    $('title').textContent = 'Parkautomat';
    $('subtitle').textContent = 'Kennzeichen eingeben, Gebühr prüfen, bezahlen';
    $('payTitle').textContent = 'Parkgebühr bezahlen';
    $('btnPay').textContent = 'Jetzt bezahlen';
    $('listSection').style.display = 'none';
  } else {
    document.title = 'Parkhaus Kasse';
  }

  var skew = 0;          // Zeit Pico minus Zeit Browser (ms)
  var haveClock = false;
  var current = null;    // zuletzt angezeigte Abfrage
  var busy = false;

  function chf(r) { return 'CHF ' + Math.floor(r / 100) + '.' + ('0' + (r % 100)).slice(-2); }
  function fmtTime(epoch) {
    try {
      return new Date(epoch * 1000).toLocaleString('de-CH', { timeZone: 'Europe/Zurich',
        day: '2-digit', month: '2-digit', year: 'numeric', hour: '2-digit', minute: '2-digit', second: '2-digit' });
    } catch (e) { return new Date(epoch * 1000).toISOString(); }
  }
  function fmtShort(epoch) {
    try {
      return new Date(epoch * 1000).toLocaleString('de-CH', { timeZone: 'Europe/Zurich',
        day: '2-digit', month: '2-digit', hour: '2-digit', minute: '2-digit', second: '2-digit' });
    } catch (e) { return fmtTime(epoch); }
  }
  function fmtClock(ms) {
    try {
      return new Date(ms).toLocaleTimeString('de-CH', { timeZone: 'Europe/Zurich', hour12: false });
    } catch (e) { return new Date(ms).toISOString().substr(11, 8); }
  }
  function api(path, method) {
    var ctl = new AbortController();
    var timer = setTimeout(function () { ctl.abort(); }, 8000);
    return fetch(path, { method: method || 'GET', cache: 'no-store', signal: ctl.signal })
      .then(function (r) { clearTimeout(timer); return r.json(); })
      .catch(function (e) { clearTimeout(timer); throw e; });
  }
  function setConn(ok) {
    var c = $('conn');
    c.className = 'chip ' + (ok ? 'ok' : 'bad');
    c.textContent = ok ? (MODE === 'automat' ? 'Automat verbunden' : 'Kasse verbunden') : 'Keine Verbindung zur Kasse';
  }
  function setClock(nowEpoch) {
    skew = nowEpoch * 1000 - Date.now();
    haveClock = true;
    $('clock').textContent = fmtClock(Date.now() + skew);
  }
  setInterval(function () { if (haveClock) $('clock').textContent = fmtClock(Date.now() + skew); }, 1000);

  function showMsg(el, kind, text) {
    el.className = 'msg show ' + kind;
    el.textContent = text;
  }
  function hideMsg(el) { el.className = 'msg'; el.textContent = ''; }

  function renderCard(d) {
    current = d;
    $('cPlate').textContent = d.plate;
    $('cEntry').textContent = fmtTime(d.entry);
    $('cMin').textContent = d.minutes + ' Min.';
    var st = $('cStatus');
    st.innerHTML = '';
    var b = document.createElement('span');
    b.className = 'badge ' + (d.paid ? 'paid' : 'open');
    b.textContent = d.paid ? 'bezahlt' : 'offen';
    st.appendChild(b);
    $('cFee').textContent = chf(d.paid ? d.paidAmount : d.fee);
    $('cPaid').textContent = d.paid ? chf(d.paidAmount) : '–';
    $('cOpen').textContent = chf(d.open);
    var pay = $('btnPay');
    pay.style.display = d.paid ? 'none' : '';
    pay.disabled = false;
    $('card').className = 'card show';
  }
  function hideCard() { current = null; $('card').className = 'card'; }

  function query(keepMessage) {
    var raw = $('plate').value;
    return api('/api/fee?plate=' + encodeURIComponent(raw)).then(function (d) {
      setConn(true);
      if (!d.ok) {
        hideCard();
        var t = d.message;
        if (d.code === 'unknown') t += ' Keine Zahlung möglich.';
        showMsg($('msg'), 'err', t);
      } else {
        if (!keepMessage) hideMsg($('msg'));
        renderCard(d);
      }
      refreshList();
    }).catch(function () {
      setConn(false);
      hideCard();
      showMsg($('msg'), 'warn', 'Verbindungsfehler: Die Kasse antwortet nicht.');
    });
  }

  $('feeForm').addEventListener('submit', function (ev) {
    ev.preventDefault();
    hideCard();
    hideMsg($('msg'));
    query(false);
  });

  $('btnPay').addEventListener('click', function () {
    if (!current || current.paid || busy) return;
    busy = true;
    $('btnPay').disabled = true;
    var d = current;
    api('/api/pay?plate=' + encodeURIComponent(d.plate) + '&amount=' + d.fee, 'POST').then(function (r) {
      setConn(true);
      busy = false;
      if (r.ok) {
        $('plate').value = d.plate;
        return query(true).then(function () { showMsg($('msg'), 'ok', r.message); });
      }
      $('plate').value = d.plate;
      return query(true).then(function () { showMsg($('msg'), 'err', r.message); });
    }).catch(function () {
      busy = false;
      setConn(false);
      $('btnPay').disabled = false;
      showMsg($('msg'), 'warn', 'Verbindungsfehler: Die Kasse antwortet nicht – Zahlung nicht bestätigt.');
    });
  });

  function renderList(d) {
    $('count').textContent = d.count;
    var wrap = $('listWrap');
    wrap.innerHTML = '';
    if (!d.vehicles.length) {
      var e = document.createElement('div');
      e.className = 'empty';
      e.textContent = 'Keine Fahrzeuge im Parkhaus.';
      wrap.appendChild(e);
      return;
    }
    var t = document.createElement('table');
    t.id = 'list';
    t.innerHTML = '<thead><tr><th>Kennzeichen</th><th>Einfahrt</th><th class="num">Min.</th><th class="num">Gebühr</th><th>Status</th></tr></thead>';
    var tb = document.createElement('tbody');
    d.vehicles.forEach(function (v) {
      var tr = document.createElement('tr');
      tr.setAttribute('data-plate', v.plate);
      function td(txt, cls) { var c = document.createElement('td'); c.textContent = txt; if (cls) c.className = cls; tr.appendChild(c); return c; }
      td(v.plate).style.fontWeight = '700';
      td(fmtShort(v.entry), 'nowrap');
      td(String(v.minutes), 'num');
      td(chf(v.paid ? v.paidAmount : v.fee), 'num');
      var s = td(v.paid ? 'bezahlt' : 'offen');
      s.className = 'status-' + (v.paid ? 'paid' : 'open');
      tb.appendChild(tr);
    });
    t.appendChild(tb);
    var tw = document.createElement('div');
    tw.className = 'tablewrap';
    tw.appendChild(t);
    wrap.appendChild(tw);
  }
  function refreshList() {
    if (MODE !== 'kasse') return Promise.resolve();
    return api('/api/list').then(function (d) { setConn(true); setClock(d.now); renderList(d); })
      .catch(function () { setConn(false); });
  }
  function pingLoop() {
    api('/api/ping').then(function (d) {
      setConn(true); setClock(d.now);
      if (MODE === 'kasse') $('testSection').style.display = d.testfield ? '' : 'none';
    }).catch(function () { setConn(false); });
  }

  // Testfeld
  $('tfForm').addEventListener('submit', function (ev) {
    ev.preventDefault();
    var p = $('tfPlate').value, m = $('tfMinutes').value;
    api('/api/testfield?plate=' + encodeURIComponent(p) + '&minutes=' + encodeURIComponent(m), 'POST').then(function (r) {
      setConn(true);
      showMsg($('tfMsg'), r.ok ? 'ok' : 'err', r.message);
      refreshList();
    }).catch(function () {
      setConn(false);
      showMsg($('tfMsg'), 'warn', 'Verbindungsfehler: Die Kasse antwortet nicht.');
    });
  });

  pingLoop();
  refreshList();
  setInterval(refreshList, 2000);
  setInterval(pingLoop, 5000);
})();
</script>
</body>
</html>
)rawliteral";
