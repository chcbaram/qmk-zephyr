#include "quantum.h"

#ifdef DEBOUNCE_RUNTIME

#include "debounce_cfg.h"
#include "port.h"
#include "via.h"
#include "log.h"

/*
 * 범위 — baram-qmk(VENOM)와 같은 5~40ms.
 *  5  : 이보다 낮추면 기계식 접점 채터링을 못 걸러 중복 입력이 난다.
 *  40 : 이보다 높이면 타이핑 지연이 체감된다.
 * 기본 10ms 는 config.h 의 컴파일타임 DEBOUNCE 와 같은 값(= EEPROM 이 비었을 때의 동작이
 * 빌드타임 동작과 일치하도록).
 */
#define DEBOUNCE_TIME_MIN       5
#define DEBOUNCE_TIME_MAX       40
#define DEBOUNCE_TIME_DEFAULT   10

enum via_qmk_debounce_value
{
  id_qmk_debounce_time = 1,
};

typedef union
{
  uint32_t raw;
  struct PACKED
  {
    uint8_t time;    // 디바운스 시간(ms)
  };
} debounce_cfg_t;

_Static_assert(sizeof(debounce_cfg_t) == sizeof(uint32_t), "EECONFIG out of spec.");

static debounce_cfg_t debounce_cfg_config;

EECONFIG_DEBOUNCE_HELPER(debounce_cfg, EECONFIG_USER_DEBOUNCE, debounce_cfg_config);

/*
 * sym_defer_pk.c 가 **키가 변할 때마다** 부른다 — 가볍게 유지할 것(단순 로드).
 * 범위 검증은 init/set 에서 이미 했다.
 */
uint8_t debounce_time_get(void)
{
  return debounce_cfg_config.time;
}

void debounce_cfg_init(void)
{
  eeconfig_init_debounce_cfg();

  /*
   * EEPROM 미초기화(0x00) 또는 손상값(0xFF 등) 방어 -> 기본값으로.
   *
   * [중요] eeconfig_init_user_datablock() 은 이 영역을 **0 으로 민다**(port.h 규칙). 0 은
   * 유효한 디바운스 시간이 아니므로 여기서 "저장된 적 없음"과 구분된다 — 별도 magic 이 필요 없다.
   * 0 을 그대로 쓰면 sym_defer_pk 가 카운터를 0(=DEBOUNCE_ELAPSED)으로 로드해 **디바운스가
   * 통째로 무력화**된다(채터링이 그대로 올라온다).
   */
  if (debounce_cfg_config.time < DEBOUNCE_TIME_MIN || debounce_cfg_config.time > DEBOUNCE_TIME_MAX)
  {
    debounce_cfg_config.time = DEBOUNCE_TIME_DEFAULT;
    eeconfig_flush_debounce_cfg(true);
  }

  logPrintf("[ON] DEBOUNCE RUNTIME (%d ms)\n", debounce_cfg_config.time);
}

static void via_qmk_debounce_get_value(uint8_t *data)
{
  // data = [ value_id, value_data ]
  uint8_t *value_id   = &(data[0]);
  uint8_t *value_data = &(data[1]);

  switch (*value_id)
  {
    case id_qmk_debounce_time:
      value_data[0] = debounce_cfg_config.time;
      break;
  }
}

static void via_qmk_debounce_set_value(uint8_t *data)
{
  // data = [ value_id, value_data ]
  uint8_t *value_id   = &(data[0]);
  uint8_t *value_data = &(data[1]);

  switch (*value_id)
  {
    case id_qmk_debounce_time:
      {
        uint8_t v = value_data[0];

        // VIA 슬라이더가 범위를 지키지만 신뢰하지 않는다 — 0 이 들어오면 디바운스가 무력화된다.
        if (v < DEBOUNCE_TIME_MIN) v = DEBOUNCE_TIME_MIN;
        if (v > DEBOUNCE_TIME_MAX) v = DEBOUNCE_TIME_MAX;

        // 저장 전에도 즉시 적용된다 — debounce_time_get() 이 이 값을 직접 읽는다.
        debounce_cfg_config.time = v;
        break;
      }
  }
}

void via_qmk_debounce_command(uint8_t *data, uint8_t length)
{
  // data = [ command_id, channel_id, value_id, value_data ]
  uint8_t *command_id        = &(data[0]);
  uint8_t *value_id_and_data = &(data[2]);

  switch (*command_id)
  {
    case id_custom_set_value:
      via_qmk_debounce_set_value(value_id_and_data);
      break;

    case id_custom_get_value:
      via_qmk_debounce_get_value(value_id_and_data);
      break;

    case id_custom_save:
      eeconfig_flush_debounce_cfg(true);
      break;

    default:
      *command_id = id_unhandled;
      break;
  }
}

#endif   // DEBOUNCE_RUNTIME
