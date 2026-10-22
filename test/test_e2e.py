#!/usr/bin/env python3
"""End-to-End-Test ueber echte HTTP-Verbindungen.

Startet die beiden echten Sketches (kasse.ino, schranken.ino), uebersetzt fuer den PC mit den
Ersatz-Headern in test/stubs, und spielt die 42 Testfaelle des Testkonzepts in Echtzeit ab:
  - Pico 1 (Kasse)     auf 127.0.0.2:8080
  - Pico 2 (Schranken) auf 127.0.0.3:8080
Gemessen werden auch die Zeiten (Schranke oeffnet innert 3 s, schliesst nach <= 10 s,
Verbindungsfehler innert 10 s) und das "Kabel ziehen" (Prozess anhalten bzw. beenden).
"""
import json, os, re, signal, subprocess, sys, time, urllib.parse, urllib.request

HERE = os.path.dirname(os.path.abspath(__file__))
BUILD = os.path.join(HERE, "build")
KASSE = "http://127.0.0.2:8080"
SCHR = "http://127.0.0.3:8080"

passed = failed = 0


def check(tc, cond, what=""):
    global passed, failed
    if cond:
        passed += 1
    else:
        failed += 1
        print(f"  FAIL {tc}: {what}")


def call(base, path, method="POST", timeout=15):
    t0 = time.time()
    req = urllib.request.Request(base + path, method=method, data=b"" if method == "POST" else None)
    with urllib.request.urlopen(req, timeout=timeout) as r:
        body = r.read().decode("utf-8")
        ctype = r.headers.get("Content-Type", "")
        status = r.status
    dt = time.time() - t0
    try:
        data = json.loads(body)
    except Exception:
        data = None
    return status, data, body, ctype, dt


def q(**kw):
    return "?" + urllib.parse.urlencode(kw)


def enter(plate):
    return call(SCHR, "/api/entry" + q(plate=plate))


def leave(plate):
    return call(SCHR, "/api/exit" + q(plate=plate))


def fee(plate):
    return call(KASSE, "/api/fee" + q(plate=plate))[1]


def pay(plate, amount=None):
    kw = {"plate": plate}
    if amount is not None:
        kw["amount"] = amount
    return call(KASSE, "/api/pay" + q(**kw))[1]


def setmin(plate, minutes):
    return call(KASSE, "/api/testfield" + q(plate=plate, minutes=minutes))[1]


def vehicles():
    return {v["plate"]: v for v in call(KASSE, "/api/list", "GET")[1]["vehicles"]}


class Pico:
    def __init__(self, name, ip, extra_env=None):
        self.name, self.ip = name, ip
        self.log_path = os.path.join(BUILD, f"{name}.log")
        self.proc = None

    def start(self):
        env = dict(os.environ, PICO_BIND_IP=self.ip)
        self.log = open(self.log_path, "ab")
        self.proc = subprocess.Popen([os.path.join(BUILD, f"{self.name}_host")], stdout=self.log, stderr=self.log, env=env)
        deadline = time.time() + 5
        base = f"http://{self.ip}:8080"
        while time.time() < deadline:
            try:
                call(base, "/favicon.ico", "GET", timeout=1)
                return
            except Exception:
                time.sleep(0.1)
        raise RuntimeError(f"{self.name} startet nicht")

    def stop(self, sig=signal.SIGKILL):
        if self.proc and self.proc.poll() is None:
            self.proc.send_signal(sig)
            self.proc.wait()


def servo_events(path):
    """Liest die SERVO-Zeilen: [(ms, pin, angle)]"""
    ev = []
    with open(path, errors="ignore") as f:
        for line in f:
            m = re.match(r"SERVO (\d+) (\d+) (\d+)", line)
            if m:
                ev.append(tuple(int(x) for x in m.groups()))
    return ev


def wait_closed(pin, since_count, timeout=12):
    """wartet bis das Servo an 'pin' wieder zu ist, gibt die Zeit zwischen Oeffnen und Schliessen zurueck"""
    t_end = time.time() + timeout
    while time.time() < t_end:
        ev = servo_events(schr.log_path)[since_count:]
        ev = [e for e in ev if e[1] == pin]
        if len(ev) >= 2 and ev[0][2] == 90 and ev[-1][2] == 0:
            return (ev[-1][0] - ev[0][0]) / 1000.0
        time.sleep(0.1)
    return None


