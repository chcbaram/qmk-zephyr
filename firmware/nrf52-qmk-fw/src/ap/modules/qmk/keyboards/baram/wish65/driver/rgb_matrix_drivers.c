#include "quantum.h"
#include "rgb_matrix.h"
#include "ws2812.h"
#include "qmk/qmk.h"
#include <zephyr/device.h>

/*
 * LED 개수의 진실은 **DTS 의 chain-length** 하나다(hw_def.h 가 거기서 읽는다).
 * QMK 쪽 RGB_MATRIX_LED_COUNT 는 g_led_config 배열 길이를 정하므로 별개로 존재할 수밖에
 * 없는데, 둘이 어긋나면 조용히 일부만 켜지거나 배열 밖을 읽는다. 여기서 못 박는다.
 */
BUILD_ASSERT(RGB_MATRIX_LED_COUNT == HW_WS2812_MAX_CH,
             "RGB_MATRIX_LED_COUNT(config.h) != DTS led_strip 의 chain-length");

/*
 * QMK RGB_MATRIX <-> 우리 hw 드라이버 연결.
 * (baram-qmk 의 키보드별 rgblight_drivers.c 와 같은 자리. 보드마다 LED 배치가 달라 키보드 트리)
 *
 * QMK 는 init/set_color/set_color_all/flush 네 개만 요구한다. 그 뒤(SPI/색순서/타이밍)는
 * hw/driver/ws2812.c -> Zephyr led_strip 이 처리한다.
 * ws2812Init() 은 hw.c 가 이미 부르므로 여기 init 은 비워둔다.
 */
static void rgb_matrix_ws2812_init(void)
{
}

static void rgb_matrix_ws2812_set_color(int index, uint8_t r, uint8_t g, uint8_t b)
{
  ws2812SetColor(index, WS2812_COLOR(r, g, b));
}

static void rgb_matrix_ws2812_set_color_all(uint8_t r, uint8_t g, uint8_t b)
{
  for (int i = 0; i < RGB_MATRIX_LED_COUNT; i++)
  {
    ws2812SetColor(i, WS2812_COLOR(r, g, b));
  }
}

/*
 * 실제 전송 + **스트립 전원을 RGB on/off 에 자동으로 따라가게** 한다.
 *
 * [주의] rgb_matrix_is_enabled() 만 보면 안 된다. 그건 **사용자 설정**이지 지금 켜져 있는지가
 * 아니다. 서스펜드(호스트 PC 잠 / activity IDLE)로 검게 표시해도 true 라, 레일이 켜진 채
 * 남아 네오픽셀 대기 전류만 7mA 를 먹는다(실측 7.12mA — idle 60µA 의 100배).
 *
 * rgb_matrix_get_suspend_state() 도 여기선 못 쓴다. rgb_matrix_set_suspend_state() 가
 *     rgb_task_render(0); rgb_task_flush(0);   <- 우리가 지금 여기 있다
 *     suspend_state = state;                   <- 플래그는 그 뒤에 세워진다
 * 순서라, 검은 프레임을 쏘는 이 flush 안에서는 아직 false 다. 그리고 이게 검정을 쏘는
 * **유일한** 기회다(그 뒤 루프는 잔다). -> qmkIsSuspended() 를 쓴다.
 *
 * 끌 때 순서: 검은색을 먼저 쏘고 전원을 내린다(ws2812SetPower 주석 참고).
 * 전원/SPI 를 실제로 다루는 건 hw/driver/ws2812.c 다 — 여기는 "언제" 만 정한다.
 */
static void rgb_matrix_ws2812_flush(void)
{
  bool want_on = rgb_matrix_is_enabled() && !qmkIsSuspended();

  if (want_on)
  {
    ws2812SetPower(true);
  }

  ws2812Refresh();   // 전원이 꺼져 있으면 알아서 건너뛴다

  if (!want_on)
  {
    ws2812SetPower(false);
  }
}

