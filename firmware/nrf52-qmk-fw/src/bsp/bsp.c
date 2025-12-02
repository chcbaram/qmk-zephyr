#include "bsp.h"





bool bspInit(void)
{
  bool ret = true;

  return ret;
}

void delay(uint32_t ms)
{
  if (ms > 0)
  {
    k_msleep(ms);
  }
}

uint32_t millis(void)
{
  return k_uptime_get_32();
}


uint32_t micros(void)
{
	k_ticks_t ticks = k_uptime_ticks();

	/* tick → us 변환 */
	uint64_t us = k_ticks_to_us_near64(ticks);
	return (uint32_t)us;
}
