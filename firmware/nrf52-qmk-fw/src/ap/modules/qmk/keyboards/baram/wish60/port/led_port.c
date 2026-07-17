#include "quantum.h"
#include <zephyr/device.h>
#include <zephyr/drivers/gpio.h>

/*
 * 키보드 인디케이터 LED (Caps Lock) — DTS: caps_led (wish60: P0.05).
 *
 * hw/driver/led.c 의 채널로 넣지 않고 여기서 직접 소유한다: 그건 **디버그용** LED 드라이버고
 * 이건 키보드 기능이라 별개 부품이다(수명도 다르다. wish65 는 Caps 만 있고 디버그 LED 가 없다).
 *
 * [왜 공통이 아니라 키보드 트리인가]
 * 지금은 wish60/wish65 의 이 파일이 핀 말고는 같아서 공통으로 뺄 수 있어 보인다. 그러면 안 된다 —
 * **인디케이터 로직은 보드마다 구조가 다를 수 있다**: Caps 를 GPIO 대신 RGB 로 표시,
 * NumLock/ScrollLock 추가, 레이어 표시 등. 공통이 led_update_kb() 를 가져가면 그때
 * 오버라이드할 방법이 없다(약한 심볼을 둘 두면 링커가 임의로 고른다 — 더 나쁘다).
 * via_port.c / keycode_port.c 를 키보드 트리에 두는 것과 같은 이유.
 *
 * 단, **핀 번호와 극성은 여기 오지 않는다** — DTS 가 정한다. 반전 구동이면 caps_led 노드에
 * GPIO_ACTIVE_LOW 를 주면 gpio_pin_set_dt() 가 알아서 뒤집는다(gpio_driver_data.invert).
 * 그런 건 C 코드를 나눌 이유가 아니다.
 *
 * [Caps LED 가 없는 보드] 아래 DT_NODE_EXISTS 가드로 통째로 빠지고 QMK 의 weak led_update_kb()
 * 가 남는다(=아무것도 안 함). 보드 트리에서 caps_led 노드를 빼는 것만으로 끝난다 —
 * hw_def.h / led.h 와 같은 원칙(§4.1.2). 이 파일을 지워도 되지만, 남겨둬도 안전하다.
 *
 * QMK 가 배선을 다 해준다: keyboard_task() -> led_task() -> host_keyboard_leds() 로 호스트
 * LED 리포트를 읽고, 바뀌면 led_update_kb() 를 부른다. host_keyboard_leds() 는 활성
 * host_driver 를 타므로 **USB/BLE 어느 쪽이든 그대로 동작**한다.
 *
 * 순정 led_update_ports() 는 LED_CAPS_LOCK_PIN + QMK gpio API 를 전제한다. 우리 핀은
 * Zephyr gpio_dt_spec 이라 led_update_kb() 를 오버라이드한다.
 * (ramune60 도 같은 구조 — 거긴 QMK gpio_write_pin(GP7) 을 쓸 뿐이다)
 *
 * [전력] 켜져 있으면 ≈1.2mA — idle 60µA 의 20배다. Caps 는 평소 꺼져 있어 괜찮지만
 * "항상 켜두는 인디케이터"를 추가할 땐 이 숫자를 기억할 것.
 * 슬립 소등은 QMK 가 해준다: USB 서스펜드 -> led_suspend() -> led_set(0) -> led_update_kb({0}).
 */
#if DT_NODE_EXISTS(DT_NODELABEL(caps_led))

static const struct gpio_dt_spec caps_led = GPIO_DT_SPEC_GET(DT_NODELABEL(caps_led), gpios);

void keyboard_post_init_kb(void)
{
  // 미설정(플로팅) 상태로 두면 입력 버퍼 관통 전류로 µA 가 샌다 — 반드시 구동해 둘 것.
  gpio_pin_configure_dt(&caps_led, GPIO_OUTPUT_INACTIVE);

  keyboard_post_init_user();
}

bool led_update_kb(led_t led_state)
{
  bool res = led_update_user(led_state);

  if (res)
  {
    gpio_pin_set_dt(&caps_led, led_state.caps_lock);
  }
  return res;
}

#endif   // DT_NODE_EXISTS(caps_led)
