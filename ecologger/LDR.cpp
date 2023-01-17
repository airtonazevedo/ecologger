#include "LDR.h"

LDR::LDR(int pin) {
  pinMode(pin, INPUT);
  pino = pin;
}

int LDR::get() {
  return map(analogRead(pino), 0, 1023, 255, 0);
}
