#pragma once
#include <stdint.h>
#include <stdbool.h>

bool ds18b20_match_rom(const uint8_t *address);
bool ds18b20_read_temperature(const uint8_t *address, float *temperature);