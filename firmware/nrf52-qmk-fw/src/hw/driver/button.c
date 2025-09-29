#include "button.h"


#ifdef _USE_HW_BUTTON
#include "cli.h"
#include <zephyr/drivers/gpio.h>



typedef struct
{
  struct gpio_dt_spec h_dt;
  uint8_t             on_state;
} button_pin_t;


#if CLI_USE(HW_BUTTON)
static void cliButton(cli_args_t *args);
#endif
static bool buttonGetPin(uint8_t ch);

static const button_pin_t button_pin[BUTTON_MAX_CH] =
{
  {GPIO_DT_SPEC_GET(DT_NODELABEL(btn0), gpios), _DEF_LOW},
  {GPIO_DT_SPEC_GET(DT_NODELABEL(btn1), gpios), _DEF_LOW},
  {GPIO_DT_SPEC_GET(DT_NODELABEL(btn2), gpios), _DEF_LOW},
  {GPIO_DT_SPEC_GET(DT_NODELABEL(btn3), gpios), _DEF_LOW},
};





bool buttonInit(void)
{
  bool ret = true;



  for (int i = 0; i < BUTTON_MAX_CH; i++)
  {
    if (gpio_pin_configure_dt(&button_pin[i].h_dt, GPIO_INPUT) < 0)
    {
      ret = false;
    }
  }

#if CLI_USE(HW_BUTTON)
  cliAdd("button", cliButton);
#endif

  return ret;
}

bool buttonGetPin(uint8_t ch)
{
  bool ret = false;

  if (ch >= BUTTON_MAX_CH)
  {
    return false;
  }

  if (gpio_pin_get_dt(&button_pin[ch].h_dt) == button_pin[ch].on_state)
  {
    ret = true;
  }

  return ret;
}


bool buttonGetPressed(uint8_t ch)
{
  if (ch >= BUTTON_MAX_CH)
  {
    return false;
  }

  return buttonGetPin(ch);
}

uint32_t buttonGetData(void)
{
  uint32_t ret = 0;


  for (int i=0; i<BUTTON_MAX_CH; i++)
  {
    ret |= (buttonGetPressed(i)<<i);
  }

  return ret;
}

uint8_t  buttonGetPressedCount(void)
{
  uint32_t i;
  uint8_t ret = 0;

  for (i=0; i<BUTTON_MAX_CH; i++)
  {
    if (buttonGetPressed(i) == true)
    {
      ret++;
    }
  }

  return ret;
}


#if CLI_USE(HW_BUTTON)
void cliButton(cli_args_t *args)
{
  bool ret = false;


  if (args->argc == 1 && args->isStr(0, "info"))
  {
    ret = true;
  }

  if (args->argc == 1 && args->isStr(0, "show"))
  {    
    while(cliKeepLoop())
    {
      for (int i=0; i<BUTTON_MAX_CH; i++)
      {
        cliPrintf("%d", buttonGetPressed(i));
      }
      delay(50);
      cliPrintf("\r");
    }
    ret = true;
  }

  if (ret == false)
  {
    cliPrintf("button info\n");
    cliPrintf("button show\n");
  }
}
#endif



#endif