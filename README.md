# Parksystem

Modul 450, Klasse INA24BL

## Gruppenmitglieder

- Aleksander
- Rafael

## Umsetzung

Das Parkhaus hat eine Einfahrts- und eine Ausfahrtsschranke. Es läuft auf zwei Raspberry Pi Pico W, programmiert mit Arduino (arduino-pico Core von Earle Philhower). Beide Picos sind im gleichen WLAN und reden per HTTP miteinander.

**Pico 1, Kasse** (`kasse/`)
Zeigt die Kasse unter `/` und den Automaten unter `/automat` an. Der Automat ist die gleiche Seite in einem zweiten Fenster. Die Kasse speichert Kennzeichen, Einfahrtszeit und ob bezahlt wurde, und rechnet die Gebühr aus.

**Pico 2, Schranken** (`schranken/`)
Statt einer Kamera tippt man das Kennzeichen auf einer Webseite ein. Der Pico steuert die zwei Schranken (Servos an GP8 und GP9) und fragt bei jeder Ein- und Ausfahrt die Kasse.

Ablauf: Einfahrt, die Kasse speichert Kennzeichen und Zeit. Dann an der Kasse oder am Automaten bezahlen. Bei der Ausfahrt prüft die Kasse, ob bezahlt ist, und trägt das Auto aus. Erst dann geht die Schranke auf.

Ein paar Entscheidungen:

- Wenn die Kasse nicht antwortet (Kabel gezogen, WLAN weg), bleiben beide Schranken zu und es kommt nach etwa 3 Sekunden ein Verbindungsfehler.
- Die Schranken schicken ihre Uhrzeit mit. Kommt eine Anfrage zu spät bei der Kasse an, wird sie ignoriert. Sonst könnte ein Auto eingetragen sein, obwohl die Schranke zu geblieben ist.
- Beim Bezahlen wird geprüft, ob der Betrag noch stimmt. Die Gebühr kann zwischen Abfragen und Bezahlen eine Stufe höher springen.
- Die Daten sind nur im RAM. Nach einem Neustart der Kasse sind sie weg.

Tarif (ganze Minuten):

| Aufenthalt | Gebühr |
|---|---|
| 0 bis 60 Min. | CHF 2.00 |
| 61 bis 180 Min. | CHF 3.50 |
| 181 bis 240 Min. | CHF 5.00 |
| 241 bis 300 Min. | CHF 6.00 |
| 301 bis 360 Min. | CHF 7.00 |
| ab 361 Min. | CHF 1.00 mehr pro angefangene Stunde |

Kennzeichen sind zwei Buchstaben und ein bis sechs Ziffern, z. B. `TI 2468`. Gross- und Kleinschreibung und Leerzeichen sind egal.

## Umgesetzte UserStories

| US | Titel | Was gemacht wurde |
|---|---|---|
| US-1 | Einfahrt registrieren | Schranke geht nach der Eingabe auf und nach 5 s wieder zu. Die Kasse speichert Kennzeichen und Einfahrtszeit (NTP). |
| US-2 | Fehlerhafte Einfahrt abweisen | Ungültige, leere und doppelte Kennzeichen werden mit einer Begründung abgewiesen. Die Schranke bleibt zu. |
| US-3 | Gebühr berechnen | Kasse und Automat zeigen Einfahrtszeit, Aufenthalt und Gebühr an. |
| US-4 | Gebühr bezahlen | Bezahlen an der Kasse oder am Automaten. Eine zweite Zahlung geht nicht. |
| US-5 | Ausfahrt nur nach Bezahlung | Die Ausfahrtsschranke geht nur auf, wenn bezahlt ist. Sonst wird der offene Betrag angezeigt. |
| US-6 | Betrieb bei gestörter Verbindung | Ohne Kasse bleiben beide Schranken zu und es wird ein Verbindungsfehler angezeigt. Danach geht es ohne Neustart weiter. |

Für die Tests gibt es auf der Kasse ein Testfeld. Damit kann man die Einfahrtszeit zurückstellen und die Tarifstufen prüfen, ohne Stunden zu warten. Nach der Abnahme `TESTFIELD_ENABLED` in `kasse/config.h` auf 0 setzen.

## Verdrahtung

Servos (SG90) am Schranken-Pico:

- rot an VBUS (Pin 40)
- braun an GND
- orange an GP8 (Pin 11) für die Einfahrt, GP9 (Pin 12) für die Ausfahrt

Pins und Winkel kann man in `schranken/config.h` ändern.

## Einrichtung

1. In der Arduino IDE unter Einstellungen diese Boardverwalter-URL eintragen und danach "Raspberry Pi Pico/RP2040/RP2350" installieren:
   `https://github.com/earlephilhower/arduino-pico/releases/download/global/package_rp2040_index.json`
2. Board "Raspberry Pi Pico W" wählen.
3. In beiden `config.h` WLAN-Name und Passwort eintragen.
4. `kasse` auf den Pico ohne Servos hochladen. Im seriellen Monitor (115200 Baud) steht die IP.
5. Diese IP in `schranken/config.h` bei `KASSE_HOST` eintragen und `schranken` auf den Pico mit den Servos hochladen.
6. Im Browser öffnen: `http://<IP Schranken>/`, `http://<IP Kasse>/` und `http://<IP Kasse>/automat`.

Wenn sich die IP der Kasse ändert, kann man sie auf der Schranken-Seite unter "Verbindung zur Kasse" anpassen.

## Tests

Im Ordner `test/` sind Tests, die ohne Hardware auf dem PC laufen (`test/run_tests.sh`, braucht g++, Python und Playwright):

- `test_core.cpp`: Testfälle TC-01 bis TC-42 aus dem Testkonzept gegen die Logik
- `test_e2e.py`: beide Sketches laufen auf dem PC und reden über HTTP miteinander
- `test_ui.py`: Browser-Test der drei Seiten, Screenshots in `test/screens/`

Mit der echten Hardware haben wir zusätzlich Einfahrt, Bezahlen, Ausfahrt, die Servos und das Ziehen des Kassen-Kabels getestet.
