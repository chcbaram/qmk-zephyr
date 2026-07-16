#include "via_port.h"
#include "activity.h"
#include "quantum.h"
#include "via.h"
#include "log.h"

#ifdef VIA_ENABLE

/*
 * 저장 위치: VIA 의 커스텀 설정 영역(VIA_EEPROM_CUSTOM_CONFIG_ADDR).
 * 크기는 keyboards/baram/wish60/config.h 의 VIA_EEPROM_CUSTOM_CONFIG_SIZE 로 예약한다.
 *
 * 주의: VIA_EEPROM_CUSTOM_CONFIG_SIZE 를 바꾸면 그 뒤에 오는 dynamic keymap 의 시작 주소가
 * 밀린다. QMK 는 QMK_BUILDDATE 기반 매직으로 유효성을 검사하므로(via.c: via_eeprom_is_valid)
 * 어차피 빌드가 바뀌면 EEPROM 을 재초기화한다 → 키맵이 초기화된다. 크기는 넉넉히 잡고 고정할 것.
 */
#define VIA_POWER_CFG_ADDR    ((void *)VIA_EEPROM_CUSTOM_CONFIG_ADDR)

typedef struct
{
  uint16_t idle_timeout_s;    // 0 = 비활성
  uint16_t sleep_timeout_m;   // 0 = 비활성(절대 안 잠)
} via_power_cfg_t;


static void viaPowerLoad(void)
{
  via_power_cfg_t cfg;

  eeprom_read_block(&cfg, VIA_POWER_CFG_ADDR, sizeof(cfg));

  // 빈 EEPROM(0xFF)이면 저장된 적이 없다 → activity.c 의 기본값(ZMK 보드 값)을 그대로 둔다.
  if (cfg.idle_timeout_s == 0xFFFF || cfg.sleep_timeout_m == 0xFFFF)
  {
    return;
  }

  activitySetIdleTimeout((uint32_t)cfg.idle_timeout_s * 1000);
  activitySetSleepTimeout((uint32_t)cfg.sleep_timeout_m * 60 * 1000);
}

static void viaPowerSave(void)
{
  via_power_cfg_t cfg;

  cfg.idle_timeout_s  = (uint16_t)(activityGetIdleTimeout() / 1000);
  cfg.sleep_timeout_m = (uint16_t)(activityGetSleepTimeout() / (60 * 1000));

  // eeprom_update_block 은 미러 비교 후 변경분만 dirty 로 표시한다(port/platforms/eeprom.c).
  // 실제 플래시 쓰기는 settle-flush 가 100ms 뒤에 한 번에 처리한다.
  eeprom_update_block(&cfg, VIA_POWER_CFG_ADDR, sizeof(cfg));
}

void viaPortInit(void)
{
  viaPowerLoad();
  logPrintf("[  ] via power cfg: idle %ds, sleep %dmin\n",
            activityGetIdleTimeout() / 1000,
            activityGetSleepTimeout() / (60 * 1000));
}

/*
 * QMK via.c 의 weak 훅 오버라이드.
 * data = [command_id, channel_id, value_id, value_hi, value_lo]
 * 처리 못 하면 command_id 에 id_unhandled 를 넣어야 VIA 가 안다.
 */
void via_custom_value_command_kb(uint8_t *data, uint8_t length)
{
  uint8_t *command_id = &(data[0]);
  uint8_t *channel_id = &(data[1]);
  uint8_t *value_id   = &(data[2]);
  uint8_t *value_data = &(data[3]);

  if (*channel_id != ID_BARAM_POWER_CHANNEL)
  {
    *command_id = id_unhandled;
    return;
  }

  switch (*command_id)
  {
    case id_custom_set_value:
    {
      uint16_t value = ((uint16_t)value_data[0] << 8) | value_data[1];

      if (*value_id == ID_POWER_IDLE_TIMEOUT)
      {
        activitySetIdleTimeout((uint32_t)value * 1000);
      }
      else if (*value_id == ID_POWER_SLEEP_TIMEOUT)
      {
        activitySetSleepTimeout((uint32_t)value * 60 * 1000);
      }
      else
      {
        *command_id = id_unhandled;
      }
      break;
    }

    case id_custom_get_value:
    {
      uint16_t value = 0;

      if (*value_id == ID_POWER_IDLE_TIMEOUT)
      {
        value = (uint16_t)(activityGetIdleTimeout() / 1000);
      }
      else if (*value_id == ID_POWER_SLEEP_TIMEOUT)
      {
        value = (uint16_t)(activityGetSleepTimeout() / (60 * 1000));
      }
      else
      {
        *command_id = id_unhandled;
        break;
      }
      value_data[0] = (uint8_t)(value >> 8);
      value_data[1] = (uint8_t)(value & 0xFF);
      break;
    }

    case id_custom_save:
      viaPowerSave();
      break;

    default:
      *command_id = id_unhandled;
      break;
  }
}

#endif
