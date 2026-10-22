// WLAN-Ersatz fuer den PC: echte TCP-Sockets auf 127.0.0.x (Port = Port + 8000)
#pragma once
#include "Arduino.h"
#include <arpa/inet.h>
#include <errno.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <poll.h>
#include <sys/socket.h>
#include <deque>

#define WIFI_STA 1
enum wl_status_t { WL_DISCONNECTED = 6, WL_CONNECTED = 3 };

inline std::string stubBindIp() {
  const char* e = getenv("PICO_BIND_IP");
  return e ? e : "127.0.0.2";
}
inline int stubPortOffset() {
  const char* e = getenv("PICO_PORT_OFFSET");
  return e ? atoi(e) : 8000;
}

class WiFiClass {
 public:
  void mode(int) {}
  void begin(const char*, const char*) { up_ = true; }
  wl_status_t status() { return up_ ? WL_CONNECTED : WL_DISCONNECTED; }
  IPAddress localIP() { return IPAddress(stubBindIp()); }
 private:
  bool up_ = false;
};
static WiFiClass WiFi;

class NTPClass {
 public:
  void begin(const char*, const char* = nullptr) {}
};
static NTPClass NTP;

class WiFiClient {
 public:
  WiFiClient() : fd_(-1), timeout_(5000) {}
  explicit WiFiClient(int fd) : fd_(fd), timeout_(5000) { fcntl(fd_, F_SETFL, fcntl(fd_, F_GETFL) | O_NONBLOCK); }
  WiFiClient(const WiFiClient&) = delete;
  WiFiClient& operator=(const WiFiClient&) = delete;
  ~WiFiClient() { stop(); }

  void setTimeout(unsigned long ms) { timeout_ = ms; }

  int connect(const char* host, unsigned short port) {
    stop();
    fd_ = socket(AF_INET, SOCK_STREAM, 0);
    if (fd_ < 0) return 0;
    fcntl(fd_, F_SETFL, fcntl(fd_, F_GETFL) | O_NONBLOCK);
    sockaddr_in a;
    memset(&a, 0, sizeof(a));
    a.sin_family = AF_INET;
    a.sin_port = htons(port + stubPortOffset());
    if (inet_pton(AF_INET, host, &a.sin_addr) != 1) { stop(); return 0; }
    int r = ::connect(fd_, (sockaddr*)&a, sizeof(a));
    if (r < 0 && errno != EINPROGRESS) { stop(); return 0; }
    if (r < 0) {
      pollfd p = {fd_, POLLOUT, 0};
      if (poll(&p, 1, (int)timeout_) <= 0) { stop(); return 0; }
      int err = 0;
      socklen_t l = sizeof(err);
      getsockopt(fd_, SOL_SOCKET, SO_ERROR, &err, &l);
      if (err) { stop(); return 0; }
    }
    return 1;
  }
  size_t print(const char* s) { return write((const uint8_t*)s, strlen(s)); }
  size_t print(const std::string& s) { return write((const uint8_t*)s.data(), s.size()); }
  size_t write(const uint8_t* d, size_t n) {
    size_t sent = 0;
    while (fd_ >= 0 && sent < n) {
      ssize_t r = ::send(fd_, d + sent, n - sent, MSG_NOSIGNAL);
      if (r > 0) sent += r;
      else if (r < 0 && (errno == EAGAIN || errno == EWOULDBLOCK)) usleep(1000);
      else break;
    }
    return sent;
  }
  int available() { fill(); return (int)buf_.size(); }
  int read() {
    fill();
    if (buf_.empty()) return -1;
    int c = buf_.front();
    buf_.pop_front();
    return c;
  }
  bool connected() {
    if (fd_ < 0) return false;
    fill();
    return fd_ >= 0 || !buf_.empty();
  }
  void stop() {
    if (fd_ >= 0) { ::close(fd_); fd_ = -1; }
    buf_.clear();
  }
  operator bool() { return fd_ >= 0; }
  int fd() const { return fd_; }
  int release() { int f = fd_; fd_ = -1; return f; }

 private:
  void fill() {
    if (fd_ < 0) return;
    uint8_t tmp[1024];
    for (;;) {
      ssize_t r = ::recv(fd_, tmp, sizeof(tmp), MSG_DONTWAIT);
      if (r > 0) { for (ssize_t i = 0; i < r; i++) buf_.push_back(tmp[i]); continue; }
      if (r == 0) { ::close(fd_); fd_ = -1; }  // Gegenseite hat geschlossen
      break;
    }
  }
  int fd_;
  unsigned long timeout_;
  std::deque<uint8_t> buf_;
};
