#include "quantum.h"

#ifdef HOLD_OKP_RUNTIME

#include "hold_okp.h"
#include "port.h"
#include "via.h"
#include "log.h"

// 0 을 유효값(=OFF)으로 쓰므로 "저장된 적 있음"을 스스로 판별해야 한다(port.h 규칙).
#define HOLD_OKP_MAGIC 0x5A

enum via_qmk_hold_okp_value
{
  id_qmk_hold_okp_enable = 1,
};

typedef union
{
  uint32_t raw;
  struct PACKED
  {
    uint8_t enable;   // 1 = ON, 0 = OFF
    uint8_t magic;    // HOLD_OKP_MAGIC 이면 초기화됨
  };
} hold_okp_cfg_t;

_Static_assert(sizeof(hold_okp_cfg_t) == sizeof(uint32_t), "EECONFIG out of spec.");

static hold_okp_cfg_t hold_okp_config;

EECONFIG_DEBOUNCE_HELPER(hold_okp, EECONFIG_USER_HOLD_OKP, hold_okp_config);

void hold_okp_init(void)
{
  eeconfig_init_hold_okp();

  // eeconfig_init_user_datablock() 이 0 으로 밀므로 magic 으로 판별한다 — enable 은 0 도 유효값이라
  // 값만 봐서는 "OFF 로 저장됨"과 "저장된 적 없음"을 구분할 수 없다.
  if (hold_okp_config.magic != HOLD_OKP_MAGIC)
  {
    hold_okp_config.enable = 1;   // 기본 ON (QMK 의 HOLD_ON_OTHER_KEY_PRESS 관례)
    hold_okp_config.magic  = HOLD_OKP_MAGIC;
    eeconfig_flush_hold_okp(true);
  }

  logPrintf("[ON] HOLD_ON_OTHER_KEY_PRESS RUNTIME (%s)\n", hold_okp_config.enable ? "ON" : "OFF");
}

/*
 * action_tapping.c 의 weak 기본(false)을 오버라이드한다.
 * 탭/홀드 판정 경로에서 불리므로 가볍게 유지할 것.
 */
bool get_hold_on_other_key_press(uint16_t keycode, keyrecord_t *record)
{
  return hold_okp_config.enable != 0;
}

static void via_qmk_hold_okp_get_value(uint8_t *data)
{
  uint8_t *value_id   = &(data[0]);
  uint8_t *value_data = &(data[1]);

  switch (*value_id)
  {
    case id_qmk_hold_okp_enable:
      value_data[0] = hold_okp_config.enable;
      break;
  }
}

static void via_qmk_hold_okp_set_value(uint8_t *data)
{
  uint8_t *value_id   = &(data[0]);
  uint8_t *value_data = &(data[1]);

  switch (*value_id)
  {
    case id_qmk_hold_okp_enable:
      // 저장 전에도 즉시 적용된다 — 위 콜백이 이 값을 직접 읽는다.
      hold_okp_config.enable = value_data[0] ? 1 : 0;
      break;
  }
}

void via_qmk_hold_okp_command(uint8_t *data, uint8_t length)
{
  // data = [ command_id, channel_id, value_id, value_data ]
  uint8_t *command_id        = &(data[0]);
  uint8_t *value_id_and_data = &(data[2]);

  switch (*command_id)
  {
    case id_custom_set_value:
      via_qmk_hold_okp_set_value(value_id_and_data);
      break;

    case id_custom_get_value:
      via_qmk_hold_okp_get_value(value_id_and_data);
      break;

    case id_custom_save:
      eeconfig_flush_hold_okp(true);
      break;

    default:
      *command_id = id_unhandled;
      break;
  }
}

#endif   // HOLD_OKP_RUNTIME
