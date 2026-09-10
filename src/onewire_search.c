#include "onewire_search.h"
#include "ds2484.h"

static uint8_t ROM_NO[8];
static uint8_t LastDiscrepancy = 0;
static uint8_t LastFamilyDiscrepancy = 0;
static bool LastDeviceFlag = false;

void onewire_search_reset(void) {
    LastDiscrepancy = 0;
    LastDeviceFlag = false;
    LastFamilyDiscrepancy = 0;
}

bool onewire_search(uint8_t *newAddr) {
    bool search_result = false;
    uint8_t id_bit_number = 1;
    uint8_t last_zero = 0;
    uint8_t rom_byte_number = 0;
    uint8_t rom_byte_mask = 1;
    uint8_t id_bit, cmp_id_bit;
    bool search_direction;

    if (!LastDeviceFlag) {
        if (!ds2484_onewire_reset()) {
            onewire_search_reset();
            return false;
        }

        if (!ds2484_onewire_write_byte(0xF0)) return false;

        do {
            if (!ds2484_onewire_read_bit(&id_bit) || !ds2484_onewire_read_bit(&cmp_id_bit)) return false;

            if (id_bit && cmp_id_bit) {
                break;
            } else {
                if (id_bit != cmp_id_bit) {
                    search_direction = id_bit;
                } else {
                    if (id_bit_number < LastDiscrepancy) {
                        search_direction = ((ROM_NO[rom_byte_number] & rom_byte_mask) > 0);
                    } else {
                        search_direction = (id_bit_number == LastDiscrepancy);
                    }

                    if (!search_direction) {
                        last_zero = id_bit_number;
                        if (last_zero < 9) LastFamilyDiscrepancy = last_zero;
                    }
                }

                if (search_direction) ROM_NO[rom_byte_number] |= rom_byte_mask;
                else ROM_NO[rom_byte_number] &= ~rom_byte_mask;

                if (!ds2484_onewire_write_bit(search_direction)) return false;

                id_bit_number++;
                rom_byte_mask <<= 1;
                if (rom_byte_mask == 0) { rom_byte_number++; rom_byte_mask = 1; }
            }
        } while (rom_byte_number < 8);

        if (!(id_bit_number < 65)) {
            LastDiscrepancy = last_zero;
            if (LastDiscrepancy == 0) LastDeviceFlag = true;
            search_result = true;
        }
    }

    if (!search_result || !ROM_NO[0]) {
        onewire_search_reset();
        search_result = false;
    }

    for (int i = 0; i < 8; i++) newAddr[i] = ROM_NO[i];
    return search_result;
}