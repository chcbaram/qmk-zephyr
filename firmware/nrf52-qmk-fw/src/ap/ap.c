#include "ap.h"
#include "qmk/qmk.h"



void apInit(void)
{
  usbInit();

  moduleInit();
}

void apMain(void)
{
  uint32_t pre_time = millis();

  qmkInit();

  while(1)
  {
    qmkUpdate();   // keyboard_task(): 매트릭스 스캔 → 액션 처리 → host_driver 전송

    if (millis() - pre_time >= 500)
    {
      pre_time = millis();
      ledToggle(_DEF_LED1);   // 동작 표시용 하트비트
    }
  }
}

