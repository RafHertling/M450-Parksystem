// Minimaler Ersatz fuer Arduino.h - nur zum Testen auf dem PC (nicht fuer den Pico!)
#pragma once
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <string>
#include <time.h>
#include <unistd.h>

#define HIGH 1
#define LOW 0
#define OUTPUT 1
#define LED_BUILTIN 25

inline unsigned long millis() {
  struct timespec ts;
  clock_gettime(CLOCK_MONOTONIC, &ts);
  return (unsigned long)(ts.tv_sec * 1000UL + ts.tv_nsec / 1000000UL);
}
inline void delay(unsigned long ms) { usleep(ms * 1000); }
inline void pinMode(int, int) {}
inline void digitalWrite(int, int) {}

class String : public std::string {
 public:
  String() {}
  String(const char* s) : std::string(s ? s : "") {}
  String(const std::string& s) : std::string(s) {}
};
inline String operator+(const String& a, const char* b) { return String(std::string(a) + b); }
inline String operator+(const String& a, const String& b) { return String(std::string(a) + std::string(b)); }

class IPAddress {
 public:
  IPAddress() {}
  IPAddress(const std::string& s) : s_(s) {}
  String toString() const { return String(s_); }
 private:
  std::string s_;
};

class HardwareSerial {
 public:
  void begin(unsigned long) {}
  void print(const char* s) { fputs(s, stdout); fflush(stdout); }
  void print(const std::string& s) { print(s.c_str()); }
  void print(int v) { printf("%d", v); fflush(stdout); }
  void print(const IPAddress& ip) { print(ip.toString().c_str()); }
  void println(const char* s) { printf("%s\n", s); fflush(stdout); }
  void println(const std::string& s) { println(s.c_str()); }
  void println(const IPAddress& ip) { println(ip.toString().c_str()); }
  void println(int v) { printf("%d\n", v); fflush(stdout); }
};
__attribute__((unused)) static HardwareSerial Serial;
