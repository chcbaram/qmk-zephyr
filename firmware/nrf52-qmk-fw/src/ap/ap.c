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

    // RTOS: USB/로그/CLI 스레드에 CPU 양보 (≈1kHz 스캔, USB FS 폴링과 정합).
    // 이게 없으면 메인 스레드(우선순위 높음)가 CLI/UART 스레드를 굶긴다.
    k_msleep(1);

    // TODO(저전력/Phase 6): 개발용 하트비트 LED. 무선 배터리 동작 시엔 상시 LED 점등이
    // 전류를 크게 먹으므로 최종 키보드 펌웨어에서는 제거하거나 극소 듀티로 바꿀 것.
    if (millis() - pre_time >= 500)
    {
      pre_time = millis();
      ledToggle(_DEF_LED1);
    }
  }
}

