#include "quantum.h"
#include <zephyr/device.h>
#include <zephyr/drivers/eeprom.h>

/*
 * QMK EEPROM 어댑터 — Zephyr 플래시 에뮬 EEPROM(zephyr,emu-eeprom, DTS: eeprom0) 백엔드.
 *
 * 설계 (원본 baram의 "RAM 미러 + 지연 flush" 개념 유지, flush 단위만 개선):
 *  - RAM 미러 eeprom_buf[]: 모든 읽기는 여기서 (런타임 플래시 접근 0 → 저전력·고속).
 *  - 쓰기: 미러만 갱신 + 변경 범위(dirty range) 확장. 플래시 접근 없음(비블로킹).
 *  - eeprom_task(): 마지막 쓰기 후 EE_FLUSH_DELAY_MS 동안 조용하면, 변경 범위를
 *    "한 번의 블록 쓰기"로 emu-eeprom 에 flush.
 *
 * 원본의 바이트 단위 쓰기 큐 대비 이점:
 *  - 전력: VIA 편집 버스트(수백~수천 바이트)를 플래시 쓰기 1회로 통합 → program/erase
 *    (compaction) 횟수 최소화. 플래시 program/erase 가 EEPROM 최대 에너지원.
 *  - RAM: 12~16KB 바이트 큐 제거, 미러 4KB + 상태변수 몇 개만.
 * 통상 타이핑 중에는 EEPROM 쓰기가 없어(키맵은 런타임 read-only) 플래시 활동 자체가 없다.
 */

#define EE_FLUSH_DELAY_MS   100   // 편집이 멎은 뒤 flush 까지 대기(버스트 통합)

static uint8_t              eeprom_buf[TOTAL_EEPROM_BYTE_COUNT];
static const struct device *ee_dev = DEVICE_DT_GET(DT_NODELABEL(eeprom0));
static bool                 ee_ready;

static bool                 dirty;
static uint32_t             dirty_min;      // 변경 범위(포함) [dirty_min, dirty_max]
static uint32_t             dirty_max;
static uint32_t             last_write_ms;
static bool                 is_req_clean = false;

void eeprom_init(void)
{
  ee_ready = device_is_ready(ee_dev);

  if (!ee_ready || eeprom_read(ee_dev, 0, eeprom_buf, TOTAL_EEPROM_BYTE_COUNT) != 0)
  {
    // 백엔드 미준비/읽기 실패 → 빈 EEPROM(0xFF). QMK 가 매직 불일치로 재초기화.
    memset(eeprom_buf, 0xFF, sizeof(eeprom_buf));
  }
  dirty = false;
}

// 변경 범위를 한 번의 블록 쓰기로 emu-eeprom 에 반영.
static void eeprom_flush(void)
{
  if (!dirty)
  {
    return;
  }
  if (ee_ready)
  {
    uint32_t len = dirty_max - dirty_min + 1;
    if (eeprom_write(ee_dev, dirty_min, &eeprom_buf[dirty_min], len) != 0)
    {
      logPrintf("emu-eeprom flush fail @%u len %u\n", dirty_min, len);
      return;   // 실패 시 dirty 유지 → 다음 task 에서 재시도
    }
  }
  dirty = false;
}

void eeprom_update(void)
{
  if (dirty && (millis() - last_write_ms) >= EE_FLUSH_DELAY_MS)
  {
    eeprom_flush();
  }
}

void eeprom_task(void)
{
  eeprom_update();

  if (is_req_clean)
  {
    eeconfig_disable();
    soft_reset_keyboard();
  }
}

void eeprom_req_clean(void)
{
  // 남은 변경분을 먼저 반영한 뒤 초기화가 이어지도록.
  eeprom_flush();
  is_req_clean = true;
}

uint8_t eeprom_read_byte(const uint8_t *addr)
{
  return eeprom_buf[(uint32_t)addr];
}

uint16_t eeprom_read_word(const uint16_t *addr)
{
  uint16_t ret = 0;

  ret  = eeprom_buf[((uint32_t)addr) + 0] << 0;
  ret |= eeprom_buf[((uint32_t)addr) + 1] << 8;

  return ret;
}

uint32_t eeprom_read_dword(const uint32_t *addr)
{
  uint32_t       ret = 0;
  const uint8_t *p   = (const uint8_t *)addr;

  ret  = eeprom_read_byte(p + 0) << 0;
  ret |= eeprom_read_byte(p + 1) << 8;
  ret |= eeprom_read_byte(p + 2) << 16;
  ret |= eeprom_read_byte(p + 3) << 24;

  return ret;
}

void eeprom_read_block(void *buf, const void *addr, uint32_t len)
{
  const uint8_t *p    = (const uint8_t *)addr;
  uint8_t       *dest = (uint8_t *)buf;
  while (len--)
  {
    *dest++ = eeprom_read_byte(p++);
  }
}

// 미러 갱신 + 변경 범위 표시(플래시 접근 없음). 실제 flush 는 eeprom_task 에서 통합.
static void eeprom_mark(uint32_t off, uint8_t value)
{
  if (off >= TOTAL_EEPROM_BYTE_COUNT)
  {
    return;
  }
  if (eeprom_buf[off] == value)
  {
    return;                 // 변화 없음 → dirty 확장 안 함(불필요 flush 방지)
  }
  eeprom_buf[off] = value;

  if (!dirty)
  {
    dirty_min = dirty_max = off;
    dirty     = true;
  }
  else
  {
    if (off < dirty_min) dirty_min = off;
    if (off > dirty_max) dirty_max = off;
  }
  last_write_ms = millis();
}

void eeprom_write_byte(uint8_t *addr, uint8_t value)
{
  eeprom_mark((uint32_t)addr, value);
}

void eeprom_write_word(uint16_t *addr, uint16_t value)
{
  uint32_t off = (uint32_t)addr;
  eeprom_mark(off,     (uint8_t)(value & 0xFF));
  eeprom_mark(off + 1, (uint8_t)(value >> 8));
}

void eeprom_write_dword(uint32_t *addr, uint32_t value)
{
  uint32_t off = (uint32_t)addr;
  eeprom_mark(off,     (uint8_t)(value & 0xFF));
  eeprom_mark(off + 1, (uint8_t)(value >> 8));
  eeprom_mark(off + 2, (uint8_t)(value >> 16));
  eeprom_mark(off + 3, (uint8_t)(value >> 24));
}

void eeprom_write_block(const void *buf, void *addr, size_t len)
{
  uint32_t       off = (uint32_t)addr;
  const uint8_t *src = (const uint8_t *)buf;
  for (size_t i = 0; i < len; i++)
  {
    eeprom_mark(off + i, src[i]);
  }
}

// update_* 는 write_* 와 동일 경로(eeprom_mark 가 이미 변경분만 반영).
void eeprom_update_byte(uint8_t *addr, uint8_t value)
{
  eeprom_write_byte(addr, value);
}

void eeprom_update_word(uint16_t *addr, uint16_t value)
{
  eeprom_write_word(addr, value);
}

void eeprom_update_dword(uint32_t *addr, uint32_t value)
{
  eeprom_write_dword(addr, value);
}

void eeprom_update_block(const void *buf, void *addr, size_t len)
{
  eeprom_write_block(buf, addr, len);
}
