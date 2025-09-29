#include "hw.h"




bool hwInit(void)
{  
  bspInit();

  cliInit();
  ledInit();
  logInit();  
  uartInit();
  for (int i=0; i<HW_UART_MAX_CH; i++)
  {
    uartOpen(i, 115200);
  }
  
  delay(100);

  logOpen(HW_LOG_CH, 115200);
  logPrintf("\r\n[ Firmware Begin... ]\r\n");
  logPrintf("Booting..Name \t\t: %s\r\n", _DEF_BOARD_NAME);
  logPrintf("Booting..Ver  \t\t: %s\r\n", _DEF_FIRMWATRE_VERSION);  
  logPrintf("Booting..Date \t\t: %s\r\n", __DATE__); 
  logPrintf("Booting..Time \t\t: %s\r\n", __TIME__);   
  
  
  return true;
}