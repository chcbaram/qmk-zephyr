#include "activity.h"
#include "qmk/qmk.h"
#include "eeprom.h"   // eeprom_update() — sleep 전 dirty flush
#include "hw_def.h"
#include "log.h"
#include "cli.h"
#include "led.h"
#include "usb.h"

#include <zephyr/kernel.h>
#include <zephyr/sys/poweroff.h>

/*
 * 타임아웃 기본값 = ZMK wish60/wish65 보드 defconfig 실제 값.
 *   CONFIG_ZMK_IDLE_TIMEOUT=30000          (30초)
 *   CONFIG_ZMK_IDLE_SLEEP_TIMEOUT=3600000  (1시간)
 * ZMK 의 Kconfig 기본값이 아니라 **보드가 지정한 값**을 따랐다.
 */
#define ACTIVITY_IDLE_TIMEOUT_MS_DEF    (30 * 1000)
#define ACTIVITY_SLEEP_TIMEOUT_MS_DEF   (60 * 60 * 1000)

#if CLI_USE(HW_ACTIVITY)
static void cliActivity(cli_args_t *args);
#endif

static activity_state_t state           = ACTIVITY_ACTIVE;
static uint32_t         idle_timeout_ms  = ACTIVITY_IDLE_TIMEOUT_MS_DEF;
static uint32_t         sleep_timeout_ms = ACTIVITY_SLEEP_TIMEOUT_MS_DEF;

/*
 * deep sleep = nRF52 System OFF.
 *
 * 중요한 성질 두 가지:
 *  1) **깨어남 = 리셋**. System OFF 는 RAM 리텐션도 끄므로(soc/nordic/common/poweroff.c)
 *     상태가 남지 않고 전체 재부팅한다. BLE 재연결에 수 초가 걸린다 → 타임아웃이 1시간인 이유.
 *  2) **웨이크업은 DETECT(PIN_CNF.SENSE)로만** 일어난다. GPIOTE 는 System OFF 에서 꺼진다.
 *     그래서 board DTS 의 `sense-edge-mask` 가 필수다 — 없으면 영영 못 깬다.
 *
 * 진입 전제(둘 다 현재 만족):
 *  - 눌린 키가 없어야 한다. SENSE 가 이미 active 인 채로 System OFF 에 들어가면 즉시
 *    깨어나 리셋 루프가 된다. activityUpdate() 는 메인 루프가 idle(키 0개)일 때만 불린다.
 *  - kbd-matrix 드라이버가 detect 모드(전 컬럼 구동 + row SENSE)여야 한다.
 *    poll_timeout 후 자동 진입하고, sleep 타임아웃은 그보다 훨씬 길다.
 *
 * sys_poweroff() 는 irq_lock() + nrf_power_system_off() 뿐이라 디바이스를 건드리지 않는다
 * (CONFIG_PM_DEVICE 불필요). 끄고 갈 것은 우리가 직접 정리한다.
 */
static void activityEnterSleep(void)
{
  logPrintf("[  ] sleep: System OFF (키 입력 시 리셋 부팅)\n");

  // EEPROM dirty 를 먼저 반영한다. System OFF 는 RAM 을 날리므로 미반영분은 그대로 유실된다.
  eeprom_update();

  ledToSleep();

  // TODO(Phase 8): ext-power off — 네오픽셀 전원 레일. 지금은 소비자가 없어 레일이 꺼져 있다.

  k_msleep(10);   // 로그 flush 여유(콘솔 빌드)

  sys_poweroff();   // 돌아오지 않는다
}

bool activityInit(void)
{
#if CLI_USE(HW_ACTIVITY)
  cliAdd("activity", cliActivity);
#endif
  state = ACTIVITY_ACTIVE;
  return true;
}

