#include "led.h"


#ifdef _USE_HW_LED
#include "cli.h"
#include <zephyr/drivers/gpio.h>


enum
{
  HW_TYPE_NRF,
  HW_TYPE_DT
};

typedef struct
{
  uint8_t   type;
  struct gpio_dt_spec h_dt;  
  uint32_t  pin;  
  uint8_t   on_state;
  uint8_t   off_state;
} led_tbl_t;

const led_tbl_t led_tbl[LED_MAX_CH] = {
  {HW_TYPE_DT, GPIO_DT_SPEC_GET(DT_NODELABEL(led), gpios), 0, _DEF_HIGH, _DEF_LOW}, // LED
};


#ifdef _USE_HW_CLI
static void cliLed(cli_args_t *args);
#endif


bool ledInit(void)
{
  bool ret = true;


  for (int i=0; i<LED_MAX_CH; i++)
  {
    if (led_tbl[i].type == HW_TYPE_NRF)
    {
      nrf_gpio_cfg_output(led_tbl[i].pin);
    }
    else
    {      
      if (gpio_pin_configure_dt(&led_tbl[i].h_dt, GPIO_OUTPUT) < 0)
      {
        ret = false;
      }
    }

    ledOff(i);
  }

#ifdef _USE_HW_CLI
  cliAdd("led", cliLed);
#endif

  return ret;
}

bool ledToSleep(void)
{
  for (int i=0; i<LED_MAX_CH; i++)
  {
    ledOff(i);
    nrf_gpio_cfg_default(led_tbl[i].pin);
  }

  return true;
}

void ledOn(uint8_t ch)
{
  if (ch >= LED_MAX_CH) return;

  if (led_tbl[ch].type == HW_TYPE_NRF)
    nrf_gpio_pin_write(led_tbl[ch].pin, led_tbl[ch].on_state);
  else
    gpio_pin_set_dt(&led_tbl[ch].h_dt, led_tbl[ch].on_state);
}

void ledOff(uint8_t ch)
{
  if (ch >= LED_MAX_CH) return;

  if (led_tbl[ch].type == HW_TYPE_NRF)
    nrf_gpio_pin_write(led_tbl[ch].pin, led_tbl[ch].off_state);
  else
    gpio_pin_set_dt(&led_tbl[ch].h_dt, led_tbl[ch].off_state);
}

void ledToggle(uint8_t ch)
{
  if (ch >= LED_MAX_CH) return;

  if (led_tbl[ch].type == HW_TYPE_NRF)
    nrf_gpio_pin_toggle(led_tbl[ch].pin);
  else
    gpio_pin_toggle_dt(&led_tbl[ch].h_dt);
}





#ifdef _USE_HW_CLI

void cliLed(cli_args_t *args)
{
  bool ret = false;


  if (args->argc == 3 && args->isStr(0, "toggle") == true)
  {
    uint8_t  led_ch;
    uint32_t toggle_time;
    uint32_t pre_time;

    led_ch      = (uint8_t)args->getData(1);
    toggle_time = (uint32_t)args->getData(2);

    if (led_ch > 0)
    {
      led_ch--;
    }

    pre_time = millis();
    while(cliKeepLoop())
    {
      if (millis()-pre_time >= toggle_time)
      {
        pre_time = millis();
        ledToggle(led_ch);
      }
    }

    ret = true;
  }


  if (ret != true)
  {
    cliPrintf("led toggle ch[1~%d] time_ms\n", LED_MAX_CH);
  }
}


#endif


#endif