// KasseCore.h - Logik der Kasse (Pico 1): Fahrzeuge speichern, Gebuehr berechnen,
// bezahlen, ausfahren. Reines C++ ohne Arduino-Funktionen (auf dem PC testbar).
// Die Uhrzeit wird von aussen als "now" (Sekunden seit 1970, UTC) uebergeben.
#pragma once
#include <algorithm>
#include "Common.h"
#include "config.h"
#include "pages.h"

namespace park {

// Tarif (Annahme A1 und A5 im Testkonzept). Aufenthalt in ganzen Minuten.
//   0-60 Min: CHF 2.00 | 61-180: 3.50 | 181-240: 5.00 | 241-300: 6.00 | 301-360: 7.00
//   ab 361 Min: plus CHF 1.00 pro angefangene weitere Stunde
inline i64 feeRappen(i64 minutes) {
  if (minutes < 0) minutes = 0;
  if (minutes <= 60) return 200;
  if (minutes <= 180) return 350;
  if (minutes <= 240) return 500;
  if (minutes <= 300) return 600;
  if (minutes <= 360) return 700;
  return 700 + 100 * ((minutes - 360 + 59) / 60);
}

// Uhr gilt erst als gestellt, wenn sie nach November 2023 steht (sonst NTP noch nicht fertig)
inline bool timeIsValid(i64 now) { return now >= 1700000000LL; }

// Die Schranken warten hoechstens 3 Sekunden auf die Kasse. Antwortet die Kasse erst spaeter
// (z. B. weil sie kurz haengt), darf sie die alte Anfrage nicht mehr ausfuehren: Sonst waere
// ein Fahrzeug eingetragen oder ausgetragen, obwohl die Schranke geschlossen blieb.
// Darum schicken die Schranken ihre Uhrzeit (ts) mit; ist sie aelter als STALE_SECONDS, wird die
// Anfrage verworfen. Beide Picos holen die Uhrzeit per NTP (Annahme A6).
static const i64 STALE_SECONDS = 2;

struct Vehicle {
  bool used;
  char key[10];
  char plate[12];
  i64 entry;       // Einfahrtszeit (Sekunden seit 1970)
  bool paid;
  i64 paidRappen;  // bezahlter Betrag
};

class KasseApp {
 public:
  static const int MAX_VEHICLES = 60;
  static const i64 MAX_TEST_MINUTES = 99999;

  KasseApp() { reset(); }
  void reset() {
    for (int i = 0; i < MAX_VEHICLES; i++) v_[i].used = false;
  }

  int count() const {
    int n = 0;
    for (int i = 0; i < MAX_VEHICLES; i++) n += v_[i].used ? 1 : 0;
    return n;
  }

  Response handle(const Request& rq, i64 now) {
    const std::string& p = rq.path;
    if (p == "/" || p == "/index.html" || p == "/automat") {
      Response r;
      r.type = "text/html; charset=utf-8";
      r.body = KASSE_HTML;
      return r;
    }
    if (p == "/favicon.ico") {
      Response r;
      r.status = 204;
      r.type = "text/plain";
      return r;
    }
    if (p == "/api/ping") return ping(now);
    if (p == "/api/list") return list(now);
    if (p == "/api/fee") return fee(rq, now);
    if (p == "/api/pay") return pay(rq, now);
    if (p == "/api/entry") return entry(rq, now);
    if (p == "/api/exit") return exitCar(rq, now);
    if (p == "/api/testfield") return testfield(rq, now);
    Response r;
    r.status = 404;
    r.type = "text/plain; charset=utf-8";
    r.body = "Nicht gefunden";
    return r;
  }

 private:
  Vehicle v_[MAX_VEHICLES];

  Vehicle* find(const char* key) {
    for (int i = 0; i < MAX_VEHICLES; i++)
      if (v_[i].used && strcmp(v_[i].key, key) == 0) return &v_[i];
    return 0;
  }

  static i64 minutesOf(const Vehicle& v, i64 now) {
    i64 s = now - v.entry;
    return s < 0 ? 0 : s / 60;  // Sekunden werden abgeschnitten
  }
  static i64 openOf(const Vehicle& v, i64 now) {
    return v.paid ? 0 : feeRappen(minutesOf(v, now));
  }

