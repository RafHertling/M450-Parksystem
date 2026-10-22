#!/usr/bin/env python3
"""Browser-Test der drei Oberflaechen (Schranken, Kasse, Automat) mit Chromium.

Die Seiten werden von den echten Sketches ausgeliefert (siehe test_e2e.py) und wie im Testkonzept
bedient: drei Fenster nebeneinander, Eingaben von Hand, Bildschirmfotos zur Kontrolle.
"""
import os, re, signal, subprocess, sys, time

from playwright.sync_api import sync_playwright, expect

HERE = os.path.dirname(os.path.abspath(__file__))
BUILD = os.path.join(HERE, "build")
SHOTS = os.path.join(HERE, "screens")
os.makedirs(SHOTS, exist_ok=True)
KASSE = "http://127.0.0.2:8080"
SCHR = "http://127.0.0.3:8080"

passed = failed = 0


def check(name, cond, what=""):
    global passed, failed
    if cond:
        passed += 1
    else:
        failed += 1
        print(f"  FAIL {name}: {what}")


def start(name, ip):
    log = open(os.path.join(BUILD, f"{name}_ui.log"), "wb")
    p = subprocess.Popen([os.path.join(BUILD, f"{name}_host")], stdout=log, stderr=log, env=dict(os.environ, PICO_BIND_IP=ip))
    return p


procs = {"kasse": start("kasse", "127.0.0.2"), "schranken": start("schranken", "127.0.0.3")}
time.sleep(1.0)

