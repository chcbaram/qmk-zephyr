#ifndef EXT_POWER_H_
#define EXT_POWER_H_

#ifdef __cplusplus
extern "C" {
#endif

#include "hw_def.h"

#ifdef _USE_HW_EXT_POWER

/*
 * 외부 전원 레일 (wish60/wish65: 네오픽셀 전용. 595 는 상시 전원).
 *
 * Zephyr 네이티브 regulator-fixed(DTS: ext_power)를 쓴다 — ZMK 의 zmk,ext-power-generic 을
 * 흡수할 필요가 없다. 부팅 시엔 꺼져 있고(regulator-boot-on 없음) 쓸 때만 올린다.
 *
 * 네오픽셀은 검은색을 표시해도 컨트롤러가 개당 ~0.7mA 를 먹는다(16개 ≈ 11mA, idle 80.9µA 의
 * 140배). 그래서 안 쓸 땐 레일을 반드시 내려야 한다.
 */

bool extPowerInit(void);
bool extPowerEnable(void);
bool extPowerDisable(void);
bool extPowerIsEnabled(void);

#endif

#ifdef __cplusplus
}
#endif

#endif
