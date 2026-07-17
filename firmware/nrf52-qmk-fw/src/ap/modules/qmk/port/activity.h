#pragma once

#include <stdint.h>
#include <stdbool.h>

/*
 * activity 상태머신 (ZMK app/src/activity.c 패턴 차용, baram 스타일로 재구현).
 *
 *   ACTIVE  : 최근 입력 있음
 *   IDLE    : idle 타임아웃 경과 (Phase 8: 여기서 RGB 소등)
 *   SLEEP   : sleep 타임아웃 경과 + USB 없음 → sys_poweroff()
 *
 * ZMK 는 이벤트 버스 + 1초 주기 타이머로 돌지만, 우리는 메인 루프가 이미 idle 일 때만
 * 깨어나므로 "다음 데드라인까지 자고 일어나서 판정"하는 편이 싸다(주기 타이머 불필요).
 */

typedef enum
{
  ACTIVITY_ACTIVE = 0,
  ACTIVITY_IDLE,
  ACTIVITY_SLEEP,
} activity_state_t;

bool             activityInit(void);

// 메인 루프가 idle 일 때 호출. sleep 조건이 차면 sys_poweroff() 로 들어가 **돌아오지 않는다**.
void             activityUpdate(void);

// 다음 상태 전이까지 남은 ms. 0 이면 볼 데드라인 없음(무한 대기).
uint32_t         activityGetWaitMs(void);

activity_state_t activityGetState(void);

/*
 * 지금 idle 인가 — 상태머신을 갱신하지 않고 계산만 한다.
 *
 * activityGetState() 를 쓰면 안 되는 자리가 있다. activityUpdate() 는 메인 루프의 **idle
 * 분기에서만** 불리므로, 키가 눌려 활성 분기로 빠지면 state 가 IDLE 인 채로 낡는다.
 * RGB 복귀처럼 활성 구간에서도 판정해야 하는 곳은 이걸 써야 한다.
 */
bool             activityIsIdle(void);

/*
 * RGB/인디케이터 소등 타임아웃 — **activity 의 idle 과 별개다**.
 *
 * [왜 별개인가] 처음엔 idle(30초)에 얹었는데, 그 30초는 ZMK 의 CONFIG_ZMK_IDLE_TIMEOUT 을
 * 가져온 값이고 **ZMK 에서 그건 OLED 끄기용**이라 RGB 와 무관하다(§6.12). 우리 IDLE 이 하는
 * 일이 RGB 소등뿐이라 우연히 굴러갔을 뿐, 두 개념은 다르다:
 *   idle  : "사용자가 잠시 멈췄다"  — 나중에 다른 소비자가 붙을 수 있다(BLE latency 등)
 *   RGB   : "LED 를 꺼도 될 만큼 자리를 비웠다" — 65mA 라 판단 기준이 다르다
 *
 * 인디케이터(Caps)도 이 타임아웃을 따른다 — QMK 의 suspend_power_down_quantum() 이 RGB 와
 * led_suspend() 를 함께 처리하므로 분리할 수 없고, 분리할 이유도 없다.
 */
bool             activityRgbIsIdle(void);
void             activitySetRgbTimeout(uint32_t ms);
uint32_t         activityGetRgbTimeout(void);

// 타임아웃(ms). 0 = 비활성. VIA 커스텀 메뉴에서 설정할 예정(Phase 5-D).
void             activitySetIdleTimeout(uint32_t ms);
void             activitySetSleepTimeout(uint32_t ms);
uint32_t         activityGetIdleTimeout(void);
uint32_t         activityGetSleepTimeout(void);
