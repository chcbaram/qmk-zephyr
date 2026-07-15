#include "bootloader.h"
#include <zephyr/kernel.h>
#include <zephyr/sys/reboot.h>
#include <hal/nrf_power.h>

// 부트로더는 보드에 미리 플래시된 Adafruit nRF52 UF2 부트로더를 사용한다.
// GPREGRET 에 매직값을 쓰고 리셋하면 부트로더가 UF2(DFU) 모드로 진입한다.
#define DFU_MAGIC_UF2_RESET   0x57

void bootloader_jump(void)
{
  nrf_power_gpregret_set(NRF_POWER, 0, DFU_MAGIC_UF2_RESET);
  sys_reboot(SYS_REBOOT_COLD);
}

void mcu_reset(void)
{
  sys_reboot(SYS_REBOOT_COLD);
}
