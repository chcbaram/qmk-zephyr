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
 * sleep 타임아웃 = ZMK wish60/wish65 보드 defconfig 실제 값(CONFIG_ZMK_IDLE_SLEEP_TIMEOUT=1시간).
 *
 * idle 은 ZMK 를 따르지 않는다. 처음엔 ZMK 의 CONFIG_ZMK_IDLE_TIMEOUT=30초를 그대로 가져왔는데
 * **용도가 다른 값이었다** — ZMK 에서 그 30초는 RGB 와 무관하고(ZMK 의 RGB 소등은 별개 옵션인
 * ZMK_RGB_UNDERGLOW_AUTO_OFF_IDLE 이며, wish60/wish65 defconfig 는 **그걸 켜지 않는다**),
 * 우리 IDLE 은 지금 **RGB/인디케이터 소등이 유일한 용도**다(qmkSuspendUpdate 가 유일한 소비자).
 * 즉 이 값은 사실상 "RGB 타임아웃" 이다.
 *
 * **30초로 둔다(사용자 결정).** RGB 는 65mA 라 idle 60µA 의 1000배다 — 자리를 뜨면 빨리 끄는
 * 편이 낫다는 판단이다. 2분으로 올려봤으나(길어도 거의 공짜다: 65mA × 4분 = 4.3mAh =
 * 1000mAh 의 0.4%) 되돌렸다.
 *
 * 짧다고 느끼면 **VIA(FEATURE > POWER > Idle Timeout)에서 올리면 된다** — Off/5s/10s/15s/30s/
 * 1m/2m/4m. **Off 를 고르면 ZMK 와 같은 동작**(deep sleep 전까지 RGB 유지)이 된다.
 *
 * [주의] 여기를 바꿔도 **EEPROM 에 저장된 값이 있으면 그게 이긴다**(power_cfg_init). 기본값은
 * EEPROM 이 비었을 때만 쓰인다 — 이미 쓰던 보드는 VIA 에서 직접 바꿔야 한다.
 */
#define ACTIVITY_IDLE_TIMEOUT_MS_DEF    (30 * 1000)
#define ACTIVITY_SLEEP_TIMEOUT_MS_DEF   (60 * 60 * 1000)

/*
 * RGB/인디케이터 소등은 **별도 타임아웃**이다(activity.h 주석 참고).
 * 5분: 30초는 뭔가 읽는 동안 꺼져 거슬리고, 길게 잡아도 비용은 작다 —
 * RGB 65mA × 5분 = 5.4mAh = 1000mAh 의 0.5%(§6.10). 진짜 절감은 그 뒤 **계속** 꺼져 있는 데서
 * 나온다(65mA -> 60µA). VIA(FEATURE > POWER > RGB Timeout)로 조절하고, 0 = 안 끔.
 */
#define ACTIVITY_RGB_TIMEOUT_MS_DEF     (5 * 60 * 1000)

#if CLI_USE(HW_ACTIVITY)
static void cliActivity(cli_args_t *args);
#endif

static activity_state_t state           = ACTIVITY_ACTIVE;
static uint32_t         idle_timeout_ms  = ACTIVITY_IDLE_TIMEOUT_MS_DEF;
static uint32_t         sleep_timeout_ms = ACTIVITY_SLEEP_TIMEOUT_MS_DEF;
static uint32_t         rgb_timeout_ms   = ACTIVITY_RGB_TIMEOUT_MS_DEF;

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

  /*
   * [필수] RGB/인디케이터를 **무조건** 끈다 — 타임아웃 설정과 무관하게.
   *
   * sys_poweroff() 는 GPIO 를 건드리지 않는다(RAM 리텐션만 끄고 nrf_power_system_off()).
   * nRF52 는 System OFF 에서 **GPIO 출력 상태를 유지**하므로, 레일이 켜진 채 잠들면
   * **네오픽셀이 65mA 로 영원히 켜져 있다**. Caps LED(1.2mA)도 마찬가지다.
   *
   * 평소엔 RGB 타임아웃(2분)이 sleep(1시간)보다 훨씬 빨라 이미 꺼져 있지만, VIA 에서
   * RGB Timeout = Off 를 고르면 그대로 터진다. 여기서 못 박는다.
   *
   * (예전 주석은 "지금은 소비자가 없어 레일이 꺼져 있다" 였는데 Phase 6 시절 이야기다 —
   *  Phase 8 에서 ws2812 가 들어오며 낡았다)
   */
  qmkSuspendForSleep();

  k_msleep(10);   // 로그 flush 여유(콘솔 빌드) + 레일 방전

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
   * RGB 타임아웃 경과를 **지금** 반영해야 한다.
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

bool activityRgbIsIdle(void)
{
  if (rgb_timeout_ms == 0)
  {
    return false;   // 안 끔 (ZMK 와 같은 동작)
  }
  return qmkGetInactiveMs() >= rgb_timeout_ms;
}

void activitySetRgbTimeout(uint32_t ms)
{
  rgb_timeout_ms = ms;
}

uint32_t activityGetRgbTimeout(void)
{
  return rgb_timeout_ms;
}

/*
 * 다음에 볼 데드라인까지 남은 ms. **세 타임아웃 중 가장 가까운 것**을 봐야 한다 —
 * 하나라도 빠뜨리면 그 시점을 놓치고 다음 데드라인까지 반영이 밀린다.
 */
static uint32_t next_deadline(uint32_t wait, uint32_t timeout_ms, uint32_t inactive)
{
  if (timeout_ms == 0 || inactive >= timeout_ms)
  {
    return wait;   // 비활성이거나 이미 지났다
  }
  uint32_t left = timeout_ms - inactive;
  return (wait == 0 || left < wait) ? left : wait;
}

uint32_t activityGetWaitMs(void)
{
  uint32_t inactive = qmkGetInactiveMs();
  uint32_t wait     = 0;

  wait = next_deadline(wait, idle_timeout_ms, inactive);
  wait = next_deadline(wait, rgb_timeout_ms,  inactive);   // RGB 는 idle 과 별개 시점이다

  if (wait > 0)
  {
    return wait;
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
