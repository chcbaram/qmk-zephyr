#include "quantum.h"
#include "rgb_matrix.h"
#include "ws2812.h"
#include "ext_power.h"
#include "qmk/qmk.h"
#include <zephyr/pm/device.h>
#include <zephyr/device.h>

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
 * SPIM3 를 RGB on/off 에 맞춰 켜고 끈다.
 *
 * spi_nrfx_spim.c 는 첫 전송 때 nrfx_spim_init() 하고 그 뒤 **계속 ENABLE 로 남는다** —
 * RGB 를 꺼도 ~1mA 를 계속 먹는다(실측). SPIM3 는 nRF52840 anomaly 195 대상이고 그
 * workaround 는 nrfx_spim_uninit() 때 적용되므로 uninit 이 반드시 필요하다.
 *
 * CONFIG_PM_DEVICE_RUNTIME(전송마다 자동 suspend)은 **쓰지 않는다** — idle 이 69µA -> 652µA 로
 * 10배 악화됐다(실측). 여기서 RGB on/off 시점에만 직접 건드린다.
 */
static const struct device *spi_dev = DEVICE_DT_GET(DT_NODELABEL(spi3));

static void rgb_matrix_spi_set(bool on)
{
  if (!device_is_ready(spi_dev))
  {
    return;
  }
  // -EALREADY 는 정상(이미 그 상태). 그 외 실패는 다음 호출에서 재시도된다.
  pm_device_action_run(spi_dev, on ? PM_DEVICE_ACTION_RESUME : PM_DEVICE_ACTION_SUSPEND);
}

/*
 * 실제 전송 + **ext-power 레일을 RGB on/off 에 자동으로 따라가게** 한다.
 *
 * 레일은 부팅 시 꺼져 있다(DTS 에 regulator-boot-on 없음). 네오픽셀은 검은색을 표시해도
 * 컨트롤러가 개당 ~0.7mA 라 16개면 ≈11mA — idle 80.9µA 의 140배다. 그래서 "RGB 를 끄면"
 * 색만 검게 하는 게 아니라 **레일까지 내려야** 전류가 실제로 0 이 된다.
 *
 * 끌 때 순서가 중요하다: 검은색을 먼저 쏘고 레일을 내린다. 레일부터 내리면 LED 가 마지막
 * 색을 붙든 채로 꺼져서, 다시 켤 때 한 프레임 동안 옛 색이 번쩍인다.
 */
static void rgb_matrix_ws2812_flush(void)
{
  /*
   * [주의] rgb_matrix_is_enabled() 만 보면 안 된다. 그건 **사용자 설정**이지 지금 켜져 있는지가
   * 아니다. 서스펜드(호스트 PC 잠 / activity IDLE)로 검게 표시해도 true 라, 레일이 켜진 채
   * 남아 네오픽셀 대기 전류만 7mA 를 먹는다(실측 7.12mA — idle 60µA 의 100배).
   *
   * rgb_matrix_get_suspend_state() 도 여기선 못 쓴다. rgb_matrix_set_suspend_state() 가
   *     rgb_task_render(0); rgb_task_flush(0);   <- 우리가 지금 여기 있다
   *     suspend_state = state;                   <- 플래그는 그 뒤에 세워진다
   * 순서라, 검은 프레임을 쏘는 이 flush 안에서는 아직 false 다. 그리고 이게 검정을 쏘는
   * **유일한** 기회다(그 뒤 루프는 잔다). -> qmkIsSuspended() 를 쓴다.
   */
  bool want_on = rgb_matrix_is_enabled() && !qmkIsSuspended();

  if (want_on && !extPowerIsEnabled())
  {
    rgb_matrix_spi_set(true);
    extPowerEnable();
    wait_ms(2);   // DTS startup-delay-us 후 레일 안정화. 바로 쏘면 첫 프레임이 깨진다
  }

  ws2812Refresh();   // 레일이 꺼져 있으면 알아서 건너뛴다

  if (!want_on && extPowerIsEnabled())
  {
    extPowerDisable();
    rgb_matrix_spi_set(false);   // SPIM3 uninit — 안 하면 ~1mA 가 남는다
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
 * [주의] 아래 좌표는 **근사값**이다. 60% 보드 외곽(224x64 좌표계)에 16개를 균등 배치했다.
 * 효과는 정상 동작하지만 splash/gradient 같은 위치 기반 효과가 실제 기판과 어긋날 수 있다 —
 * 실물 LED 위치에 맞춰 튜닝할 것. (x: 0~224, y: 0~64 가 QMK 관례)
 *
 * 언더글로우 전용이라 키 매트릭스 -> LED 매핑은 전부 NO_LED 다(키마다 LED 가 없다).
 */
led_config_t g_led_config = {
  // 키 매트릭스 -> LED 인덱스 (언더글로우뿐이므로 없음)
  {
    { NO_LED, NO_LED, NO_LED, NO_LED, NO_LED, NO_LED, NO_LED, NO_LED, NO_LED, NO_LED, NO_LED, NO_LED, NO_LED, NO_LED, NO_LED },
    { NO_LED, NO_LED, NO_LED, NO_LED, NO_LED, NO_LED, NO_LED, NO_LED, NO_LED, NO_LED, NO_LED, NO_LED, NO_LED, NO_LED, NO_LED },
    { NO_LED, NO_LED, NO_LED, NO_LED, NO_LED, NO_LED, NO_LED, NO_LED, NO_LED, NO_LED, NO_LED, NO_LED, NO_LED, NO_LED, NO_LED },
    { NO_LED, NO_LED, NO_LED, NO_LED, NO_LED, NO_LED, NO_LED, NO_LED, NO_LED, NO_LED, NO_LED, NO_LED, NO_LED, NO_LED, NO_LED },
    { NO_LED, NO_LED, NO_LED, NO_LED, NO_LED, NO_LED, NO_LED, NO_LED, NO_LED, NO_LED, NO_LED, NO_LED, NO_LED, NO_LED, NO_LED },
  },
  // LED 인덱스 -> 물리 좌표 (근사: 시계방향 외곽 배치)
  {
    {  14,  56 }, {  52,  56 }, {  90,  56 }, { 128,  56 }, { 166,  56 }, { 204,  56 },  // 아래쪽 6
    { 216,  32 },                                                                        // 오른쪽
    { 204,   8 }, { 166,   8 }, { 128,   8 }, {  90,   8 }, {  52,   8 }, {  14,   8 },  // 위쪽 6
    {   4,  32 },                                                                        // 왼쪽
    {  70,  32 }, { 154,  32 },                                                          // 가운데 2
  },
  // LED 인덱스 -> 플래그 (전부 언더글로우 — ramune60 도 flags=2)
  {
    LED_FLAG_UNDERGLOW, LED_FLAG_UNDERGLOW, LED_FLAG_UNDERGLOW, LED_FLAG_UNDERGLOW,
    LED_FLAG_UNDERGLOW, LED_FLAG_UNDERGLOW, LED_FLAG_UNDERGLOW, LED_FLAG_UNDERGLOW,
    LED_FLAG_UNDERGLOW, LED_FLAG_UNDERGLOW, LED_FLAG_UNDERGLOW, LED_FLAG_UNDERGLOW,
    LED_FLAG_UNDERGLOW, LED_FLAG_UNDERGLOW, LED_FLAG_UNDERGLOW, LED_FLAG_UNDERGLOW,
  },
};
