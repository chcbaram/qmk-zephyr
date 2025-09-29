#include "eeprom.h"


#ifdef _USE_HW_EEPROM
#include "cli.h"
#include <zephyr/drivers/eeprom.h>


#if CLI_USE(HW_EEPROM)
void cliEeprom(cli_args_t *args);
#endif


#define EEPROM_MAX_SIZE   HW_EEPROM_MAX_SIZE


static bool is_init = false;
static uint8_t i2c_addr = 0x50;
static const struct device *const eeprom_dev = DEVICE_DT_GET(DT_NODELABEL(eeprom0));



bool eepromInit()
{
  bool ret;


  ret = eepromValid(0x00);

  logPrintf("[%s] eepromInit()\n", ret ? "OK":"E_");
  if (ret == true)
  {
    logPrintf("     found : 0x%02X\n", i2c_addr);
    logPrintf("     size  : %dKB\n", eepromGetLength()/1024);
  }
  else
  {
    logPrintf("     empty\n");
  }

#if CLI_USE(HW_EEPROM)
  cliAdd("eeprom", cliEeprom);
#endif

  is_init = ret;

  return ret;
}

bool eepromIsInit(void)
{
  return is_init;
}

bool eepromValid(uint32_t addr)
{
  bool ret = true;

  if (addr >= EEPROM_MAX_SIZE)
  {
    return false;
  }

  if (!device_is_ready(eeprom_dev))
  {
    logPrintf("[E_] eeprom devivce %s not ready\n", eeprom_dev->name);
    ret = false;    
  }

  return ret;
}

bool eepromReadByte(uint32_t addr, uint8_t *p_data)
{
  bool ret;

  if (!is_init)
    return false;

  if (addr >= EEPROM_MAX_SIZE)
    return false;

  int eep_ret;

  eep_ret = eeprom_read(eeprom_dev, addr, p_data, 1);

  ret = (eep_ret == 0) ? true:false;

  return ret;
}

bool eepromWriteByte(uint32_t addr, uint8_t data_in)
{
  bool ret;

  if (!is_init)
    return false;

  if (addr >= EEPROM_MAX_SIZE)
    return false;

  int eep_ret;

  eep_ret = eeprom_write(eeprom_dev, addr, &data_in, 1);

  ret = (eep_ret == 0) ? true:false;

  return ret;
}

bool eepromRead(uint32_t addr, uint8_t *p_data, uint32_t length)
{
  bool ret = true;
  uint32_t i;


  for (i=0; i<length; i++)
  {
    ret = eepromReadByte(addr, &p_data[i]);
    if (ret != true)
    {
      break;
    }
  }

  return ret;
}

bool eepromWrite(uint32_t addr, uint8_t *p_data, uint32_t length)
{
  bool ret = false;
  uint32_t i;


  for (i=0; i<length; i++)
  {
    ret = eepromWriteByte(addr, p_data[i]);
    if (ret == false)
    {
      break;
    }
  }

  return ret;
}

uint32_t eepromGetLength(void)
{
  uint32_t ret = 0;

  ret = eeprom_get_size(eeprom_dev);

  return ret;
}

bool eepromFormat(void)
{
  return true;
}




#if CLI_USE(HW_EEPROM)
void cliEeprom(cli_args_t *args)
{
  bool ret = true;
  uint32_t i;
  uint32_t addr;
  uint32_t length;
  uint8_t  data;
  uint32_t pre_time;
  bool eep_ret;


  if (args->argc == 1)
  {
    if(args->isStr(0, "info") == true)
    {
      cliPrintf("eeprom init   : %s\n", eepromIsInit() ? "True":"False");
      cliPrintf("eeprom length : %d bytes\n", eepromGetLength());
    }
    else if(args->isStr(0, "format") == true)
    {
      if (eepromFormat() == true)
      {
        cliPrintf("format OK\n");
      }
      else
      {
        cliPrintf("format Fail\n");
      }
    }
    else
    {
      ret = false;
    }
  }
  else if (args->argc == 3)
  {
    if(args->isStr(0, "read") == true)
    {
      addr   = (uint32_t)args->getData(1);
      length = (uint32_t)args->getData(2);

      if (length > eepromGetLength())
      {
        cliPrintf( "length error\n");
      }
      for (i=0; i<length; i++)
      {
        if (eepromReadByte(addr+i, &data) == true)
        {
          cliPrintf( "addr : %d\t 0x%02X\n", addr+i, data);          
        }
        else
        {
          cliPrintf("eepromReadByte() Error\n");
          break;
        }
      }
    }
    else if(args->isStr(0, "write") == true)
    {
      addr = (uint32_t)args->getData(1);
      data = (uint8_t )args->getData(2);

      pre_time = millis();
      eep_ret = eepromWriteByte(addr, data);

      cliPrintf( "addr : %d\t 0x%02X %dms\n", addr, data, millis()-pre_time);
      if (eep_ret)
      {
        cliPrintf("OK\n");
      }
      else
      {
        cliPrintf("FAIL\n");
      }
    }
    else
    {
      ret = false;
    }
  }
  else
  {
    ret = false;
  }


  if (ret == false)
  {
    cliPrintf( "eeprom info\n");
    cliPrintf( "eeprom format\n");
    cliPrintf( "eeprom read  [addr] [length]\n");
    cliPrintf( "eeprom write [addr] [data]\n");
  }

}
#endif 


#endif 