#pragma once

#include <stdint.h>

// Get CRC 8 remainder for up to 256 values using the given polynomial using normal representation (see Wikipedia article)
inline uint8_t get_crc8_remainder(uint8_t *values, uint8_t number_of_values, uint8_t polynomial)
{
    if (number_of_values == 0)
    {
        return 0;
    }
    uint8_t scratch_high = values[0];
    for (uint8_t value_index = 1; value_index <= number_of_values; ++value_index)
    {
        uint8_t scratch_low = (value_index < number_of_values) ? values[value_index] : 0;
        for (uint8_t bit_index = 0; bit_index < 8; ++bit_index)
        {
            asm(R"==(
            ; Shift left the scratch
            add %[low],%[low]
            adc %[high],%[high]
            ; Skip the xor if carry clear
            brcc .+2
            eor %[high],%[poly]
)=="
                : [low] "+&r" (scratch_low), [high] "+&r" (scratch_high)
                : [poly] "r" (polynomial)
                : "cc"
            );
        }
    }
    return scratch_high;
}
