#include "hw.h"




bool hwInit(void)
{
  bspInit();

  ledInit();
  batteryInit();

#ifdef _USE_HW_DEBUG_CONSOLE
  // 디버그 콘솔(UART/CLI/로그). 저전력 빌드에선 통째로 빠진다(hw_def.h 참고).
  cliInit();
  logInit();
  uartInit();
  for (int i=0; i<HW_UART_MAX_CH; i++)
  {
    uartOpen(i, 115200);
  }

  delay(100);

  logOpen(HW_LOG_CH, 115200);
#endif

  logPrintf("\r\n[ Firmware Begin... ]\r\n");
  logPrintf("Booting..Name \t\t: %s\r\n", _DEF_BOARD_NAME);
  logPrintf("Booting..Ver  \t\t: %s\r\n", _DEF_FIRMWATRE_VERSION);
  logPrintf("Booting..Date \t\t: %s\r\n", __DATE__);
  logPrintf("Booting..Time \t\t: %s\r\n", __TIME__);

  // 키 매트릭스는 Zephyr gpio-kbd-matrix 드라이버가 자동 초기화(DTS kbd_matrix).
  // QMK 쪽 연결은 qmkInit() -> matrix_init() 에서 수행.

  return true;
}
