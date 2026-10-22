// Testfaelle TC-01 bis TC-42 gegen die Logik, ohne Hardware und Netzwerk.
#include <stdio.h>
#include <algorithm>
#include <map>
#include <set>
#include "../kasse/KasseCore.h"
#define SCHRANKEN_TEST 1

namespace kasse_ns {}  // (nur zur Lesbarkeit)

#include "../schranken/SchrankenCore.h"

using namespace park;

static int g_pass = 0, g_fail = 0;
#define CHECK(id, cond)                                                       \
  do {                                                                        \
    if (cond) { g_pass++; }                                                   \
    else { g_fail++; printf("  FAIL %-8s %s   (Zeile %d)\n", id, #cond, __LINE__); } \
  } while (0)

static bool has(const std::string& hay, const char* needle) { return hay.find(needle) != std::string::npos; }

// Verbindung Schranken -> Kasse (direkt, mit Schalter fuer "Kabel gezogen")
struct Env;
struct DirectLink : KasseLink {
  KasseApp* kasse;
  i64* now;
  bool up;
  int mode;  // 0 normal, 1 Muell zurueckgeben, 2 Status 500
  std::string host_;
  DirectLink() : kasse(0), now(0), up(true), mode(0), host_("192.168.1.50") {}
  static Request parse(const std::string& pq) {
    Request r;
    r.method = "POST";
    size_t q = pq.find('?');
    r.path = pq.substr(0, q);
    if (q != std::string::npos) {
      std::string s = pq.substr(q + 1);
      size_t i = 0;
      while (i < s.size()) {
        size_t amp = s.find('&', i);
        std::string kv = s.substr(i, amp == std::string::npos ? std::string::npos : amp - i);
        size_t eq = kv.find('=');
        r.args.push_back(std::make_pair(kv.substr(0, eq), eq == std::string::npos ? "" : kv.substr(eq + 1)));
        if (amp == std::string::npos) break;
        i = amp + 1;
      }
    }
    return r;
  }
  KasseReply post(const std::string& pq) override {
    KasseReply k;
    if (!up) return k;
    k.reachable = true;
    if (mode == 1) { k.status = 200; k.body = "<html>kaputt</html>"; return k; }
    if (mode == 2) { k.status = 500; k.body = "{}"; return k; }
    Response r = kasse->handle(parse(pq), *now);
    k.status = r.status;
    k.body = r.body;
    return k;
  }
  std::string host() const override { return host_; }
  void setHost(const std::string& h) override { host_ = h; }
};

struct Drv : BarrierDriver {
  bool open[2];
  int moves;
  Drv() : moves(0) { open[0] = open[1] = false; }
  void set(int w, bool o) override { open[w] = o; moves++; }
};

struct Res {
  bool ok;
  std::string code, message, barrier, plate;
};

struct Env {
  KasseApp kasse;
  DirectLink link;
  Drv drv;
  SchrankenApp sch;
  i64 now;
  unsigned long ms;
  Env() : sch(&link, &drv, 6000), now(1790000000LL), ms(1000) {
    link.kasse = &kasse;
    link.now = &now;
    sch.begin();
  }
  void advance(int seconds) {
    now += seconds;
    ms += (unsigned long)seconds * 1000;
    sch.tick(ms);
  }
  static Request req(const char* path, const char* k1 = 0, const std::string& v1 = "", const char* k2 = 0,
                     const std::string& v2 = "") {
    Request r;
    r.method = "POST";
    r.path = path;
    if (k1) r.args.push_back(std::make_pair(std::string(k1), v1));
    if (k2) r.args.push_back(std::make_pair(std::string(k2), v2));
    return r;
  }
  static Res parseRes(const std::string& body) {
    Res r;
    r.ok = false;
    jsonGetBool(body, "ok", r.ok);
    jsonGetString(body, "code", r.code);
    jsonGetString(body, "message", r.message);
    jsonGetString(body, "barrier", r.barrier);
    jsonGetString(body, "plate", r.plate);
    return r;
  }
  Res enter(const std::string& plate) {
    return parseRes(sch.handle(req("/api/entry", "plate", plate), ms, now).body);
  }
  Res leave(const std::string& plate) {
    return parseRes(sch.handle(req("/api/exit", "plate", plate), ms, now).body);
  }
  std::string fee(const std::string& plate) { return kasse.handle(req("/api/fee", "plate", plate), now).body; }
  std::string pay(const std::string& plate, long long amount = -1) {
    Request r = req("/api/pay", "plate", plate);
    if (amount >= 0) r.args.push_back(std::make_pair(std::string("amount"), std::to_string(amount)));
    return kasse.handle(r, now).body;
  }
  std::string setMin(const std::string& plate, long long minutes) {
    return kasse.handle(req("/api/testfield", "plate", plate, "minutes", std::to_string(minutes)), now).body;
  }
  std::string list() { return kasse.handle(req("/api/list"), now).body; }
  bool inList(const char* plate) { return has(list(), (std::string("\"plate\":\"") + plate + "\"").c_str()); }
  int listCount() {
    i64 c = -1;
    jsonGetNum(list(), "count", c);
    return (int)c;
  }
};

static i64 num(const std::string& j, const char* k) {
  i64 v = -999999;
  jsonGetNum(j, k, v);
  return v;
}
static bool flag(const std::string& j, const char* k) {
  bool b = false;
  jsonGetBool(j, k, b);
  return b;
}

static void testPlates() {
  printf("[Kennzeichen]\n");
  struct { const char* in; PlateStatus st; const char* disp; } t[] = {
      {"TI 2468", PLATE_OK, "TI 2468"},       {"VS 864209", PLATE_OK, "VS 864209"},
      {"FR 3", PLATE_OK, "FR 3"},             {"  gr  100 ", PLATE_OK, "GR 100"},
      {"TI2468", PLATE_OK, "TI 2468"},        {" ti2468 ", PLATE_OK, "TI 2468"},
      {"ti\t2468\r\n", PLATE_OK, "TI 2468"},  {"2468", PLATE_NO_LETTERS, ""},
      {"T 2468", PLATE_ONE_LETTER, ""},       {"TI 7654321", PLATE_TOO_MANY_DIGITS, ""},
      {"TI", PLATE_NO_DIGITS, ""},            {"TI-2468", PLATE_BAD_CHAR, ""},
      {"", PLATE_EMPTY, ""},                  {"   ", PLATE_EMPTY, ""},
      {"ABC 123", PLATE_TOO_MANY_LETTERS, ""},{"12 AB", PLATE_NO_LETTERS, ""},
      {"TI 12 AB", PLATE_BAD_ORDER, ""},      {"T", PLATE_ONE_LETTER, ""},
      {"TI 1234567", PLATE_TOO_MANY_DIGITS, ""},{"TI 123456", PLATE_OK, "TI 123456"},
      {"ZÜ 123", PLATE_BAD_CHAR, ""},         {"TI \"1\"", PLATE_BAD_CHAR, ""},
      {"TI 24.68", PLATE_BAD_CHAR, ""},
  };
  for (size_t i = 0; i < sizeof(t) / sizeof(t[0]); i++) {
    Plate p = parsePlate(t[i].in);
    bool ok = p.status == t[i].st && (t[i].st != PLATE_OK || std::string(p.display) == t[i].disp);
    if (!ok) printf("  Eingabe '%s' -> Status %d '%s'\n", t[i].in, p.status, p.display);
    CHECK("PLATE", ok);
  }
  CHECK("PLATE", parsePlate(0).status == PLATE_EMPTY);
  // sehr lange Eingabe darf nicht abstuerzen
  CHECK("PLATE", parsePlate(std::string(5000, 'A').c_str()).status == PLATE_TOO_MANY_LETTERS);
  CHECK("PLATE", parsePlate((std::string("TI") + std::string(5000, '1')).c_str()).status == PLATE_TOO_MANY_DIGITS);
  // Anfuehrungszeichen im Kennzeichen muss im JSON sauber maskiert werden
  Env e;
  std::string r = e.sch.handle(Env::req("/api/entry", "plate", "TI \"1\""), e.ms, e.now).body;
  std::string m;
  CHECK("PLATE", jsonGetString(r, "message", m) && has(m, "\"") );
}

static void testFee() {
  printf("[Tarif]\n");
  struct { i64 min, rappen; } t[] = {
      {0, 200}, {1, 200}, {59, 200}, {60, 200}, {61, 350}, {90, 350}, {120, 350}, {121, 350}, {130, 350},
      {180, 350}, {181, 500}, {200, 500}, {240, 500}, {241, 600}, {300, 600}, {301, 700}, {360, 700},
      {361, 800}, {420, 800}, {421, 900}, {480, 900}, {481, 1000}, {1440, 700 + 100 * 18}, {-5, 200}};
  for (size_t i = 0; i < sizeof(t) / sizeof(t[0]); i++) {
    bool ok = feeRappen(t[i].min) == t[i].rappen;
    if (!ok) printf("  %lld min -> %lld statt %lld\n", t[i].min, feeRappen(t[i].min), t[i].rappen);
    CHECK("FEE", ok);
  }
  CHECK("FEE", itos(0) == "0" && itos(-5) == "-5" && itos(1790000000123LL) == "1790000000123" &&
                   itos(-9223372036854775807LL - 1) == "-9223372036854775808" && itos(10) == "10");
  // Beispiele aus dem Testkonzept: 75 Minuten = CHF 3.50, 4 h 10 min = CHF 6.00
  CHECK("FEE", feeRappen(75) == 350);
  CHECK("FEE", feeRappen(4 * 60 + 10) == 600);
  CHECK("FEE", chf(350) == "CHF 3.50" && chf(200) == "CHF 2.00" && chf(0) == "CHF 0.00" && chf(705) == "CHF 7.05");
  // Sekunden werden abgeschnitten: 60 min 59 s sind noch 60 Minuten
  Env e;
  e.enter("TI 2468");
  e.advance(60 * 60 + 59);
  CHECK("FEE", num(e.fee("TI 2468"), "minutes") == 60 && num(e.fee("TI 2468"), "fee") == 200);
  e.advance(1);
  CHECK("FEE", num(e.fee("TI 2468"), "minutes") == 61 && num(e.fee("TI 2468"), "fee") == 350);
}

// Die 42 Testfaelle des Testkonzepts in der vorgegebenen Reihenfolge
static void testTestkonzept() {
  printf("[Testfaelle TC-01 bis TC-42]\n");
  Env e;
  Res r;
  std::string j;

  // TC-01
  r = e.enter("TI 2468");
  CHECK("TC-01", r.ok && r.barrier == "open" && e.drv.open[0]);
  CHECK("TC-01", e.inList("TI 2468"));
  // TC-02: schliesst spaetestens nach 10 s
  e.advance(5);
  CHECK("TC-02", e.drv.open[0]);
  e.advance(1);
  CHECK("TC-02", !e.drv.open[0]);
  // TC-03: Einfahrtszeit stimmt (Abweichung <= 1 Minute)
  j = e.fee("TI 2468");
  CHECK("TC-03", llabs(num(j, "entry") - (e.now - 6)) <= 60 && num(j, "entry") == e.now - 6);
  // TC-04
  e.advance(10);
  r = e.enter("VS 864209");
  CHECK("TC-04", r.ok && r.barrier == "open");
  CHECK("TC-04", e.inList("TI 2468") && e.inList("VS 864209") && e.listCount() == 2);
  CHECK("TC-04", num(e.fee("TI 2468"), "entry") != num(e.fee("VS 864209"), "entry"));
  // TC-05
  r = e.enter("FR 3");
  CHECK("TC-05", r.ok && e.inList("FR 3"));
  // TC-06
  r = e.enter("  gr  100 ");
  CHECK("TC-06", r.ok && r.plate == "GR 100" && e.inList("GR 100"));
  e.advance(7);
  // TC-07 doppelte Einfahrt
  int before = e.listCount();
  r = e.enter("TI 2468");
  CHECK("TC-07", !r.ok && r.barrier == "closed" && !e.drv.open[0] && has(r.message, "schon im Parkhaus"));
  CHECK("TC-07", e.listCount() == before);
  // TC-08
  r = e.enter("TI2468");
  CHECK("TC-08", !r.ok && has(r.message, "schon im Parkhaus") && e.listCount() == before);
  // TC-09 .. TC-14
  r = e.enter("2468");
  CHECK("TC-09", !r.ok && !e.drv.open[0] && has(r.message, "Ungültiges Format") && has(r.message, "Buchstaben fehlen"));
  e.advance(12);  // die Meldung bleibt, es gibt keinen Timer
  CHECK("TC-09", true);
  r = e.enter("T 2468");
  CHECK("TC-10", !r.ok && !e.drv.open[0] && has(r.message, "Ungültiges Format") && has(r.message, "nur ein Buchstabe"));
  r = e.enter("TI 7654321");
  CHECK("TC-11", !r.ok && !e.drv.open[0] && has(r.message, "Ungültiges Format") && has(r.message, "mehr als sechs Ziffern"));
  r = e.enter("TI");
  CHECK("TC-12", !r.ok && !e.drv.open[0] && has(r.message, "keine Ziffern"));
  r = e.enter("TI-2468");
  CHECK("TC-13", !r.ok && !e.drv.open[0] && has(r.message, "Ungültiges Zeichen"));
  r = e.enter("");
  CHECK("TC-14", !r.ok && !e.drv.open[0] && has(r.message, "Kennzeichen fehlt"));
  CHECK("TC-14", e.listCount() == before);
  // TC-15
  r = e.enter("SZ 3300");
  CHECK("TC-15", r.ok && e.drv.open[0] && e.inList("SZ 3300") && e.listCount() == before + 1);

  // TC-16 .. TC-22 Gebuehr
  e.advance(7);
  j = e.setMin("TI 2468", 0);
  CHECK("TC-16", flag(j, "ok"));
  j = e.fee("TI 2468");
  CHECK("TC-16", num(j, "fee") == 200 && num(j, "minutes") == 0);
  e.setMin("TI 2468", 60);
  CHECK("TC-17", num(e.fee("TI 2468"), "fee") == 200);
  e.setMin("TI 2468", 61);
  CHECK("TC-18", num(e.fee("TI 2468"), "fee") == 350);
  e.setMin("TI 2468", 120);
  CHECK("TC-18", num(e.fee("TI 2468"), "fee") == 350);
  e.setMin("TI 2468", 180);
  CHECK("TC-19", num(e.fee("TI 2468"), "fee") == 350);
  e.setMin("TI 2468", 181);
  CHECK("TC-20", num(e.fee("TI 2468"), "fee") == 500);
  e.setMin("TI 2468", 240);
  CHECK("TC-21", num(e.fee("TI 2468"), "fee") == 500);
  e.setMin("TI 2468", 241);
  CHECK("TC-21", num(e.fee("TI 2468"), "fee") == 600);
  e.setMin("TI 2468", 300);
  CHECK("TC-22", num(e.fee("TI 2468"), "fee") == 600);
  e.setMin("TI 2468", 301);
  CHECK("TC-22", num(e.fee("TI 2468"), "fee") == 700);
  // TC-23 unbekannt
  j = e.fee("UR 8008");
  CHECK("TC-23", !flag(j, "ok") && has(j, "nicht im Parkhaus") && !has(j, "\"fee\""));
  j = e.pay("UR 8008");
  CHECK("TC-23", !flag(j, "ok"));
  // TC-24
  j = e.fee(" ti2468 ");
  CHECK("TC-24", flag(j, "ok") && has(j, "TI 2468") && num(j, "fee") == 700);

  // TC-25 bezahlen an der Kasse
  e.setMin("VS 864209", 130);
  j = e.fee("VS 864209");
  CHECK("TC-25", num(j, "fee") == 350 && !flag(j, "paid"));
  j = e.pay("VS 864209", 350);
  CHECK("TC-25", flag(j, "ok") && num(j, "paidAmount") == 350);
  j = e.fee("VS 864209");
  CHECK("TC-25", flag(j, "paid") && num(j, "paidAmount") == 350);
  // TC-26
  CHECK("TC-26", flag(j, "paid") && num(j, "open") == 0);
  // TC-27 keine zweite Zahlung
  j = e.pay("VS 864209", 350);
  CHECK("TC-27", !flag(j, "ok") && has(j, "already_paid"));
  j = e.fee("VS 864209");
  CHECK("TC-27", num(j, "paidAmount") == 350 && flag(j, "paid"));
  // TC-28 Automat
  e.setMin("FR 3", 200);
  j = e.fee("FR 3");
  CHECK("TC-28", num(j, "fee") == 500);
  j = e.pay("FR 3", 500);
  CHECK("TC-28", flag(j, "ok") && num(j, "paidAmount") == 500);
  j = e.fee("FR 3");
  CHECK("TC-28", flag(j, "paid") && num(j, "open") == 0);
  // TC-29
  j = e.pay("UR 8008", 200);
  CHECK("TC-29", !flag(j, "ok") && has(j, "Kennzeichen unbekannt"));

  // TC-30 Ausfahrt ohne Bezahlung
  e.setMin("SZ 3300", 90);
  r = e.leave("SZ 3300");
  CHECK("TC-30", !r.ok && r.barrier == "closed" && !e.drv.open[1] && has(r.message, "CHF 3.50") && r.code == "unpaid");
  // TC-31
  r = e.leave("VS 864209");
  CHECK("TC-31", r.ok && e.drv.open[1] && !e.inList("VS 864209"));
  // TC-32
  e.advance(5);
  CHECK("TC-32", e.drv.open[1]);
  e.advance(1);
  CHECK("TC-32", !e.drv.open[1]);
  // TC-33
  r = e.leave("VS 864209");
  CHECK("TC-33", !r.ok && !e.drv.open[1] && has(r.message, "nicht im Parkhaus"));
  // TC-34
  r = e.leave("UR 8008");
  CHECK("TC-34", !r.ok && !e.drv.open[1] && has(r.message, "Kennzeichen unbekannt"));
  // TC-35
  r = e.leave("FR 3");
  CHECK("TC-35", r.ok && e.drv.open[1] && !e.inList("FR 3"));
  e.advance(7);
  // TC-36 Startgebuehr
  e.setMin("GR 100", 0);
  r = e.leave("GR 100");
  CHECK("TC-36", !r.ok && !e.drv.open[1] && has(r.message, "CHF 2.00") && e.inList("GR 100"));
  // TC-37 Wiedereinfahrt
  r = e.enter("VS 864209");
  CHECK("TC-37", r.ok && e.inList("VS 864209"));
  e.setMin("VS 864209", 30);
  j = e.fee("VS 864209");
  CHECK("TC-37", !flag(j, "paid") && num(j, "paidAmount") == 0);
  r = e.leave("VS 864209");
  CHECK("TC-37", !r.ok && !e.drv.open[1] && has(r.message, "CHF 2.00"));
  // TC-38 Fahrzeugliste am Schluss
  {
    std::set<std::string> want, got;
    want.insert("TI 2468"); want.insert("SZ 3300"); want.insert("GR 100"); want.insert("VS 864209");
    const char* names[] = {"TI 2468", "SZ 3300", "GR 100", "VS 864209", "FR 3", "UR 8008", "NW 12"};
    for (size_t i = 0; i < 7; i++) if (e.inList(names[i])) got.insert(names[i]);
    CHECK("TC-38", got == want && e.listCount() == 4);
  }

  // TC-39 Ausfahrt bei getrenntem Kassen-Pico
  e.advance(7);  // Schranken von frueher sind wieder zu
  e.setMin("VS 864209", 30);
  j = e.pay("VS 864209", 200);
  CHECK("TC-39", flag(j, "ok"));
  e.link.up = false;
  r = e.leave("VS 864209");
  CHECK("TC-39", !r.ok && r.code == "connection" && !e.drv.open[1] && has(r.message, "Verbindungsfehler"));
  CHECK("TC-39", !has(r.message, "Gebühr offen"));  // andere Meldung als bei offener Gebuehr (AK-6.3)
  // TC-40 Einfahrt bei getrenntem Kassen-Pico
  r = e.enter("NW 12");
  CHECK("TC-40", !r.ok && r.code == "connection" && !e.drv.open[0] && has(r.message, "Verbindungsfehler"));
  // TC-41 Weiterbetrieb
  e.link.up = true;
  r = e.leave("VS 864209");
  CHECK("TC-41", r.ok && e.drv.open[1]);
  CHECK("TC-41", !e.inList("NW 12"));
  j = e.sch.handle(Env::req("/api/state"), e.ms, e.now).body;
  CHECK("TC-41", flag(j, "ok") && has(j, "\"kasseState\":\"ok\""));
  // TC-42: nur aufschreiben - Neustart der Kasse loescht die Daten (RAM)
  int vorher = e.listCount();
  e.kasse.reset();
  printf("  TC-42 (Info): vor Neustart der Kasse %d Fahrzeuge, danach %d (Daten sind nach Neustart weg)\n", vorher,
         e.listCount());
}

static void testExtras() {
  printf("[Zusatzfaelle]\n");
  Env e;
  Res r;
  std::string j;

  // Schranken sind beim Start zu und es gibt keinen Eintrag ohne Einfahrtszeit
  CHECK("EXTRA", !e.drv.open[0] && !e.drv.open[1]);

  // Kasse ohne gueltige Uhrzeit: keine Einfahrt, kein Eintrag (US-6: nie ein Fahrzeug ohne Einfahrtszeit)
  e.now = 5000;
  r = e.enter("TI 2468");
  CHECK("EXTRA", !r.ok && !e.drv.open[0] && e.listCount() == 0 && has(r.message, "Uhrzeit"));
  e.now = 1790000000LL;

  // Kasse liefert Muell oder Status 500 -> Verbindungsfehler, Schranke zu
  e.link.mode = 1;
  r = e.enter("TI 2468");
  CHECK("EXTRA", !r.ok && r.code == "connection" && !e.drv.open[0]);
  e.link.mode = 2;
  r = e.leave("TI 2468");
  CHECK("EXTRA", !r.ok && r.code == "connection" && !e.drv.open[1]);
  e.link.mode = 0;

  // Gebuehr springt zwischen Abfrage und Zahlung -> Zahlung wird abgelehnt, nichts verbucht
  e.enter("TI 2468");
  e.setMin("TI 2468", 60);
  j = e.fee("TI 2468");
  CHECK("EXTRA", num(j, "fee") == 200);
  e.advance(61);  // jetzt 61 Minuten: CHF 3.50
  j = e.pay("TI 2468", 200);
  CHECK("EXTRA", !flag(j, "ok") && has(j, "fee_changed") && has(j, "CHF 3.50"));
  j = e.fee("TI 2468");
  CHECK("EXTRA", !flag(j, "paid"));
  r = e.leave("TI 2468");
  CHECK("EXTRA", !r.ok && has(r.message, "CHF 3.50"));
  j = e.pay("TI 2468", 350);
  CHECK("EXTRA", flag(j, "ok") && num(j, "paidAmount") == 350);
  // falscher oder kaputter Betrag
  e.enter("AB 1");
  j = e.pay("AB 1", 999);
  CHECK("EXTRA", !flag(j, "ok") && has(j, "fee_changed"));
  Request rq = Env::req("/api/pay", "plate", "AB 1", "amount", "abc");
  CHECK("EXTRA", !flag(e.kasse.handle(rq, e.now).body, "ok"));
  // ohne amount wird der aktuelle Betrag gebucht
  j = e.pay("AB 1");
  CHECK("EXTRA", flag(j, "ok") && num(j, "paidAmount") == 200);

  // Verspaetete Anfragen (ts zu alt) werden von der Kasse verworfen und aendern nichts
  {
    Env s;
    char ts[32];
    auto withTs = [&](const char* path, const char* plate, long long t) {
      Request q = Env::req(path, "plate", plate);
      snprintf(ts, sizeof(ts), "%lld", t);
      q.args.push_back(std::make_pair(std::string("ts"), std::string(ts)));
      return s.kasse.handle(q, s.now).body;
    };
    j = withTs("/api/entry", "TI 2468", s.now - 3);
    CHECK("STALE", !flag(j, "ok") && has(j, "stale") && s.listCount() == 0);
    j = withTs("/api/entry", "TI 2468", s.now + 3);
    CHECK("STALE", !flag(j, "ok") && has(j, "stale") && s.listCount() == 0);
    j = withTs("/api/entry", "TI 2468", s.now - 2);
    CHECK("STALE", flag(j, "ok") && s.listCount() == 1);  // Grenze: 2 s Unterschied sind noch ok
    s.pay("TI 2468");
    j = withTs("/api/exit", "TI 2468", s.now - 10);
    CHECK("STALE", !flag(j, "ok") && has(j, "stale") && s.inList("TI 2468"));
    Request bad2 = Env::req("/api/exit", "plate", "TI 2468", "ts", "abc");
    CHECK("STALE", has(s.kasse.handle(bad2, s.now).body, "stale") && s.inList("TI 2468"));
    j = withTs("/api/exit", "TI 2468", s.now);
    CHECK("STALE", flag(j, "ok") && !s.inList("TI 2468"));
    // Die Schranken schicken ts wirklich mit: eine Kasse mit gestellter Uhr weit weg verwirft alles
    Env t2;
    t2.link.now = new i64(t2.now + 100);  // Kasse laeuft 100 s vor
    Res rr = t2.enter("TI 2468");
    CHECK("STALE", !rr.ok && !t2.drv.open[0] && t2.listCount() == 0);
    delete t2.link.now;
    t2.link.now = &t2.now;
  }

  // Zahlung wird nach der Ausfahrt und Wiedereinfahrt nicht uebernommen
  r = e.leave("AB 1");
  CHECK("EXTRA", r.ok);
  r = e.leave("AB 1");
  CHECK("EXTRA", !r.ok);
  r = e.enter("AB 1");
  CHECK("EXTRA", r.ok);
  r = e.leave("AB 1");
  CHECK("EXTRA", !r.ok && has(r.message, "CHF 2.00"));

  // Testfeld: ungueltige Werte
  const char* bad[] = {"", "-1", "abc", "1.5", "100000", "1000000", "12 3", " "};
  for (size_t i = 0; i < sizeof(bad) / sizeof(bad[0]); i++) {
    j = e.setMin("TI 2468", 0);  // gueltig vorher
    std::string x = e.kasse.handle(Env::req("/api/testfield", "plate", "TI 2468", "minutes", bad[i]), e.now).body;
    CHECK("EXTRA", !flag(x, "ok") && has(x, "invalid"));
  }
  j = e.setMin("UR 8008", 10);
  CHECK("EXTRA", !flag(j, "ok") && has(j, "unknown"));
  j = e.setMin("TI 2468", 99999);
  CHECK("EXTRA", flag(j, "ok") && num(e.fee("TI 2468"), "minutes") == 99999);

  // Kasse-Adresse aendern
  r = Env::parseRes(e.sch.handle(Env::req("/api/config", "kasse", "10.0.0.7"), e.ms, e.now).body);
  CHECK("EXTRA", r.ok && e.link.host() == "10.0.0.7");
  r = Env::parseRes(e.sch.handle(Env::req("/api/config", "kasse", "a b;c"), e.ms, e.now).body);
  CHECK("EXTRA", !r.ok && e.link.host() == "10.0.0.7");
  r = Env::parseRes(e.sch.handle(Env::req("/api/config", "kasse", ""), e.ms, e.now).body);
  CHECK("EXTRA", !r.ok);
  e.link.up = false;
  j = e.sch.handle(Env::req("/api/kasse"), e.ms, e.now).body;
  CHECK("EXTRA", !flag(j, "ok") && has(j, "Verbindungsfehler"));
  e.link.up = true;
  j = e.sch.handle(Env::req("/api/kasse"), e.ms, e.now).body;
  CHECK("EXTRA", flag(j, "ok") && flag(j, "connected"));

  // Schranke bleibt bei erneuter Durchfahrt wieder 6 s offen (Timer wird neu gestartet)
  Env f;
  f.enter("TI 2468");
  f.advance(4);
  f.enter("VS 1");
  f.advance(4);
  CHECK("EXTRA", f.drv.open[0]);  // 8 s nach der ersten, aber nur 4 s nach der zweiten Einfahrt
  f.advance(2);
  CHECK("EXTRA", !f.drv.open[0]);
  // Ausfahrt oeffnet nur die Ausfahrtsschranke und umgekehrt
  Env g;
  g.enter("TI 2468");
  CHECK("EXTRA", g.drv.open[0] && !g.drv.open[1]);
  g.advance(7);
  g.setMin("TI 2468", 10);
  g.pay("TI 2468", 200);
  g.leave("TI 2468");
  CHECK("EXTRA", g.drv.open[1] && !g.drv.open[0]);

  // Kapazitaet: 60 Fahrzeuge, danach klare Meldung, kein Absturz
  Env h;
  for (int i = 0; i < KasseApp::MAX_VEHICLES; i++) {
    char p[16];
    snprintf(p, sizeof(p), "AB %d", i + 1);
    CHECK("EXTRA", h.enter(p).ok);
  }
  h.advance(7);
  r = h.enter("CD 1");
  CHECK("EXTRA", !r.ok && !h.drv.open[0] && h.listCount() == KasseApp::MAX_VEHICLES);

  // Liste ist nach Einfahrtszeit sortiert
  Env k;
  k.enter("AA 1"); k.advance(10); k.enter("BB 2"); k.advance(10); k.enter("CC 3");
  k.setMin("CC 3", 500);
  std::string l = k.list();
  CHECK("EXTRA", l.find("CC 3") < l.find("AA 1") && l.find("AA 1") < l.find("BB 2"));

  // Unbekannte Adresse und Seiten
  Response nf = k.kasse.handle(Env::req("/nix"), k.now);
  CHECK("EXTRA", nf.status == 404);
  CHECK("EXTRA", k.kasse.handle(Env::req("/"), k.now).body.find("<html") != std::string::npos);
  CHECK("EXTRA", k.kasse.handle(Env::req("/automat"), k.now).body.find("<html") != std::string::npos);
  CHECK("EXTRA", k.sch.handle(Env::req("/"), k.ms, k.now).body.find("<html") != std::string::npos);
  CHECK("EXTRA", k.sch.handle(Env::req("/nix"), k.ms, k.now).status == 404);

  // HTTP-Hilfsfunktionen
  std::string raw = "HTTP/1.1 200 OK\r\nContent-Type: x\r\nContent-Length: 5\r\n\r\nhello";
  int st = 0;
  std::string body;
  CHECK("EXTRA", httpComplete(raw) && httpParse(raw, st, body) && st == 200 && body == "hello");
  CHECK("EXTRA", !httpComplete("HTTP/1.1 200 OK\r\nContent-Length: 5\r\n\r\nhel"));
  CHECK("EXTRA", !httpComplete("HTTP/1.1 200 OK\r\nContent-Le"));
  CHECK("EXTRA", !httpParse("", st, body) && !httpParse("garbage", st, body));
  CHECK("EXTRA", validHost("192.168.1.50") && validHost("kasse.local") && !validHost("") && !validHost("a b") &&
                     !validHost("x;y") && !validHost(".x"));
}

int main() {
  testPlates();
  testFee();
  testTestkonzept();
  testExtras();
  printf("\nErgebnis: %d bestanden, %d fehlgeschlagen\n", g_pass, g_fail);
  return g_fail ? 1 : 0;
}
