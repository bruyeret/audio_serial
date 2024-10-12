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

    // Print the computed fft
    for (uint8_t sample_idx = 0; sample_idx < number_of_data_samples; ++sample_idx)
    {
        USART_PrintInt16(filled_buffer[sample_idx]);
        USART_PrintString(", ");
    }
    USART_PrintString("\n");
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
