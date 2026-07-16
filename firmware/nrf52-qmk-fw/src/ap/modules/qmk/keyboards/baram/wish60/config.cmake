cmake_minimum_required(VERSION 3.13)

# RGB (미포팅: Phase 6에서 ZMK led_strip 흡수 예정)
set(RGBLIGHT_ENABLE false)

# 디바운스 알고리즘 (타이핑용).
#   sym_defer_pk : 접점이 DEBOUNCE ms 안정된 뒤 보고 → 채터링에 강함. 타이핑용(QMK 표준 기본).
#   sym_eager_pk : 눌림 즉시 보고 후 락아웃 → 지연 최소. 게이밍용(VENOM 기본).
# 스캔/디바운스는 QMK 가 담당하고 드라이버(gpio-kbd-matrix) 디바운스는 0 으로 둔다.
set(DEBOUNCE_TYPE sym_defer_pk)

# --- 아래는 baram-8k VENOM 커스텀 런타임 기능 (현재 미지원) ---
# 대응 port 파일(port/debounce_runtime.c, debounce_cfg.c, hold_okp.c)과
# VIA 커스텀 채널(port/via_port.c, 각 보드 port/)이 필요하다. 포팅 후 활성화:
# set(DEBOUNCE_RUNTIME ON)   # 런타임 디바운스 타입/시간 전환 (VIA)
# set(HOLD_OKP_RUNTIME ON)   # HOLD_ON_OTHER_KEY_PRESS 런타임 on/off (VIA)
