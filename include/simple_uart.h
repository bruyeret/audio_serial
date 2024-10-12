#include <avr/io.h>
#include <util/delay.h>
#include <avr/interrupt.h>

void USART_Init(uint32_t baud_rate)
{
    // Set baud rate
    uint32_t baud_prescale = ((F_CPU / (baud_rate * 8UL))) - 1;
    UBRR0H = (unsigned char) (baud_prescale >> 8);
    UBRR0L = (unsigned char) (baud_prescale >> 0);

    // 8 data bits, no parity, 1 stop bit
    UCSR0C = 3 << UCSZ00;

    // Enable transmitter
    UCSR0B = 1 << TXEN0;

    // Ansynchronous mode: double transfer speed
    UCSR0A = 1 << U2X0;
}

void USART_SendByte(uint8_t u8Data)
{
    // Wait until last byte has been transmitted
    while ((UCSR0A & (1 << UDRE0)) == 0)
        ;

    // Transmit data
    UDR0 = u8Data;
}

// Print null terminated string
void USART_PrintString(const char *str)
{
    while (*str)
    {
        USART_SendByte(*str);
        ++str;
    }
}

// Print an int16
void USART_PrintInt16(int16_t value_to_print)
{
    // Print minus sign if needed
    if (value_to_print < 0)
    {
        USART_SendByte('-');
        value_to_print = -value_to_print;
    }

    // Absolute value never exceed 5 digits
    constexpr uint8_t digits_to_print = 5;

    // From least significant digit to most significant digit
    char digits[digits_to_print];
    for (uint8_t digit_index = 0; digit_index < digits_to_print; ++digit_index)
    {
        char current_digit = value_to_print % 10;
        value_to_print = value_to_print / 10;
        digits[digit_index] = current_digit;
    }

    // Print from most significant digit to least significant digit
    for (uint8_t digit_index = digits_to_print; digit_index-- > 0;)
    {
        USART_SendByte('0' + digits[digit_index]);
    }
}
