#include <stdint.h>

// Compute (x * y) >> 16
// This multiplication can be off by one (for performance reasons)
// Useful when x represent an int16 and y represent the fixed-point fractional number y/(2^16)
inline int16_t approx_mul_shift16(int16_t x, uint16_t y)
{
    int16_t value;
    asm(R"==(
    ; ret = MSB(x) * MSB(y)
    mulsu %B[x],%B[y]   ; Multiply most significant bytes
    movw %A[ret],r0     ; Move the result in ret

    ; ret += (LSB(x) * MSB(y)) >> 8
    mul %A[x],%B[y]     ; Multiply LSB of x with MSB of y
    clr r0              ; Clear LSB of the result
    add %A[ret],r1      ; Add MSB of the result to ret
    adc %B[ret],r0

    ; ret += (MSB(x) * LSB(y)) >> 8
    mulsu %B[x],%A[y]   ; Multiply MSB of x with LSB of y
    mov r0,r1           ; Copy MSB of result in LSB place of result
    lsl r1              ; Fill r1 with the sign bit of the result
    sbc r1,r1
    add %A[ret],r0      ; Add signed MSB of the result to ret
    adc %B[ret],r1

    clr r1              ; Clear r1 (__zero_reg__) to match the AVR ABI
)=="
        : [ret] "=&r" (value)
        : [x] "a" (x), [y] "a" (y)
        : "r0", "r1", "cc"
    );
    return value;
}
