/*
 * alokator.h
 *
 * Alokator halaman fisik bersama untuk semua arsitektur.
 * Menggunakan bitmap untuk melacak halaman yang terpakai
 * dan bebas, dengan pencarian blok berurutan untuk
 * alokasi multi-halaman.
 *
 * Arsitektur yang didukung:
 *   - x86   (halaman 4 KB, direktori 2 level)
 *   - x64   (halaman 4 KB/2 MB, paging 4 level)
 *   - ARM32 (seksi 1 MB, section mapping)
 *   - ARM64 (halaman 4 KB/2 MB block, paging 4 level)
 *
 * Setiap arsitektur menyediakan konfigurasi melalui
 * makro yang didefinisikan di sini atau di mesin.h.
 */

#ifndef ALOKATOR_H
#define ALOKATOR_H

#include "../lampiran/arsitek.h"

/* ================================================================
 * KONFIGURASI PER ARSITEKTUR
 * ================================================================ */

#if defined(ARSITEK_X86)
    /* x86: halaman 4 KB, mendukung hingga 128 MB */
    #define ALOKATOR_UKURAN_HALAMAN  4096UL
    #define ALOKATOR_BITMAP_UKURAN   4096
    #define ALOKATOR_HALAMAN_KERNEL  512
#elif defined(ARSITEK_X64)
    /* x64: halaman 4 KB, mendukung hingga 4 GB */
    #define ALOKATOR_UKURAN_HALAMAN  4096UL
    #define ALOKATOR_BITMAP_UKURAN   32768
    #define ALOKATOR_HALAMAN_KERNEL  1024
#elif defined(ARSITEK_ARM32)
    /* ARM32: seksi 1 MB, mendukung hingga 128 MB */
    #define ALOKATOR_UKURAN_HALAMAN  0x00100000UL
    #define ALOKATOR_BITMAP_UKURAN   128
    #define ALOKATOR_HALAMAN_KERNEL  64
#elif defined(ARSITEK_ARM64)
    /* ARM64: halaman 4 KB, mendukung hingga 256 MB */
    #define ALOKATOR_UKURAN_HALAMAN  4096UL
    #define ALOKATOR_BITMAP_UKURAN   8192
    #define ALOKATOR_HALAMAN_KERNEL  512
#else
    #error "Arsitektur tidak dikenali untuk alokator"
#endif

/* ================================================================
 * FUNGSI ALOKATOR
 * ================================================================ */

/*
 * alokator_siapkan - Inisialisasi alokator halaman fisik.
 * Menandai halaman kernel sebagai terpakai dan menghitung
 * jumlah halaman fisik total berdasarkan RAM terdeteksi.
 * Dipanggil sekali oleh mesin_siapkan_paging().
 */
void alokator_siapkan(ukuran_t total_ram_kb);

/*
 * alokator_alokasi - Alokasi blok halaman berurutan.
 * Mengembalikan alamat fisik awal blok, atau NULL
 * jika tidak ditemukan blok yang cukup besar.
 *
 * Parameter:
 *   ukuran - jumlah byte yang dibutuhkan (dibulatkan
 *            ke atas ke kelipatan ukuran halaman)
 */
void *alokator_alokasi(ukuran_t ukuran);

/*
 * alokator_bebaskan - Bebaskan blok halaman fisik.
 * Menandai halaman sebagai bebas dalam bitmap.
 *
 * Parameter:
 *   alamat - alamat awal blok yang akan dibebaskan
 *   ukuran - jumlah byte yang dibebaskan
 */
void alokator_bebaskan(void *alamat, ukuran_t ukuran);

/*
 * alokator_jumlah_halaman - Kembalikan jumlah total
 * halaman fisik yang dilacak oleh alokator.
 */
ukuran_t alokator_jumlah_halaman(void);

#endif /* ALOKATOR_H */
