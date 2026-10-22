// Hilfsfunktionen fuer beide Picos. Die Datei ist in kasse/ und schranken/ gleich.
#ifndef PARKHAUS_COMMON_H
#define PARKHAUS_COMMON_H
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <string>
#include <utility>
#include <vector>

namespace park {

typedef long long i64;

// Kennzeichen: zwei Buchstaben + ein bis sechs Ziffern
enum PlateStatus {
  PLATE_OK = 0,
  PLATE_EMPTY,
  PLATE_BAD_CHAR,
  PLATE_NO_LETTERS,
  PLATE_ONE_LETTER,
  PLATE_TOO_MANY_LETTERS,
  PLATE_NO_DIGITS,
  PLATE_TOO_MANY_DIGITS,
  PLATE_BAD_ORDER
};

struct Plate {
  PlateStatus status;
  char key[10];      // "TI2468"
  char display[12];  // "TI 2468"
  unsigned char badChar;
};

inline bool isLetterCh(unsigned char c) { return c >= 'A' && c <= 'Z'; }
inline bool isDigitCh(unsigned char c) { return c >= '0' && c <= '9'; }

inline Plate parsePlate(const char* raw) {
  Plate p;
  p.status = PLATE_OK;
  p.key[0] = 0;
  p.display[0] = 0;
  p.badChar = 0;
  if (!raw) raw = "";

  std::string s;
  for (const unsigned char* c = (const unsigned char*)raw; *c; ++c) {
    if (*c == ' ' || *c == '\t' || *c == '\r' || *c == '\n') continue;
    if (*c >= 'a' && *c <= 'z') s += (char)(*c - 'a' + 'A');
    else s += (char)*c;
    if (s.size() > 64) break;
  }
  if (s.empty()) {
    p.status = PLATE_EMPTY;
    return p;
  }

  for (size_t i = 0; i < s.size(); i++) {
    unsigned char c = (unsigned char)s[i];
    if (!isLetterCh(c) && !isDigitCh(c)) {
      p.status = PLATE_BAD_CHAR;
      p.badChar = c;
      return p;
    }
  }

  size_t i = 0, letters = 0, digits = 0;
  while (i < s.size() && isLetterCh((unsigned char)s[i])) { letters++; i++; }
  while (i < s.size() && isDigitCh((unsigned char)s[i])) { digits++; i++; }
  if (letters == 0) { p.status = PLATE_NO_LETTERS; return p; }
  if (letters == 1) { p.status = PLATE_ONE_LETTER; return p; }
  if (letters > 2) { p.status = PLATE_TOO_MANY_LETTERS; return p; }
  if (digits == 0) { p.status = PLATE_NO_DIGITS; return p; }
  if (digits > 6) { p.status = PLATE_TOO_MANY_DIGITS; return p; }
  if (i < s.size()) { p.status = PLATE_BAD_ORDER; return p; }

  snprintf(p.key, sizeof(p.key), "%s", s.c_str());
  snprintf(p.display, sizeof(p.display), "%c%c %s", s[0], s[1], s.c_str() + 2);
  return p;
}

// immer ein Satz (AK-2.4)
inline std::string plateMessage(const Plate& p) {
  const char* tail = " (erwartet: zwei Buchstaben und ein bis sechs Ziffern)";
  switch (p.status) {
    case PLATE_OK: return "Kennzeichen ist gültig.";
    case PLATE_EMPTY: return "Kennzeichen fehlt.";
    case PLATE_BAD_CHAR: {
      std::string m = "Ungültiges Zeichen im Kennzeichen";
      if (p.badChar >= 33 && p.badChar < 127) {
        m += " (»";
        m += (char)p.badChar;
        m += "«)";
      }
      m += ", erlaubt sind nur Buchstaben und Ziffern.";
      return m;
    }
    case PLATE_NO_LETTERS: return std::string("Ungültiges Format: Buchstaben fehlen") + tail + ".";
    case PLATE_ONE_LETTER: return std::string("Ungültiges Format: nur ein Buchstabe") + tail + ".";
    case PLATE_TOO_MANY_LETTERS: return std::string("Ungültiges Format: mehr als zwei Buchstaben") + tail + ".";
    case PLATE_NO_DIGITS: return std::string("Ungültiges Format: keine Ziffern") + tail + ".";
    case PLATE_TOO_MANY_DIGITS: return std::string("Ungültiges Format: mehr als sechs Ziffern") + tail + ".";
    case PLATE_BAD_ORDER: return std::string("Ungültiges Format: erst Buchstaben, dann Ziffern") + tail + ".";
  }
  return "Ungültiges Kennzeichen.";
}

// Betraege in Rappen. Kein std::to_string / %lld, fehlt in newlib-nano.
inline std::string itos(i64 v) {
  if (v == 0) return "0";
  bool neg = v < 0;
  unsigned long long u = neg ? 0ULL - (unsigned long long)v : (unsigned long long)v;
  char b[24];
  int n = 0;
  while (u > 0) {
    b[n++] = (char)('0' + (int)(u % 10ULL));
    u /= 10ULL;
  }
  std::string o;
  if (neg) o += '-';
  while (n > 0) o += b[--n];
  return o;
}

inline std::string chf(i64 rappen) {
  std::string o = "CHF ";
  o += itos(rappen / 100);
  o += '.';
  i64 r = rappen % 100;
  if (r < 0) r = -r;
  o += (char)('0' + r / 10);
  o += (char)('0' + r % 10);
  return o;
}

// JSON, nur flache Objekte
inline std::string jsonEscape(const std::string& s) {
  std::string o;
  for (size_t i = 0; i < s.size(); i++) {
    unsigned char c = (unsigned char)s[i];
    if (c == '"') o += "\\\"";
    else if (c == '\\') o += "\\\\";
    else if (c == '\n') o += "\\n";
    else if (c == '\r') o += "\\r";
    else if (c == '\t') o += "\\t";
    else if (c < 0x20) {
      char b[8];
      snprintf(b, sizeof(b), "\\u%04x", c);
      o += b;
    } else o += (char)c;
  }
  return o;
}

class JObj {
 public:
  JObj() : first_(true), s_("{") {}
  JObj& str(const char* k, const std::string& v) {
    key(k);
    s_ += '"';
    s_ += jsonEscape(v);
    s_ += '"';
    return *this;
  }
  JObj& num(const char* k, i64 v) {
    key(k);
    s_ += itos(v);
    return *this;
  }
  JObj& boolean(const char* k, bool v) {
    key(k);
    s_ += v ? "true" : "false";
    return *this;
  }
  JObj& raw(const char* k, const std::string& json) {
    key(k);
    s_ += json;
    return *this;
  }
  std::string done() const { return s_ + "}"; }

