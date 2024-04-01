#include "unrolled_fft.h"
#include <Arduino.h>

#define ADC_PIN 0

// 125 samples * 8 (prescaler) at 16MHz => 16kHz
// 31.25 samples * 8 (prescaler) at 16MHz => 64kHz
// For my board, using 124 and 31 instead gives better results

// constexpr uint8_t number_of_zero_samples = 4 * 2; // Equivalent to 2 samples at 16kHz
// constexpr uint8_t zero_samples_period = 31; // 64kHz

// constexpr uint8_t number_of_wait_samples = 6;
// constexpr uint8_t wait_samples_period = 124; // 16kHz

constexpr uint8_t log2_number_of_data_samples = 5; // 2^5 == 32
constexpr uint8_t number_of_data_samples = 1 << log2_number_of_data_samples; // 2^(log2(n)) = n
constexpr uint8_t data_samples_period = 124; // 16kHz

// Sampling variables
volatile uint8_t sample_buffer_idx = 0;
int16_t sample_buffer_0[number_of_data_samples];
int16_t sample_buffer_1[number_of_data_samples];
// Points to sample_buffer_0 or sample_buffer_1
int16_t *sample_buffer = sample_buffer_0;


inline void set_timer0_period(uint8_t samples)
{
    OCR0A = samples;
}

inline void setup_timer0()
{
    TCCR0A = (1 << WGM01);  // CTC on OCR0A
    TCCR0B = (1 << CS01);   // Prescaler of 8
    OCR0A = 0xFF;           // Maximum period to avoid infinite loop
    // "A conversion will be triggered by the rising edge of the selected interrupt flag"
    TIMSK0 = (1 << OCIE0A); // Enable an empty interrupt to trigger conversion
    TCNT0 = 0;              // Reset counter
}

inline void setup_adc()
{
    ADMUX = (1 << REFS0) |      // Reference: AVcc with external capacitor at AREF pin
            (ADC_PIN & 0b111);  // Use ADC_PIN for sampling

    ADCSRA = (1 << ADEN) |      // Enable ADC
             (1 << ADATE) |     // Enable auto trigger
             (1 << ADIE) |      // Enable conversion complete interrupt
             (1 << ADPS2) |     // Prescaler 64 (for 16kHz sampling)
             (1 << ADPS1);

    ADCSRB = (1 << ADTS1) |     // Sample on: Timer/Counter0 compare match A
             (1 << ADTS0);
}

inline int16_t *get_samples()
{
    // Wait for sampling to be done
    while (sample_buffer_idx < number_of_data_samples) {}

    // Swap buffers
    auto filled_buffer = sample_buffer;
    auto other_buffer = filled_buffer == sample_buffer_0 ? sample_buffer_1 : sample_buffer_0;
    sample_buffer = other_buffer;

    // Reseting index will start sampling
    sample_buffer_idx = 0;

    return filled_buffer;
}

void setup()
{
    Serial.begin(115200);
    Serial.println("Starting program");
    Serial.flush();
    setup_timer0();
    set_timer0_period(data_samples_period);
    setup_adc();
}

void loop()
{
    auto buffer = get_samples(); // Wait for some samples
    approx_fft32(buffer);

    for (uint8_t i = 0; i < number_of_data_samples; ++i)
    {
        Serial.print(buffer[i]);
        Serial.print(", ");
    }
    Serial.println();
}

// ADC conversion done interrupt
ISR(ADC_vect)
{
    if (sample_buffer_idx < number_of_data_samples)
    {
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
            (shift >= 0) ?
                offseted_sample << shift :
                offseted_sample >> -shift;
    }
}

// Empty interrupt for TIMER0_COMPA, but interrupt enabled to trigger ADC
EMPTY_INTERRUPT(TIMER0_COMPA_vect);
