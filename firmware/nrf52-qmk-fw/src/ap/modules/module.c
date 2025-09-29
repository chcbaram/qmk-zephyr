#include "module.h"




typedef struct
{
  int32_t count;
  module_t *p_module;
} module_info_t;


static bool moduleBegin(void);
static void cliModule(cli_args_t *args);

static module_info_t info;
extern uint32_t _smodule;
extern uint32_t _emodule;





bool moduleInit(void)
{
  bool ret;

  info.count = ((int)&_emodule - (int)&_smodule)/sizeof(module_t);
  info.p_module = (module_t *)&_smodule;

  logPrintf("[  ] moduleInit()\n");
  logPrintf("       count : %d\n", info.count);

  ret = moduleBegin();

  cliAdd("module", cliModule);
  return ret;
}

bool moduleBegin(void)
{
  bool ret = true;

  logPrintf("[  ] moduleBegin()\n");

  for (int pri = MODULE_PRI_HIGH; pri <= MODULE_PRI_LOW; pri++)
  {
    for (int i=0; i<info.count; i++)
    {
      assert(info.p_module[i].priority >= MODULE_PRI_HIGH && info.p_module[i].priority <= MODULE_PRI_LOW);

      if (info.p_module[i].priority == pri && info.p_module[i].init != NULL)
      {
        bool mod_ret;

        mod_ret = info.p_module[i].init();
        ret &= mod_ret;
        logPrintf("       %s %s\n", info.p_module[i].name, mod_ret ? "OK":"Fail");
      }
      else
      {
        if (info.p_module[i].priority < MODULE_PRI_HIGH || info.p_module[i].priority > MODULE_PRI_LOW)
        {
          logPrintf("       %s Priority %d Fail\n", info.p_module[i].name, info.p_module[i].priority);
          ret = false;
        }
      }
    }
  }

  return ret;
}

void cliModule(cli_args_t *args)
{
  bool ret = false;


  if (args->argc == 1 && args->isStr(0, "info"))
  {
    module_t *p_module;

    cliPrintf("count     : %d\n", info.count);
    p_module = info.p_module ;
    for (int i=0; i<info.count; i++)
    {
      cliPrintf("%d : %-16s\n",
                i,
                p_module[i].name
                );
    }
    ret = true;
  }

  if (ret == false)
  {
    cliPrintf("module info\n");

  }
}
