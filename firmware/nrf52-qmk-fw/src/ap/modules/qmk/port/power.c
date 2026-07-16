#include "power.h"
#include "hw_def.h"
#include "usb.h"

#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/pm/device.h>

/*
 * 저전력: 디버그 인프라(콘솔 UART) 게이팅.
 *
 * 배경 — 실측(배터리, USB 미연결, BLE 연결):
 *   전체 idle 1.21mA 중 ~1.1mA 가 UARTE 였다. Zephyr 콘솔(console_init/CONSOLE_GETCHAR)이
 *   RX 를 상시 대기시켜 HFCLK 를 계속 잡기 때문이다. (키보드 로직/BLE 는 이미 µA 수준)
 *
 * 정책 — USB 전원 유무로 판단 (ZMK 의 zmk_usb_is_powered() 와 같은 기준):
 *   USB 있음  = 개발/디버그 중  → UART 정상(콘솔·CLI 사용 가능)
 *   USB 없음  = 배터리 실사용   → UART suspend (HFCLK 해제)
 * 배터리 모드에선 어차피 시리얼을 볼 수 없으므로 잃는 게 없다.
 *
 * [현재 비활성 — qmkUpdate 에서 호출하지 않는다]
 * 실측 결과 (배터리, USB 미연결, BLE 연결):
 *   - 바닥: 1.1mA → 0.65mA  ✅ UART suspend 자체는 먹힌다(HFCLK 해제 확인)
 *   - 그런데 ~1.6ms 주기 wakeup 이 새로 생겨 평균 1.21mA → 1.98mA 로 **악화**.
 *   → suspend 된 UART 와 콘솔 서브시스템의 상호작용으로 보인다(과거 console_init() 를
 *      스킵했을 때 console_read() 가 블록하지 않고 즉시 리턴해 uartReadThread 가 스핀한
 *      것과 같은 계열). 순이득이 없어 일단 호출하지 않는다.
 *
 * 다음에 시도할 것:
 *   1) suspend 전에 콘솔 RX 소비 스레드를 먼저 멈추거나, 콘솔 서브시스템 대신 UART 드라이버
 *      인터럽트 RX 를 직접 써서 PM 과 충돌하지 않게 한다.
 *   2) 또는 릴리스 빌드에서 콘솔/CLI 를 아예 제외(빌드 타임 분리) — 런타임 게이팅보다 단순.
 *   ※ 바닥 0.65mA 도 목표(µA)엔 멀다. CLI 스레드 delay(5) 5ms 폴링도 함께 없애야 한다.
 */

static const struct device *uart_dev = DEVICE_DT_GET(DT_NODELABEL(uart0));
static bool                 uart_on = true;   // 부팅 시 콘솔은 켜진 상태

void powerConsoleUpdate(void)
{
  bool want_on;
  int  ret;

  if (!device_is_ready(uart_dev))
  {
    return;
  }

  want_on = usbIsVbusPresent();
  if (want_on == uart_on)
  {
    return;
  }

  ret = pm_device_action_run(uart_dev,
                             want_on ? PM_DEVICE_ACTION_RESUME : PM_DEVICE_ACTION_SUSPEND);
  if (ret != 0 && ret != -EALREADY)
  {
    return;   // 실패 시 상태 유지(다음 호출에서 재시도)
  }

  uart_on = want_on;

  if (want_on)
  {
    logPrintf("[  ] console on (USB)\n");
  }
}

bool powerIsConsoleOn(void)
{
  return uart_on;
}
