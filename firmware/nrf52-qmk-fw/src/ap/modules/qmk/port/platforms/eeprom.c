#include "eeprom.h"
#include <string.h>

// Phase 1: RAM 백업 EEPROM (비영속). 부팅마다 기본 키맵.
// Phase 3에서 emu-eeprom(플래시 에뮬) 백엔드로 교체하여 영속화한다.
// 크기는 config.h 의 TOTAL_EEPROM_BYTE_COUNT 와 일치시켜야 한다.
// config.h 의 TOTAL_EEPROM_BYTE_COUNT 와 일치시킬 것.
#ifndef PORT_EEPROM_SIZE
#define PORT_EEPROM_SIZE   4096
#endif

static uint8_t eeprom_buf[PORT_EEPROM_SIZE];

void eeprom_init(void)
{
  memset(eeprom_buf, 0xFF, sizeof(eeprom_buf));
}

void eeprom_update(void)
{
}

void eeprom_task(void)
{
}

void eeprom_req_clean(void)
{
}

static inline uint32_t ee_off(const void *addr)
{
  return (uint32_t)(uintptr_t)addr;
}

uint8_t eeprom_read_byte(const uint8_t *addr)
{
  uint32_t off = ee_off(addr);
  if (off >= PORT_EEPROM_SIZE) return 0;
  return eeprom_buf[off];
}

uint16_t eeprom_read_word(const uint16_t *addr)
{
  const uint8_t *p = (const uint8_t *)addr;
  return (uint16_t)eeprom_read_byte(p) | ((uint16_t)eeprom_read_byte(p + 1) << 8);
}

uint32_t eeprom_read_dword(const uint32_t *addr)
{
  const uint8_t *p = (const uint8_t *)addr;
  return (uint32_t)eeprom_read_byte(p) |
         ((uint32_t)eeprom_read_byte(p + 1) << 8) |
         ((uint32_t)eeprom_read_byte(p + 2) << 16) |
         ((uint32_t)eeprom_read_byte(p + 3) << 24);
}

void eeprom_read_block(void *buf, const void *addr, uint32_t len)
{
  uint32_t off = ee_off(addr);
  uint8_t *dst = (uint8_t *)buf;
  for (uint32_t i = 0; i < len; i++)
  {
    dst[i] = (off + i < PORT_EEPROM_SIZE) ? eeprom_buf[off + i] : 0;
  }
}

void eeprom_write_byte(uint8_t *addr, uint8_t value)
{
  uint32_t off = ee_off(addr);
  if (off < PORT_EEPROM_SIZE) eeprom_buf[off] = value;
}

void eeprom_write_word(uint16_t *addr, uint16_t value)
{
  uint8_t *p = (uint8_t *)addr;
  eeprom_write_byte(p, (uint8_t)(value & 0xFF));
  eeprom_write_byte(p + 1, (uint8_t)(value >> 8));
}

void eeprom_write_dword(uint32_t *addr, uint32_t value)
{
  uint8_t *p = (uint8_t *)addr;
  eeprom_write_byte(p, (uint8_t)(value & 0xFF));
  eeprom_write_byte(p + 1, (uint8_t)(value >> 8));
  eeprom_write_byte(p + 2, (uint8_t)(value >> 16));
  eeprom_write_byte(p + 3, (uint8_t)(value >> 24));
}

void eeprom_write_block(const void *buf, void *addr, size_t len)
{
  uint32_t off = ee_off(addr);
  const uint8_t *src = (const uint8_t *)buf;
  for (size_t i = 0; i < len; i++)
  {
    if (off + i < PORT_EEPROM_SIZE) eeprom_buf[off + i] = src[i];
  }
}

void eeprom_update_byte(uint8_t *addr, uint8_t value)
{
  if (eeprom_read_byte(addr) != value) eeprom_write_byte(addr, value);
}

void eeprom_update_word(uint16_t *addr, uint16_t value)
{
  if (eeprom_read_word(addr) != value) eeprom_write_word(addr, value);
}

void eeprom_update_dword(uint32_t *addr, uint32_t value)
{
  if (eeprom_read_dword(addr) != value) eeprom_write_dword(addr, value);
}

void eeprom_update_block(const void *buf, void *addr, size_t len)
{
  eeprom_write_block(buf, addr, len);
}
