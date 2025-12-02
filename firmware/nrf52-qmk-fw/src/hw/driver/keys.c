#include "keys.h"


#ifdef _USE_HW_KEYS
#include "button.h"
#include "cli.h"
#include <zephyr/drivers/gpio.h>


#define MATRIX_ROWS   5
#define MATRIX_COLS   15



typedef struct
{
  struct gpio_dt_spec h_dt;
  uint8_t             on_state;
  uint8_t             init_state;
} keys_pin_t;

static const keys_pin_t keys_out_pin[MATRIX_ROWS] =
{
  {GPIO_DT_SPEC_GET(DT_NODELABEL(row0), gpios), _DEF_HIGH, _DEF_LOW},
  {GPIO_DT_SPEC_GET(DT_NODELABEL(row1), gpios), _DEF_HIGH, _DEF_LOW},
  {GPIO_DT_SPEC_GET(DT_NODELABEL(row2), gpios), _DEF_HIGH, _DEF_LOW},
  {GPIO_DT_SPEC_GET(DT_NODELABEL(row3), gpios), _DEF_HIGH, _DEF_LOW},
  {GPIO_DT_SPEC_GET(DT_NODELABEL(row4), gpios), _DEF_HIGH, _DEF_LOW},
};

static const keys_pin_t keys_in_pin[MATRIX_COLS] =
{
  {GPIO_DT_SPEC_GET(DT_NODELABEL(col0),  gpios), _DEF_HIGH, _DEF_LOW},
  {GPIO_DT_SPEC_GET(DT_NODELABEL(col1),  gpios), _DEF_HIGH, _DEF_LOW},
  {GPIO_DT_SPEC_GET(DT_NODELABEL(col2),  gpios), _DEF_HIGH, _DEF_LOW},
  {GPIO_DT_SPEC_GET(DT_NODELABEL(col3),  gpios), _DEF_HIGH, _DEF_LOW},
  {GPIO_DT_SPEC_GET(DT_NODELABEL(col4),  gpios), _DEF_HIGH, _DEF_LOW},
  {GPIO_DT_SPEC_GET(DT_NODELABEL(col5),  gpios), _DEF_HIGH, _DEF_LOW},
  {GPIO_DT_SPEC_GET(DT_NODELABEL(col6),  gpios), _DEF_HIGH, _DEF_LOW},
  {GPIO_DT_SPEC_GET(DT_NODELABEL(col7),  gpios), _DEF_HIGH, _DEF_LOW},
  {GPIO_DT_SPEC_GET(DT_NODELABEL(col8),  gpios), _DEF_HIGH, _DEF_LOW},
  {GPIO_DT_SPEC_GET(DT_NODELABEL(col9),  gpios), _DEF_HIGH, _DEF_LOW},
  {GPIO_DT_SPEC_GET(DT_NODELABEL(col10), gpios), _DEF_HIGH, _DEF_LOW},
  {GPIO_DT_SPEC_GET(DT_NODELABEL(col11), gpios), _DEF_HIGH, _DEF_LOW},
  {GPIO_DT_SPEC_GET(DT_NODELABEL(col12), gpios), _DEF_HIGH, _DEF_LOW},
  {GPIO_DT_SPEC_GET(DT_NODELABEL(col13), gpios), _DEF_HIGH, _DEF_LOW},
  {GPIO_DT_SPEC_GET(DT_NODELABEL(col14), gpios), _DEF_HIGH, _DEF_LOW},
};



#if CLI_USE(HW_KEYS)
static void cliCmd(cli_args_t *args);
#endif
static bool keysInitGpio(void);


static uint32_t col_rd_buf[MATRIX_ROWS] = {0,};



bool keysInit(void)
{
  keysInitGpio();

#if CLI_USE(HW_KEYS)
  cliAdd("keys", cliCmd);
#endif

  return true;
}

bool keysInitGpio(void)
{
  bool ret = true;

  // ROWS
  //
  for (int i = 0; i < MATRIX_ROWS; i++)
  {
    if (gpio_pin_configure_dt(&keys_out_pin[i].h_dt, GPIO_OUTPUT_LOW) < 0)
    {
      ret = false;      
    }
  }

  // COLS
  //
  for (int i = 0; i < MATRIX_COLS; i++)
  {
    if (gpio_pin_configure_dt(&keys_in_pin[i].h_dt, GPIO_INPUT | GPIO_PULL_DOWN) < 0)
    {
      ret = false;      
    }
  }

  return ret;
}

bool keysIsBusy(void)
{
  return false;
}

void keysDelay(void)
{
  volatile uint32_t cnt;

  for (cnt=0; cnt<500; cnt++)
  {
    //
  }
}

void keysSelRow(uint16_t row)
{
  for (int i=0; i<MATRIX_ROWS; i++)
  {
    if (i == row)
      gpio_pin_set_dt(&keys_out_pin[i].h_dt, keys_out_pin[i].on_state);
    else
      gpio_pin_set_dt(&keys_out_pin[i].h_dt, !keys_out_pin[i].on_state);
  }
}

bool keysUpdate(void)
{
  
  for (int i=0; i<MATRIX_ROWS; i++)
  {    
    gpio_pin_set_dt(&keys_out_pin[i].h_dt, keys_out_pin[i].on_state);
    
    uint32_t data = 0;

    for (int j=0; j<MATRIX_COLS; j++)
    {
      if (gpio_pin_get_dt(&keys_in_pin[j].h_dt) == keys_in_pin[j].on_state)
      {
        data |= (1<<j);
      }      
    }
    col_rd_buf[i] = data;
    k_yield();

    gpio_pin_set_dt(&keys_out_pin[i].h_dt, !keys_out_pin[i].on_state);
  }

  return true;
}

bool keysReadBuf(uint8_t *p_data, uint32_t length)
{
  return true;
}

bool keysReadColsBuf(uint32_t *p_data, uint32_t rows_cnt)
{
  memcpy(p_data, col_rd_buf, rows_cnt * sizeof(uint32_t));
  return true;
}

bool keysGetPressed(uint16_t row, uint16_t col)
{
  bool     ret = false;
  uint32_t col_bit;  

  col_bit = col_rd_buf[row];

  if (col_bit & (1<<col))
  {
    ret = true;
  }

  return ret;
}

#if CLI_USE(HW_KEYS)
void cliCmd(cli_args_t *args)
{
  bool ret = false;



  if (args->argc == 1 && args->isStr(0, "info"))
  {
    uint32_t pre_time;
    uint32_t exe_time;

    cliShowCursor(false);

    pre_time = micros();
    keysUpdate();
    exe_time = micros() - pre_time;

    cliPrintf("scan time : %d us\n", exe_time);
    
    while(cliKeepLoop())
    {
      keysUpdate();
      delay(10);

      cliPrintf("     ");
      for (int cols=0; cols<MATRIX_COLS; cols++)
      {
        cliPrintf("%02d ", cols);
      }
      cliPrintf("\n");

      for (int rows=0; rows<MATRIX_ROWS; rows++)
      {
        cliPrintf("%02d : ", rows);

        for (int cols=0; cols<MATRIX_COLS; cols++)
        {
          if (keysGetPressed(rows, cols))
            cliPrintf("O  ");
          else
            cliPrintf("_  ");
        }
        cliPrintf("\n");
      }
      cliMoveUp(MATRIX_ROWS+1);
    }
    cliMoveDown(MATRIX_ROWS+1);

    cliShowCursor(true);
    ret = true;
  }

  if (ret == false)
  {
    cliPrintf("keys info\n");
  }
}
#endif

#endif