# Parksystem – Parkhaus mit Ein- und Ausfahrtsschranke

Modul 450 · Klasse INA24BL

## Gruppenmitglieder

- Aleksander
- Rafael

*(Namen bei Bedarf anpassen.)*

## Kurze Beschreibung der Umsetzung

Das Parksystem besteht aus **zwei Raspberry Pi Pico W**, programmiert mit **Arduino** (Arduino-Pico-Core von Earle Philhower). Beide Picos hängen im selben WLAN und sprechen per HTTP miteinander.

```
 Browser (Notebook)                 WLAN
 ┌─────────────────┐        ┌───────────────────────┐        ┌────────────────────────┐
 │ Fenster 1: Schranken ───▶│ Pico 2 – SCHRANKEN     │──HTTP─▶│ Pico 1 – KASSE         │
 │ Fenster 2: Kasse     ─────────────────────────────────────▶│  • Kasse  (/)          │
 │ Fenster 3: Automat   ─────────────────────────────────────▶│  • Automat (/automat)  │
 └─────────────────┘        │  • nachgemachte Kamera │        │  • speichert Fahrzeuge │
                            │  • 2 Servo-Schranken   │        │  • berechnet Gebühr    │
                            └───────────────────────┘        └────────────────────────┘
```

| Pico | Sketch | Aufgabe |
|---|---|---|
| **Pico 1 – Kasse** | `kasse/kasse.ino` | Weboberfläche der **Kasse** (`/`) und des **Automaten** (`/automat`, gleiche Oberfläche im zweiten Fenster). Speichert Kennzeichen, Einfahrtszeit und Bezahlstatus, berechnet die Gebühr aus der Aufenthaltsdauer, nimmt Zahlungen entgegen. |
| **Pico 2 – Schranken** | `schranken/schranken.ino` | Weboberfläche mit **nachgemachter Kamera** (Kennzeichen von Hand eingeben, Ein- und Ausfahrt). Steuert die **Einfahrts- und Ausfahrtsschranke** (Servos). Fragt bei jeder Ein- und Ausfahrt die Kasse. |

**Ablauf eines Fahrzeugs:** Einfahrt (Kasse speichert Kennzeichen + Zeit) → Gebühr offen → bezahlt (Kasse oder Automat) → Ausfahrt (Kasse prüft «bezahlt», trägt das Fahrzeug aus, Schranke öffnet).

**Wichtige Entscheidungen**

- Antwortet die Kasse nicht (Kabel gezogen, WLAN weg), bleiben **beide Schranken zu** und es erscheint ein *Verbindungsfehler* (nach etwa 3 s, Testkonzept verlangt höchstens 10 s) – eine andere Meldung als bei offener Gebühr.
- Die Schranken schicken ihre Uhrzeit mit. Kommt eine Anfrage bei der Kasse erst später an (die Schranken haben längst aufgegeben), verwirft die Kasse sie. So gibt es nie ein Fahrzeug, das eingetragen oder ausgetragen ist, obwohl die Schranke geschlossen blieb.
- Die Kasse prüft bei der Zahlung, dass der Betrag noch stimmt, den der Kunde gesehen hat (die Gebühr kann zwischen Abfrage und Zahlung eine Stufe weiterspringen). Sonst wird nichts verbucht.
- Die Daten liegen nur im RAM der Kasse. Nach einem Neustart sind sie weg (Testfall TC-42 schreibt das nur auf).

### Tarif und Kennzeichen (Annahmen aus dem Testkonzept 2.4)

| Aufenthalt (ganze Minuten, Sekunden werden abgeschnitten) | Gebühr |
|---|---|
| 0–60 Min. (Startgebühr bei der Einfahrt, keine Gratiszeit) | CHF 2.00 |
| 61–180 Min. | CHF 3.50 |
| 181–240 Min. | CHF 5.00 |
| 241–300 Min. | CHF 6.00 |
| 301–360 Min. | CHF 7.00 |
| ab 361 Min. | plus CHF 1.00 pro angefangene weitere Stunde |

