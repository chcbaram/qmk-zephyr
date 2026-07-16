#include "ap.h"
#include "qmk/qmk.h"
#include "qmk/port/activity.h"


// 개발용 하트비트 LED (루프가 도는지/잠들었는지 눈으로 확인용).
// 무선 배터리 동작에선 상시 점등이 전류를 크게 먹으므로(실측 ≈1.2mA) 기본은 끈다.
// 디버깅 때만 아래 define 을 켤 것. 켜면 잠들기 전 반드시 소등한다(토글 방식이라
// 그냥 블록하면 "켜진 채" 멈출 확률이 50%).
//#define AP_USE_HEARTBEAT_LED


void apInit(void)
{
  usbInit();

  moduleInit();
}

void apMain(void)
{
#ifdef AP_USE_HEARTBEAT_LED
  uint32_t pre_time = millis();
#endif

  ledOff(_DEF_LED1);   // 저전력: 기본 소등

  qmkInit();
  activityInit();

  while(1)
  {
    qmkUpdate();   // keyboard_task(): 매트릭스 스캔 → 액션 처리 → host_driver 전송

    if (qmkIsIdle())
    {
#ifdef AP_USE_HEARTBEAT_LED
      ledOff(_DEF_LED1);   // 켜진 채로 잠들면 계속 전류를 먹는다
#endif
      // 완전 idle(눌린 키 없음 + 디바운스 정착): 입력이 올 때까지 블록 → CPU sleep.
      // kbd-matrix 드라이버도 전 컬럼 구동 + row 인터럽트 대기로 들어간다(저전력).
      //
      // activityUpdate() 가 idle/sleep 을 판정한다. sleep 조건이 차면 System OFF 로
      // 들어가 **돌아오지 않는다**(키 누르면 리셋 부팅).
      activityUpdate();

      // 키 입력 또는 다음 상태 전이(데드라인)까지 블록. 0 이면 무한.
      qmkWaitActivity(activityGetWaitMs());
    }
    else
    {
      // 활성 구간: ≈1kHz 로 스캔/타이머 진행. QMK 디바운스·탭핑이 여기서 돈다.
      // (RTOS: USB/로그/CLI 스레드에 CPU 양보 — 없으면 그 스레드들이 굶는다)
      k_msleep(1);
    }

#ifdef AP_USE_HEARTBEAT_LED
    if (millis() - pre_time >= 500)
    {
      pre_time = millis();
      ledToggle(_DEF_LED1);
    }
#endif
  }
}