 private:
  void key(const char* k) {
    if (!first_) s_ += ',';
    first_ = false;
    s_ += '"';
    s_ += k;
    s_ += "\":";
  }
  bool first_;
  std::string s_;
};

inline bool jsonFindKey(const std::string& j, const char* key, size_t& pos) {
  std::string pat = std::string("\"") + key + "\":";
  size_t p = j.find(pat);
  if (p == std::string::npos) return false;
  pos = p + pat.size();
  return true;
}
inline bool jsonGetBool(const std::string& j, const char* key, bool& out) {
  size_t p;
  if (!jsonFindKey(j, key, p)) return false;
  if (j.compare(p, 4, "true") == 0) { out = true; return true; }
  if (j.compare(p, 5, "false") == 0) { out = false; return true; }
  return false;
}
inline bool jsonGetNum(const std::string& j, const char* key, i64& out) {
  size_t p;
  if (!jsonFindKey(j, key, p)) return false;
  char* end = 0;
  out = strtoll(j.c_str() + p, &end, 10);
  return end != j.c_str() + p;
}
inline bool jsonGetString(const std::string& j, const char* key, std::string& out) {
  size_t p;
  if (!jsonFindKey(j, key, p)) return false;
  if (p >= j.size() || j[p] != '"') return false;
  p++;
  out.clear();
  while (p < j.size()) {
    char c = j[p++];
    if (c == '"') return true;
    if (c == '\\' && p < j.size()) {
      char e = j[p++];
      if (e == 'n') out += '\n';
      else if (e == 't') out += '\t';
      else if (e == 'r') out += '\r';
      else if (e == 'u' && p + 4 <= j.size()) {
        int v = (int)strtol(j.substr(p, 4).c_str(), 0, 16);
        out += (char)(v < 128 ? v : '?');
        p += 4;
      } else out += e;
    } else out += c;
  }
  return false;
}

struct Request {
  std::string method;  // "GET", "POST"
  std::string path;    // "/api/fee"
  std::vector<std::pair<std::string, std::string> > args;
  std::string arg(const char* name) const {
    for (size_t i = 0; i < args.size(); i++)
      if (args[i].first == name) return args[i].second;
    return "";
  }
  bool has(const char* name) const {
    for (size_t i = 0; i < args.size(); i++)
      if (args[i].first == name) return true;
    return false;
  }
};

struct Response {
  int status;
  std::string type;
  std::string body;
  Response() : status(200), type("application/json; charset=utf-8") {}
};

inline Response jsonResponse(const JObj& o, int status = 200) {
  Response r;
  r.status = status;
  r.body = o.done();
  return r;
}

inline bool httpComplete(const std::string& raw) {
  size_t h = raw.find("\r\n\r\n");
  if (h == std::string::npos) return false;
  size_t cl = raw.find("Content-Length:");
  if (cl == std::string::npos || cl > h) cl = raw.find("content-length:");
  if (cl == std::string::npos || cl > h) return false;  // ohne Content-Length bis Verbindungsende
  long len = atol(raw.c_str() + cl + 15);
  return (long)(raw.size() - (h + 4)) >= len;
}

inline bool httpParse(const std::string& raw, int& status, std::string& body) {
  if (raw.compare(0, 5, "HTTP/") != 0) return false;
  size_t sp = raw.find(' ');
  if (sp == std::string::npos) return false;
  status = atoi(raw.c_str() + sp + 1);
  size_t h = raw.find("\r\n\r\n");
  if (h == std::string::npos) return false;
  body = raw.substr(h + 4);
  return status > 0;
}

}  // namespace park

#endif  // PARKHAUS_COMMON_H