Kennzeichen: **zwei Buchstaben + ein bis sechs Ziffern** (z. B. `TI 2468`, `VS 864209`). Gross-/Kleinschreibung und Leerzeichen spielen keine Rolle.

## Umgesetzte UserStories / Anforderungen

Alle sechs UserStories aus dem Testkonzept sind umgesetzt.

| US | Titel | Umsetzung |
|---|---|---|
| **US-1** | Einfahrt registrieren | Kennzeichen eingeben → Schranke öffnet innert 3 s und schliesst nach 5 s selbständig (`BARRIER_OPEN_MS`, höchstens 10 s). Kasse speichert Kennzeichen + Einfahrtszeit (NTP, Schweizer Zeit in der Anzeige). Mehrere Fahrzeuge gleichzeitig. |
| **US-2** | Fehlerhafte Einfahrt abweisen | Ungültiges, leeres und doppeltes Kennzeichen werden mit einem Satz Begründung abgewiesen. Die Meldung bleibt bis zur nächsten Eingabe stehen, die Schranke bleibt zu, es entsteht kein Eintrag. |
| **US-3** | Gebühr berechnen | Kasse und Automat zeigen zu einem Kennzeichen Einfahrtszeit, Aufenthalt und Gebühr nach Tarif. Unbekanntes Kennzeichen: kein Betrag. |
| **US-4** | Gebühr bezahlen | Zahlung an der Kasse **oder** am Automaten, Status wechselt auf *bezahlt* und ist in beiden Fenstern sichtbar. Keine zweite Zahlung, keine Zahlung für unbekannte Kennzeichen. |
| **US-5** | Ausfahrt nur nach Bezahlung | Ausfahrtsschranke öffnet nur bei bezahltem Status. Bei offenem Betrag bleibt sie zu und die Meldung nennt den Betrag. Nach der Ausfahrt öffnet dasselbe Kennzeichen nicht ein zweites Mal; eine neue Einfahrt beginnt einen neuen Aufenthalt. Auch bei 0 Minuten muss die Startgebühr bezahlt werden. |
| **US-6** | Betrieb bei gestörter Verbindung | Antwortet die Kasse nicht, bleiben beide Schranken zu, es gibt keinen Eintrag ohne Einfahrtszeit. Meldung «Verbindungsfehler». Die Schranken bleiben ohne Neustart bedienbar. |

Zusätzlich aus dem Testkonzept: das **Testfeld** auf der Kasse (setzt die Einfahrtszeit auf «jetzt minus X Minuten»), damit man die Gebührengrenzen in einer Lektion prüfen kann.

## Hardware und Verdrahtung

- 2 × Raspberry Pi Pico **W** (WLAN nötig), beide per USB am Notebook.
- Servos (z. B. SG90) an Pico 2 (Schranken), wie im Schema der Aufgabe: **rot → VBUS (Pin 40)**, **braun → GND (z. B. Pin 8)**, **orange (Signal) → GP8 (Pin 11)** = Einfahrtsschranke. Die Ausfahrtsschranke hängt am Signal-Pin **GP9** (Pin 12).
- Pins und Winkel stehen in `schranken/config.h` (`ENTRY_SERVO_PIN`, `EXIT_SERVO_PIN`, `SERVO_CLOSED_ANGLE`, `SERVO_OPEN_ANGLE`). Ist nur ein Servo angeschlossen, funktioniert alles trotzdem; die zweite Schranke sieht man dann nur im Browser.

## Einrichtung

1. **Arduino IDE** → *Einstellungen* → *Zusätzliche Boardverwalter-URLs*:
   `https://github.com/earlephilhower/arduino-pico/releases/download/global/package_rp2040_index.json`
   Dann im Boardverwalter **«Raspberry Pi Pico/RP2040/RP2350»** installieren.
