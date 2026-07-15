#include "suspend.h"

// 플랫폼(protocol) 레벨 suspend 진입점만 제공한다.
// suspend_*_quantum / _kb / _user 는 quantum.c 가 정의하므로 여기서 중복 정의하지 않는다.
// Phase 0/1: 실제 저전력 동작 없음. Phase 6에서 activity/PM 연동.

void suspend_power_down(void)
{
  suspend_power_down_quantum();
}

bool suspend_wakeup_condition(void)
{
  return true;
}

void suspend_wakeup_init(void)
{
  suspend_wakeup_init_quantum();
}
