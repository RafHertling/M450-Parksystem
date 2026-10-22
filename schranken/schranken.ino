// Schranken (Pico 2): nachgemachte Kamera und zwei Servo-Schranken.
// Bei jeder Ein- und Ausfahrt wird die Kasse gefragt.
#include <WiFi.h>
#include <WebServer.h>
#include <Servo.h>
#include <time.h>
#include <string>
#include "config.h"
#include "SchrankenCore.h"

class ServoBarriers : public park::BarrierDriver {
 public:
  void begin() {
    entry_.attach(ENTRY_SERVO_PIN);
    exit_.attach(EXIT_SERVO_PIN);
    entry_.write(SERVO_CLOSED_ANGLE);
    exit_.write(SERVO_CLOSED_ANGLE);
  }
  void set(int which, bool open) override {
    int angle = open ? SERVO_OPEN_ANGLE : SERVO_CLOSED_ANGLE;
    if (which == 0) entry_.write(angle);
    else exit_.write(angle);
  }

 private:
  Servo entry_;
  Servo exit_;
};

class KasseHttp : public park::KasseLink {
 public:
  KasseHttp() : host_(KASSE_HOST) {}

  std::string host() const override { return host_; }
  void setHost(const std::string& h) override { host_ = h; }

  park::KasseReply post(const std::string& pathAndQuery) override {
    park::KasseReply reply;
    unsigned long start = millis();  // Zeitlimit fuer Verbinden und Antwort zusammen
    WiFiClient client;
    client.setTimeout(KASSE_TIMEOUT_MS);
    if (!client.connect(host_.c_str(), KASSE_PORT)) return reply;

    client.print(String("POST ") + pathAndQuery.c_str() + " HTTP/1.0\r\nHost: " + host_.c_str() +
                 "\r\nContent-Length: 0\r\nConnection: close\r\n\r\n");

    std::string raw;
    while (millis() - start < (unsigned long)KASSE_TIMEOUT_MS) {
      while (client.available()) {
        raw += (char)client.read();
      }
      if (park::httpComplete(raw)) break;
      if (!client.connected() && !client.available()) break;
      delay(1);
    }
    client.stop();

    int status = 0;
    std::string body;
    if (park::httpParse(raw, status, body)) {
      reply.reachable = true;
      reply.status = status;
      reply.body = body;
    }
    return reply;
  }

 private:
  std::string host_;
};

WebServer server(80);
ServoBarriers barriers;
KasseHttp kasse;
park::SchrankenApp app(&kasse, &barriers, BARRIER_OPEN_MS);

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
  park::Response r = app.handle(rq, millis(), nowEpoch());
  server.sendHeader("Cache-Control", "no-store");
  server.send(r.status, r.type.c_str(), r.body.c_str());
}

void setup() {
  Serial.begin(115200);
  pinMode(LED_BUILTIN, OUTPUT);
  digitalWrite(LED_BUILTIN, LOW);
  setenv("TZ", TIMEZONE, 1);
  tzset();

  barriers.begin();
  app.begin();

  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  lastWifiTry = millis();
  server.onNotFound(handleAll);
  Serial.println("Schranken (Pico 2) startet, verbinde mit WLAN ...");
}

void loop() {
  app.tick(millis());  // schliesst offene Schranken

  if (WiFi.status() == WL_CONNECTED) {
    if (!started) {
      NTP.begin("pool.ntp.org", "time.nist.gov");
      server.begin();
      started = true;
      digitalWrite(LED_BUILTIN, HIGH);
      Serial.print("WLAN verbunden. Schranken: http://");
      Serial.println(WiFi.localIP());
      Serial.print("Kasse: ");
      Serial.println(kasse.host().c_str());
    }
    server.handleClient();
  } else {
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
    Serial.print("  Kasse: ");
    Serial.println(kasse.host().c_str());
  }
}
