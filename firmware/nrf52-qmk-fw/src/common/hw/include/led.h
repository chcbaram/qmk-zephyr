#ifndef LED_H_
#define LED_H_

#ifdef __cplusplus
extern "C" {
#endif

#include "hw_def.h"


#define LED_MAX_CH  HW_LED_MAX_CH


bool ledInit(void);
// 슬립 진입 전 소등 + 핀을 기본 상태로(누설 방지). port/activity.c 에서 호출.
bool ledToSleep(void);
void ledOn(uint8_t ch);
void ledOff(uint8_t ch);
void ledToggle(uint8_t ch);

#ifdef __cplusplus
}
#endif

#endif