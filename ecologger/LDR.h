#ifndef LDR_H
#define LDR_H

#include <Arduino.h>

class LDR
{
public:
   LDR(int pin);
   int  get();
 
private:
   int pino;
};
 

#endif