/*
  kasse.ino - Parksystem, Pico 1 = KASSE (Raspberry Pi Pico W)

  Aufgaben:
    - Weboberflaeche der Kasse (http://<IP>/) und des Automaten (http://<IP>/automat)
    - speichert Kennzeichen, Einfahrtszeit und Bezahlstatus (RAM, geht beim Neustart verloren)
    - berechnet die Parkgebuehr aus der Aufenthaltsdauer
    - wird vom Schranken-Pico (Pico 2) per HTTP angefragt (/api/entry, /api/exit)

  Board:  "Raspberry Pi Pico W" im Arduino-Pico-Core von Earle Philhower
          (Boardverwalter-URL: https://github.com/earlephilhower/arduino-pico/releases/download/global/package_rp2040_index.json)
  Vorher: WLAN-Name und Passwort in config.h eintragen.
  Danach: Serieller Monitor (115200 Baud) zeigt die IP-Adresse. Diese IP im Schranken-Pico
          (config.h, KASSE_HOST) eintragen oder in dessen Weboberflaeche setzen.
*/
#include <WiFi.h>
#include <WebServer.h>
#include <time.h>
#include <string>
#include "config.h"
#include "KasseCore.h"

WebServer server(80);
park::KasseApp app;

static bool started = false;          // Webserver und NTP laufen
static unsigned long lastWifiTry = 0;
static unsigned long lastInfo = 0;

static park::i64 nowEpoch() { return (park::i64)time(nullptr); }

// Alle Anfragen gehen an KasseApp::handle() (siehe KasseCore.h)
static void handleAll() {
  park::Request rq;
  switch (server.method()) {
    case HTTP_GET: rq.method = "GET"; break;
    case HTTP_POST: rq.method = "POST"; break;
    default: rq.method = "OTHER"; break;
  }
  rq.path = server.uri().c_str();
  for (int i = 0; i < server.args(); i++) {
    rq.args.push_back(std::make_pair(std::string(server.argName(i).c_str()),
                                     std::string(server.arg(i).c_str())));
  }
  park::Response r = app.handle(rq, nowEpoch());
  server.sendHeader("Cache-Control", "no-store");
  server.send(r.status, r.type.c_str(), r.body.c_str());
}

void setup() {
  Serial.begin(115200);
  pinMode(LED_BUILTIN, OUTPUT);
  digitalWrite(LED_BUILTIN, LOW);
  setenv("TZ", TIMEZONE, 1);  // Schweizer Zeit
  tzset();

  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  lastWifiTry = millis();
  server.onNotFound(handleAll);
  Serial.println("Kasse (Pico 1) startet, verbinde mit WLAN ...");
}

void loop() {
  if (WiFi.status() == WL_CONNECTED) {
    if (!started) {
      NTP.begin("pool.ntp.org", "time.nist.gov");  // Uhrzeit per NTP holen
      server.begin();
      started = true;
      digitalWrite(LED_BUILTIN, HIGH);
      Serial.print("WLAN verbunden. Kasse: http://");
      Serial.println(WiFi.localIP());
    }
    server.handleClient();
  } else {
    // WLAN weg: LED blinkt, alle 10 Sekunden neu versuchen
    digitalWrite(LED_BUILTIN, (millis() / 300) % 2);
    if (millis() - lastWifiTry > 10000) {
      lastWifiTry = millis();
      WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
    }
  }

  // alle 30 Sekunden Lebenszeichen mit IP und Fahrzeugzahl
  if (millis() - lastInfo > 30000) {
    lastInfo = millis();
    Serial.print("IP: ");
    Serial.print(WiFi.localIP());
    Serial.print("  Fahrzeuge: ");
    Serial.print(app.count());
    Serial.print("  Zeit gueltig: ");
    Serial.println(park::timeIsValid(nowEpoch()) ? "ja" : "nein");
  }
}
