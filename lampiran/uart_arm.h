/*
 * uart_arm.h
 *
 * Pengendali UART PL011 untuk arsitektur ARM.
 * Menyediakan output konsol teks melalui port serial
 * UART pada platform VersatilePB (ARM32) dan QEMU virt
 * (ARM64).
 *
 * PL011 UART adalah standar de facto untuk platform ARM.
 * Register-nya diakses melalui MMIO (memory-mapped I/O).
 *
 * Platform yang didukung:
 *   - ARM32 VersatilePB: UART0 di 0x101F1000
 *   - ARM64 QEMU virt:   UART0 di 0x09000000
 */

#ifndef UART_ARM_H
#define UART_ARM_H

#include "../lampiran/arsitek.h"

#if defined(ARSITEK_ARM32) || defined(ARSITEK_ARM64)

/* ================================================================
 * KONFIGURASI ALAMAT PER PLATFORM
 * ================================================================ */

#if defined(ARSITEK_ARM32)
    /* VersatilePB: UART0 (PL011) di 0x101F1000 */
    #define UART_ALAMAT_DASAR  0x101F1000UL
#elif defined(ARSITEK_ARM64)
    /* QEMU virt: UART0 (PL011) di 0x09000000 */
    #define UART_ALAMAT_DASAR  0x09000000UL
#endif

/* ================================================================
 * OFFSET REGISTER PL011
 * ================================================================ */

#define UART_DR     0x00  /* Data Register */
#define UART_RSR    0x04  /* Receive Status Register */
#define UART_FR     0x18  /* Flag Register */
#define UART_ILPR   0x20  /* IrDA Low-Power Counter */
#define UART_IBRD   0x24  /* Integer Baud Rate */
#define UART_FBRD   0x28  /* Fractional Baud Rate */
#define UART_LCRH   0x2C  /* Line Control Register */
#define UART_CR     0x30  /* Control Register */
#define UART_IFLS   0x34  /* Interrupt FIFO Level */
#define UART_IMSC   0x38  /* Interrupt Mask Set/Clear */
#define UART_RIS    0x3C  /* Raw Interrupt Status */
#define UART_MIS    0x40  /* Masked Interrupt Status */
#define UART_ICR    0x44  /* Interrupt Clear Register */
#define UART_DMACR  0x48  /* DMA Control Register */

/* Bit dalam Flag Register (UART_FR) */
#define UART_FR_TXFF  0x20  /* Transmit FIFO full */
#define UART_FR_RXFE  0x10  /* Receive FIFO empty */
#define UART_FR_BUSY  0x08  /* UART busy */

/* Bit dalam Control Register (UART_CR) */
#define UART_CR_UARTEN  0x01  /* UART enable */
#define UART_CR_TXE     0x08  /* Transmit enable */
#define UART_CR_RXE     0x04  /* Receive enable */

/* Bit dalam Line Control Register (UART_LCRH) */
#define UART_LCRH_FEN   0x10  /* FIFO enable */
#define UART_LCRH_WLEN8 0x60  /* 8-bit word */

/* ================================================================
 * FUNGSI UART
 * ================================================================ */

/*
 * uart_arm_siapkan - Inisialisasi UART PL011.
 * Mengkonfigurasi baud rate 115200, 8N1, tanpa parity.
 * Dipanggil sekali saat pigura_mulai() pada platform ARM.
 */
void uart_arm_siapkan(void);

/*
 * uart_arm_tulis_char - Kirim satu karakter via UART.
 * Menunggu sampai Transmit FIFO tidak penuh
 * sebelum menulis ke Data Register.
 */
void uart_arm_tulis_char(char c);

/*
 * uart_arm_tulis_string - Kirim string via UART.
 * Menangani newline (\n) dengan mengirim \r\n.
 */
void uart_arm_tulis_string(const char *str);

/*
 * uart_arm_baca_char - Baca satu karakter dari UART.
 * Menunggu sampai Receive FIFO tidak kosong.
 * Mengembalikan karakter yang dibaca.
 */
char uart_arm_baca_char(void);

/*
 * uart_arm_bisa_baca - Periksa apakah ada data
 * tersedia untuk dibaca dari UART.
 */
logika uart_arm_bisa_baca(void);

#endif /* ARSITEK_ARM32 || ARSITEK_ARM64 */

#endif /* UART_ARM_H */
