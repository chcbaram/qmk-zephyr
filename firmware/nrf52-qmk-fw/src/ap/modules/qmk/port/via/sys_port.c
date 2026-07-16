#include "sys_port.h"
#include "quantum.h"
#include "via.h"
#include "eeprom.h"       // eeprom_req_clean()
#include "bootloader.h"   // bootloader_jump()
#include "log.h"

#ifdef VIA_ENABLE

// 확인 토글 3개의 비트를 모은다. 0x07 이 되어야 실제로 지운다.
static uint8_t eep_reset_confirm = 0;


static void via_qmk_system_get_value(uint8_t *data)
{
  // data = [ value_id, value_data ]
  uint8_t *value_id   = &(data[0]);
  uint8_t *value_data = &(data[1]);

  // 토글은 항상 꺼진 상태로 보여준다 — 확인 절차는 매번 처음부터 밟아야 한다.
  switch (*value_id)
  {
    case id_qmk_system_dfu:
    case id_qmk_system_eep_reset_0:
    case id_qmk_system_eep_reset_1:
    case id_qmk_system_eep_reset_done:
      value_data[0] = 0;
      break;
  }
}

static void via_qmk_system_set_value(uint8_t *data)
{
  // data = [ value_id, value_data ]
  uint8_t *value_id   = &(data[0]);
  uint8_t *value_data = &(data[1]);

  switch (*value_id)
  {
    case id_qmk_system_dfu:
      if (value_data[0])
      {
        logPrintf("[  ] via: bootloader jump\n");
        bootloader_jump();   // 돌아오지 않는다
      }
      break;

    case id_qmk_system_eep_reset_0:
      eep_reset_confirm |= (value_data[0] << 0);
      break;

    case id_qmk_system_eep_reset_1:
      eep_reset_confirm |= (value_data[0] << 1);
      break;

    case id_qmk_system_eep_reset_done:
      eep_reset_confirm |= (value_data[0] << 2);

      if (eep_reset_confirm == 0x07)
      {
        logPrintf("[  ] via: eeprom clean\n");
        // dirty flush 후 eeconfig_disable() + 재부팅 → 다음 부팅에 전부 기본값으로 재생성.
        eeprom_req_clean();
      }
      break;
  }
}

void via_qmk_system_command(uint8_t *data, uint8_t length)
{
  // data = [ command_id, channel_id, value_id, value_data ]
  uint8_t *command_id        = &(data[0]);
  uint8_t *value_id_and_data = &(data[2]);

  switch (*command_id)
  {
    case id_custom_set_value:
      via_qmk_system_set_value(value_id_and_data);
      break;

    case id_custom_get_value:
      via_qmk_system_get_value(value_id_and_data);
      break;

    case id_custom_save:
      break;   // 저장할 게 없다(즉시 실행형)

    default:
      *command_id = id_unhandled;
      break;
  }
}

#endif
