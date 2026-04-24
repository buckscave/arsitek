/*
 * arm32_divisi.c
 *
 * Implementasi fungsi pembagian ARM32 untuk mode freestanding.
 * Compiler ARM menghasilkan panggilan ke __aeabi_uidivmod dan
 * __aeabi_uidiv untuk operasi pembagian unsigned. Karena kita
 * menggunakan -nostdlib, kita harus menyediakan implementasi
 * sendiri.
 *
 * Fungsi yang diimplementasikan:
 *   __aeabi_uidivmod — pembagian unsigned 32-bit, mengembalikan
 *                       hasil bagi di r0 dan sisa di r1
 *   __aeabi_uidiv    — pembagian unsigned 32-bit, hanya hasil bagi
 *
 * Algoritma: pembagian panjang (long division) bit per bit.
 * Mulai dari bit tertinggi, geser sisa ke kiri, tambahkan
 * bit pembilang, dan kurangi penyebut jika sisa >= penyebut.
 */

/* ================================================================
 * __aeabi_uidivmod — Pembagian unsigned 32-bit dengan sisa.
 *
 * Konvensi ARM EABI: hasil bagi dikembalikan di r0,
 * sisa pembagian di r1. Karena C tidak bisa mengembalikan
 * dua nilai secara langsung, kita menggunakan inline assembly
 * untuk menyetel kedua register sesuai ABI.
 *
 * Parameter:
 *   pembilang  — angka yang dibagi (r0)
 *   penyebut   — angka pembagi (r1)
 *
 * Mengembalikan: hasil bagi di r0, sisa di r1.
 * ================================================================ */
unsigned __aeabi_uidivmod(unsigned pembilang, unsigned penyebut)
{
    unsigned hasil_bagi = 0;
    unsigned sisa = 0;
    unsigned bit;

    if (penyebut == 0) {
        /* Pembagian dengan nol — kembalikan nol untuk kedua nilai */
        __asm__ volatile (
            "mov r0, #0\n\t"
            "mov r1, #0"
            :
            :
            : "r0", "r1"
        );
        return 0;
    }

    /* Algoritma pembagian panjang:
     * Untuk setiap bit dari tertinggi ke terendah:
     *   1. Geser sisa ke kiri satu posisi
     *   2. Ambil bit pembilang yang sesuai
     *   3. Jika sisa >= penyebut, kurangi dan set bit hasil bagi */
    for (bit = 0x80000000U; bit != 0; bit >>= 1) {
        sisa <<= 1;
        if (pembilang & bit) {
            sisa |= 1;
        }
        if (sisa >= penyebut) {
            sisa -= penyebut;
            hasil_bagi |= bit;
        }
    }

    /* Set both r0 (hasil bagi) and r1 (sisa) per ARM EABI */
    __asm__ volatile (
        "mov r0, %0\n\t"
        "mov r1, %1"
        :
        : "r"(hasil_bagi), "r"(sisa)
        : "r0", "r1"
    );

    return hasil_bagi;
}

/* ================================================================
 * __aeabi_uidiv — Pembagian unsigned 32-bit (hanya hasil bagi).
 *
 * Parameter:
 *   pembilang — angka yang dibagi
 *   penyebut  — angka pembagi
 *
 * Mengembalikan: hasil bagi pembagian.
 * ================================================================ */
unsigned __aeabi_uidiv(unsigned pembilang, unsigned penyebut)
{
    unsigned hasil_bagi = 0;
    unsigned sisa = 0;
    unsigned bit;

    if (penyebut == 0) {
        return 0;
    }

    /* Algoritma pembagian panjang yang sama,
     * tetapi kita hanya mengembalikan hasil bagi */
    for (bit = 0x80000000U; bit != 0; bit >>= 1) {
        sisa <<= 1;
        if (pembilang & bit) {
            sisa |= 1;
        }
        if (sisa >= penyebut) {
            sisa -= penyebut;
            hasil_bagi |= bit;
        }
    }

    return hasil_bagi;
}
