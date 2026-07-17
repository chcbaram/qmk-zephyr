#include "ble_cfg.h"
#include "ble.h"
#include "quantum.h"
#include "via.h"
#include "log.h"

#ifdef VIA_ENABLE

/*
 * TX power 만 EEPROM 에 저장한다(프로파일/본딩은 port/ble.c 가 settings 로 저장).
 * 저장 위치는 port/port.h 의 오프셋 맵(EECONFIG_USER_BLE).
 */
#define BLE_CFG_MAGIC   0xA5   // eeconfig_init_user_datablock() 이 0 으로 밀기 때문에 필요

static const int8_t tx_power_tbl[] = BLE_TX_POWER_TBL;

typedef union
{
  uint32_t raw;

  struct PACKED
  {
    uint8_t magic;      // BLE_CFG_MAGIC = 저장된 적 있음
    uint8_t txp_idx;    // tx_power_tbl 인덱스
    uint8_t rsv[2];
  };
} ble_cfg_t;

_Static_assert(sizeof(ble_cfg_t) == sizeof(uint32_t), "EECONFIG out of spec.");

static ble_cfg_t ble_cfg_config;

EECONFIG_DEBOUNCE_HELPER(ble_cfg, EECONFIG_USER_BLE, ble_cfg_config);


void ble_cfg_init(void)
{
  eeconfig_init_ble_cfg();

  if (ble_cfg_config.magic != BLE_CFG_MAGIC ||
      ble_cfg_config.txp_idx >= ARRAY_SIZE(tx_power_tbl))
  {
    ble_cfg_config.magic   = BLE_CFG_MAGIC;
    ble_cfg_config.txp_idx = BLE_TX_POWER_DEF;
    ble_cfg_config.rsv[0]  = 0;
    ble_cfg_config.rsv[1]  = 0;
    eeconfig_flush_ble_cfg(true);
  }

  bleSetTxPower(tx_power_tbl[ble_cfg_config.txp_idx]);
  logPrintf("[OK] ble_cfg (tx %ddBm)\n", tx_power_tbl[ble_cfg_config.txp_idx]);
}


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

    case id_qmk_ble_tx_power:
      value_data[0] = ble_cfg_config.txp_idx;
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

    case id_qmk_ble_tx_power:
      if (value_data[0] < ARRAY_SIZE(tx_power_tbl))
      {
        ble_cfg_config.txp_idx = value_data[0];
        bleSetTxPower(tx_power_tbl[value_data[0]]);
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
      // 프로파일/본딩은 port/ble.c 가 settings 에 즉시 저장한다. TX power 만 여기서 flush.
      eeconfig_flush_ble_cfg(true);
      break;

    default:
      *command_id = id_unhandled;
      break;
  }
}

#endif
