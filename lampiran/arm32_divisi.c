/*
 * arm32_divisi.c
 *
 * Implementasi fungsi pembagian ARM32 untuk mode freestanding.
 * Compiler ARM menghasilkan panggilan ke __aeabi_uidivmod untuk
 * operasi pembagian unsigned. Karena -nostdlib, kita harus
 * menyediakan implementasi sendiri.
 */

/* __aeabi_uidivmod — pembagian unsigned 32-bit
 * Mengembalikan hasil bagi di r0, sisa di r1
 * Konvensi pemanggilan ARM ABI */
unsigned __aeabi_uidivmod(unsigned numerator, unsigned denominator)
{
    unsigned quotient = 0;
    unsigned remainder = 0;
    unsigned bit;

    if (denominator == 0) {
        /* Pembagian dengan nol — kembalikan 0 */
        return 0;
    }

    /* Long division algorithm */
    for (bit = 0x80000000; bit != 0; bit >>= 1) {
        remainder <<= 1;
        if (numerator & bit) {
            remainder |= 1;
        }
        if (remainder >= denominator) {
            remainder -= denominator;
            quotient |= bit;
        }
    }

    /* ARM ABI: quotient in r0, remainder in r1
     * We return quotient and set remainder via pointer convention.
     * Actually, __aeabi_uidivmod returns {quotient, remainder} in {r0, r1}.
     * In C, we can't directly return two values in two registers.
     * Use inline assembly to properly set both registers. */
    __asm__ volatile (
        "mov r0, %0\n\t"
        "mov r1, %1\n\t"
        :
        : "r"(quotient), "r"(remainder)
        : "r0", "r1"
    );

    return quotient;
}

/* __aeabi_uidiv — pembagian unsigned 32-bit (hanya hasil bagi) */
unsigned __aeabi_uidiv(unsigned numerator, unsigned denominator)
{
    unsigned quotient = 0;
    unsigned bit;

    if (denominator == 0) {
        return 0;
    }

    for (bit = 0x80000000; bit != 0; bit >>= 1) {
        quotient <<= 1;
        if (numerator >= denominator) {
            numerator -= denominator;
            quotient |= 1;
        }
        denominator >>= 1;
    }

    return quotient;
}
