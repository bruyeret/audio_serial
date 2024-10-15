#pragma once

#include "custom_mul.h"

// Compute squared magnitude of a frequency given spectrum buffer and frequency index
inline uint32_t get_buffer_squared_value(int16_t *buffer, uint8_t frequency_index)
{
    uint8_t real_part_index = 2 * frequency_index;
    uint8_t imag_part_index = real_part_index + 1;
    return square(buffer[real_part_index]) + square(buffer[imag_part_index]);
}

// Get an 8 bit value from 4 consecutive frequencies in the input spectrum
// frequency_index: the first frequency of the 4 consecutive frequencies holding the value
// spectrum_buffer: the input spectrum [re0, im0, re1, im1, ...]
// square_limit_1, square_limit_2, square_limit_3: square of the magnitudes used to get a single base 4 digit per frequency:
//      - square_magnitude <= square_limit_1: frequency represents the digit 0
//      - square_limit_1 < square_magnitude <= square_limit_2: frequency represents the digit 1
//      - square_limit_2 < square_magnitude <= square_limit_3: frequency represents the digit 2
//      - square_limit_3 < square_magnitude: frequency represents the digit 3
inline uint8_t decode_value_from_spectrum(uint8_t first_frequency_index, int16_t *spectrum_buffer, uint32_t square_limit_1, uint32_t square_limit_2, uint32_t square_limit_3)
{
    uint8_t input_value = 0;
    for (uint8_t frequency_index = first_frequency_index; frequency_index < first_frequency_index + 4; ++frequency_index)
    {
        uint32_t frequency_value_squared = get_buffer_squared_value(spectrum_buffer, frequency_index);
        input_value <<= 1;
        if (frequency_value_squared > square_limit_2)
        {
            input_value |= 1;
            input_value <<= 1;
            if (frequency_value_squared > square_limit_3)
            {
                input_value |= 1;
            }
        }
        else
        {
            input_value <<= 1;
            if (frequency_value_squared > square_limit_1)
            {
                input_value |= 1;
            }
        }
    }
    return input_value;
}

inline void get_values_from_spectrum(int16_t *spectrum, uint8_t number_of_values, uint8_t *values)
{
    // Ref frequency is frequency index 1
    constexpr uint8_t reference_frequency_index = 1;
    uint32_t reference_value_squared = get_buffer_squared_value(spectrum, reference_frequency_index);

    // Deduce limits from squared reference frequency
    uint32_t first_limit = reference_value_squared / 36;
    uint32_t second_limit = reference_value_squared / 4;
    uint32_t third_limit = 25 * reference_value_squared / 36;

    // First data byte frequency starts at index 3
    constexpr uint8_t frequency_index_start = 3;
    // Each value uses 4 consecutive frequencies, but are 5 frequencies apart
    constexpr uint8_t frequency_index_stride = 5;
    // Get all values
    for (
        uint8_t frequency_index = frequency_index_start;
        frequency_index < frequency_index_start + frequency_index_stride * number_of_values;
        frequency_index += frequency_index_stride)
    {
        uint8_t input_value = decode_value_from_spectrum(
            frequency_index, spectrum, first_limit, second_limit, third_limit);
        values++[0] = input_value;
    }
}
