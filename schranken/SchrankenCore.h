// SchrankenCore.h - Logik der Schranken (Pico 2): nachgemachte Kamera (Kennzeichen von Hand),
// Einfahrts- und Ausfahrtsschranke, Anfragen an die Kasse (Pico 1).
// Reines C++ ohne Arduino-Funktionen. Die Hardware (Servos, Netzwerk) wird ueber die zwei
// kleinen Schnittstellen KasseLink und BarrierDriver angeschlossen (siehe schranken.ino).
#pragma once
#include "Common.h"
#include "config.h"
#include "pages.h"

namespace park {

// Antwort der Kasse. reachable=false: keine gueltige Antwort (Pico aus, WLAN weg, Zeitueberschreitung).
struct KasseReply {
  bool reachable;
  int status;
  std::string body;
  KasseReply() : reachable(false), status(0) {}
};

class KasseLink {
 public:
  virtual ~KasseLink() {}
  virtual KasseReply post(const std::string& pathAndQuery) = 0;
  virtual std::string host() const = 0;
  virtual void setHost(const std::string& h) = 0;
};

class BarrierDriver {
 public:
  virtual ~BarrierDriver() {}
  virtual void set(int which, bool open) = 0;  // which: 0 = Einfahrt, 1 = Ausfahrt
};

inline bool validHost(const std::string& h) {
  if (h.empty() || h.size() > 40) return false;
  if (h[0] == '.' || h[0] == '-') return false;
  for (size_t i = 0; i < h.size(); i++) {
    char c = h[i];
    bool ok = (c >= '0' && c <= '9') || (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || c == '.' || c == '-';
    if (!ok) return false;
  }
  return true;
}

class SchrankenApp {
 public:
  enum { GATE_ENTRY = 0, GATE_EXIT = 1 };
  enum KasseState { K_UNKNOWN = 0, K_OK, K_ERROR };

  SchrankenApp(KasseLink* link, BarrierDriver* drv, unsigned long openMs)
      : link_(link), drv_(drv), openMs_(openMs), kasse_(K_UNKNOWN) {
    for (int i = 0; i < 2; i++) { open_[i] = false; openedAt_[i] = 0; }
  }

  // beim Start beide Schranken schliessen
  void begin() {
    for (int i = 0; i < 2; i++) { open_[i] = false; drv_->set(i, false); }
  }

  // regelmaessig aus loop() aufrufen: schliesst offene Schranken nach openMs
  void tick(unsigned long nowMs) {
    for (int i = 0; i < 2; i++) {
      if (open_[i] && (unsigned long)(nowMs - openedAt_[i]) >= openMs_) {
        open_[i] = false;
        drv_->set(i, false);
      }
    }
  }

  bool isOpen(int which) const { return open_[which]; }

  Response handle(const Request& rq, unsigned long nowMs, i64 nowEpoch) {
    const std::string& p = rq.path;
    if (p == "/" || p == "/index.html") {
      Response r;
      r.type = "text/html; charset=utf-8";
      r.body = SCHRANKEN_HTML;
      return r;
    }
    if (p == "/favicon.ico") {
      Response r;
      r.status = 204;
      r.type = "text/plain";
      return r;
    }
    if (p == "/api/state") return state(nowEpoch);
    if (p == "/api/entry") return pass(GATE_ENTRY, rq, nowMs, nowEpoch);
    if (p == "/api/exit") return pass(GATE_EXIT, rq, nowMs, nowEpoch);
    if (p == "/api/config") return config(rq);
    if (p == "/api/kasse") return kasseCheck();
    Response r;
    r.status = 404;
    r.type = "text/plain; charset=utf-8";
    r.body = "Nicht gefunden";
    return r;
  }

 private:
  KasseLink* link_;
  BarrierDriver* drv_;
  unsigned long openMs_;
  bool open_[2];
  unsigned long openedAt_[2];
  KasseState kasse_;

  static const char* kasseStateName(KasseState s) {
    return s == K_OK ? "ok" : (s == K_ERROR ? "error" : "unknown");
  }

  void openBarrier(int which, unsigned long nowMs) {
    open_[which] = true;
    openedAt_[which] = nowMs;
    drv_->set(which, true);
  }

  Response result(int which, bool ok, const char* code, const std::string& msg, const std::string& plate) {
    JObj o;
    o.boolean("ok", ok).str("code", code).str("message", msg).str("plate", plate);
    o.str("barrier", open_[which] ? "open" : "closed");
    return jsonResponse(o);
  }

  Response state(i64 nowEpoch) {
    JObj o;
    o.boolean("ok", true)
        .boolean("entryOpen", open_[GATE_ENTRY])
        .boolean("exitOpen", open_[GATE_EXIT])
        .str("kasse", link_->host())
        .str("kasseState", kasseStateName(kasse_))
        .num("now", nowEpoch)
        .boolean("timeValid", nowEpoch >= 1700000000LL)
        .num("openMs", (i64)openMs_);
    return jsonResponse(o);
  }

  // Einfahrt (GATE_ENTRY) oder Ausfahrt (GATE_EXIT): Kennzeichen pruefen, Kasse fragen, Schranke steuern
  Response pass(int which, const Request& rq, unsigned long nowMs, i64 nowEpoch) {
    Plate pl = parsePlate(rq.arg("plate").c_str());
    if (pl.status != PLATE_OK) return result(which, false, "invalid", plateMessage(pl), "");

    std::string path = std::string(which == GATE_ENTRY ? "/api/entry" : "/api/exit") + "?plate=" + pl.key;
    // Uhrzeit mitschicken: die Kasse verwirft Anfragen, die zu spaet bei ihr ankommen (siehe KasseCore.h)
    if (nowEpoch >= 1700000000LL) {
      path += "&ts=" + itos(nowEpoch);
    }
    KasseReply kr = link_->post(path);

    bool ok = false;
    if (!kr.reachable) {
      kasse_ = K_ERROR;
      return result(which, false, "connection",
                    "Verbindungsfehler: Die Kasse antwortet nicht – Schranke bleibt geschlossen.", pl.display);
    }
    if (kr.status != 200 || !jsonGetBool(kr.body, "ok", ok)) {
      kasse_ = K_ERROR;
      return result(which, false, "connection",
                    "Verbindungsfehler: Ungültige Antwort der Kasse – Schranke bleibt geschlossen.", pl.display);
    }
    kasse_ = K_OK;

    if (ok) {
      openBarrier(which, nowMs);
      std::string msg = std::string(which == GATE_ENTRY ? "Einfahrt frei: " : "Ausfahrt frei: ") + pl.display +
                        " – Schranke öffnet.";
      return result(which, true, "ok", msg, pl.display);
    }
    // Kasse hat abgelehnt (doppelt, unbekannt, Gebuehr offen, ...): Schranke bleibt zu
    std::string code = "rejected", msg;
    if (!jsonGetString(kr.body, "code", code)) code = "rejected";
    if (!jsonGetString(kr.body, "message", msg)) msg = "Abgewiesen.";
    return result(which, false, code.c_str(), msg, pl.display);
  }

  // Kasse-Adresse aendern: /api/config?kasse=192.168.1.50
  Response config(const Request& rq) {
    std::string h = rq.arg("kasse");
    if (!validHost(h)) {
      JObj o;
      o.boolean("ok", false).str("code", "invalid").str("message", "Ungültige Adresse. Beispiel: 192.168.1.50");
      return jsonResponse(o);
    }
    link_->setHost(h);
    kasse_ = K_UNKNOWN;
    JObj o;
    o.boolean("ok", true).str("kasse", h).str("message", "Kasse-Adresse gespeichert: " + h);
    return jsonResponse(o);
  }

  // Verbindung zur Kasse pruefen (Knopf in der Oberflaeche)
  Response kasseCheck() {
    KasseReply kr = link_->post("/api/ping");
    bool ok = false;
    if (kr.reachable && kr.status == 200 && jsonGetBool(kr.body, "ok", ok) && ok) {
      kasse_ = K_OK;
      i64 count = 0;
      jsonGetNum(kr.body, "count", count);
      JObj o;
      o.boolean("ok", true)
          .boolean("connected", true)
          .str("kasse", link_->host())
          .num("count", count)
          .str("message", "Kasse erreichbar (" + link_->host() + ").");
      return jsonResponse(o);
    }
    kasse_ = K_ERROR;
    JObj o;
    o.boolean("ok", false)
        .boolean("connected", false)
        .str("kasse", link_->host())
        .str("message", "Verbindungsfehler: Die Kasse (" + link_->host() + ") antwortet nicht.");
    return jsonResponse(o);
  }
};

}  // namespace park