void activityUpdate(void)
{
  uint32_t inactive = qmkGetInactiveMs();

  // USB 가 있으면 자지 않는다(ZMK 와 동일 기준). 전원이 있는데 재부팅 비용을 낼 이유가 없고,
  // 개발 중 콘솔이 끊기는 것도 막는다.
  if (sleep_timeout_ms > 0 && inactive >= sleep_timeout_ms && !usbIsVbusPresent())
  {
    state = ACTIVITY_SLEEP;
    activityEnterSleep();   // 돌아오지 않음
  }

  if (activityIsIdle())
  {
    if (state != ACTIVITY_IDLE)
    {
      state = ACTIVITY_IDLE;
      logPrintf("[  ] activity: IDLE\n");
    }
  }
  else
  {
    state = ACTIVITY_ACTIVE;
  }

  /*
   * IDLE 진입을 RGB/인디케이터에 **지금** 반영해야 한다.
   * 이 함수는 ap.c 의 idle 분기에서 qmkWaitActivity() **직전에** 불린다. 여기서 안 걸면
   * 다음 qmkUpdate() 까지 미뤄지는데, RGB 가 꺼져 있으면 그 "다음"이 sleep 데드라인(1시간)
   * 뒤라 인디케이터가 한 시간 켜져 있게 된다. (LED 반영이 30초 늦던 버그와 같은 계열)
   */
  qmkSuspendUpdate();
}

bool activityIsIdle(void)
{
  if (idle_timeout_ms == 0)
  {
    return false;
  }
  return qmkGetInactiveMs() >= idle_timeout_ms;
}

uint32_t activityGetWaitMs(void)
{
  uint32_t inactive = qmkGetInactiveMs();

  if (idle_timeout_ms > 0 && inactive < idle_timeout_ms)
  {
    return idle_timeout_ms - inactive;
  }
  if (sleep_timeout_ms == 0)
  {
    return 0;   // sleep 비활성 → 더 볼 데드라인 없음 → 무한 대기
  }
  if (inactive < sleep_timeout_ms)
  {
    return sleep_timeout_ms - inactive;
  }
  // sleep 시점을 지났는데 아직 여기 있다 = USB 때문에 못 잤다.
  // USB 를 뺐는지 주기적으로 재확인한다(1시간에 1회라 전력엔 무의미).
  return sleep_timeout_ms;
}

activity_state_t activityGetState(void)
{
  return state;
}

void activitySetIdleTimeout(uint32_t ms)
{
  idle_timeout_ms = ms;
}

void activitySetSleepTimeout(uint32_t ms)
{
  sleep_timeout_ms = ms;
}

uint32_t activityGetIdleTimeout(void)
{
  return idle_timeout_ms;
}

uint32_t activityGetSleepTimeout(void)
{
  return sleep_timeout_ms;
}


#if CLI_USE(HW_ACTIVITY)
void cliActivity(cli_args_t *args)
{
  bool ret = false;

  if (args->argc == 1 && args->isStr(0, "info"))
  {
    const char *name[] = {"ACTIVE", "IDLE", "SLEEP"};

    cliPrintf("state         : %s\n", name[state]);
    cliPrintf("inactive      : %d ms\n", qmkGetInactiveMs());
    cliPrintf("idle timeout  : %d ms\n", idle_timeout_ms);
    cliPrintf("sleep timeout : %d ms%s\n", sleep_timeout_ms,
              sleep_timeout_ms == 0 ? " (비활성)" : "");
    cliPrintf("usb vbus      : %s%s\n", usbIsVbusPresent() ? "present" : "absent",
              usbIsVbusPresent() ? " -> sleep 안 함" : "");
    ret = true;
  }

  if (args->argc == 2 && args->isStr(0, "idle"))
  {
    activitySetIdleTimeout(args->getData(1));
    cliPrintf("idle timeout : %d ms\n", idle_timeout_ms);
    ret = true;
  }

  if (args->argc == 2 && args->isStr(0, "sleep"))
  {
    activitySetSleepTimeout(args->getData(1));
    cliPrintf("sleep timeout : %d ms\n", sleep_timeout_ms);
    ret = true;
  }

  // 웨이크업 경로 시험용 — 1시간 기다리지 않고 지금 System OFF 로 들어간다.
  // USB 가 꽂혀 있어도 강제로 잔다(activityUpdate 의 USB 조건을 우회).
  if (args->argc == 1 && args->isStr(0, "off"))
  {
    cliPrintf("System OFF 진입. 키를 누르면 리셋 부팅한다.\n");
    cliPrintf("(안 깨어나면 sense-edge-mask 문제 — 리셋 버튼으로 복구)\n");
    delay(100);
    activityEnterSleep();
    ret = true;
  }

  if (ret == false)
  {
    cliPrintf("activity info\n");
    cliPrintf("activity idle  [ms]\n");
    cliPrintf("activity sleep [ms]\n");
    cliPrintf("activity off        : 지금 즉시 System OFF (웨이크업 시험용)\n");
  }
}
#endif
