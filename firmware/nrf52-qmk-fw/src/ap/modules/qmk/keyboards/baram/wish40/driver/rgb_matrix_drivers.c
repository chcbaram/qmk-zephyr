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

led_config_t g_led_config = {
  // 키 매트릭스 -> LED 인덱스. **per-key RGB** 다(wish60/65 의 언더글로우와 다르다).
  // SK6812MINI-E 체인은 **지그재그**로 돈다: row0 좌->우, row1 우->좌, row2 좌->우, row3 우->좌.
  // 배선 없는 셀(transform 이 안 쓰는 자리)은 NO_LED — 체인에서도 건너뛴다.
  {
    {      0,      1,      2,      3,      4,      5,      6,      7,      8,      9,     10,     11 },
    {     22,     21,     20,     19,     18,     17,     16,     15,     14,     13,     12, NO_LED },
    {     23, NO_LED,     24,     25,     26,     27,     28,     29,     30,     31,     32,     33 },
    {     41, NO_LED,     40,     39, NO_LED,     38, NO_LED,     37, NO_LED,     36,     35,     34 },
  },
  // LED 인덱스 -> 물리 좌표 (x:0~224, y:0~64, 키 중심 근사). 위 체인 순서와 같은 순서다.
  {
    {   0,  0 },   //  0 = key(0,0)
    {  20,  0 },   //  1 = key(0,1)
    {  41,  0 },   //  2 = key(0,2)
    {  61,  0 },   //  3 = key(0,3)
    {  81,  0 },   //  4 = key(0,4)
    { 102,  0 },   //  5 = key(0,5)
    { 122,  0 },   //  6 = key(0,6)
    { 143,  0 },   //  7 = key(0,7)
    { 163,  0 },   //  8 = key(0,8)
    { 183,  0 },   //  9 = key(0,9)
    { 204,  0 },   // 10 = key(0,10)
    { 224,  0 },   // 11 = key(0,11)
    { 204, 21 },   // 12 = key(1,10)
    { 183, 21 },   // 13 = key(1,9)
    { 163, 21 },   // 14 = key(1,8)
    { 143, 21 },   // 15 = key(1,7)
    { 122, 21 },   // 16 = key(1,6)
    { 102, 21 },   // 17 = key(1,5)
    {  81, 21 },   // 18 = key(1,4)
    {  61, 21 },   // 19 = key(1,3)
    {  41, 21 },   // 20 = key(1,2)
    {  20, 21 },   // 21 = key(1,1)
    {   0, 21 },   // 22 = key(1,0)
    {   0, 43 },   // 23 = key(2,0)
    {  41, 43 },   // 24 = key(2,2)
    {  61, 43 },   // 25 = key(2,3)
    {  81, 43 },   // 26 = key(2,4)
    { 102, 43 },   // 27 = key(2,5)
    { 122, 43 },   // 28 = key(2,6)
    { 143, 43 },   // 29 = key(2,7)
    { 163, 43 },   // 30 = key(2,8)
    { 183, 43 },   // 31 = key(2,9)
    { 204, 43 },   // 32 = key(2,10)
    { 224, 43 },   // 33 = key(2,11)
    { 224, 64 },   // 34 = key(3,11)
    { 204, 64 },   // 35 = key(3,10)
    { 183, 64 },   // 36 = key(3,9)
    { 143, 64 },   // 37 = key(3,7)
    { 102, 64 },   // 38 = key(3,5)
    {  61, 64 },   // 39 = key(3,3)
    {  41, 64 },   // 40 = key(3,2)
    {   0, 64 },   // 41 = key(3,0)
  },
  // LED 인덱스 -> 플래그. per-key 라 전부 KEYLIGHT (언더글로우가 아니다).
  {
    LED_FLAG_KEYLIGHT, LED_FLAG_KEYLIGHT, LED_FLAG_KEYLIGHT, LED_FLAG_KEYLIGHT, LED_FLAG_KEYLIGHT, LED_FLAG_KEYLIGHT,
    LED_FLAG_KEYLIGHT, LED_FLAG_KEYLIGHT, LED_FLAG_KEYLIGHT, LED_FLAG_KEYLIGHT, LED_FLAG_KEYLIGHT, LED_FLAG_KEYLIGHT,
    LED_FLAG_KEYLIGHT, LED_FLAG_KEYLIGHT, LED_FLAG_KEYLIGHT, LED_FLAG_KEYLIGHT, LED_FLAG_KEYLIGHT, LED_FLAG_KEYLIGHT,
    LED_FLAG_KEYLIGHT, LED_FLAG_KEYLIGHT, LED_FLAG_KEYLIGHT, LED_FLAG_KEYLIGHT, LED_FLAG_KEYLIGHT, LED_FLAG_KEYLIGHT,
    LED_FLAG_KEYLIGHT, LED_FLAG_KEYLIGHT, LED_FLAG_KEYLIGHT, LED_FLAG_KEYLIGHT, LED_FLAG_KEYLIGHT, LED_FLAG_KEYLIGHT,
    LED_FLAG_KEYLIGHT, LED_FLAG_KEYLIGHT, LED_FLAG_KEYLIGHT, LED_FLAG_KEYLIGHT, LED_FLAG_KEYLIGHT, LED_FLAG_KEYLIGHT,
    LED_FLAG_KEYLIGHT, LED_FLAG_KEYLIGHT, LED_FLAG_KEYLIGHT, LED_FLAG_KEYLIGHT, LED_FLAG_KEYLIGHT, LED_FLAG_KEYLIGHT,
  },
};
