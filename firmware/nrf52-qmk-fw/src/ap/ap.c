#include "ap.h"



void apInit(void)
{
  usbInit();

  moduleInit();
}

void apMain(void)
{
  while(1)
  {
    ledToggle(_DEF_LED1);
    delay(500);
  }
}

