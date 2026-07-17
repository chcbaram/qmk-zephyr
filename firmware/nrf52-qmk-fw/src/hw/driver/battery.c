/*
 * 배터리 잔량 드라이버.
 *
 * lithium_ion_mv_to_pct() 와 VDDHDIV5 측정 방식은 ZMK 에서 가져왔다.
 *   zmk/app/module/drivers/sensor/battery/{battery_nrf_vddh,battery_common}.c
 *   Copyright (c) 2021 The ZMK Contributors / SPDX-License-Identifier: MIT
 *
 * ZMK 의 sensor 드라이버 껍데기(DEVICE_DT_INST_DEFINE + sensor API)는 가져오지 않았다.
 * ZMK 는 이벤트 버스가 sensor API 를 요구하지만 우리는 batteryUpdate() 를 직접 부르는
 * 폴링 구조라, 껍데기 없이 측정 로직만 쓰는 편이 단순하다.
 */

#include "battery.h"

#ifdef _USE_HW_BATTERY
#include "log.h"
#include "cli.h"

#if CLI_USE(HW_BATTERY)
static void cliBattery(cli_args_t *args);
#endif

/*
 * 백엔드는 DTS 가 정한다 — 호출자는 어느 쪽인지 모른다.
 *
 *  MAX17048(I2C 연료게이지): wish60 회로엔 i2c1 @0x36 에 실장돼 있다. DTS 에서 노드를
 *    켜고 CONFIG_MAX17048=y 만 하면 여기로 자동 전환된다. Zephyr 네이티브 드라이버라
 *    (drivers/fuel_gauge/max17048) ZMK 처럼 드라이버를 흡수할 필요가 없다.
 *  VDDH(내장 ADC): 부품 0. 배터리가 VDDH 핀에 직결된 고전압 모드여야 유효하다.
 *    일반 모드(VDDH=VDD)면 레귤레이터 출력을 읽어 잔량과 무관한 값이 나온다.
 */
#define BAT_USE_MAX17048   DT_HAS_COMPAT_STATUS_OKAY(maxim_max17048)

#if BAT_USE_MAX17048
#include <zephyr/drivers/fuel_gauge.h>
static const struct device *bat_dev = DEVICE_DT_GET_ANY(maxim_max17048);
#else
#include <zephyr/drivers/adc.h>
#include <hal/nrf_saadc.h>
static const struct device *bat_dev = DEVICE_DT_GET(DT_NODELABEL(adc));

#define BAT_VDDH_DIV       5      // SAADC 내부 입력이 VDDH/5 라 5 를 곱해 되돌린다
#define BAT_ADC_RESOLUTION 12
#define BAT_ADC_CHANNEL    0

static int16_t                 adc_raw;
static struct adc_channel_cfg  adc_ch = {
  .gain             = ADC_GAIN_1_2,
  .reference        = ADC_REF_INTERNAL,
  .acquisition_time = ADC_ACQ_TIME(ADC_ACQ_TIME_MICROSECONDS, 40),
  .channel_id       = BAT_ADC_CHANNEL,
  .input_positive   = SAADC_CH_PSELP_PSELP_VDDHDIV5,
};
static struct adc_sequence     adc_seq = {
  .channels    = BIT(BAT_ADC_CHANNEL),
  .buffer      = &adc_raw,
  .buffer_size = sizeof(adc_raw),
  .resolution  = BAT_ADC_RESOLUTION,
  .oversampling = 4,
  .calibrate   = true,      // 첫 측정만 캘리브레이션(이후 batteryUpdate 에서 끈다)
};
#endif

static bool     is_init  = false;
static uint16_t bat_mv   = 0;
static uint8_t  bat_pct  = 0;


