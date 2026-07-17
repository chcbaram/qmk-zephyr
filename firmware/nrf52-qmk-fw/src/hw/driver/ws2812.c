#include "ws2812.h"

#ifdef _USE_HW_WS2812
#include "ext_power.h"
#include "log.h"
#include "cli.h"
#include <zephyr/device.h>
#include <zephyr/drivers/led_strip.h>
#include <zephyr/pm/device.h>

/*
 * 네오픽셀 — Zephyr 네이티브 led_strip(worldsemi,ws2812-spi) 백엔드.
 * baram 의 hw 드라이버 API(ws2812SetColor/ws2812Refresh)를 그대로 유지해서
 * 키보드별 rgblight_drivers.c 가 baram 과 같은 모양이 되게 한다.
 *
 * 전원은 ext_power 레일이 공급한다 — 레일이 꺼져 있으면 SPI 를 쏴봐야 소용없다.
 */

static const struct device  *strip_dev = DEVICE_DT_GET(DT_NODELABEL(led_strip));

/*
 * 스트립이 매달린 **SPI 버스를 DTS 에서 유도한다** — 보드마다 다른 버스를 쓴다
 * (wish60 은 spi3, wish65 는 spi3 지만 595 가 spi2 에 있다). DT_PARENT 로 잡으면
 * 보드가 무엇을 쓰든 맞는 버스를 끈다. 예전엔 키보드 트리의 rgb_matrix_drivers.c 가
 * DT_NODELABEL(spi3) 을 직접 잡고 있었다 — LED 배치(키보드 관심사)가 아니라 SoC
 * 페리페럴 전력관리(hw 관심사)라 여기가 제자리다.
 *
 * [왜 수동으로 끄나] spi_nrfx_spim.c 는 첫 전송 때 nrfx_spim_init() 하고 그 뒤 계속
 * ENABLE 로 남아 ~1mA 를 먹는다(실측). nRF52840 의 SPIM3 는 anomaly 195 대상이고 그
 * workaround 가 nrfx_spim_uninit() 에서 적용되므로 uninit 이 반드시 필요하다.
 * CONFIG_PM_DEVICE_RUNTIME(전송마다 자동 suspend)은 쓰지 않는다 — idle 이 69µA -> 652µA 로
 * 10배 악화됐다(실측). RGB on/off 시점에만 직접 건드린다.
 */
static const struct device  *spi_dev   = DEVICE_DT_GET(DT_PARENT(DT_NODELABEL(led_strip)));
static struct led_rgb        pixels[WS2812_MAX_CH];
static bool                  is_init = false;

#if CLI_USE(HW_WS2812)
static void cliWs2812(cli_args_t *args);
#endif

bool ws2812Init(void)
{
  if (!device_is_ready(strip_dev))
  {
    logPrintf("[E_] ws2812Init() not ready\n");
    return false;
  }

  memset(pixels, 0, sizeof(pixels));
  is_init = true;

#if CLI_USE(HW_WS2812)
  cliAdd("ws2812", cliWs2812);
#endif

  logPrintf("[OK] ws2812Init() %d ch\n", WS2812_MAX_CH);
  return true;
}

void ws2812SetColor(uint32_t ch, uint32_t color)
{
  if (ch >= WS2812_MAX_CH)
  {
    return;
  }
  // WS2812_COLOR(r,g,b) = 0x00RRGGBB. 물리적 전송 순서(GRB 등)는 DTS 의
  // color-mapping 이 처리하므로 여기선 논리 RGB 로만 채운다.
  pixels[ch].r = (color >> 16) & 0xFF;
  pixels[ch].g = (color >>  8) & 0xFF;
  pixels[ch].b = (color >>  0) & 0xFF;
}

void ws2812SetPower(bool enable)
{
  if (enable == extPowerIsEnabled())
  {
    return;
  }

  if (enable)
  {
    if (device_is_ready(spi_dev))
    {
      pm_device_action_run(spi_dev, PM_DEVICE_ACTION_RESUME);   // -EALREADY 는 정상
    }
    extPowerEnable();
    delay(2);   // DTS startup-delay-us 후 레일 안정화. 바로 쏘면 첫 프레임이 깨진다
  }
  else
  {
    extPowerDisable();
    if (device_is_ready(spi_dev))
    {
      pm_device_action_run(spi_dev, PM_DEVICE_ACTION_SUSPEND);  // SPIM uninit
    }
  }
}

bool ws2812IsPowered(void)
{
  return extPowerIsEnabled();
}

bool ws2812Refresh(void)
{
  if (!is_init)
  {
    return false;
  }
  // 레일이 내려가 있으면 전송이 무의미하다(그리고 SPI 만 깨워 전력을 먹는다).
  if (!extPowerIsEnabled())
  {
    return false;
  }
  return led_strip_update_rgb(strip_dev, pixels, WS2812_MAX_CH) == 0;
}


#if CLI_USE(HW_WS2812)
/*
 * QMK RGBLIGHT 를 얹기 전에 **하드웨어(레일/스트립/색순서)를 먼저** 검증하기 위한 명령.
 * 여기서 색이 제대로 안 나오면 DTS(color-mapping, chain-length, MOSI 핀) 문제다.
 */
void cliWs2812(cli_args_t *args)
{
  bool ret = false;

  if (args->argc == 1 && args->isStr(0, "info"))
  {
    cliPrintf("led count  : %d\n", WS2812_MAX_CH);
    cliPrintf("ext power  : %s\n", extPowerIsEnabled() ? "ON" : "OFF");
    cliPrintf("\n네오픽셀은 검은색을 표시해도 개당 ~0.7mA 를 먹는다(16개 ≈ 11mA).\n");
    cliPrintf("안 쓸 땐 반드시 ext power 를 내릴 것 — idle 80.9µA 의 140배다.\n");
    ret = true;
  }

  // 레일부터 올린다. 레일이 꺼져 있으면 SPI 를 쏴도 아무것도 안 켜진다.
  if (args->argc == 4 && args->isStr(0, "color"))
  {
    uint8_t r = args->getData(1);
    uint8_t g = args->getData(2);
    uint8_t b = args->getData(3);

    extPowerEnable();
    delay(5);   // DTS startup-delay-us 후 레일 안정화
    for (int i = 0; i < WS2812_MAX_CH; i++)
    {
      ws2812SetColor(i, WS2812_COLOR(r, g, b));
    }
    cliPrintf("%s : R%d G%d B%d\n", ws2812Refresh() ? "OK" : "FAIL", r, g, b);
    ret = true;
  }

  if (args->argc == 1 && args->isStr(0, "off"))
  {
    for (int i = 0; i < WS2812_MAX_CH; i++)
    {
      ws2812SetColor(i, WS2812_COLOR_OFF);
    }
    ws2812Refresh();
    delay(5);
    extPowerDisable();   // 레일까지 내려야 전류가 실제로 0 이 된다
    cliPrintf("off (ext power down)\n");
    ret = true;
  }

  if (ret == false)
  {
    cliPrintf("ws2812 info\n");
    cliPrintf("ws2812 color [r] [g] [b]   : 예) ws2812 color 32 0 0 -> 빨강(어둡게)\n");
    cliPrintf("ws2812 off                 : 소등 + 레일 내림\n");
  }
}
#endif

#endif
