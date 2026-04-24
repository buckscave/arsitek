/*
 * uart_arm.c
 *
 * Implementasi pengendali UART PL011 untuk ARM.
 *
 * PL011 adalah UART yang ditemukan pada hampir semua
 * platform ARM, termasuk VersatilePB, Raspberry Pi,
 * dan QEMU virt. Modul ini menyediakan konsol teks
 * dasar untuk output kernel pada arsitektur ARM.
 *
 * Konfigurasi:
 *   Baud rate: 115200
 *   Data bits: 8
 *   Stop bits: 1
 *   Parity:    none
 *   Flow ctrl: none
 *
 * Perhitungan baud rate PL011:
 *   Baud rate = UART_CLOCK / (16 * BRD)
 *   BRD = IBRD + FBRD/64
 *
 * Untuk clock 24 MHz dan baud 115200:
 *   BRD = 24000000 / (16 * 115200) = 13.020833
 *   IBRD = 13
 *   FBRD = 0.020833 * 64 = 1.33 -> 1
 *
 * Untuk QEMU virt (clock biasanya sama):
 *   Gunakan nilai yang sama, QEMU akan menyesuaikan.
 */

#include "../lampiran/arsitek.h"
#include "../lampiran/mesin.h"
#include "../lampiran/uart_arm.h"

#if defined(ARSITEK_ARM32) || defined(ARSITEK_ARM64)

/* ================================================================
 * FUNGSI AKSES REGISTER MMIO
 * ================================================================ */

/*
 * baca_reg - Baca register UART 32-bit via MMIO.
 */
static uint32 baca_reg(ukuran_t offset)
{
    volatile uint32 *reg;
    reg = (volatile uint32 *)
        (UART_ALAMAT_DASAR + offset);
    return *reg;
}

/*
 * tulis_reg - Tulis register UART 32-bit via MMIO.
 */
static void tulis_reg(ukuran_t offset, uint32 nilai)
{
    volatile uint32 *reg;
    reg = (volatile uint32 *)
        (UART_ALAMAT_DASAR + offset);
    *reg = nilai;
}

/* ================================================================
 * INISIALISASI UART
 * ================================================================ */

/*
 * uart_arm_siapkan - Inisialisasi UART PL011.
 *
 * Langkah:
 *   1. Nonaktifkan UART terlebih dahulu
 *   2. Nonaktifkan semua interupsi UART
 *   3. Atur baud rate (IBRD + FBRD)
 *   4. Atur format line (8N1, FIFO aktif)
 *   5. Aktifkan transmit dan receive
 *   6. Aktifkan UART
 */
void uart_arm_siapkan(void)
{
    /* 1. Nonaktifkan UART */
    tulis_reg(UART_CR, 0x00000000);

    /* 2. Nonaktifkan semua interupsi */
    tulis_reg(UART_IMSC, 0x00000000);

    /* 3. Atur baud rate 115200 */
    tulis_reg(UART_IBRD, 13);
    tulis_reg(UART_FBRD, 1);

    /* 4. Atur format: 8 data bits, 1 stop bit,
     *    tanpa parity, FIFO aktif */
    tulis_reg(UART_LCRH,
              UART_LCRH_WLEN8 | UART_LCRH_FEN);

    /* 5. Aktifkan transmit dan receive */
    tulis_reg(UART_CR,
              UART_CR_UARTEN | UART_CR_TXE |
              UART_CR_RXE);
}

/* ================================================================
 * OUTPUT UART
 * ================================================================ */

/*
 * uart_arm_tulis_char - Kirim satu karakter via UART.
 *
 * Menunggu sampai Transmit FIFO tidak penuh (bit
 * TXFF dalam Flag Register menjadi 0) sebelum
 * menulis karakter ke Data Register.
 */
void uart_arm_tulis_char(char c)
{
    /* Tunggu sampai Transmit FIFO tidak penuh */
    while (baca_reg(UART_FR) & UART_FR_TXFF) {
        /* Spin-wait */
    }

    /* Tulis karakter ke Data Register */
    tulis_reg(UART_DR, (uint32)(uint8)c);
}

/*
 * uart_arm_tulis_string - Kirim string via UART.
 *
 * Mengirim setiap karakter secara berurutan.
 * Karakter newline (\n) otomatis diikuti oleh
 * carriage return (\r) untuk kompatibilitas
 * dengan terminal serial.
 */
void uart_arm_tulis_string(const char *str)
{
    if (str == NULL) {
        return;
    }

    while (*str) {
        if (*str == '\n') {
            uart_arm_tulis_char('\r');
        }
        uart_arm_tulis_char(*str);
        str++;
    }
}

/* ================================================================
 * INPUT UART
 * ================================================================ */

/*
 * uart_arm_baca_char - Baca satu karakter dari UART.
 *
 * Menunggu sampai Receive FIFO tidak kosong (bit
 * RXFE dalam Flag Register menjadi 0) sebelum
 * membaca karakter dari Data Register.
 */
char uart_arm_baca_char(void)
{
    /* Tunggu sampai Receive FIFO tidak kosong */
    while (baca_reg(UART_FR) & UART_FR_RXFE) {
        /* Spin-wait */
    }

    /* Baca karakter dari Data Register */
    return (char)(uint8)(baca_reg(UART_DR) & 0xFF);
}

/*
 * uart_arm_bisa_baca - Periksa apakah ada data
 * tersedia untuk dibaca.
 *
 * Mengembalikan BENAR jika Receive FIFO tidak kosong,
 * SALAH jika kosong.
 */
logika uart_arm_bisa_baca(void)
{
    return (baca_reg(UART_FR) & UART_FR_RXFE)
        ? SALAH : BENAR;
}

#endif /* ARSITEK_ARM32 || ARSITEK_ARM64 */
