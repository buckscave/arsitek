/*
 * alokator.c
 *
 * Implementasi alokator halaman fisik bersama.
 * Menggunakan bitmap 1-bit-per-halaman untuk melacak
 * status alokasi setiap halaman fisik.
 *
 * Pencarian alokasi menggunakan algoritma first-fit
 * dengan deteksi blok berurutan, yang memberikan
 * keseimbangan antara kecepatan dan fragmentasi.
 *
 * Thread-safety: TIDAK. Kernel Arsitek berjalan
 * single-threaded pada tahap ini. Jika multiprocessing
 * ditambahkan, perlu spinlock per operasi bitmap.
 */

#include "../lampiran/arsitek.h"
#include "../lampiran/alokator.h"

/* ================================================================
 * DATA INTERNAL
 * ================================================================ */

/* Bitmap alokasi — 1 bit per halaman, 0=bebas, 1=terpakai */
static uint8 alokator_bitmap[ALOKATOR_BITMAP_UKURAN];

/* Jumlah halaman fisik total yang dilacak */
static ukuran_t alokator_total_halaman = 0;

/* ================================================================
 * FUNGSI BITMAP INTERNAL
 * ================================================================ */

/*
 * tandai_terpakai - Tandai halaman sebagai terpakai (1).
 * Parameter indeks: nomor halaman (bukan alamat).
 */
static void tandai_terpakai(ukuran_t indeks)
{
    ukuran_t byte_pos = indeks / 8;
    uint8 bit_pos = (uint8)(indeks % 8);

    if (byte_pos < ALOKATOR_BITMAP_UKURAN) {
        alokator_bitmap[byte_pos] |=
            (uint8)(1 << bit_pos);
    }
}

/*
 * tandai_bebas - Tandai halaman sebagai bebas (0).
 * Parameter indeks: nomor halaman.
 */
static void tandai_bebas(ukuran_t indeks)
{
    ukuran_t byte_pos = indeks / 8;
    uint8 bit_pos = (uint8)(indeks % 8);

    if (byte_pos < ALOKATOR_BITMAP_UKURAN) {
        alokator_bitmap[byte_pos] &=
            (uint8)~(1 << bit_pos);
    }
}

/*
 * cek_bebas - Periksa apakah halaman bebas.
 * Mengembalikan BENAR jika bebas, SALAH jika terpakai.
 */
static logika cek_bebas(ukuran_t indeks)
{
    ukuran_t byte_pos = indeks / 8;
    uint8 bit_pos = (uint8)(indeks % 8);

    if (byte_pos >= ALOKATOR_BITMAP_UKURAN) {
        return SALAH;
    }

    return (alokator_bitmap[byte_pos] &
            (1 << bit_pos)) ? SALAH : BENAR;
}

/* ================================================================
 * FUNGSI PUBLIK
 * ================================================================ */

/*
 * alokator_siapkan - Inisialisasi alokator halaman.
 *
 * Langkah:
 *   1. Bersihkan seluruh bitmap (semua bebas)
 *   2. Hitung jumlah halaman dari total RAM
 *   3. Tandai halaman kernel sebagai terpakai
 *
 * Parameter:
 *   total_ram_kb - total RAM terdeteksi dalam kilobyte
 */
void alokator_siapkan(ukuran_t total_ram_kb)
{
    ukuran_t i;

    /* Bersihkan seluruh bitmap */
    isi_memori(alokator_bitmap, 0,
               sizeof(alokator_bitmap));

    /* Hitung jumlah halaman fisik */
    alokator_total_halaman =
        (total_ram_kb * 1024UL) /
        ALOKATOR_UKURAN_HALAMAN;

    /* Batasi ke kapasitas bitmap */
    {
        ukuran_t maks_halaman =
            (ukuran_t)ALOKATOR_BITMAP_UKURAN * 8UL;

        if (alokator_total_halaman > maks_halaman) {
            alokator_total_halaman = maks_halaman;
        }
    }

    /* Tandai halaman kernel sebagai terpakai */
    for (i = 0; i < ALOKATOR_HALAMAN_KERNEL &&
         i < alokator_total_halaman; i++) {
        tandai_terpakai(i);
    }
}

/*
 * alokator_alokasi - Alokasi blok halaman berurutan.
 *
 * Algoritma first-fit: mencari blok berurutan pertama
 * yang cukup besar, lalu menandai semua halaman dalam
 * blok tersebut sebagai terpakai.
 *
 * Parameter:
 *   ukuran - jumlah byte yang dibutuhkan
 *
 * Mengembalikan: alamat fisik awal, atau NULL jika
 * tidak ada blok yang cukup besar tersedia.
 */
void *alokator_alokasi(ukuran_t ukuran)
{
    ukuran_t hal_dibutuhkan;
    ukuran_t hal_beruntun = 0;
    ukuran_t hal_mulai = 0;
    ukuran_t i;

    /* Bulatkan ke atas ke kelipatan halaman */
    hal_dibutuhkan =
        (ukuran + ALOKATOR_UKURAN_HALAMAN - 1) /
        ALOKATOR_UKURAN_HALAMAN;

    if (hal_dibutuhkan == 0) {
        hal_dibutuhkan = 1;
    }

    /* Cari blok berurutan yang bebas */
    for (i = 0; i < alokator_total_halaman; i++) {
        if (cek_bebas(i)) {
            if (hal_beruntun == 0) {
                hal_mulai = i;
            }
            hal_beruntun++;
            if (hal_beruntun >= hal_dibutuhkan) {
                ukuran_t j;
                for (j = hal_mulai;
                     j < hal_mulai + hal_dibutuhkan;
                     j++) {
                    tandai_terpakai(j);
                }
                return (void *)(ukuran_ptr)(
                    hal_mulai *
                    ALOKATOR_UKURAN_HALAMAN);
            }
        } else {
            hal_beruntun = 0;
        }
    }

    /* Tidak cukup memori fisik bebas */
    return NULL;
}

/*
 * alokator_bebaskan - Bebaskan blok halaman fisik.
 *
 * Menghitung nomor halaman awal dan jumlah halaman
 * dari alamat dan ukuran, lalu menandai semuanya
 * sebagai bebas dalam bitmap.
 *
 * Parameter:
 *   alamat - alamat awal blok
 *   ukuran - jumlah byte yang dibebaskan
 */
void alokator_bebaskan(void *alamat, ukuran_t ukuran)
{
    ukuran_t hal_mulai;
    ukuran_t hal_jumlah;
    ukuran_t i;

    if (alamat == NULL) {
        return;
    }

    hal_mulai = (ukuran_t)(ukuran_ptr)alamat /
        ALOKATOR_UKURAN_HALAMAN;
    hal_jumlah =
        (ukuran + ALOKATOR_UKURAN_HALAMAN - 1) /
        ALOKATOR_UKURAN_HALAMAN;

    for (i = hal_mulai; i < hal_mulai + hal_jumlah &&
         i < alokator_total_halaman; i++) {
        tandai_bebas(i);
    }
}

/*
 * alokator_jumlah_halaman - Kembalikan jumlah total
 * halaman fisik yang dilacak oleh alokator.
 */
ukuran_t alokator_jumlah_halaman(void)
{
    return alokator_total_halaman;
}
