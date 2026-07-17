// QMK 의 platforms/suspend.c 중 **weak 훅만** 가져온 것.
// SPDX-License-Identifier: GPL-2.0-or-later

#include "suspend.h"

/*
 * quantum.c 의 suspend_power_down_quantum()/suspend_wakeup_init_quantum() 이 이 훅들을 부른다.
 * upstream 은 platforms/suspend.c 에 두지만, 그 파일엔 wakeup 매트릭스 기계
 * (suspend_wakeup_condition / keypress_is_wakeup_key ...)도 같이 들어있다. 그건 extern
 * matrix_previous 를 요구하는데 우리 keyboard.c 는 그걸 **static 지역변수**로 갖고 있고
 * 아무도 그 함수들을 부르지 않는다 → 훅만 가져온다.
 *
 * 호출 경로: usbd_next USBD_MSG_SUSPEND -> usb.c 콜백 -> qmk.c -> suspend_power_down_quantum()
 *            -> rgb_matrix_set_suspend_state(true) -> LED 소등 + flush -> ext-power 레일 down
 */
__attribute__((weak)) void suspend_power_down_user(void) {}

__attribute__((weak)) void suspend_power_down_kb(void) {
    suspend_power_down_user();
}

__attribute__((weak)) void suspend_wakeup_init_user(void) {}

__attribute__((weak)) void suspend_wakeup_init_kb(void) {
    suspend_wakeup_init_user();
}
