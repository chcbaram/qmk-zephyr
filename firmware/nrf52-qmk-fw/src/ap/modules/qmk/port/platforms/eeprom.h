// Copyright 2018-2022 QMK
// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once


#include "hw_def.h"



void     eeprom_init(void);
void     eeprom_update(void);

/*
 * 아직 플래시에 안 쓴 변경이 남아있나(settle-flush 대기 중).
 *
 * 메인 루프가 이걸 보고 **flush 될 때까지는 오래 자지 않는다**(qmkGetIdleWaitMs).
 * 안 그러면 VIA 저장/키맵 편집 직후 루프가 수십 초 블록해버려, 그 사이 전원이 꺼지면
 * 유실된다(실제로 겪음: RGB 를 끄고 전원을 껐다 켜면 다시 켜져 있었다).
 */
bool     eeprom_is_dirty(void);
void     eeprom_task(void);
void     eeprom_req_clean(void);
uint8_t  eeprom_read_byte(const uint8_t *addr);
uint16_t eeprom_read_word(const uint16_t *addr);
uint32_t eeprom_read_dword(const uint32_t *addr);
void     eeprom_read_block(void *buf, const void *addr, uint32_t len);
void     eeprom_write_byte(uint8_t *addr, uint8_t value);
void     eeprom_write_word(uint16_t *addr, uint16_t value);
void     eeprom_write_dword(uint32_t *addr, uint32_t value);
void     eeprom_write_block(const void *buf, void *addr, size_t len);
void     eeprom_update_byte(uint8_t *addr, uint8_t value);
void     eeprom_update_word(uint16_t *addr, uint16_t value);
void     eeprom_update_dword(uint32_t *addr, uint32_t value);
void     eeprom_update_block(const void *buf, void *addr, size_t len);