2. Board **«Raspberry Pi Pico W»** wählen. Zusätzliche Bibliotheken sind nicht nötig (WiFi, WebServer und Servo gehören zum Core).
3. In **beiden** `config.h` (`kasse/` und `schranken/`) `WIFI_SSID` und `WIFI_PASSWORD` eintragen (gleiches WLAN, zum Beispiel Handy-Hotspot). Passwörter nicht einchecken.
4. `kasse/kasse.ino` auf **Pico 1** hochladen. Im seriellen Monitor (115200 Baud) erscheint `Kasse: http://<IP>`.
5. Diese IP in `schranken/config.h` bei `KASSE_HOST` eintragen (oder später in der Weboberfläche der Schranken ändern) und `schranken/schranken.ino` auf **Pico 2** hochladen. Im seriellen Monitor erscheint `Schranken: http://<IP>`.
6. Im Browser drei Fenster öffnen:
   - Schranken: `http://<IP-Schranken>/`
   - Kasse: `http://<IP-Kasse>/`
   - Automat: `http://<IP-Kasse>/automat`

Bekommt ein Pico nach einem Neustart eine neue IP, die Adresse im seriellen Monitor nachschauen. Die Adresse der Kasse kann in der Schranken-Oberfläche unter «Verbindung zur Kasse» geändert werden.

**Nach der Abnahme:** in `kasse/config.h` `TESTFIELD_ENABLED` auf `0` setzen (das Testfeld kann Einfahrtszeiten verändern).

## Aufbau des Repositories

```
kasse/          Sketch Pico 1: kasse.ino, KasseCore.h (Logik), pages.h (Weboberfläche), Common.h, config.h
schranken/      Sketch Pico 2: schranken.ino, SchrankenCore.h (Logik), pages.h (Weboberfläche), Common.h, config.h
test/           Tests, die ohne Hardware auf dem PC laufen (siehe unten)
```

`Common.h` (Kennzeichen prüfen, JSON, HTTP-Hilfen) liegt in beiden Sketch-Ordnern und ist **identisch**; Änderungen immer in beiden machen (`test/run_tests.sh` prüft das).

Die Schnittstelle zwischen den Picos (HTTP, Antworten als JSON):

| Aufruf an die Kasse | Zweck |
|---|---|
| `POST /api/entry?plate=TI2468&ts=<Zeit>` | Einfahrt registrieren |
| `POST /api/exit?plate=TI2468&ts=<Zeit>` | Ausfahrt: prüft «bezahlt» und trägt das Fahrzeug aus |
| `GET /api/fee?plate=…` | Gebühr abfragen |
| `POST /api/pay?plate=…&amount=<Rappen>` | Bezahlen |
| `GET /api/list` | Fahrzeugliste |
| `POST /api/testfield?plate=…&minutes=…` | Testfeld |

## Tests

`test/run_tests.sh` führt alles aus (braucht `g++`, `python3` und für den Browser-Test Playwright mit Chromium):

1. **Logik-Tests** (`test_core.cpp`): spielt TC-01 bis TC-42 aus dem Testkonzept gegen die Logik ab, dazu Tarifgrenzen, Kennzeichen-Sonderfälle, Ausfälle der Kasse, verspätete Anfragen. 235 Prüfungen.
2. **End-to-End-Test** (`test_e2e.py`): startet beide Sketches (mit Ersatz-Headern für WLAN, Webserver und Servo auf dem PC), lässt sie über echte HTTP-Verbindungen miteinander reden und spielt TC-01 bis TC-42 in Echtzeit ab, inklusive Zeitmessung (Schranke öffnet < 3 s, schliesst nach 5 s, Verbindungsfehler nach 3 s) und «Kabel ziehen» (Kasse anhalten bzw. beenden).
3. **Browser-Test** (`test_ui.py`): bedient die drei Oberflächen in Chromium wie im Testkonzept (Einfahrt, Abweisungen, Gebühr, Zahlung an Kasse und Automat, Ausfahrt, Verbindungsfehler, Handy-Ansicht). Bildschirmfotos liegen in `test/screens/`.

**Nicht getestet** (nur mit der echten Hardware möglich): das Hochladen auf die Picos, WLAN/NTP auf dem echten Pico, die Servobewegung. Das Testkonzept ist dafür der Abnahmetest an der Hardware.
