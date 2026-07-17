#include "ext_power.h"

#ifdef _USE_HW_EXT_POWER
#include "log.h"
#include <zephyr/device.h>
#include <zephyr/drivers/regulator.h>

static const struct device *reg_dev = DEVICE_DT_GET(DT_NODELABEL(ext_power));
static bool                 is_init = false;

bool extPowerInit(void)
{
  if (!device_is_ready(reg_dev))
  {
    logPrintf("[E_] extPowerInit() not ready\n");
    return false;
  }
  is_init = true;

  // 부팅 시엔 내려둔다. regulator-fixed 는 boot-on 이 없으면 disable 상태로 시작하지만
  // 명시적으로 확인해 둔다(레일이 뜬 채면 네오픽셀이 조용히 mA 를 먹는다).
  logPrintf("[OK] extPowerInit() %s\n", regulator_is_enabled(reg_dev) ? "ON" : "OFF");
  return true;
}

bool extPowerEnable(void)
{
  if (!is_init)
  {
    return false;
  }
  // regulator API 가 참조 카운트를 관리한다(중복 호출 안전).
  return regulator_enable(reg_dev) == 0;
}

bool extPowerDisable(void)
{
  if (!is_init)
  {
    return false;
  }
  return regulator_disable(reg_dev) == 0;
}

bool extPowerIsEnabled(void)
{
  return is_init && regulator_is_enabled(reg_dev);
}

#endif
