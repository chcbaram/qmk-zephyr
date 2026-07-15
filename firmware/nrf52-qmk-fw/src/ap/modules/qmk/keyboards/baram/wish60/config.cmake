cmake_minimum_required(VERSION 3.13)

# RGB (미포팅: Phase 6에서 ZMK led_strip 흡수 예정)
set(RGBLIGHT_ENABLE false)

# 디바운스 알고리즘 (VENOM 기준: sym_eager_pk)
set(DEBOUNCE_TYPE sym_eager_pk)

# --- 아래는 baram-8k VENOM 커스텀 런타임 기능 (현재 미지원) ---
# 대응 port 파일(port/debounce_runtime.c, debounce_cfg.c, hold_okp.c)과
# VIA 커스텀 채널(port/via_port.c, 각 보드 port/)이 필요하다. 포팅 후 활성화:
# set(DEBOUNCE_RUNTIME ON)   # 런타임 디바운스 타입/시간 전환 (VIA)
# set(HOLD_OKP_RUNTIME ON)   # HOLD_ON_OTHER_KEY_PRESS 런타임 on/off (VIA)
