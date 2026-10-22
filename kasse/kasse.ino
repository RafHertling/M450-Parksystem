// Kasse (Pico 1): Weboberflaeche fuer Kasse (/) und Automat (/automat),
// speichert die Fahrzeuge und berechnet die Gebuehr.
#include <WiFi.h>
#include <WebServer.h>
#include <time.h>
#include <string>
#include "config.h"
#include "KasseCore.h"

WebServer server(80);
park::KasseApp app;

static bool started = false;
static unsigned long lastWifiTry = 0;
static unsigned long lastInfo = 0;

static park::i64 nowEpoch() { return (park::i64)time(nullptr); }

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
  setenv("TZ", TIMEZONE, 1);
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
      NTP.begin("pool.ntp.org", "time.nist.gov");
      server.begin();
      started = true;
      digitalWrite(LED_BUILTIN, HIGH);
      Serial.print("WLAN verbunden. Kasse: http://");
      Serial.println(WiFi.localIP());
    }
    server.handleClient();
  } else {
    // WLAN weg: alle 10 s neu verbinden
    digitalWrite(LED_BUILTIN, (millis() / 300) % 2);
    if (millis() - lastWifiTry > 10000) {
      lastWifiTry = millis();
      WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
    }
  }

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
