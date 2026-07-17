#ifndef WS2812_H_
#define WS2812_H_

#ifdef __cplusplus
extern "C" {
#endif

#include "hw_def.h"

#ifdef _USE_HW_WS2812

#define WS2812_MAX_CH  HW_WS2812_MAX_CH


#define WS2812_COLOR(r, g, b)   (((r)<<16) | ((g)<<8) | ((b)<<0))

#define WS2812_COLOR_RED        WS2812_COLOR(255,   0,   0)
#define WS2812_COLOR_GREEN      WS2812_COLOR(  0, 255,   0)
#define WS2812_COLOR_BLUE       WS2812_COLOR(  0,   0, 255)
#define WS2812_COLOR_OFF        WS2812_COLOR(  0,   0,   0)


bool ws2812Init(void);
void ws2812SetColor(uint32_t ch, uint32_t color);
bool ws2812Refresh(void);

/*
 * 스트립 전원 + SPI 버스를 함께 켜고 끈다.
 *
 * 레일만 내리면 안 된다 — SPI 컨트롤러가 ENABLE 로 남아 ~1mA 를 계속 먹는다(실측).
 * 반대로 SPI 만 끄고 레일을 두면 네오픽셀 대기 전류가 개당 ~0.4mA 남는다(16개면 7mA).
 * 둘은 항상 같이 움직여야 해서 한 API 로 묶었다.
 *
 * 끌 때는 **검은색을 먼저 쏜 뒤** 부르라 — 레일부터 내리면 LED 가 마지막 색을 붙든 채
 * 꺼져서 다시 켤 때 옛 색이 한 프레임 번쩍인다.
 */
void ws2812SetPower(bool enable);
bool ws2812IsPowered(void);


#endif

#ifdef __cplusplus
}
#endif

#endif 
