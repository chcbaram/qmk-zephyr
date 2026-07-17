#pragma once

#include <stdint.h>

/*
 * HOLD_ON_OTHER_KEY_PRESS 런타임 on/off (VIA).
 *
 * 무엇인가: 탭/홀드 키(LT/MT)를 누른 채 **다른 키를 누르면** 탭핑 term 을 기다리지 않고
 * 즉시 홀드로 확정한다. 빠른 타이핑에서 레이어 키가 씹히는 것을 줄이지만, 롤오버가 잦으면
 * 의도치 않은 홀드가 난다 — 취향이 갈려서 런타임 노브로 뺀다.
 *
 * [QMK 쪽 수정 없음] action_tapping.c 가 HOLD_ON_OTHER_KEY_PRESS_PER_KEY 빌드에서
 * get_hold_on_other_key_press() 를 부른다(weak 기본 false). 여기서 오버라이드한다.
 * 그래서 config.h 에 HOLD_ON_OTHER_KEY_PRESS_PER_KEY 가 반드시 있어야 한다 — 없으면
 * 컴파일은 되고 **VIA 토글이 아무 효과도 없다**(TAP_GET_HOLD_ON_OTHER_KEY_PRESS 가 상수 false).
 */

void hold_okp_init(void);
void via_qmk_hold_okp_command(uint8_t *data, uint8_t length);
