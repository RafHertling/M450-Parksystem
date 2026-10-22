// WebServer-Ersatz fuer den PC (gleiche Aufrufe wie der WebServer im Arduino-Pico-Core)
#pragma once
#include "WiFi.h"
#include <ctype.h>
#include <functional>
#include <utility>
#include <vector>

enum HTTPMethod { HTTP_ANY, HTTP_GET, HTTP_HEAD, HTTP_POST, HTTP_PUT, HTTP_PATCH, HTTP_DELETE, HTTP_OPTIONS };

class WebServer {
 public:
  typedef std::function<void(void)> THandlerFunction;
  explicit WebServer(int port) : port_(port), lfd_(-1), cfd_(-1), method_(HTTP_GET) {}

  void onNotFound(THandlerFunction f) { notFound_ = f; }
  void begin() {
    lfd_ = socket(AF_INET, SOCK_STREAM, 0);
    int one = 1;
    setsockopt(lfd_, SOL_SOCKET, SO_REUSEADDR, &one, sizeof(one));
    sockaddr_in a;
    memset(&a, 0, sizeof(a));
    a.sin_family = AF_INET;
    a.sin_port = htons(port_ + stubPortOffset());
    inet_pton(AF_INET, stubBindIp().c_str(), &a.sin_addr);
    if (bind(lfd_, (sockaddr*)&a, sizeof(a)) < 0) { perror("bind"); exit(2); }
    listen(lfd_, 16);
    fcntl(lfd_, F_SETFL, fcntl(lfd_, F_GETFL) | O_NONBLOCK);
    fprintf(stderr, "[stub] WebServer lauscht auf %s:%d\n", stubBindIp().c_str(), port_ + stubPortOffset());
  }

  void handleClient() {
    if (lfd_ < 0) return;
    int fd = accept(lfd_, 0, 0);
    if (fd < 0) return;
    cfd_ = fd;
    if (readRequest()) {
      if (notFound_) notFound_();
      else send(404, "text/plain", "Not found");
    }
    if (cfd_ >= 0) { ::close(cfd_); cfd_ = -1; }
  }

  HTTPMethod method() { return method_; }
  String uri() { return String(uri_); }
  int args() { return (int)args_.size(); }
  String argName(int i) { return String(args_[i].first); }
  String arg(int i) { return String(args_[i].second); }
  String arg(const char* n) {
    for (size_t i = 0; i < args_.size(); i++) if (args_[i].first == n) return String(args_[i].second);
    return String();
  }
  void sendHeader(const char* n, const char* v) { extra_ += std::string(n) + ": " + v + "\r\n"; }
  void send(int code, const char* type, const char* content) {
    std::string body = content ? content : "";
    std::string h = "HTTP/1.1 " + std::to_string(code) + " X\r\nContent-Type: " + type +
                    "\r\nContent-Length: " + std::to_string(body.size()) + "\r\nConnection: close\r\n" + extra_ + "\r\n";
    extra_.clear();
    std::string all = h + body;
    size_t sent = 0;
    while (sent < all.size()) {
      ssize_t r = ::send(cfd_, all.data() + sent, all.size() - sent, MSG_NOSIGNAL);
      if (r > 0) sent += r;
      else if (r < 0 && (errno == EAGAIN || errno == EWOULDBLOCK)) usleep(500);
      else break;
    }
  }
  void send(int code, const char* type, const String& content) { send(code, type, content.c_str()); }

 private:
  static std::string urldecode(const std::string& s) {
    std::string o;
    for (size_t i = 0; i < s.size(); i++) {
      if (s[i] == '+') o += ' ';
      else if (s[i] == '%' && i + 2 < s.size() + 0 && isxdigit((unsigned char)s[i + 1]) && isxdigit((unsigned char)s[i + 2])) {
        o += (char)strtol(s.substr(i + 1, 2).c_str(), 0, 16);
        i += 2;
      } else o += s[i];
    }
    return o;
  }
  void parseArgs(const std::string& q) {
    size_t i = 0;
    while (i <= q.size() && !q.empty()) {
      size_t amp = q.find('&', i);
      std::string kv = q.substr(i, amp == std::string::npos ? std::string::npos : amp - i);
      size_t eq = kv.find('=');
      if (eq == std::string::npos) args_.push_back(std::make_pair(urldecode(kv), std::string()));
      else args_.push_back(std::make_pair(urldecode(kv.substr(0, eq)), urldecode(kv.substr(eq + 1))));
      if (amp == std::string::npos) break;
      i = amp + 1;
    }
  }
  bool readRequest() {
    std::string raw;
    unsigned long start = millis();
    size_t hend = std::string::npos;
    while (millis() - start < 2000) {
      char tmp[2048];
      ssize_t r = recv(cfd_, tmp, sizeof(tmp), MSG_DONTWAIT);
      if (r > 0) raw.append(tmp, r);
      else if (r == 0) break;
      else usleep(500);
      hend = raw.find("\r\n\r\n");
      if (hend != std::string::npos) {
        size_t cl = raw.find("Content-Length:");
        long len = (cl != std::string::npos && cl < hend) ? atol(raw.c_str() + cl + 15) : 0;
        if ((long)(raw.size() - hend - 4) >= len) break;
      }
    }
    if (hend == std::string::npos) return false;
    size_t e = raw.find("\r\n");
    std::string line = raw.substr(0, e);
    size_t s1 = line.find(' '), s2 = line.find(' ', s1 + 1);
    if (s1 == std::string::npos || s2 == std::string::npos) return false;
    std::string m = line.substr(0, s1), target = line.substr(s1 + 1, s2 - s1 - 1);
    method_ = m == "GET" ? HTTP_GET : m == "POST" ? HTTP_POST : m == "HEAD" ? HTTP_HEAD : HTTP_ANY;
    size_t q = target.find('?');
    uri_ = urldecode(target.substr(0, q));
    args_.clear();
    if (q != std::string::npos) parseArgs(target.substr(q + 1));
    return true;
  }

  int port_, lfd_, cfd_;
  HTTPMethod method_;
  std::string uri_, extra_;
  std::vector<std::pair<std::string, std::string> > args_;
  THandlerFunction notFound_;
};