  static Response fail(const char* code, const std::string& msg) {
    JObj o;
    o.boolean("ok", false).str("code", code).str("message", msg);
    return jsonResponse(o);
  }
  // true = Anfrage ist zu alt (siehe STALE_SECONDS). Ohne 'ts' wird nichts geprueft.
  static bool isStale(const Request& rq, i64 now) {
    if (!rq.has("ts") || !timeIsValid(now)) return false;
    std::string t = rq.arg("ts");
    char* end = 0;
    long long ts = strtoll(t.c_str(), &end, 10);
    if (t.empty() || *end != 0) return true;
    long long d = now - ts;
    return d > STALE_SECONDS || d < -STALE_SECONDS;
  }
  static Response staleReply() {
    return fail("stale", "Anfrage zu alt und verworfen – bitte erneut versuchen.");
  }

  static Response unknownPlate() {
    return fail("unknown", "Kennzeichen unbekannt – Fahrzeug steht nicht im Parkhaus.");
  }

  // Kennzeichen aus dem Request lesen; bei Fehler ist 'bad' gesetzt
  static bool readPlate(const Request& rq, Plate& pl, Response& bad) {
    pl = parsePlate(rq.arg("plate").c_str());
    if (pl.status != PLATE_OK) {
      bad = fail("invalid", plateMessage(pl));
      return false;
    }
    return true;
  }

  static std::string vehicleJson(const Vehicle& v, i64 now) {
    i64 min = minutesOf(v, now);
    JObj o;
    o.str("plate", v.plate)
        .num("entry", v.entry)
        .num("minutes", min)
        .num("fee", feeRappen(min))
        .boolean("paid", v.paid)
        .num("paidAmount", v.paid ? v.paidRappen : 0)
        .num("open", openOf(v, now));
    return o.done();
  }

  Response ping(i64 now) {
    JObj o;
    o.boolean("ok", true)
        .str("service", "kasse")
        .num("now", now)
        .boolean("timeValid", timeIsValid(now))
        .boolean("testfield", TESTFIELD_ENABLED != 0)
        .num("count", count());
    return jsonResponse(o);
  }

  Response list(i64 now) {
    std::vector<int> idx;
    for (int i = 0; i < MAX_VEHICLES; i++)
      if (v_[i].used) idx.push_back(i);
    // nach Einfahrtszeit sortieren (aelteste zuerst)
    for (size_t a = 0; a < idx.size(); a++)
      for (size_t b = a + 1; b < idx.size(); b++)
        if (v_[idx[b]].entry < v_[idx[a]].entry) std::swap(idx[a], idx[b]);
    std::string arr = "[";
    for (size_t i = 0; i < idx.size(); i++) {
      if (i) arr += ',';
      arr += vehicleJson(v_[idx[i]], now);
    }
    arr += "]";
    JObj o;
    o.boolean("ok", true).num("now", now).num("count", (i64)idx.size()).raw("vehicles", arr);
    return jsonResponse(o);
  }

  // US-3: Gebuehr anzeigen
  Response fee(const Request& rq, i64 now) {
    Plate pl;
    Response bad;
    if (!readPlate(rq, pl, bad)) return bad;
    Vehicle* v = find(pl.key);
    if (!v) return unknownPlate();
    i64 min = minutesOf(*v, now);
    JObj o;
    o.boolean("ok", true)
        .str("plate", v->plate)
        .num("entry", v->entry)
        .num("now", now)
        .num("minutes", min)
        .num("fee", feeRappen(min))
        .boolean("paid", v->paid)
        .num("paidAmount", v->paid ? v->paidRappen : 0)
        .num("open", openOf(*v, now));
    return jsonResponse(o);
  }

  // US-4: bezahlen. Optional 'amount' (Rappen) = der Betrag, den der Kunde gesehen hat.
  Response pay(const Request& rq, i64 now) {
    Plate pl;
    Response bad;
    if (!readPlate(rq, pl, bad)) return bad;
    Vehicle* v = find(pl.key);
    if (!v) return unknownPlate();
    if (v->paid)
      return fail("already_paid", "Bereits bezahlt (" + chf(v->paidRappen) + ") – keine zweite Zahlung möglich.");
    i64 current = feeRappen(minutesOf(*v, now));
    if (rq.has("amount")) {
      std::string a = rq.arg("amount");
      char* end = 0;
      long long want = strtoll(a.c_str(), &end, 10);
      if (a.empty() || *end != 0 || want != current)
        return fail("fee_changed", "Die Gebühr hat sich geändert, jetzt " + chf(current) + " – bitte erneut abfragen.");
    }
    v->paid = true;
    v->paidRappen = current;
    JObj o;
    o.boolean("ok", true)
        .str("plate", v->plate)
        .num("paidAmount", current)
        .num("minutes", minutesOf(*v, now))
        .str("message", "Zahlung bestätigt: " + chf(current) + ".");
    return jsonResponse(o);
  }

