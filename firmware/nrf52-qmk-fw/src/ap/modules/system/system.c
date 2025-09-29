#include "system.h"



#define lock(x)      k_mutex_lock(&x, K_FOREVER);
#define unLock(x)    k_mutex_unlock(&x);


static K_MUTEX_DEFINE(mutex_ready);



bool systemInit(void)
{  
  return true;
}

bool systemIsReady(void)
{
  lock(mutex_ready);
  unLock(mutex_ready);
  return true;
}

void systemMain(void)
{
  bool init_ret = true;

  lock(mutex_ready);

  init_ret &= moduleInit();

  logBoot(false);
  logPrintf("[%s] Thread Started : System\n", init_ret ? "OK":"E_" );
  unLock(mutex_ready);

  while(1)
  {
    ledToggle(_DEF_LED1);

    if (init_ret)
      delay(500);
    else
      delay(50);
  }
}