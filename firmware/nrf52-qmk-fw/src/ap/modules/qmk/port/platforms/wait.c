#include "wait.h"
#include <zephyr/kernel.h>

void wait_ms(uint32_t ms)
{
  k_msleep(ms);
}
