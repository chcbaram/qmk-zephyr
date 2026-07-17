#pragma once

#include <stdint.h>

/*
 * 런타임 디바운스 **시간** (VIA 로 조절).
 *
 * [타입은 런타임이 아니다] baram-qmk(VENOM)는 알고리즘 타입(sym_eager_pk/sym_defer_pk)도
 * 런타임에 바꾸지만 우리는 **시간만** 연다(사용자 결정). 타입을 열면 두 알고리즘을 다 링크하고
 * QMK 순정 debounce() 를 우회하는 층(baram 의 debounce_runtime.c)이 필요한데, 우리 알고리즘은
 * sym_defer_pk 고정이라(config.cmake) 그 비용을 낼 이유가 없다.
 *
 * [QMK 쪽 수정 없음] vendored sym_defer_pk.c 에 이미 훅이 있다:
 *     #ifdef DEBOUNCE_RUNTIME
 *         *debounce_pointer = debounce_time_get();   <- 우리가 제공
 *     #else
 *         *debounce_pointer = DEBOUNCE;
 *     #endif
 * 이건 baram 포크의 패치다(upstream QMK 엔 없다). DEBOUNCE_RUNTIME 은 config.cmake 가 켠다.
 *
 * [컴파일타임 DEBOUNCE 도 여전히 필요하다] sym_defer_pk.c 는 `#if DEBOUNCE > 0` 으로 구현 전체를
 * 가둔다 — 0 이면 파일이 통째로 패스스루가 된다. config.h 의 DEBOUNCE 는 **런타임 값과 무관하게**
 * 0 보다 커야 한다(지금 10).
 */

void    debounce_cfg_init(void);
void    via_qmk_debounce_command(uint8_t *data, uint8_t length);

// sym_defer_pk.c 가 카운터를 로드할 때 부른다(DEBOUNCE_RUNTIME 빌드).
uint8_t debounce_time_get(void);
