// Einstellungen Schranken (Pico 2)
#pragma once

#ifndef WIFI_SSID
#define WIFI_SSID "INF-LAB"
#endif
#ifndef WIFI_PASSWORD
#define WIFI_PASSWORD "DEIN_PASSWORT"
#endif

// IP der Kasse, kann auch in der Weboberflaeche geaendert werden
#ifndef KASSE_HOST
#define KASSE_HOST "172.18.13.75"
#endif
#ifndef KASSE_PORT
#define KASSE_PORT 80
#endif

// danach "Verbindungsfehler" (TC-39 verlangt hoechstens 10 s)
#ifndef KASSE_TIMEOUT_MS
#define KASSE_TIMEOUT_MS 3000
#endif

// Servo-Signal (orange), rot an VBUS (Pin 40), braun an GND
#ifndef ENTRY_SERVO_PIN
#define ENTRY_SERVO_PIN 8   // Pin 11
#endif
#ifndef EXIT_SERVO_PIN
#define EXIT_SERVO_PIN 9    // Pin 12
#endif
#ifndef SERVO_CLOSED_ANGLE
#define SERVO_CLOSED_ANGLE 90
#endif
#ifndef SERVO_OPEN_ANGLE
#define SERVO_OPEN_ANGLE 0
#endif

// so lange bleibt eine Schranke offen (hoechstens 10 s)
#ifndef BARRIER_OPEN_MS
#define BARRIER_OPEN_MS 5000
#endif

#ifndef TIMEZONE
#define TIMEZONE "CET-1CEST,M3.5.0,M10.5.0/3"
#endif
