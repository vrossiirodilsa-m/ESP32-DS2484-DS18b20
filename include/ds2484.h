#pragma once

#include <stdint.h>
#include <stdbool.h>
#include "esp_err.h"

esp_err_t ds2484_init(void);
bool ds2484_reset(void);
bool ds2484_active_pullup(bool enable);

// Низкоуровневые примитивы 1-Wire для работы через мост
bool ds2484_onewire_reset(void);
bool ds2484_onewire_write_byte(uint8_t byte);
bool ds2484_onewire_read_byte(uint8_t *byte);
bool ds2484_onewire_write_bit(bool bit);
bool ds2484_onewire_read_bit(uint8_t *bit);