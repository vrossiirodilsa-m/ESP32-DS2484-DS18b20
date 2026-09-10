#include "ds18b20.h"
#include "ds2484.h"

bool ds18b20_match_rom(const uint8_t *address) {
    if (!ds2484_onewire_reset()) return false;
    if (!ds2484_onewire_write_byte(0x55)) return false; // Match ROM
    for (uint8_t i = 0; i < 8; i++) {
        if (!ds2484_onewire_write_byte(address[i])) return false;
    }
    return true;
}

bool ds18b20_read_temperature(const uint8_t *address, float *temperature) {
    if (!ds18b20_match_rom(address)) return false;
    if (!ds2484_onewire_write_byte(0xBE)) return false; // Read Scratchpad

    uint8_t scratchpad[9];
    for (int i = 0; i < 9; i++) {
        if (!ds2484_onewire_read_byte(&scratchpad[i])) return false;
    }

    int16_t raw = (scratchpad[1] << 8) | scratchpad[0];
    *temperature = (raw * 0.0625f);
    return true;
}