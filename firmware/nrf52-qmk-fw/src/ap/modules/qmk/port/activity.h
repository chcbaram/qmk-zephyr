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

// 타임아웃(ms). 0 = 비활성. VIA 커스텀 메뉴에서 설정할 예정(Phase 5-D).
void             activitySetIdleTimeout(uint32_t ms);
void             activitySetSleepTimeout(uint32_t ms);
uint32_t         activityGetIdleTimeout(void);
uint32_t         activityGetSleepTimeout(void);
