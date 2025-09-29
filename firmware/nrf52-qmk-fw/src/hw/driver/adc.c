#include "adc.h"



#ifdef _USE_HW_ADC
#include <zephyr/drivers/adc.h>

#include "cli.h"
#include "cli_gui.h"


#define NAME_DEF(x)  x, #x

#ifdef _USE_HW_RTOS
#define lock()      xSemaphoreTake(mutex_lock, portMAX_DELAY);
#define unLock()    xSemaphoreGive(mutex_lock);
#else
#define lock()      
#define unLock()    
#endif


typedef struct
{
  struct adc_dt_spec  h_dt;
  AdcPinName_t        pin_name;
  const char         *p_name;
} adc_tbl_t;


#if CLI_USE(HW_ADC)
static void cliAdc(cli_args_t *args);
#endif



#ifdef _USE_HW_RTOS
static SemaphoreHandle_t mutex_lock;
#endif
static bool is_init = false;


static int16_t adc_data_buf[ADC_MAX_CH];

static const adc_tbl_t adc_tbl[ADC_MAX_CH] =
{
  {ADC_DT_SPEC_GET_BY_NAME(DT_PATH(zephyr_user), a0), NAME_DEF(ADC_CH1)},
  {ADC_DT_SPEC_GET_BY_NAME(DT_PATH(zephyr_user), a1), NAME_DEF(ADC_CH2)},
};


bool adcInit(void)
{
  bool ret = true;  
  

#ifdef _USE_HW_RTOS
  mutex_lock = xSemaphoreCreateMutex();
#endif


  for (int i=0; i<ADC_MAX_CH; i++)
  {
    if (!adc_is_ready_dt(&adc_tbl[i].h_dt))
    {
      logPrintf("[E_] adc : ADC controller devivce %s not ready\n", adc_tbl[i].h_dt.dev->name);
      ret = false;
      break;
    }

    int err;
    err = adc_channel_setup_dt(&adc_tbl[i].h_dt);
    if (err < 0)
    {
      logPrintf("[E_] adc : Could not setup channel #%d (%d)\n", i, err);
      ret = false;
      break;
    }
  }


  is_init = ret;

  logPrintf("[%s] adcInit()\n", is_init ? "OK":"E_");

#if CLI_USE(HW_ADC)
  cliAdd("adc", cliAdc);
#endif
  return ret;
}

bool adcIsInit(void)
{
  return is_init;
}

int32_t adcRead(uint8_t ch)
{
  int err;
  uint16_t buf;
  struct adc_sequence sequence =
  {
    .buffer = &buf,
    /* buffer size in bytes, not number of samples */
    .buffer_size = sizeof(buf),
    // Optional
    //.calibrate = true,
  };

  (void)adc_sequence_init_dt(&adc_tbl[ch].h_dt, &sequence);
      
  err = adc_read(adc_tbl[ch].h_dt.dev, &sequence);
  if (err >= 0) 
  {
    adc_data_buf[ch] = buf;
	}  
  else
  {
    adc_data_buf[ch] = 0;
  }

  return adc_data_buf[ch];
}

int32_t adcRead8(uint8_t ch)
{
  return adcRead(ch)>>4;
}

int32_t adcRead10(uint8_t ch)
{
  return adcRead(ch)>>2;
}

int32_t adcRead12(uint8_t ch)
{
  return adcRead(ch)>>0;
}

int32_t adcRead16(uint8_t ch)
{
  return adcRead(ch)<<4;  
}

uint8_t adcGetRes(uint8_t ch)
{
  return 12;
}

float adcReadVoltage(uint8_t ch)
{
  return adcConvVoltage(ch, adcRead(ch));
}

float adcConvVoltage(uint8_t ch, uint32_t adc_value)
{
  float ret = 0;


  switch (ch)
  {
    case ADC_CH1:
    case ADC_CH2:
      ret  = ((float)adc_value * (0.9f/(1.0f/4.0f))) / (4096.f); 
      break;

    default :
      ret  = ((float)adc_value * 3.3f) / (4096.f);
      break;
  }

  return ret;
}


#if CLI_USE(HW_ADC)
void cliAdc(cli_args_t *args)
{
  bool ret = false;


  if (args->argc == 1 && args->isStr(0, "info") == true)
  {
    cliPrintf("adc init : %d\n", is_init);
    cliPrintf("adc res  : %d\n", adcGetRes(0));
    for (int i=0; i<ADC_MAX_CH; i++)
    {
      cliPrintf("%02d. %-32s : %04d\n", i, adc_tbl[i].p_name, adcRead(i));
    }

    ret = true;
  }

  if (args->argc == 1 && args->isStr(0, "show") == true)
  {
    cliShowCursor(false);
    while(cliKeepLoop())
    {
      for (int i=0; i<ADC_MAX_CH; i++)
      {
        cliPrintf("%02d. %-32s : %04d \n", i, adc_tbl[i].p_name, adcRead(i));
      }
      delay(50);
      cliPrintf("\x1B[%dA", ADC_MAX_CH);
    }
    cliPrintf("\x1B[%dB", ADC_MAX_CH);
    cliShowCursor(true);
    ret = true;
  }

  if (args->argc == 2 && args->isStr(0, "show") && args->isStr(1, "vol"))
  {
    cliShowCursor(false);
    while(cliKeepLoop())
    {
      for (int i=0; i<ADC_MAX_CH; i++)
      {
        float adc_data;

        adc_data = adcReadVoltage(i);

        cliPrintf("%02d. %-32s : %d.%02dV \n",i, adc_tbl[i].p_name, (int)adc_data, (int)(adc_data * 100)%100);
      }
      delay(50);
      cliPrintf("\x1B[%dA", ADC_MAX_CH);
    }
    cliPrintf("\x1B[%dB", ADC_MAX_CH);
    cliShowCursor(true);
    ret = true;
  }

  if (ret != true)
  {
    cliPrintf("adc info\n");
    cliPrintf("adc show\n");
    cliPrintf("adc show vol\n");
  }
}
#endif

#endif