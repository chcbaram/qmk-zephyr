#include "ble_cfg.h"
#include "ble.h"
#include "quantum.h"
#include "via.h"
#include "log.h"

#ifdef VIA_ENABLE

static void via_qmk_ble_get_value(uint8_t *data)
{
  // data = [ value_id, value_data ]
  uint8_t *value_id   = &(data[0]);
  uint8_t *value_data = &(data[1]);

  switch (*value_id)
  {
    case id_qmk_ble_profile:
      value_data[0] = bleProfileGetActive();
      break;

    // 토글은 "본딩이 있나"를 그대로 보여준다 → VIA 에서 어느 슬롯이 차 있는지 한눈에 보인다.
    case id_qmk_ble_bond_0 ... id_qmk_ble_bond_4:
      value_data[0] = bleProfileIsOpen(*value_id - id_qmk_ble_bond_0) ? 0 : 1;
      break;

    case id_qmk_ble_clear_all:
      value_data[0] = 0;
      break;
  }
}

static void via_qmk_ble_set_value(uint8_t *data)
{
  // data = [ value_id, value_data ]
  uint8_t *value_id   = &(data[0]);
  uint8_t *value_data = &(data[1]);

  switch (*value_id)
  {
    case id_qmk_ble_profile:
      bleProfileSelect(value_data[0]);
      break;

    case id_qmk_ble_bond_0 ... id_qmk_ble_bond_4:
      // 끄는 방향(1 -> 0)만 의미가 있다 = 그 프로파일 본딩 삭제.
      // 켜는 방향은 무시한다 — 본딩은 호스트가 페어링해야 생기지 VIA 로 만들 수 없다.
      if (value_data[0] == 0)
      {
        bleProfileClear(*value_id - id_qmk_ble_bond_0);
      }
      break;

    case id_qmk_ble_clear_all:
      if (value_data[0])
      {
        bleProfileClearAll();
      }
      break;
  }
}

void via_qmk_ble_command(uint8_t *data, uint8_t length)
{
  // data = [ command_id, channel_id, value_id, value_data ]
  uint8_t *command_id        = &(data[0]);
  uint8_t *value_id_and_data = &(data[2]);

  switch (*command_id)
  {
    case id_custom_set_value:
      via_qmk_ble_set_value(value_id_and_data);
      break;

    case id_custom_get_value:
      via_qmk_ble_get_value(value_id_and_data);
      break;

    case id_custom_save:
      break;   // 프로파일은 port/ble.c 가 settings 에 즉시 저장한다

    default:
      *command_id = id_unhandled;
      break;
  }
}

#endif
