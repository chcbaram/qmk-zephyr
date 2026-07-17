#ifndef LED_H_
#define LED_H_

#ifdef __cplusplus
extern "C" {
#endif

#include "hw_def.h"

/*
 * 디버그용 LED(하트비트 등). **보드에 없을 수 있다** — DTS 에 led 노드가 있으면 hw_def.h 가
 * _USE_HW_LED 를 켠다(wish60 은 P0.31 에 있고, wish65 는 아예 없다).
 *
 * 키보드 인디케이터(Caps)는 이게 아니다 — DTS 의 caps_led 노드 + keyboards/<kbd>/port/led_port.c.
 */

#ifdef _USE_HW_LED

#define LED_MAX_CH  HW_LED_MAX_CH

bool ledInit(void);
// 슬립 진입 전 소등 + 핀을 기본 상태로(누설 방지). port/activity.c 에서 호출.
bool ledToSleep(void);
void ledOn(uint8_t ch);
void ledOff(uint8_t ch);
void ledToggle(uint8_t ch);

#else

// LED 없는 보드에서도 호출부가 그대로 컴파일되게 한다(log.h 와 같은 방식).
#define LED_MAX_CH            0
#define ledInit()             (true)
#define ledToSleep()          (true)
#define ledOn(ch)             ((void)0)
#define ledOff(ch)            ((void)0)
#define ledToggle(ch)         ((void)0)

#endif

#ifdef __cplusplus
}
#endif

#endif