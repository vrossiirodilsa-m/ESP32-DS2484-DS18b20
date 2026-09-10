#pragma once
#include <stdint.h>
#include <stdbool.h>

void onewire_search_reset(void);
bool onewire_search(uint8_t *newAddr);