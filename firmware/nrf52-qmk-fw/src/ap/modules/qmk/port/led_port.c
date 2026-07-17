#include "quantum.h"
#include <zephyr/device.h>
#include <zephyr/init.h>
#include <zephyr/drivers/gpio.h>

/*
 * 키보드 인디케이터 LED (Caps Lock) — **기본 구현**. DTS 의 `caps_led` 노드를 그대로 구동한다.
 *
 * [보드에 없으면 노드를 빼면 된다] caps_led 가 없으면 통째로 빠지고 QMK 의 weak
 * led_update_kb() 가 남는다(=아무것도 안 함). hw_def.h / led.h 와 같은 원칙(§4.1.2).
 *
 * [핀/극성은 여기 오지 않는다] DTS 가 정한다. 반전 구동이면 caps_led 노드에 GPIO_ACTIVE_LOW 를
 * 주면 gpio_pin_set_dt() 가 알아서 뒤집는다(gpio_driver_data.invert — gpio_595.c 이식 때 고친
 * 그 필드다). 극성 때문에 보드별 C 파일을 만들 이유는 없다.
 *
 * [로직이 다른 보드는 opt-out 한다]
 * 인디케이터 로직이 **구조적으로** 다를 수 있다 — Caps 를 GPIO 대신 RGB 로 표시, NumLock/
 * ScrollLock 추가, 레이어 표시 등. 그런 키보드는 config.h 에 KBD_CUSTOM_INDICATOR 를 정의하고
 * keyboards/<kbd>/port/ 에 자기 led_update_kb() 를 두면 된다. 이 파일은 그때 통째로 빠진다.
 * (약한 심볼을 둘 두는 방식은 쓰지 않는다 — 링커가 임의로 골라 조용히 틀린다)
 *
 * QMK 가 배선을 다 해준다: keyboard_task() -> led_task() -> host_keyboard_leds() 로 호스트
 * LED 리포트를 읽고, 바뀌면 led_update_kb() 를 부른다. host_keyboard_leds() 는 활성
 * host_driver 를 타므로 **USB/BLE 어느 쪽이든 그대로 동작**한다.
 *
 * 순정 led_update_ports() 는 LED_CAPS_LOCK_PIN + QMK gpio API 를 전제한다. 우리 핀은
 * Zephyr gpio_dt_spec 이라 led_update_kb() 를 오버라이드한다.
 * (ramune60 도 같은 구조 — 거긴 QMK gpio_write_pin(GP7) 을 쓸 뿐이다)
 *
 * [디버그 LED 와 별개다] hw/driver/led.c 는 하트비트용 개발 편의 기능이고 이건 키보드 기능이다.
 * 부품도 수명도 다르다 — wish60 은 둘 다 있고(디버그 P0.31 / Caps P0.05), wish65 는 Caps 만
 * 있다(P1.09).
 *
 * [전력] 켜져 있으면 ≈1.2mA — idle 60µA 의 20배다. Caps 는 평소 꺼져 있어 괜찮지만
 * "항상 켜두는 인디케이터"를 추가할 땐 이 숫자를 기억할 것.
 * 슬립 소등은 QMK 가 해준다: qmkSuspendUpdate() -> suspend_power_down_quantum() ->
 * led_suspend() -> led_set(0) -> led_update_kb({0}). (§6.9)
 */

#if DT_NODE_EXISTS(DT_NODELABEL(caps_led)) && !defined(KBD_CUSTOM_INDICATOR)

static const struct gpio_dt_spec caps_led = GPIO_DT_SPEC_GET(DT_NODELABEL(caps_led), gpios);

/*
 * 핀 설정은 SYS_INIT 으로 한다 — QMK 의 keyboard_post_init_kb() 를 쓰지 않는 이유:
 * 그건 **키보드 몫의 훅**이라 공통이 가져가면 키보드가 인디케이터와 무관한 초기화를 할 자리를
 * 잃는다. 핀을 출력으로 잡는 건 어차피 하드웨어 초기화라 여기가 제자리다.
 *
 * 미설정(플로팅)으로 두면 입력 버퍼 관통 전류로 µA 가 샌다 — wish60 에서 이 핀을 설정한
 * 것만으로 idle 이 69µA -> 57.41µA 로 떨어졌다. 그래서 main 전에(APPLICATION) 잡는다.
 */
static int caps_led_init(void)
{
  return gpio_pin_configure_dt(&caps_led, GPIO_OUTPUT_INACTIVE);
}
SYS_INIT(caps_led_init, APPLICATION, 0);

bool led_update_kb(led_t led_state)
{
  bool res = led_update_user(led_state);

  if (res)
  {
    gpio_pin_set_dt(&caps_led, led_state.caps_lock);
  }
  return res;
}

#endif   // DT_NODE_EXISTS(caps_led) && !KBD_CUSTOM_INDICATOR