/*
 * 전압 → 잔량(%). OCV 곡선 구간 선형보간.
 *
 * ZMK 는 3.45~4.2V 를 하나의 직선으로 근사한다(`mv * 2 / 15 - 459`, 주석에도
 * "Simple linear approximation" 이라고 밝혀둠). 리튬폴리머 방전 곡선은 3.7~3.9V 구간이
 * 아주 평평해서 직선 근사는 초반 급락 / 중반 정체로 체감된다. 그래서 테이블 보간으로 바꿨다.
 *
 * 한계(중요): 전압 기반 SOC 는 무엇을 해도 근사다.
 *  - 평평한 구간에선 10mV 오차가 잔량 ~10% 로 번역된다
 *  - 타이핑 중엔 부하로 전압이 처져 잔량이 낮게 보인다(무부하 회복 시 되돌아옴)
 *  - 충전 중엔 셀이 아니라 충전 전압을 읽는다
 * 정확한 잔량이 필요하면 MAX17048 백엔드로 간다 — 칩이 부하/이력을 보정한다.
 */
typedef struct
{
  uint16_t mv;
  uint8_t  pct;
} bat_curve_t;

// 무부하(OCV) 기준 4.2V 리튬폴리머 방전 곡선. 내림차순.
static const bat_curve_t bat_curve[] = {
  {4200, 100}, {4150,  95}, {4110,  90}, {4080,  85}, {4020,  80},
  {3980,  75}, {3950,  70}, {3910,  65}, {3870,  60}, {3850,  55},
  {3840,  50}, {3820,  45}, {3800,  40}, {3790,  35}, {3770,  30},
  {3750,  25}, {3730,  20}, {3710,  16}, {3690,  13}, {3610,  10},
  {3520,   7}, {3420,   5}, {3300,   3}, {3000,   0},
};

static uint8_t batteryMvToPct(uint16_t mv)
{
  const uint32_t curve_len = sizeof(bat_curve) / sizeof(bat_curve[0]);

  if (mv >= bat_curve[0].mv)
  {
    return 100;
  }
  if (mv <= bat_curve[curve_len - 1].mv)
  {
    return 0;
  }

  for (uint32_t i = 1; i < curve_len; i++)
  {
    if (mv >= bat_curve[i].mv)
    {
      // bat_curve[i] ~ bat_curve[i-1] 구간 선형보간
      const bat_curve_t *lo = &bat_curve[i];
      const bat_curve_t *hi = &bat_curve[i - 1];
      uint32_t span = hi->mv - lo->mv;

      return (uint8_t)(lo->pct + ((uint32_t)(mv - lo->mv) * (hi->pct - lo->pct) + span / 2) / span);
    }
  }
  return 0;
}

bool batteryInit(void)
{
  if (!device_is_ready(bat_dev))
  {
    logPrintf("[E_] battery: %s not ready\n", bat_dev->name);
    return false;
  }

#if !BAT_USE_MAX17048
  int ret = adc_channel_setup(bat_dev, &adc_ch);
  if (ret != 0)
  {
    logPrintf("[E_] battery: adc_channel_setup (%d)\n", ret);
    return false;
  }
#endif

  is_init = true;
  batteryUpdate();

#if CLI_USE(HW_BATTERY)
  cliAdd("battery", cliBattery);
#endif

  logPrintf("[OK] batteryInit() %s : %d mV, %d%%\n",
            BAT_USE_MAX17048 ? "MAX17048" : "VDDH", bat_mv, bat_pct);
  return true;
}

