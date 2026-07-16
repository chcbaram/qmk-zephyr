#include "via_port.h"
#include "power_cfg.h"
#include "sys_port.h"
#include "quantum.h"
#include "via.h"

#ifdef VIA_ENABLE

void viaPortInit(void)
{
  power_cfg_init();
}

// QMK via.c 의 weak 훅 오버라이드. 채널만 보고 각 기능으로 넘긴다.
// 처리 못 하면 command_id 에 id_unhandled(0xFF) 를 넣어야 VIA 가 안다.
void via_custom_value_command_kb(uint8_t *data, uint8_t length)
{
  uint8_t *command_id = &(data[0]);
  uint8_t *channel_id = &(data[1]);

  if (*channel_id == ID_QMK_POWER_CHANNEL)
  {
    via_qmk_power_command(data, length);
    return;
  }

  if (*channel_id == ID_QMK_SYSTEM_CHANNEL)
  {
    via_qmk_system_command(data, length);
    return;
  }

  *command_id = id_unhandled;
}

#endif
