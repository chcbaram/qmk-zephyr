#include "power_cfg.h"
#include "activity.h"
#include "quantum.h"
#include "via.h"
#include "log.h"

#ifdef VIA_ENABLE

/*
 * 저장 위치: EECONFIG_USER_POWER (port/port.h 의 오프셋 맵).
 * 사용자 영역(512B)은 크기가 고정이라 여기에 항목을 늘려도 키맵 주소가 안 밀린다.
 */

// 기본값 = activity.c 와 동일해야 한다. VIA 드롭다운 목록에도 있어야 한다(없으면 "선택 없음").
#define POWER_IDLE_S_DEFAULT    30
#define POWER_SLEEP_M_DEFAULT   60
#define POWER_RGB_M_DEFAULT     5     // 5분 — activity.c 의 ACTIVITY_RGB_TIMEOUT_MS_DEF

// EEPROM 이 0 으로 밀렸는지(eeconfig_init_user_datablock) / 0xFF 빈값인지 구분하는 마커.
// 0 과 0xFF 둘 다 idle/sleep 의 유효값 범위와 겹치므로 값만으로는 판별할 수 없다.
#define POWER_CFG_MAGIC         0xA5

typedef union
{
  uint32_t raw;

  struct PACKED
  {
    uint8_t magic;     // POWER_CFG_MAGIC = 저장된 적 있음
    uint8_t idle_s;    // idle 타임아웃(초).  0 = 비활성
    uint8_t sleep_m;   // sleep 타임아웃(분). 0 = 안 잠
    uint8_t rsv;
  };
} power_cfg_t;

_Static_assert(sizeof(power_cfg_t) == sizeof(uint32_t), "EECONFIG out of spec.");

static power_cfg_t power_cfg_config;

EECONFIG_DEBOUNCE_HELPER(power_cfg, EECONFIG_USER_POWER, power_cfg_config);

/*
 * RGB 소등 타임아웃 — **별도 EEPROM 블록**이다(port.h 의 +16).
 *
 * 위 power_cfg_t 의 남는 rsv 바이트를 쓰면 안 된다: 기존 보드엔 rsv=0 이 magic 과 함께 이미
 * 저장돼 있어, 업그레이드하면 0 = "안 끔" 으로 읽혀 **RGB 가 영영 안 꺼진다**(그 상태로 deep
 * sleep 에 들어가면 네오픽셀이 65mA 로 계속 켜져 있다). 새 항목은 새 오프셋 + 자기 magic.
 */
#define RGB_CFG_MAGIC   0xA5

typedef union
{
  uint32_t raw;

  struct PACKED
  {
    uint8_t magic;       // RGB_CFG_MAGIC = 저장된 적 있음 (0 이 유효값이라 값만으론 판별 불가)
    uint8_t timeout_m;   // RGB/인디케이터 소등 타임아웃(분). 0 = 안 끔
                         // 초가 아니라 분인 이유: VIA 드롭다운은 1바이트라 초 단위면 4분 15초가
                         // 한계다. sleep_m 과도 일관된다.
    uint8_t rsv[2];
  };
} rgb_cfg_t;

_Static_assert(sizeof(rgb_cfg_t) == sizeof(uint32_t), "EECONFIG out of spec.");

static rgb_cfg_t rgb_cfg_config;

EECONFIG_DEBOUNCE_HELPER(rgb_cfg, EECONFIG_USER_RGB_CFG, rgb_cfg_config);


// 설정값을 activity 상태머신에 반영.
static void power_cfg_apply(void)
{
  activitySetIdleTimeout((uint32_t)power_cfg_config.idle_s * 1000);
  activitySetSleepTimeout((uint32_t)power_cfg_config.sleep_m * 60 * 1000);
  activitySetRgbTimeout((uint32_t)rgb_cfg_config.timeout_m * 60 * 1000);
}

void power_cfg_init(void)
{
  eeconfig_init_power_cfg();

  eeconfig_init_rgb_cfg();

  // 미초기화(0x00 / 0xFF) 방어 → 기본값으로 되돌리고 한 번 flush.
  // RGB 는 별도 블록이라 따로 본다 — POWER 를 이미 저장한 보드도 여기선 처음이다.
  if (rgb_cfg_config.magic != RGB_CFG_MAGIC)
  {
    rgb_cfg_config.magic     = RGB_CFG_MAGIC;
    rgb_cfg_config.timeout_m = POWER_RGB_M_DEFAULT;
    rgb_cfg_config.rsv[0]    = 0;
    rgb_cfg_config.rsv[1]    = 0;
    eeconfig_flush_rgb_cfg(true);
  }

  if (power_cfg_config.magic != POWER_CFG_MAGIC)
  {
    power_cfg_config.magic   = POWER_CFG_MAGIC;
    power_cfg_config.idle_s  = POWER_IDLE_S_DEFAULT;
    power_cfg_config.sleep_m = POWER_SLEEP_M_DEFAULT;
    power_cfg_config.rsv     = 0;
    eeconfig_flush_power_cfg(true);
  }

  power_cfg_apply();

  logPrintf("[OK] power_cfg (idle:%ds sleep:%dmin)\n",
            power_cfg_config.idle_s, power_cfg_config.sleep_m);
}

static void via_qmk_power_get_value(uint8_t *data)
{
  // data = [ value_id, value_data ]
  uint8_t *value_id   = &(data[0]);
  uint8_t *value_data = &(data[1]);

  switch (*value_id)
  {
    case id_qmk_power_idle_timeout:
      value_data[0] = power_cfg_config.idle_s;
      break;

    case id_qmk_power_sleep_timeout:
      value_data[0] = power_cfg_config.sleep_m;
      break;

    case id_qmk_power_rgb_timeout:
      value_data[0] = rgb_cfg_config.timeout_m;
      break;
  }
}

static void via_qmk_power_set_value(uint8_t *data)
{
  // data = [ value_id, value_data ]
  uint8_t *value_id   = &(data[0]);
  uint8_t *value_data = &(data[1]);

  switch (*value_id)
  {
    case id_qmk_power_idle_timeout:
      power_cfg_config.idle_s = value_data[0];
      break;

    case id_qmk_power_sleep_timeout:
      power_cfg_config.sleep_m = value_data[0];
      break;

    case id_qmk_power_rgb_timeout:
      rgb_cfg_config.timeout_m = value_data[0];
      break;
  }

  power_cfg_apply();   // 저장 전에도 즉시 반영(VIA 는 set 후 save 를 따로 보낸다)
}

void via_qmk_power_command(uint8_t *data, uint8_t length)
{
  // data = [ command_id, channel_id, value_id, value_data ]
  uint8_t *command_id        = &(data[0]);
  uint8_t *value_id_and_data = &(data[2]);

  switch (*command_id)
  {
    case id_custom_set_value:
      via_qmk_power_set_value(value_id_and_data);
      break;

    case id_custom_get_value:
      via_qmk_power_get_value(value_id_and_data);
      break;

    case id_custom_save:
      eeconfig_flush_power_cfg(true);
      eeconfig_flush_rgb_cfg(true);   // RGB 는 별도 블록이라 따로 flush 해야 한다
      break;

    default:
      *command_id = id_unhandled;
      break;
  }
}

#endif