def n_events():
    return len(servo_events(schr.log_path))


def servo_state(pin):
    ev = [e for e in servo_events(schr.log_path) if e[1] == pin]
    return ev[-1][2] if ev else None


kasse = Pico("kasse", "127.0.0.2")
schr = Pico("schranken", "127.0.0.3")

try:
    for p in (kasse, schr):
        if os.path.exists(p.log_path):
            os.remove(p.log_path)
        p.start()

    print("[Seiten]")
    for base, path in ((KASSE, "/"), (KASSE, "/automat"), (SCHR, "/")):
        st, _, body, ct, _ = call(base, path, "GET")
        check("PAGE", st == 200 and "text/html" in ct and "<!doctype html>" in body, f"{base}{path}")
    st, d, _, _, _ = call(KASSE, "/api/ping", "GET")
    check("PING", d["ok"] and d["timeValid"] and d["testfield"])
    st, d, _, _, _ = call(SCHR, "/api/state", "GET")
    check("STATE", d["ok"] and not d["entryOpen"] and not d["exitOpen"] and d["timeValid"])
    check("START", servo_state(8) == 0 and servo_state(9) == 0, "beide Schranken beim Start zu")

    print("[Testfaelle TC-01 bis TC-42 in Echtzeit]")
    # TC-01
    ev0 = n_events()
    st, d, _, _, dt = enter("TI 2468")
    check("TC-01", d["ok"] and d["barrier"] == "open" and dt < 3.0, f"{d} {dt:.2f}s")
    check("TC-01", servo_state(8) == 90, "Servo Einfahrt offen")
    check("TC-01", "TI 2468" in vehicles())
    # TC-02: schliesst selbst, spaetestens nach 10 s
    t = wait_closed(8, ev0)
    check("TC-02", t is not None and t <= 10.0, f"schliesst nach {t}")
    print(f"  Info: Einfahrtsschranke war {t:.1f} s offen")
    # TC-03: Einfahrtszeit vs. Uhr des Notebooks
    v = vehicles()["TI 2468"]
    check("TC-03", abs(v["entry"] - time.time()) <= 60, f"entry={v['entry']} now={time.time()}")
    # TC-04..06
    time.sleep(1.1)
    st, d, *_ = enter("VS 864209")
    check("TC-04", d["ok"])
    vs = vehicles()
    check("TC-04", {"TI 2468", "VS 864209"} <= set(vs) and vs["TI 2468"]["entry"] != vs["VS 864209"]["entry"])
    st, d, *_ = enter("FR 3")
    check("TC-05", d["ok"] and "FR 3" in vehicles())
    st, d, *_ = enter("  gr  100 ")
    check("TC-06", d["ok"] and "GR 100" in vehicles())
    # TC-07/08
    n = len(vehicles())
    ev0 = n_events()
    st, d, *_ = enter("TI 2468")
    check("TC-07", not d["ok"] and n_events() == ev0 and "schon im Parkhaus" in d["message"], str(d))
    check("TC-07", len(vehicles()) == n)
    st, d, *_ = enter("TI2468")
    check("TC-08", not d["ok"] and "schon im Parkhaus" in d["message"])
    time.sleep(5.5)  # Schranken schliessen lassen (BARRIER_OPEN_MS = 5000)
    # TC-09..14
    ev0 = n_events()
    for tc, plate, needles in [
        ("TC-09", "2468", ["Ungültiges Format", "Buchstaben fehlen"]),
        ("TC-10", "T 2468", ["Ungültiges Format", "nur ein Buchstabe"]),
        ("TC-11", "TI 7654321", ["Ungültiges Format", "mehr als sechs Ziffern"]),
        ("TC-12", "TI", ["keine Ziffern"]),
        ("TC-13", "TI-2468", ["Ungültiges Zeichen"]),
        ("TC-14", "", ["Kennzeichen fehlt"]),
    ]:
        st, d, *_ = enter(plate)
        check(tc, not d["ok"] and d["barrier"] == "closed" and all(x in d["message"] for x in needles), str(d))
    check("TC-09..14", n_events() == ev0, "kein Servo hat sich bewegt")
    check("TC-14", len(vehicles()) == n)
    # TC-15
    st, d, *_ = enter("SZ 3300")
    check("TC-15", d["ok"] and "SZ 3300" in vehicles() and len(vehicles()) == n + 1)
    # TC-16..22
    for tc, minutes, want in [("TC-16", 0, 200), ("TC-17", 60, 200), ("TC-18", 61, 350), ("TC-18", 120, 350),
                              ("TC-19", 180, 350), ("TC-20", 181, 500), ("TC-21", 240, 500),
                              ("TC-21", 241, 600), ("TC-22", 300, 600), ("TC-22", 301, 700)]:
        r = setmin("TI 2468", minutes)
        d = fee("TI 2468")
        check(tc, r["ok"] and d["fee"] == want and d["minutes"] == minutes, f"{minutes} Min -> {d}")
    d = fee("UR 8008")
    check("TC-23", not d["ok"] and "nicht im Parkhaus" in d["message"] and "fee" not in d)
    d = fee(" ti2468 ")
    check("TC-24", d["ok"] and d["plate"] == "TI 2468" and d["fee"] == 700)
    # TC-25..29
    setmin("VS 864209", 130)
    d = fee("VS 864209")
    check("TC-25", d["fee"] == 350 and not d["paid"])
    r = pay("VS 864209", d["fee"])
    check("TC-25", r["ok"] and r["paidAmount"] == 350)
    d = fee("VS 864209")
    check("TC-26", d["paid"] and d["open"] == 0 and d["paidAmount"] == 350)
    r = pay("VS 864209", 350)
    check("TC-27", not r["ok"] and r["code"] == "already_paid")
    setmin("FR 3", 200)
    d = fee("FR 3")
    check("TC-28", d["fee"] == 500)
    check("TC-28", pay("FR 3", 500)["ok"])
    d = fee("FR 3")
    check("TC-28", d["paid"] and d["open"] == 0)
    r = pay("UR 8008", 200)
    check("TC-29", not r["ok"] and "Kennzeichen unbekannt" in r["message"])
    # TC-30..35
    setmin("SZ 3300", 90)
    ev0 = n_events()
    st, d, _, _, dt = leave("SZ 3300")
    check("TC-30", not d["ok"] and d["code"] == "unpaid" and "CHF 3.50" in d["message"] and d["barrier"] == "closed", str(d))
    check("TC-30", n_events() == ev0)
    ev0 = n_events()
    st, d, _, _, dt = leave("VS 864209")
    check("TC-31", d["ok"] and dt < 3.0 and servo_state(9) == 90 and "VS 864209" not in vehicles(), str(d))
    t = wait_closed(9, ev0)
    check("TC-32", t is not None and t <= 10.0, f"{t}")
    st, d, *_ = leave("VS 864209")
    check("TC-33", not d["ok"] and "nicht im Parkhaus" in d["message"] and servo_state(9) == 0)
    st, d, *_ = leave("UR 8008")
    check("TC-34", not d["ok"] and "Kennzeichen unbekannt" in d["message"] and servo_state(9) == 0)
    ev0 = n_events()
    st, d, _, _, dt = leave("FR 3")
    check("TC-35", d["ok"] and dt < 3.0 and "FR 3" not in vehicles())
    wait_closed(9, ev0)
    # TC-36/37
    setmin("GR 100", 0)
    st, d, *_ = leave("GR 100")
    check("TC-36", not d["ok"] and "CHF 2.00" in d["message"] and servo_state(9) == 0 and "GR 100" in vehicles())
    st, d, *_ = enter("VS 864209")
    check("TC-37", d["ok"])
    setmin("VS 864209", 30)
    st, d, *_ = leave("VS 864209")
    check("TC-37", not d["ok"] and "CHF 2.00" in d["message"])
    # TC-38
    check("TC-38", set(vehicles()) == {"TI 2468", "SZ 3300", "GR 100", "VS 864209"}, str(set(vehicles())))
    time.sleep(5.5)

    # TC-39: Kassen-Pico "Kabel ziehen" -> hier: Prozess anhalten (antwortet nicht, Verbindung steht)
    setmin("VS 864209", 30)
    check("TC-39", pay("VS 864209", 200)["ok"])
    ev0 = n_events()
    os.kill(kasse.proc.pid, signal.SIGSTOP)
    st, d, _, _, dt = leave("VS 864209")
    check("TC-39", not d["ok"] and d["code"] == "connection" and "Verbindungsfehler" in d["message"] and dt <= 10.0, f"{d} {dt:.2f}s")
    check("TC-39", "Gebühr offen" not in d["message"] and d["barrier"] == "closed" and n_events() == ev0)
    print(f"  Info: Verbindungsfehler (Kasse haengt) nach {dt:.1f} s")
    st, d, _, _, dt = enter("NW 12")
    check("TC-40", not d["ok"] and d["code"] == "connection" and servo_state(8) == 0 and dt <= 10.0, f"{d} {dt:.2f}s")
    st, state, *_ = call(SCHR, "/api/state", "GET")
    check("TC-40", state["kasseState"] == "error")
    # Kasse laeuft weiter (Kabel wieder angeschlossen) -> Schranken ohne Neustart weiter bedienbar
    os.kill(kasse.proc.pid, signal.SIGCONT)
    time.sleep(0.2)
    st, d, _, _, dt = leave("VS 864209")
    check("TC-41", d["ok"] and "VS 864209" not in vehicles(), str(d))
    check("TC-41", "NW 12" not in vehicles(), "verspaetete Einfahrt NW 12 darf nicht eingetragen werden")
    wait_closed(9, ev0)

    # TC-39b/40b: Kassen-Pico ist ganz weg (Prozess beendet, Verbindung wird sofort abgelehnt)
    kasse.stop()
    st, d, _, _, dt = enter("NW 12")
    check("TC-40b", not d["ok"] and d["code"] == "connection" and servo_state(8) == 0 and dt <= 10.0, str(d))
    st, d, _, _, dt = leave("TI 2468")
    check("TC-39b", not d["ok"] and d["code"] == "connection" and servo_state(9) == 0)
    # Schranken-Pico bleibt bedienbar, auch ungueltige Eingaben werden weiter erklaert
    st, d, *_ = enter("T 1")
    check("TC-41b", not d["ok"] and "nur ein Buchstabe" in d["message"])
    # TC-42: Kassen-Pico neu starten: Daten sind weg, Schranken laufen ohne Neustart weiter
    kasse.start()
    check("TC-42", vehicles() == {}, "Daten nach Neustart leer (RAM)")
    print("  TC-42 (Info): nach Neustart der Kasse ist die Fahrzeugliste leer (Daten nur im RAM)")
    st, d, *_ = enter("NW 12")
    check("TC-41b", d["ok"] and "NW 12" in vehicles(), str(d))

    # Kasse-IP in den Schranken aendern
    st, d, *_ = call(SCHR, "/api/config" + q(kasse="127.0.0.9"))
    check("CFG", d["ok"])
    st, d, _, _, dt = call(SCHR, "/api/kasse")[0:5]
    check("CFG", not d["ok"] and "Verbindungsfehler" in d["message"])
    call(SCHR, "/api/config" + q(kasse="127.0.0.2"))
    st, d, *_ = call(SCHR, "/api/kasse")
    check("CFG", d["ok"] and d["connected"])

    # Parallele Anfragen (Browser mit mehreren Fenstern) bringen nichts durcheinander
    import concurrent.futures as cf
    plates = [f"AB {i}" for i in range(1, 21)]
    with cf.ThreadPoolExecutor(8) as ex:
        res = list(ex.map(lambda p: enter(p)[1]["ok"], plates))
    check("PARALLEL", all(res) and all(p in vehicles() for p in plates))

finally:
    kasse.stop()
    schr.stop()

print(f"\nErgebnis: {passed} bestanden, {failed} fehlgeschlagen")
sys.exit(1 if failed else 0)
