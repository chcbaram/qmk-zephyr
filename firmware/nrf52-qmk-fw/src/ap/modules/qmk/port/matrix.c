#include "matrix.h"
#include "debounce.h"
#include <string.h>
#include "keys.h"

/*
 * Phase 1 매트릭스 어댑터.
 * 기존 baram 스캔 드라이버(src/hw/driver/keys.c)의 keysUpdate()/keysGetPressed()
 * 경로로 raw 매트릭스를 채우고 QMK 디바운스를 적용한다.
 * (Phase 4 에서 ZMK kscan 저전력 인터럽트 스캔으로 교체 예정.)
 */

static matrix_row_t raw_matrix[MATRIX_ROWS];
static matrix_row_t matrix[MATRIX_ROWS];

void matrix_init(void)
{
  memset(matrix, 0, sizeof(matrix));
  memset(raw_matrix, 0, sizeof(raw_matrix));

  debounce_init(MATRIX_ROWS);
}

void matrix_print(void)
{
}

bool matrix_can_read(void)
{
  return true;
}

matrix_row_t matrix_get_row(uint8_t row)
{
  return matrix[row];
}

uint8_t matrix_scan(void)
{
  matrix_row_t curr_matrix[MATRIX_ROWS] = {0};

  keysUpdate();

  for (uint8_t row = 0; row < MATRIX_ROWS; row++)
  {
    for (uint8_t col = 0; col < MATRIX_COLS; col++)
    {
      if (keysGetPressed(row, col))
      {
        curr_matrix[row] |= ((matrix_row_t)1 << col);
      }
    }
  }

  bool changed = memcmp(raw_matrix, curr_matrix, sizeof(curr_matrix)) != 0;
  if (changed)
  {
    memcpy(raw_matrix, curr_matrix, sizeof(curr_matrix));
  }

  changed = debounce(raw_matrix, matrix, MATRIX_ROWS, changed);

  return (uint8_t)changed;
}