bool batteryUpdate(void)
{
  if (!is_init)
  {
    return false;
  }

#if BAT_USE_MAX17048
  {
    // 연료게이지는 칩이 잔량 곡선을 관리하므로 % 를 그대로 받는다.
    fuel_gauge_prop_t props[] = {FUEL_GAUGE_VOLTAGE, FUEL_GAUGE_RELATIVE_STATE_OF_CHARGE};
    union fuel_gauge_prop_val vals[2];
    int ret = fuel_gauge_get_props(bat_dev, props, vals, 2);
    if (ret != 0)
    {
      logPrintf("[E_] battery: fuel_gauge_get_props (%d)\n", ret);
      return false;
    }
    bat_mv  = (uint16_t)(vals[0].voltage / 1000);   // µV -> mV

    /*
     * SOC 를 0~100 으로 클램프한다 — **MAX17048 은 100 을 넘겨서 준다**(실측 122%, 125%).
     * 칩의 ModelGauge 알고리즘은 셀 전압이 모델의 만충 전압보다 높으면(충전 중/USB 연결/
     * 전원 켠 직후 미학습) 100 을 초과한 값을 낸다 — 데이터시트가 SOC 를 순간 100 초과로
     * 허용한다. 버그가 아니라 정상 동작이다. 하지만:
     *   - BLE BAS 는 0~100 만 유효하다(초과값은 스펙 위반).
     *   - 사용자에게 122% 는 무의미하다.
     * 그래서 여기서 자른다. (전압은 그대로 둔다 — 4068mV 는 실제 값이라 참고가 된다)
     */
    uint16_t soc = vals[1].relative_state_of_charge;
    bat_pct = (soc > 100) ? 100 : (uint8_t)soc;
  }
#else
  {
    int32_t mv;
    int     ret = adc_read(bat_dev, &adc_seq);

    adc_seq.calibrate = false;    // 캘리브레이션은 첫 회만(매번 하면 느리고 전력만 먹음)
    if (ret != 0)
    {
      logPrintf("[E_] battery: adc_read (%d)\n", ret);
      return false;
    }

    mv  = adc_raw;
    ret = adc_raw_to_millivolts(adc_ref_internal(bat_dev), adc_ch.gain, BAT_ADC_RESOLUTION, &mv);
    if (ret != 0)
    {
      logPrintf("[E_] battery: raw_to_mv (%d)\n", ret);
      return false;
    }

    bat_mv  = (uint16_t)(mv * BAT_VDDH_DIV);
    bat_pct = batteryMvToPct(bat_mv);
  }
#endif

  return true;
}

bool batteryGetVoltage(uint16_t *p_mv)
{
  if (!is_init)
  {
    return false;
  }
  *p_mv = bat_mv;
  return true;
}

bool batteryGetPercent(uint8_t *p_pct)
{
  if (!is_init)
  {
    return false;
  }
  *p_pct = bat_pct;
  return true;
}


#if CLI_USE(HW_BATTERY)
void cliBattery(cli_args_t *args)
{
  bool ret = false;

  if (args->argc == 1 && args->isStr(0, "info"))
  {
    cliPrintf("backend  : %s\n", BAT_USE_MAX17048 ? "MAX17048 (I2C fuel gauge)" : "VDDH (SAADC)");
    cliPrintf("voltage  : %d mV\n", bat_mv);
    cliPrintf("percent  : %d %%\n", bat_pct);
#if !BAT_USE_MAX17048
    cliPrintf("adc raw  : %d\n", adc_raw);
    cliPrintf("\n");
    cliPrintf("VDDH 방식은 배터리가 VDDH 핀에 직결된 고전압 모드가 전제다.\n");
    cliPrintf("위 voltage 를 실제 배터리 전압(멀티미터)과 비교할 것:\n");
    cliPrintf("  일치       -> 고전압 모드 OK\n");
    cliPrintf("  항상 ~3.3V -> 일반 모드(VDDH=VDD). 레귤레이터 출력을 읽는 중이라 무의미.\n");
    cliPrintf("               -> MAX17048(i2c1 @0x36) 백엔드로 전환 필요\n");
#endif
    ret = true;
  }

  if (args->argc == 1 && args->isStr(0, "update"))
  {
    // 1초 주기로 재측정. 충전기 연결/해제나 타이핑 부하에 따른 전압 변동을 볼 때 유용.
    while (cliKeepLoop())
    {
      batteryUpdate();
      cliPrintf("%d mV, %d %%\n", bat_mv, bat_pct);
      delay(1000);
    }
    ret = true;
  }

  if (ret == false)
  {
    cliPrintf("battery info\n");
    cliPrintf("battery update\n");
  }
}
#endif

#endif
