#include "unrolled_fft.h"
#include <Arduino.h>

#define ADC_PIN 0

enum SamplingState : uint8_t {
    FIND_ZERO,
    FIND_NON_ZERO,
    WAIT,
    DATA,
};
// Only used in ISR, no need to be volatile
SamplingState state;
volatile bool is_buffer_read = true;

// 125 samples * 8 (prescaler) at 16MHz => 16kHz
// 31.25 samples * 8 (prescaler) at 16MHz => 64kHz
// For my board, using 124 and 31 instead gives better results (clock accuracy issues)


// ZERO state variables
constexpr uint8_t number_of_zero_samples = 4 * 2; // Equivalent to 2 samples at 16kHz
constexpr uint8_t zero_samples_period = 31; // 64kHz
constexpr uint16_t zero_threshold = 8; // Absolute difference with 512
uint8_t remaining_zero_samples;

// NON ZERO state variables
constexpr uint8_t non_zero_samples_period = zero_samples_period;
constexpr uint16_t non_zero_threshold = 10; // Absolute difference with 512

// WAIT state variables
constexpr uint8_t number_of_wait_samples = 2;
constexpr uint8_t wait_samples_period = 124; // 16kHz
uint8_t remaining_wait_samples;


// DATA state variables
constexpr uint8_t log2_number_of_data_samples = 5; // 2^5 == 32
constexpr uint8_t number_of_data_samples = 1 << log2_number_of_data_samples; // log2(32) = 5
constexpr uint8_t data_samples_period = 124; // 16kHz
// Buffer to contain the samples data
int16_t sample_buffer_0[number_of_data_samples];
int16_t sample_buffer_1[number_of_data_samples];
// Points to sample_buffer_0 or sample_buffer_1 alternatively
int16_t *volatile sample_buffer = sample_buffer_0;
volatile uint8_t sample_buffer_idx;


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
    // Don't read the same buffer twice
    // Wait for sampling to be done if the current buffer is being written to
    while (is_buffer_read || sample_buffer_idx < number_of_data_samples) {}
    is_buffer_read = true;
    // Return the buffer that contains the samples
    return sample_buffer;
}


inline void setup_new_state(SamplingState newState)
{
    state = newState;
    switch (newState)
    {
    case SamplingState::FIND_ZERO:
        set_timer0_period(zero_samples_period);
        remaining_zero_samples = number_of_zero_samples;
        break;
    
    case SamplingState::FIND_NON_ZERO:
        set_timer0_period(non_zero_samples_period);
        break;

    case SamplingState::WAIT:
        set_timer0_period(wait_samples_period);
        remaining_wait_samples = number_of_wait_samples;
        break;

    case SamplingState::DATA:
        set_timer0_period(data_samples_period);
        // First, reset buffer index
        sample_buffer_idx = 0;
        // Now swap buffers
        auto filled_buffer = sample_buffer;
        auto other_buffer = filled_buffer == sample_buffer_0 ? sample_buffer_1 : sample_buffer_0;
        sample_buffer = other_buffer;
        break;
    }
}


void setup()
{
    Serial.begin(115200);
    Serial.println("Starting program");
    Serial.flush();

    setup_timer0();
    setup_adc();
    setup_new_state(SamplingState::FIND_ZERO);
}


void loop()
{
    auto buffer = get_samples(); // Wait for some samples
    approx_fft32(buffer);

    for (uint8_t i = 0; i < number_of_data_samples; i += 2)
    {
        int32_t re = static_cast<int32_t>(buffer[i]);
        int32_t im = static_cast<int32_t>(buffer[i + 1]);
        int32_t norm = re * re + im * im;
        Serial.print(norm);
        Serial.print(", ");
    }
    Serial.println();
}


inline void zero_sampling()
{
    uint16_t sample = ADC;
    if (sample > 512 - zero_threshold && sample < 512 + zero_threshold)
    {
        // Sampled a zero
        if (--remaining_zero_samples == 0)
        {
            setup_new_state(SamplingState::FIND_NON_ZERO);
        }
    } else {
        // Not a zero: reset count
        remaining_zero_samples = number_of_zero_samples;
    }
}


inline void non_zero_sampling()
{
    uint16_t sample = ADC;
    if (sample <= 512 - non_zero_threshold || sample >= 512 + non_zero_threshold)
    {
        setup_new_state(SamplingState::WAIT);
    }
}


inline void wait_sampling()
{
    if (--remaining_wait_samples == 0)
    {
        setup_new_state(SamplingState::DATA);
    }
}


inline void data_sampling()
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
    } else {
        // Buffer has been updated: it has not been read yet
        is_buffer_read = false;
        setup_new_state(SamplingState::FIND_ZERO);
    }
}


// ADC conversion done interrupt
ISR(ADC_vect)
{
    switch (state)
    {
    case SamplingState::FIND_ZERO:
        zero_sampling();
        break;

    case SamplingState::FIND_NON_ZERO:
        non_zero_sampling();
        break;
    
    case SamplingState::WAIT:
        wait_sampling();
        break;
    
    case SamplingState::DATA:
        data_sampling();
        break;
    }
}


// Empty interrupt for TIMER0_COMPA, but interrupt enabled to trigger ADC
EMPTY_INTERRUPT(TIMER0_COMPA_vect);