try:
    with sync_playwright() as pw:
        browser = pw.chromium.launch()
        ctx = browser.new_context(viewport={"width": 1100, "height": 900}, locale="de-CH", timezone_id="Europe/Zurich")
        schr = ctx.new_page()
        kasse = ctx.new_page()
        auto = ctx.new_page()
        errors = []
        for name, pg in (("schranken", schr), ("kasse", kasse), ("automat", auto)):
            pg.on("pageerror", lambda e, n=name: errors.append(f"{n}: {e}"))
            pg.on("console", lambda m, n=name: errors.append(f"{n}: {m.text}") if m.type == "error" else None)
        schr.goto(SCHR + "/")
        kasse.goto(KASSE + "/")
        auto.goto(KASSE + "/automat")

        def shot(page, name):
            page.screenshot(path=os.path.join(SHOTS, name + ".png"), full_page=True)

        print("[Anzeige]")
        expect(schr.locator("#clock")).not_to_have_text("–", timeout=5000)
        expect(kasse.locator("#clock")).not_to_have_text("–", timeout=5000)
        expect(auto.locator("#clock")).not_to_have_text("–", timeout=5000)
        check("TITLE", schr.title() == "Parkhaus Schranken" and kasse.title() == "Parkhaus Kasse" and auto.title() == "Parkhaus Automat")
        check("MODE", kasse.locator("#listSection").is_visible() and not auto.locator("#listSection").is_visible())
        check("TESTFIELD", kasse.locator("#testSection").is_visible() and not auto.locator("#testSection").is_visible())
        # Uhr der Picos stimmt mit dem Notebook ueberein (E4)
        clock = kasse.locator("#clock").inner_text()
        now = time.strftime("%H:%M", time.localtime())
        check("CLOCK", clock.startswith(now[:2]), f"{clock} vs {now}")
        expect(kasse.locator("#conn")).to_contain_text("verbunden")
        check("EMPTY", "Keine Fahrzeuge" in kasse.locator("#listWrap").inner_text())
        shot(schr, "01_schranken_start")
        shot(kasse, "02_kasse_start")
        shot(auto, "03_automat_start")

        def gate(prefix):
            return schr.locator(f"#{prefix}Gate").get_attribute("data-state")

        def submit(prefix, plate):
            schr.fill(f"#{prefix}Plate", plate)
            schr.click(f"#{prefix}Btn")

        def msg(prefix):
            return schr.locator(f"#{prefix}Msg")

        print("[Einfahrt]")
        t0 = time.time()
        submit("entry", "ti 2468")
        expect(msg("entry")).to_contain_text("Einfahrt frei: TI 2468", timeout=3000)
        check("TC-01", time.time() - t0 < 3 and "msg ok" in msg("entry").get_attribute("class"))
        expect(schr.locator("#entryGate")).to_have_attribute("data-state", "open", timeout=2000)
        check("TC-01", gate("exit") == "closed")
        expect(kasse.locator("#list tbody tr")).to_have_count(1, timeout=4000)
        check("TC-01", kasse.locator("#list tbody tr td").first.inner_text() == "TI 2468")
        shot(schr, "04_schranken_einfahrt_offen")
        # Ungueltige Eingaben, Meldung bleibt stehen
        time.sleep(5.3)
        expect(schr.locator("#entryGate")).to_have_attribute("data-state", "closed", timeout=2000)
        check("TC-02", True)
        for plate, needle in [("2468", "Buchstaben fehlen"), ("T 2468", "nur ein Buchstabe"), ("TI 7654321", "mehr als sechs Ziffern"),
                              ("TI", "keine Ziffern"), ("TI-2468", "Ungültiges Zeichen"), ("", "Kennzeichen fehlt"), ("TI2468", "schon im Parkhaus")]:
            submit("entry", plate)
            expect(msg("entry")).to_contain_text(needle, timeout=3000)
            check("REJECT " + plate, "msg err" in msg("entry").get_attribute("class") and gate("entry") == "closed")
        time.sleep(10.5)  # Meldung bleibt auch nach 10 Sekunden stehen (AK-2.4)
        check("TC-09", "schon im Parkhaus" in msg("entry").inner_text())
        shot(schr, "05_schranken_abgewiesen")
        expect(kasse.locator("#list tbody tr")).to_have_count(1)

        print("[Kasse: Gebuehr und Zahlung]")
        kasse.fill("#tfPlate", "TI 2468")
        kasse.fill("#tfMinutes", "130")
        kasse.click("#tfBtn")
        expect(kasse.locator("#tfMsg")).to_contain_text("130 Minuten", timeout=3000)
        kasse.fill("#plate", " ti2468 ")
        kasse.click("#btnFee")
        expect(kasse.locator("#card")).to_have_class(re.compile("show"), timeout=3000)
        check("TC-24", kasse.locator("#cPlate").inner_text() == "TI 2468")
        check("TC-18", kasse.locator("#cFee").inner_text() == "CHF 3.50" and "130" in kasse.locator("#cMin").inner_text())
        check("TC-25", "offen" in kasse.locator("#cStatus").inner_text() and kasse.locator("#btnPay").is_visible())
        shot(kasse, "06_kasse_gebuehr")
        # Ausfahrt ohne Zahlung
        schr.fill("#exitPlate", "TI 2468")
        schr.click("#exitBtn")
        expect(msg("exit")).to_contain_text("CHF 3.50", timeout=3000)
        check("TC-30", gate("exit") == "closed" and "msg err" in msg("exit").get_attribute("class"))
        shot(schr, "07_schranken_ausfahrt_abgewiesen")
        # Bezahlen
        kasse.click("#btnPay")
        expect(kasse.locator("#cStatus")).to_contain_text("bezahlt", timeout=3000)
        check("TC-25", kasse.locator("#cPaid").inner_text() == "CHF 3.50" and kasse.locator("#cOpen").inner_text() == "CHF 0.00")
        check("TC-25", not kasse.locator("#btnPay").is_visible())
        expect(kasse.locator("#msg")).to_contain_text("Zahlung bestätigt")
        expect(kasse.locator("#list tbody tr td.status-paid")).to_have_count(1, timeout=4000)
        shot(kasse, "08_kasse_bezahlt")
        # Automat: keine zweite Zahlung
        auto.fill("#plate", "TI 2468")
        auto.click("#btnFee")
        expect(auto.locator("#cStatus")).to_contain_text("bezahlt", timeout=3000)
        check("TC-27", not auto.locator("#btnPay").is_visible() and auto.locator("#cOpen").inner_text() == "CHF 0.00")
        # Ausfahrt nach Zahlung
        t0 = time.time()
        schr.fill("#exitPlate", "TI 2468")
        schr.click("#exitBtn")
        expect(msg("exit")).to_contain_text("Ausfahrt frei", timeout=3000)
        expect(schr.locator("#exitGate")).to_have_attribute("data-state", "open", timeout=2000)
        check("TC-31", time.time() - t0 < 3)
        expect(kasse.locator("#list")).to_have_count(0, timeout=4000)
        shot(schr, "09_schranken_ausfahrt_offen")
        time.sleep(5.3)
        expect(schr.locator("#exitGate")).to_have_attribute("data-state", "closed", timeout=2000)
        check("TC-32", True)
        schr.fill("#exitPlate", "TI 2468")
        schr.click("#exitBtn")
        expect(msg("exit")).to_contain_text("nicht im Parkhaus", timeout=3000)
        check("TC-33", gate("exit") == "closed")

        print("[Automat]")
        submit("entry", "FR 3")
        expect(msg("entry")).to_contain_text("Einfahrt frei: FR 3", timeout=3000)
        kasse.fill("#tfPlate", "FR 3")
        kasse.fill("#tfMinutes", "200")
        kasse.click("#tfBtn")
        expect(kasse.locator("#tfMsg")).to_contain_text("200 Minuten")
        auto.fill("#plate", "fr3")
        auto.click("#btnFee")
        expect(auto.locator("#cFee")).to_have_text("CHF 5.00", timeout=3000)
        shot(auto, "10_automat_gebuehr")
        auto.click("#btnPay")
        expect(auto.locator("#cStatus")).to_contain_text("bezahlt", timeout=3000)
        expect(auto.locator("#msg")).to_contain_text("Zahlung bestätigt: CHF 5.00")
        shot(auto, "11_automat_bezahlt")
        # an der Kasse sofort sichtbar
        kasse.fill("#plate", "FR 3")
        kasse.click("#btnFee")
        expect(kasse.locator("#cStatus")).to_contain_text("bezahlt", timeout=3000)
        check("TC-28", kasse.locator("#cOpen").inner_text() == "CHF 0.00")
        # unbekanntes Kennzeichen
        auto.fill("#plate", "UR 8008")
        auto.click("#btnFee")
        expect(auto.locator("#msg")).to_contain_text("Kennzeichen unbekannt", timeout=3000)
        check("TC-29", "Keine Zahlung möglich" in auto.locator("#msg").inner_text() and not auto.locator("#card").is_visible())
        kasse.fill("#plate", "UR 8008")
        kasse.click("#btnFee")
        expect(kasse.locator("#msg")).to_contain_text("nicht im Parkhaus", timeout=3000)
        schr.fill("#exitPlate", "UR 8008")
        schr.click("#exitBtn")
        expect(msg("exit")).to_contain_text("Kennzeichen unbekannt", timeout=3000)
        # Gebuehr springt zwischen Abfrage und Zahlung: Zahlung wird abgelehnt
        submit("entry", "GR 100")
        expect(msg("entry")).to_contain_text("Einfahrt frei: GR 100", timeout=3000)
        kasse.fill("#tfPlate", "GR 100"); kasse.fill("#tfMinutes", "60"); kasse.click("#tfBtn")
        expect(kasse.locator("#tfMsg")).to_contain_text("60 Minuten")
        kasse.fill("#plate", "GR 100"); kasse.click("#btnFee")
        expect(kasse.locator("#cFee")).to_have_text("CHF 2.00", timeout=3000)
        kasse.fill("#tfMinutes", "61"); kasse.click("#tfBtn")  # Zeit springt, Karte zeigt noch CHF 2.00
        expect(kasse.locator("#tfMsg")).to_contain_text("61 Minuten")
        kasse.click("#btnPay")
        expect(kasse.locator("#msg")).to_contain_text("Gebühr hat sich geändert", timeout=3000)
        expect(kasse.locator("#cFee")).to_have_text("CHF 3.50", timeout=3000)
        check("FEE-CHANGED", "offen" in kasse.locator("#cStatus").inner_text())
        shot(kasse, "12_kasse_gebuehr_geaendert")

        print("[Gestoerte Verbindung]")
        time.sleep(5.3)
        kasse_pid = procs["kasse"].pid
        os.kill(kasse_pid, signal.SIGSTOP)   # "Kabel ziehen": Kasse antwortet nicht mehr
        t0 = time.time()
        schr.fill("#exitPlate", "FR 3")
        schr.click("#exitBtn")
        expect(msg("exit")).to_contain_text("Verbindungsfehler", timeout=9000)
        dt = time.time() - t0
        check("TC-39", dt <= 10 and gate("exit") == "closed" and "msg warn" in msg("exit").get_attribute("class") and "Gebühr offen" not in msg("exit").inner_text(), f"{dt:.1f}s")
        shot(schr, "13_schranken_verbindungsfehler")
        schr.fill("#entryPlate", "NW 12")
        schr.click("#entryBtn")
        expect(msg("entry")).to_contain_text("Verbindungsfehler", timeout=9000)
        check("TC-40", gate("entry") == "closed")
        expect(schr.locator("#kasseChip")).to_contain_text("Verbindungsfehler", timeout=3000)
        expect(kasse.locator("#conn")).to_have_class(re.compile("bad"), timeout=12000)
        shot(kasse, "14_kasse_ohne_verbindung")
        os.kill(kasse_pid, signal.SIGCONT)
        # Schranken ohne Neustart weiter bedienbar
        time.sleep(0.5)
        schr.fill("#exitPlate", "FR 3")
        schr.click("#exitBtn")
        expect(msg("exit")).to_contain_text("Ausfahrt frei: FR 3", timeout=5000)
        check("TC-41", True)
        expect(kasse.locator("#conn")).to_have_class(re.compile("ok"), timeout=8000)
        expect(schr.locator("#kasseChip")).to_contain_text("verbunden", timeout=3000)
        check("TC-41", kasse.locator("#list tbody tr[data-plate='NW 12']").count() == 0)
        # Kasse-Verbindung testen und Adresse aendern
        schr.click("#pingBtn")
        expect(schr.locator("#cfgMsg")).to_contain_text("Kasse erreichbar", timeout=5000)
        schr.fill("#kasseHost", "127.0.0.9")
        schr.click("#cfgBtn")
        expect(schr.locator("#cfgMsg")).to_contain_text("gespeichert", timeout=3000)
        schr.click("#pingBtn")
        expect(schr.locator("#cfgMsg")).to_contain_text("Verbindungsfehler", timeout=8000)
        schr.fill("#kasseHost", "127.0.0.2")
        schr.click("#cfgBtn")
        schr.click("#pingBtn")
        expect(schr.locator("#cfgMsg")).to_contain_text("Kasse erreichbar", timeout=5000)
        shot(schr, "15_schranken_einstellungen")

        print("[Handy-Ansicht und Sicherheit]")
        mob = browser.new_context(viewport={"width": 390, "height": 800}, locale="de-CH")
        mp = mob.new_page()
        mp.goto(KASSE + "/")
        mp.wait_for_timeout(800)
        check("MOBILE", mp.evaluate("document.documentElement.scrollWidth <= window.innerWidth + 1"))
        mp.screenshot(path=os.path.join(SHOTS, "16_kasse_mobil.png"), full_page=True)
        mp.goto(SCHR + "/")
        mp.wait_for_timeout(800)
        check("MOBILE", mp.evaluate("document.documentElement.scrollWidth <= window.innerWidth + 1"))
        mp.screenshot(path=os.path.join(SHOTS, "17_schranken_mobil.png"), full_page=True)
        # Eingaben mit HTML duerfen nie als HTML erscheinen
        schr.fill("#entryPlate", "<img src=x onerror=alert(1)>")
        schr.click("#entryBtn")
        expect(msg("entry")).to_contain_text("Ungültiges Zeichen", timeout=3000)
        check("XSS", schr.locator("#entryMsg img").count() == 0)
        check("ERRORS", not errors, str(errors))
        browser.close()
finally:
    for p in procs.values():
        try:
            p.send_signal(signal.SIGCONT)
        except Exception:
            pass
        p.kill()

print(f"\nErgebnis: {passed} bestanden, {failed} fehlgeschlagen")
sys.exit(1 if failed else 0)
