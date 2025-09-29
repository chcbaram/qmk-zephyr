#ifndef BSP_H_
#define BSP_H_

#ifdef __cplusplus
extern "C" {
#endif

#include "def.h"

#include <hal/nrf_gpio.h>
#include <zephyr/kernel.h>


#define assert              assert_param
#define assert_param(expr)  ((void)0U)


void logPrintf(const char *fmt, ...);
// #define logPrintf printf


bool bspInit(void);

void delay(uint32_t time_ms);
uint32_t millis(void);
uint32_t micros(void);


#ifdef __cplusplus
}
#endif

#endif