  // US-1/US-2: Einfahrt registrieren
  Response entry(const Request& rq, i64 now) {
    Plate pl;
    Response bad;
    if (!readPlate(rq, pl, bad)) return bad;
    if (isStale(rq, now)) return staleReply();
    if (!timeIsValid(now)) return fail("time", "Die Uhrzeit der Kasse ist noch nicht gestellt (NTP) – keine Einfahrt möglich.");
    if (find(pl.key)) return fail("duplicate", "Fahrzeug steht schon im Parkhaus.");
    Vehicle* slot = 0;
    for (int i = 0; i < MAX_VEHICLES; i++)
      if (!v_[i].used) { slot = &v_[i]; break; }
    if (!slot) return fail("full", "Die Kasse kann keine weiteren Fahrzeuge speichern.");
    slot->used = true;
    snprintf(slot->key, sizeof(slot->key), "%s", pl.key);
    snprintf(slot->plate, sizeof(slot->plate), "%s", pl.display);
    slot->entry = now;
    slot->paid = false;
    slot->paidRappen = 0;
    JObj o;
    o.boolean("ok", true)
        .str("plate", slot->plate)
        .num("entry", now)
        .str("message", "Einfahrt registriert.");
    return jsonResponse(o);
  }

  // US-5: Ausfahrt nur wenn bezahlt. Prueft und entfernt in einem Schritt.
  Response exitCar(const Request& rq, i64 now) {
    Plate pl;
    Response bad;
    if (!readPlate(rq, pl, bad)) return bad;
    if (isStale(rq, now)) return staleReply();
    Vehicle* v = find(pl.key);
    if (!v) return unknownPlate();
    if (!v->paid) {
      i64 open = openOf(*v, now);
      JObj o;
      o.boolean("ok", false)
          .str("code", "unpaid")
          .num("open", open)
          .str("message", "Gebühr offen: " + chf(open) + " – bitte zuerst an der Kasse oder am Automaten bezahlen.");
      return jsonResponse(o);
    }
    JObj o;
    o.boolean("ok", true)
        .str("plate", v->plate)
        .num("paidAmount", v->paidRappen)
        .str("message", "Ausfahrt frei.");
    v->used = false;
    return jsonResponse(o);
  }

  // Testfeld: Einfahrtszeit auf "jetzt minus X Minuten" setzen
  Response testfield(const Request& rq, i64 now) {
    if (!TESTFIELD_ENABLED) {
      Response r;
      r.status = 404;
      r.type = "text/plain; charset=utf-8";
      r.body = "Nicht gefunden";
      return r;
    }
    Plate pl;
    Response bad;
    if (!readPlate(rq, pl, bad)) return bad;
    std::string m = rq.arg("minutes");
    bool digits = !m.empty() && m.size() <= 6;
    for (size_t i = 0; i < m.size(); i++)
      if (m[i] < '0' || m[i] > '9') digits = false;
    long long minutes = digits ? atoll(m.c_str()) : -1;
    if (minutes < 0 || minutes > MAX_TEST_MINUTES)
      return fail("invalid", "Minuten müssen eine ganze Zahl von 0 bis 99999 sein.");
    if (!timeIsValid(now)) return fail("time", "Die Uhrzeit der Kasse ist noch nicht gestellt (NTP).");
    Vehicle* v = find(pl.key);
    if (!v) return unknownPlate();
    v->entry = now - minutes * 60;
    JObj o;
    o.boolean("ok", true)
        .str("plate", v->plate)
        .num("entry", v->entry)
        .num("minutes", minutes)
        .str("message", "Aufenthalt gesetzt: " + itos(minutes) + " Minuten. Bitte innert 30 Sekunden abfragen.");
    return jsonResponse(o);
  }
};

}  // namespace park
