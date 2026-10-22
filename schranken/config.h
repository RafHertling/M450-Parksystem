// config.h - Einstellungen fuer die SCHRANKEN (Pico 2)
// Vor dem Hochladen WLAN-Name, Passwort und die IP-Adresse der Kasse eintragen.
// Passwort nicht ins Repository einchecken!
#pragma once

#ifndef WIFI_SSID
#define WIFI_SSID "INF-LAB"          // <-- anpassen (gleiches WLAN wie die Kasse)
#endif
#ifndef WIFI_PASSWORD
#define WIFI_PASSWORD "DEIN_PASSWORT"  // <-- anpassen
#endif

// IP-Adresse des Kassen-Pico (steht im seriellen Monitor der Kasse).
// Kann auch spaeter in der Weboberflaeche der Schranken geaendert werden.
#ifndef KASSE_HOST
#define KASSE_HOST "172.18.13.75"      // <-- anpassen
#endif
#ifndef KASSE_PORT
#define KASSE_PORT 80
#endif

// Wie lange auf die Kasse gewartet wird, bevor "Verbindungsfehler" erscheint (Millisekunden).
// Muss deutlich unter 10 Sekunden liegen (AK-6.3 / TC-39).
#ifndef KASSE_TIMEOUT_MS
#define KASSE_TIMEOUT_MS 3000
#endif

// Servos der beiden Schranken (Signalleitung, orange). Plus an VBUS (Pin 40), Minus an GND.
// Das Schema aus der Aufgabe benutzt GP8 fuer ein Servo.
#ifndef ENTRY_SERVO_PIN
#define ENTRY_SERVO_PIN 8              // Einfahrtsschranke: GP8 (Pin 11)
#endif
#ifndef EXIT_SERVO_PIN
#define EXIT_SERVO_PIN 9               // Ausfahrtsschranke: GP9 (Pin 12)
#endif
#ifndef SERVO_CLOSED_ANGLE
#define SERVO_CLOSED_ANGLE 90          // Schranke unten (Arm waagrecht)
#endif
#ifndef SERVO_OPEN_ANGLE
#define SERVO_OPEN_ANGLE 0             // Schranke oben
#endif

// So lange bleibt eine Schranke offen, dann schliesst sie selbst (muss <= 10 s sein, AK-1.4 / AK-5.3)
#ifndef BARRIER_OPEN_MS
#define BARRIER_OPEN_MS 5000
#endif

// Zeitzone der Schweiz (fuer die Ausgabe im seriellen Monitor)
#ifndef TIMEZONE
#define TIMEZONE "CET-1CEST,M3.5.0,M10.5.0/3"
#endif
