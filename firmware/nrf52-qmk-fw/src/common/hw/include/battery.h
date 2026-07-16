#ifndef BATTERY_H_
#define BATTERY_H_

#ifdef __cplusplus
extern "C" {
#endif

#include "hw_def.h"

#ifdef _USE_HW_BATTERY

/*
 * 배터리 잔량 — 백엔드 중립 API.
 *
 * 호출자(port/ble.c 의 BAS 등)는 어떤 백엔드가 쓰이는지 몰라야 한다.
 * 백엔드는 DTS 에서 자동 선택된다(battery.c 참고):
 *   - MAX17048 노드가 켜져 있으면 → I2C 연료게이지
 *   - 아니면                      → nRF VDDH 내장 ADC
 */

bool     batteryInit(void);

// 마지막 샘플 기준. 실패/미준비 시 false (out 값은 건드리지 않음).
bool     batteryGetVoltage(uint16_t *p_mv);
bool     batteryGetPercent(uint8_t *p_pct);

// 새로 측정한다. 호출자가 주기를 정한다(예: BAS 60초 주기).
// SAADC/I2C 를 깨우므로 idle 전력을 위해 자주 부르지 말 것.
bool     batteryUpdate(void);

#endif

#ifdef __cplusplus
}
#endif

#endif
