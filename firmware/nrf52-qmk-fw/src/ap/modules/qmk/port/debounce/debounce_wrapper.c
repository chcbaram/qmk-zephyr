/*
 * DEBOUNCE_RUNTIME 빌드에서 순정 디바운스 알고리즘 파일을 **감싸서** 컴파일한다.
 *
 * [왜 필요한가] vendored sym_defer_pk.c 는 DEBOUNCE_RUNTIME 이면 debounce_time_get() 을
 * 부르는데(baram 포크의 훅), 그 파일이 include 하는 quantum/debounce.h 엔 선언이 없다.
 * 그대로 컴파일하면 **암묵적 선언**이 되어 반환 타입이 int 로 추정된다 — 지금은 우연히
 * 동작하지만 결함이고, -Werror 면 빌드가 깨진다.
 *
 * 선언을 quantum/debounce.h 에 넣으면 간단하지만 그건 vendor-in 트리를 고치는 것이라
 * 안 한다. 대신 선언을 먼저 넣고 원본을 include 한다 — baram 의 port/debounce_defer_pk.c 와
 * 같은 방식이다(거긴 타입 전환 때문에 심볼명까지 바꾸지만, 우리는 시간만 바꾸므로 그럴 필요 없다).
 *
 * 이 파일이 컴파일될 때 qmk/CMakeLists.txt 는 원본을 QMK_SRC_FILES 에서 **뺀다** —
 * 안 그러면 debounce() 가 중복 정의된다.
 *
 * DEBOUNCE_IMPL_FILE 은 CMake 가 config.cmake 의 DEBOUNCE_TYPE 에서 만들어 넘긴다.
 */
#ifdef DEBOUNCE_RUNTIME

#include "../via/debounce_cfg.h"   // debounce_time_get() 선언
#include DEBOUNCE_IMPL_FILE

#endif
