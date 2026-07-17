/*
 * 74HC595 시프트레지스터 -> GPIO 컨트롤러 (SPI, 직렬 연결 지원)
 *
 * 원본: ZMK app/module/drivers/gpio/gpio_595.c (MIT, Copyright (c) 2022 The ZMK Contributors)
 * 변경점은 아래 [이식 메모] 참고.
 *
 * [왜 이식했나]
 * Zephyr 네이티브 `ti,sn74hc595` 는 바인딩이 `ngpios: const: 8`(1칩 고정)이고 드라이버도
 * `uint8_t output` 이라 **직렬 연결을 지원하지 않는다**. wish65 는 595 두 개를 직렬로 물려
 * 16컬럼을 구동하므로 쓸 수 없다. ZMK 것은 nwrite = ngpios/8 로 직렬을 지원하고, kscan 과
 * 달리 제거된 서브시스템 의존이 없어(gpio.h/spi.h 뿐) 이식 비용이 낮았다.
 *
 * [이식 메모 — ZMK 원본에서 바꾼 것]
 *  1. compatible: zmk,gpio-595 -> baram,gpio-595 (우리 바인딩)
 *  2. API 등록: `static const struct gpio_driver_api` -> `static DEVICE_API(gpio, ...)`.
 *     Zephyr 4.1 은 드라이버 API 를 iterable section 에 넣어 검증한다(device.h 의 DEVICE_API).
 *     in-tree gpio_sn74hc595.c 도 이 방식이다.
 *  3. **drv_data 첫 멤버 타입 수정** — 원본은 `struct gpio_driver_config data;` 인데
 *     GPIO 드라이버의 data 는 `struct gpio_driver_data` 로 시작해야 한다(in-tree 관례).
 *     둘 다 첫 멤버가 gpio_port_pins_t 라 크기가 같아 **우연히** 동작하지만, gpio_pin_set_dt()
 *     가 GPIO_ACTIVE_LOW 를 처리할 때 읽는 `invert` 필드가 gpio_driver_data 에 있다.
 *     지금 wish65 는 595 핀을 전부 GPIO_ACTIVE_HIGH 로 쓰므로 증상이 없지만, ACTIVE_LOW 를
 *     쓰는 순간 조용히 틀린다.
 *  4. init 우선순위를 Kconfig 대신 상수로 뒀다(아래 주석 참고).
 *
 * [출력 전용] 595 는 읽기가 없다. port_get_raw 는 -ENOTSUP 이고 pin_configure 도 GPIO_OUTPUT
 * 이 아니면 거부한다. 키 매트릭스에서 595 는 **구동측(col)** 에만 쓴다 — 읽기측(row)은 진짜
 * MCU GPIO 라야 인터럽트 웨이크업이 성립한다(docs/PORTING-NOTES.md §4.2).
 */

#define DT_DRV_COMPAT baram_gpio_595

#include <errno.h>

#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/sys/byteorder.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/drivers/spi.h>

#include <zephyr/logging/log.h>

/*
 * DTS 에 595 노드가 없는 보드(wish60 등)에서는 통째로 빠진다.
 * 이 파일은 src/hw/**.c glob 으로 항상 컴파일 대상에 들어가므로(Kconfig 로 고르는 in-tree
 * 드라이버와 다르다) 여기서 직접 가드한다. 없으면 "정의됐지만 안 쓰임" 경고가 남는다.
 */
#if DT_HAS_COMPAT_STATUS_OKAY(DT_DRV_COMPAT)

LOG_MODULE_REGISTER(gpio_595, CONFIG_GPIO_LOG_LEVEL);

struct reg_595_config
{
  struct gpio_driver_config common;   // 반드시 첫 멤버
  struct spi_dt_spec        bus;
  uint8_t                   ngpios;
};

struct reg_595_data
{
  struct gpio_driver_data data;       // 반드시 첫 멤버 (invert 필드를 gpio_pin_set_dt 가 본다)
  struct k_sem            lock;
  uint32_t                cache;      // 595 는 읽기가 없다 -> 마지막에 쓴 값을 우리가 기억한다
};

/*
 * 체인 전체를 한 번에 쓴다.
 *
 * 595 는 시프트 -> 래치 구조라 **부분 갱신이 불가능**하다. 그래서 cache 에 전체 상태를 들고
 * 있다가 매번 통째로 쏜다. 체인의 첫 칩이 최하위 바이트인데, SPI 로는 상위 바이트가 먼저
 * 나가야 마지막 칩까지 시프트되어 밀려간다 -> big-endian 변환 후 뒤쪽 nwrite 바이트만 보낸다.
 */
static int reg_595_write(const struct device *dev, uint32_t value)
{
  const struct reg_595_config *config = dev->config;
  struct reg_595_data         *data   = dev->data;
  uint8_t                      nwrite = config->ngpios / 8;
  uint32_t                     be     = sys_cpu_to_be32(value);
  int                          ret;

  const struct spi_buf     tx_buf = { .buf = ((uint8_t *)&be) + (4 - nwrite), .len = nwrite };
  const struct spi_buf_set tx     = { .buffers = &tx_buf, .count = 1 };

  ret = spi_write_dt(&config->bus, &tx);
  if (ret < 0)
  {
    LOG_ERR("spi_write %d", ret);
    return ret;
  }

  data->cache = value;
  return 0;
}

