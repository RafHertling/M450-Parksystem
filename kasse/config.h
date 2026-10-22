// config.h - Einstellungen fuer die KASSE (Pico 1)
// Vor dem Hochladen WLAN-Name und Passwort eintragen. Passwort nicht ins Repository einchecken!
#pragma once

#ifndef WIFI_SSID
#define WIFI_SSID "INF-LAB"          // <-- anpassen (zum Beispiel Handy-Hotspot)
#endif
#ifndef WIFI_PASSWORD
#define WIFI_PASSWORD "DEIN_PASSWORT"  // <-- anpassen
#endif

// Testfeld (Testkonzept 3.2): setzt die Einfahrtszeit auf "jetzt minus X Minuten".
// 1 = eingebaut, 0 = ausgeschaltet. Nach der Abnahme auf 0 stellen!
#ifndef TESTFIELD_ENABLED
#define TESTFIELD_ENABLED 1
#endif

// Zeitzone der Schweiz (fuer die Ausgabe im seriellen Monitor)
#ifndef TIMEZONE
#define TIMEZONE "CET-1CEST,M3.5.0,M10.5.0/3"
#endif
