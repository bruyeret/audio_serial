#include "custom_mul.h"
#include "unrolled_fft.h"
#include "simple_uart.h"
#include <avr/io.h>
#include <avr/interrupt.h>

#define ADC_PIN 0

// How to determine sampling frequency settings:
// 125 periods * 8 (prescaler) at 16MHz => 16kHz
// 62.5 periods * 8 (prescaler) at 16MHz => 32kHz
// 31.25 periods * 8 (prescaler) at 16MHz => 64kHz

// Two things have to be taken into account:
// - The number of periods can't be floating points
// - The clock has not 100% accuracy

// To solve these issues, tweak the frequency on the emitter side and round the periods

constexpr uint8_t log2_number_of_data_samples = 6;                           // 2^6 == 64
constexpr uint8_t number_of_data_samples = 1 << log2_number_of_data_samples; // log2(64) = 6
constexpr uint8_t data_samples_period = 62;                                  // 32kHz
// Buffer to contain the samples data
int16_t sample_buffer_0[number_of_data_samples];
int16_t sample_buffer_1[number_of_data_samples];
// Points to sample_buffer_0 or sample_buffer_1 alternatively
int16_t *volatile sample_buffer = sample_buffer_0;
// Index of the next sample in the sample buffer
volatile uint8_t sample_buffer_idx = 0;

// Function used to compute squared magnitude of a frequency given buffer and frequency index

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
inline uint8_t get_input_value_from_spectrum(uint8_t first_frequency_index, int16_t *spectrum_buffer, uint32_t square_limit_1, uint32_t square_limit_2, uint32_t square_limit_3)
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

inline void setup_timer0()
{
    TCCR0A = (1 << WGM01); // CTC on OCR0A
    TCCR0B = (1 << CS01);  // Prescaler of 8

    // Always make sure to set OCR0A to something greater than 0 to avoid infinite loop
    OCR0A = data_samples_period;

    // "A conversion will be triggered by the rising edge of the selected interrupt flag"
    TIMSK0 = (1 << OCIE0A); // Enable an empty interrupt to trigger conversion
    TCNT0 = 0;              // Reset counter
}

inline void setup_adc()
{
    ADMUX = (1 << REFS0) |     // Reference: AVcc with external capacitor at AREF pin
            (ADC_PIN & 0b111); // Use ADC_PIN for sampling

    ADCSRA = (1 << ADEN) |  // Enable ADC
             (1 << ADATE) | // Enable auto trigger
             (1 << ADIE) |  // Enable conversion complete interrupt
             (1 << ADPS2) | // Prescaler 32 (up to 37kHz sampling)
             (1 << ADPS0);

    ADCSRB = (1 << ADTS1) | // Sample on: Timer/Counter0 compare match A
             (1 << ADTS0);
}

void setup()
{
    sei();
    setup_timer0();
    setup_adc();

    USART_Init(115200);
    USART_PrintString("Hello !\n");
}

void loop()
{
    // Wait until the sample buffer is full
    while (sample_buffer_idx < number_of_data_samples)
        ;

    // We will use the sample buffer that is now filled with samples
    auto filled_buffer = sample_buffer;
    // The other buffer can be used for sampling while we work on the filled buffer
    auto other_buffer = (filled_buffer == sample_buffer_0) ? sample_buffer_1 : sample_buffer_0;
    // First, set the sample_buffer that will be used by interrupt
    sample_buffer = other_buffer;
    // Then, set the index to 0 to start sampling
    sample_buffer_idx = 0;

    // Compute fft
    approx_fft64(filled_buffer);

    // Get ref value squared
    constexpr uint8_t reference_frequency_index = 1;
    uint32_t reference_value_squared = get_buffer_squared_value(filled_buffer, reference_frequency_index);
    // Deduce limits
    uint32_t first_limit = reference_value_squared / 36;
    uint32_t second_limit = reference_value_squared / 4;
    uint32_t third_limit = 25 * reference_value_squared / 36;
    // Each value uses 4 consecutive frequencies, but are 5 frequencies apart
    // There are 5 values to read, and first frequency starts at index 3
    constexpr uint8_t frequency_index_start = 3;
    constexpr uint8_t frequency_index_stride = 5;
    constexpr uint8_t number_of_input_values = 5;
    for (
        uint8_t frequency_index = frequency_index_start;
        frequency_index < frequency_index_start + frequency_index_stride * number_of_input_values;
        frequency_index += frequency_index_stride)
    {
        uint8_t input_value = get_input_value_from_spectrum(
            frequency_index, filled_buffer, first_limit, second_limit, third_limit);
        USART_SendByte(input_value);
    }
}

// ADC conversion done interrupt
ISR(ADC_vect)
{
    // Don't sample if the index is out of bounds
    if (sample_buffer_idx >= number_of_data_samples)
    {
        return;
    }

    // We use int16 in calculations, so we can store values in [-2^15, 2^15[
    // Value of ADC is in [0, 2^10[
    // Value of [(ADC - 512) << shift] is in [-2^(9 + shift), 2^(9 + shift)[
    // After FFT, we multiply by at most n_samples, so we are in:
    //   [-2^(9 + shift + log2_n_samples), 2^(9 + shift + log2_n_samples)[
    // To get maximum accuracy without overflow, we can use: 9 + shift + log2_n_samples = 15
    // So: shift = 6 - log2_n_samples
    constexpr int8_t shift = 6 - log2_number_of_data_samples;
    // Ternary operator should be optimised out by the compiler
    int16_t offseted_sample = ADC - 512;
    sample_buffer[sample_buffer_idx++] =
        (shift >= 0) ? offseted_sample << shift : offseted_sample >> -shift;
}

// Empty interrupt for TIMER0_COMPA, but interrupt enabled to trigger ADC
EMPTY_INTERRUPT(TIMER0_COMPA_vect);

int main()
{
    setup();
    for (;;)
    {
        loop();
    }
}
