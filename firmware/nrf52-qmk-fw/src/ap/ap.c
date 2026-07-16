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

    if (qmkIsIdle())
    {
      // 완전 idle(눌린 키 없음 + 디바운스 정착): 입력이 올 때까지 블록 → CPU sleep.
      // kbd-matrix 드라이버도 전 컬럼 구동 + row 인터럽트 대기로 들어간다(저전력).
      qmkWaitActivity();
    }
    else
    {
      // 활성 구간: ≈1kHz 로 스캔/타이머 진행. QMK 디바운스·탭핑이 여기서 돈다.
      // (RTOS: USB/로그/CLI 스레드에 CPU 양보 — 없으면 그 스레드들이 굶는다)
      k_msleep(1);
    }

    // TODO(저전력/Phase 6): 개발용 하트비트 LED. 무선 배터리 동작 시엔 상시 LED 점등이
    // 전류를 크게 먹으므로 최종 키보드 펌웨어에서는 제거하거나 극소 듀티로 바꿀 것.
    if (millis() - pre_time >= 500)
    {
      pre_time = millis();
      ledToggle(_DEF_LED1);
    }
  }
}