static int reg_595_port_set_masked_raw(const struct device *dev, uint32_t mask, uint32_t value)
{
  struct reg_595_data *data = dev->data;
  int                  ret;

  // SPI 는 블로킹이라 ISR 에서 못 쓴다. input-kbd-matrix 는 스레드에서 스캔하므로 정상 경로다.
  if (k_is_in_isr())
  {
    return -EWOULDBLOCK;
  }

  k_sem_take(&data->lock, K_FOREVER);
  ret = reg_595_write(dev, (data->cache & ~mask) | (mask & value));
  k_sem_give(&data->lock);
  return ret;
}

static int reg_595_port_get_raw(const struct device *dev, uint32_t *value)
{
  ARG_UNUSED(dev);
  ARG_UNUSED(value);
  return -ENOTSUP;   // 595 는 출력 전용
}

static int reg_595_port_set_bits_raw(const struct device *dev, uint32_t mask)
{
  return reg_595_port_set_masked_raw(dev, mask, mask);
}

static int reg_595_port_clear_bits_raw(const struct device *dev, uint32_t mask)
{
  return reg_595_port_set_masked_raw(dev, mask, 0);
}

static int reg_595_port_toggle_bits(const struct device *dev, uint32_t mask)
{
  struct reg_595_data *data = dev->data;
  int                  ret;

  if (k_is_in_isr())
  {
    return -EWOULDBLOCK;
  }

  k_sem_take(&data->lock, K_FOREVER);
  ret = reg_595_write(dev, data->cache ^ mask);
  k_sem_give(&data->lock);
  return ret;
}

static int reg_595_pin_configure(const struct device *dev, gpio_pin_t pin, gpio_flags_t flags)
{
  if (k_is_in_isr())
  {
    return -EWOULDBLOCK;
  }

  if ((flags & GPIO_OUTPUT) == 0U)
  {
    return -ENOTSUP;   // 출력 전용
  }
  if ((flags & GPIO_OPEN_DRAIN) != 0U)
  {
    return -ENOTSUP;   // 595 는 푸시풀만
  }

  if (flags & GPIO_OUTPUT_INIT_LOW)
  {
    return reg_595_port_clear_bits_raw(dev, BIT(pin));
  }
  if (flags & GPIO_OUTPUT_INIT_HIGH)
  {
    return reg_595_port_set_bits_raw(dev, BIT(pin));
  }
  return 0;
}

static DEVICE_API(gpio, reg_595_api) = {
  .pin_configure       = reg_595_pin_configure,
  .port_get_raw        = reg_595_port_get_raw,
  .port_set_masked_raw = reg_595_port_set_masked_raw,
  .port_set_bits_raw   = reg_595_port_set_bits_raw,
  .port_clear_bits_raw = reg_595_port_clear_bits_raw,
  .port_toggle_bits    = reg_595_port_toggle_bits,
};

static int reg_595_init(const struct device *dev)
{
  const struct reg_595_config *config = dev->config;
  struct reg_595_data         *data   = dev->data;

  if (!spi_is_ready_dt(&config->bus))
  {
    LOG_ERR("SPI bus not ready");
    return -ENODEV;
  }

  k_sem_init(&data->lock, 1, 1);
  return 0;
}

/*
 * init 우선순위 75.
 *
 * SPI(POST_KERNEL, CONFIG_SPI_INIT_PRIORITY=70) **뒤**, 이걸 col-gpios 로 쓰는
 * input-kbd-matrix(CONFIG_INPUT_INIT_PRIORITY=90) **앞**이어야 한다. 셋의 순서가 어긋나면
 * 매트릭스가 아직 없는 GPIO 컨트롤러를 잡는다.
 */
#define REG_595_INIT(n)                                                                   \
  static const struct reg_595_config reg_595_cfg_##n = {                                  \
    .common = {                                                                           \
      .port_pin_mask = (gpio_port_pins_t)(((uint64_t)1 << DT_INST_PROP(n, ngpios)) - 1U), \
    },                                                                                    \
    .bus    = SPI_DT_SPEC_INST_GET(                                                       \
                n, SPI_OP_MODE_MASTER | SPI_TRANSFER_MSB | SPI_WORD_SET(8), 0),           \
    .ngpios = DT_INST_PROP(n, ngpios),                                                    \
  };                                                                                      \
  static struct reg_595_data reg_595_data_##n;                                            \
  DEVICE_DT_INST_DEFINE(n, reg_595_init, NULL, &reg_595_data_##n, &reg_595_cfg_##n,       \
                        POST_KERNEL, 75, &reg_595_api);

DT_INST_FOREACH_STATUS_OKAY(REG_595_INIT)

#endif   // DT_HAS_COMPAT_STATUS_OKAY(baram_gpio_595)