const rgb_matrix_driver_t rgb_matrix_driver = {
  .init          = rgb_matrix_ws2812_init,
  .set_color     = rgb_matrix_ws2812_set_color,
  .set_color_all = rgb_matrix_ws2812_set_color_all,
  .flush         = rgb_matrix_ws2812_flush,
};

/*
 * LED 물리 배치 (ramune60 이 keyboard.json 의 rgb_matrix.layout 으로 하는 것을 C 로 직접 쓴다 —
 * 우리는 QMK 의 python 빌드 시스템을 안 쓰므로 g_led_config 를 손으로 정의한다).
 *
 * [주의] 아래 좌표는 **근사값**이다. 65% 보드 외곽(224x64 좌표계)에 18개를 균등 배치했다.
 * 효과는 정상 동작하지만 splash/gradient 같은 위치 기반 효과가 실제 기판과 어긋날 수 있다 —
 * 실물 LED 위치에 맞춰 튜닝할 것. (x: 0~224, y: 0~64 가 QMK 관례)
 *
 * 언더글로우 전용이라 키 매트릭스 -> LED 매핑은 전부 NO_LED 다(키마다 LED 가 없다).
 */
led_config_t g_led_config = {
  // 키 매트릭스 -> LED 인덱스 (언더글로우뿐이므로 없음)
  {
    { NO_LED, NO_LED, NO_LED, NO_LED, NO_LED, NO_LED, NO_LED, NO_LED, NO_LED, NO_LED, NO_LED, NO_LED, NO_LED, NO_LED, NO_LED, NO_LED },
    { NO_LED, NO_LED, NO_LED, NO_LED, NO_LED, NO_LED, NO_LED, NO_LED, NO_LED, NO_LED, NO_LED, NO_LED, NO_LED, NO_LED, NO_LED, NO_LED },
    { NO_LED, NO_LED, NO_LED, NO_LED, NO_LED, NO_LED, NO_LED, NO_LED, NO_LED, NO_LED, NO_LED, NO_LED, NO_LED, NO_LED, NO_LED, NO_LED },
    { NO_LED, NO_LED, NO_LED, NO_LED, NO_LED, NO_LED, NO_LED, NO_LED, NO_LED, NO_LED, NO_LED, NO_LED, NO_LED, NO_LED, NO_LED, NO_LED },
    { NO_LED, NO_LED, NO_LED, NO_LED, NO_LED, NO_LED, NO_LED, NO_LED, NO_LED, NO_LED, NO_LED, NO_LED, NO_LED, NO_LED, NO_LED, NO_LED },
  },
  // LED 인덱스 -> 물리 좌표 (근사: 시계방향 외곽 배치)
  {
    {  12,  56 }, {  44,  56 }, {  76,  56 }, { 108,  56 }, { 140,  56 }, { 172,  56 }, { 204,  56 },  // 아래쪽 7
    { 218,  32 },                                                                                      // 오른쪽
    { 204,   8 }, { 172,   8 }, { 140,   8 }, { 108,   8 }, {  76,   8 }, {  44,   8 }, {  12,   8 },  // 위쪽 7
    {   4,  32 },                                                                                      // 왼쪽
    {  76,  32 }, { 148,  32 },                                                                        // 가운데 2
  },
  // LED 인덱스 -> 플래그 (전부 언더글로우)
  {
    LED_FLAG_UNDERGLOW, LED_FLAG_UNDERGLOW, LED_FLAG_UNDERGLOW, LED_FLAG_UNDERGLOW,
    LED_FLAG_UNDERGLOW, LED_FLAG_UNDERGLOW, LED_FLAG_UNDERGLOW, LED_FLAG_UNDERGLOW,
    LED_FLAG_UNDERGLOW, LED_FLAG_UNDERGLOW, LED_FLAG_UNDERGLOW, LED_FLAG_UNDERGLOW,
    LED_FLAG_UNDERGLOW, LED_FLAG_UNDERGLOW, LED_FLAG_UNDERGLOW, LED_FLAG_UNDERGLOW,
    LED_FLAG_UNDERGLOW, LED_FLAG_UNDERGLOW,
  },
};
