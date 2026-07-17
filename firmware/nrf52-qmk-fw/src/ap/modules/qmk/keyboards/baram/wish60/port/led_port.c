#include "quantum.h"
#include <zephyr/device.h>
#include <zephyr/drivers/gpio.h>

/*
 * 키보드 인디케이터 LED (Caps Lock) — DTS: caps_led (P0.05).
 *
 * hw/driver/led.c 의 채널로 넣지 않고 여기서 직접 소유한다: 그건 **디버그용** LED 드라이버고
 * 이건 키보드 기능이라 별개 부품이다(수명도 다르다).
 * 보드마다 인디케이터 구성이 달라 키보드 트리에 둔다(via_port.c / keycode_port.c 와 같은 이유).
 *
 * QMK 가 배선을 다 해준다: keyboard_task() -> led_task() -> host_keyboard_leds() 로 호스트
 * LED 리포트를 읽고, 바뀌면 led_update_kb() 를 부른다. host_keyboard_leds() 는 활성
 * host_driver 를 타므로 **USB/BLE 어느 쪽이든 그대로 동작**한다.
 *
 * 순정 led_update_ports() 는 LED_CAPS_LOCK_PIN + QMK gpio API 를 전제한다. 우리 핀은
 * Zephyr gpio_dt_spec 이라 led_update_kb() 를 오버라이드한다.
 * (ramune60 도 같은 구조 — 거긴 QMK gpio_write_pin(GP7) 을 쓸 뿐이다)
 *
 * [전력] 켜져 있으면 ≈1.2mA — idle 57µA 의 20배다. Caps 는 평소 꺼져 있어 괜찮지만
 * "항상 켜두는 인디케이터"를 추가할 땐 이 숫자를 기억할 것.
 * 슬립 소등은 QMK 가 해준다: USB 서스펜드 -> led_suspend() -> led_set(0) -> led_update_kb({0}).
 */
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
