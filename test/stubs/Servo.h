// Servo-Ersatz fuer den PC: schreibt jede Bewegung mit Zeitstempel ins Log
#pragma once
#include "Arduino.h"
class Servo {
 public:
  void attach(int pin) { pin_ = pin; }
  void write(int angle) {
    printf("SERVO %lu %d %d\n", millis(), pin_, angle);
    fflush(stdout);
  }
 private:
  int pin_ = -1;
};
