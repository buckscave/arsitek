/*
 * x86/memori.c
 *
 * Pengelolaan memori untuk arsitektur x86 (IA-32).
 * Menyiapkan pemetaan identitas (identity mapping) menggunakan
 * paging 2-level: Direktori Halaman + Tabel Halaman.
 *
 * Struktur paging x86:
 *   - Direktori Halaman (Page Directory): 1024 entri, masing-masing
 *     menunjuk ke satu Tabel Halaman
 *   - Tabel Halaman (Page Table): 1024 entri, masing-masing
 *     memetakan satu halaman 4 KB
 *   - Total pemetaan: 1024 × 1024 × 4 KB = 4 GB
 *
 * Untuk tahap awal, kita hanya memetakan 4 MB pertama
 * (1 direktori + 1 tabel = 1024 halaman × 4 KB).
 */

#include "../../lampiran/arsitek.h"
#include "../../lampiran/mesin.h"

/* ================================================================
 * KONSTANTA PAGING x86
 * ================================================================ */

/* Jumlah entri per tabel/direktori */
#define HALAMAN_ENTRI_MAKS      1024

/* Bit flag entri halaman */
#define HALAMAN_ADA             0x001   /* Bit 0: Halaman ada di memori */
#define HALAMAN_TULIS           0x002   /* Bit 1: Dapat ditulis */
#define HALAMAN_PENGGUNA        0x004   /* Bit 2: Akses ring 3 */
#define HALAMAN_AKSES           0x020   /* Bit 5: Halaman telah diakses */
#define HALAMAN_KOTOR           0x040   /* Bit 6: Halaman telah ditulis */
#define HALAMAN_BESAR           0x080   /* Bit 7: Halaman besar (4MB) */
#define HALAMAN_GLOBAL          0x100   /* Bit 8: Tidak di-flush saat CR3 berubah */

/* Alamat penempatan struktur paging */
#define DIREKTORI_ALAMAT        0x100000   /* 1 MB — setelah kernel */
#define TABEL_HALAMAN_ALAMAT    0x101000   /* 1 MB + 4 KB */

/* Jumlah tabel halaman yang akan dialokasikan awal */
#define TABEL_HALAMAN_AWAL      2          /* 2 tabel = 8 MB terpetakan */

/* ================================================================
 * VARIABEL GLOBAL
 * ================================================================ */

/* Penanda apakah paging sudah aktif */
static int paging_aktif = SALAH;

/* Bitmap alokasi halaman fisik — 1 bit per halaman 4 KB
 * Mendukung hingga 128 MB RAM (32768 halaman = 4096 byte bitmap) */
#define BITMAP_UKURAN  4096
static uint8 bitmap_halaman[BITMAP_UKURAN];

/* Jumlah halaman fisik yang terdeteksi */
static ukuran_t jumlah_halaman_fisik = 0;

/* ================================================================
 * FUNGSI BITMAP HALAMAN
 * ================================================================ */

/*
 * tandai_halaman — Tandai halaman fisik sebagai terpakai.
 * Parameter: indeks halaman (bukan alamat)
 */
static void tandai_halaman(ukuran_t indeks)
{
    ukuran_t byte_posisi = indeks / 8;
    uint8 bit_posisi = (uint8)(indeks % 8);
    if (byte_posisi < BITMAP_UKURAN) {
        bitmap_halaman[byte_posisi] |= (uint8)(1 << bit_posisi);
    }
}

/*
 * bebaskan_halaman — Tandai halaman fisik sebagai bebas.
 * Parameter: indeks halaman
 */
static void bebaskan_halaman(ukuran_t indeks)
{
    ukuran_t byte_posisi = indeks / 8;
    uint8 bit_posisi = (uint8)(indeks % 8);
    if (byte_posisi < BITMAP_UKURAN) {
        bitmap_halaman[byte_posisi] &= (uint8)~(1 << bit_posisi);
    }
}

/*
 * cek_halaman_bebas — Periksa apakah halaman fisik bebas.
 * Mengembalikan BENAR jika bebas, SALAH jika terpakai.
 */
static logika cek_halaman_bebas(ukuran_t indeks)
{
    ukuran_t byte_posisi = indeks / 8;
    uint8 bit_posisi = (uint8)(indeks % 8);
    if (byte_posisi < BITMAP_UKURAN) {
        return (bitmap_halaman[byte_posisi] & (1 << bit_posisi)) ? SALAH : BENAR;
    }
    return SALAH;
}

/* ================================================================
 * IMPLEMENTASI PAGING x86
 * ================================================================ */

/*
 * mesin_siapkan_paging — Siapkan pemetaan identitas untuk x86.
 *
 * Kita memetakan 8 MB pertama secara identitas:
 *   Alamat virtual = Alamat fisik
 *
 * Langkah:
 *   1. Bersihkan direktori halaman (1024 entri = 4 KB)
 *   2. Isi 2 entri direktori pertama → 2 tabel halaman
 *   3. Isi tabel halaman dengan pemetaan identitas
 *   4. Set bit CR4.PSE (opsional, untuk halaman 4MB)
 *   5. Muat alamat direktori ke CR3
 *   6. Set bit CR0.PG untuk mengaktifkan paging
 */
void mesin_siapkan_paging(void)
{
    uint32 *direktori;
    uint32 *tabel;
    ukuran_t i;
    ukuran_t alamat;

    /* Alamat dasar direktori halaman */
    direktori = (uint32 *)DIREKTORI_ALAMAT;

    /* 1. Bersihkan seluruh direktori halaman (1024 entri) */
    for (i = 0; i < HALAMAN_ENTRI_MAKS; i++) {
        direktori[i] = 0;
    }

    /* 2. Siapkan tabel halaman dan isi entri direktori */
    for (i = 0; i < TABEL_HALAMAN_AWAL; i++) {
        /* Alamat tabel halaman ke-i */
        tabel = (uint32 *)(TABEL_HALAMAN_ALAMAT + (i * HALAMAN_4KB));

        /* Entri direktori menunjuk ke tabel halaman */
        direktori[i] = (uint32)(ukuran_ptr)tabel;
        direktori[i] |= HALAMAN_ADA | HALAMAN_TULIS;

        /* 3. Isi tabel halaman dengan pemetaan identitas */
        {
            ukuran_t j;
            alamat = i * HALAMAN_ENTRI_MAKS * HALAMAN_4KB;
            for (j = 0; j < HALAMAN_ENTRI_MAKS; j++) {
                tabel[j] = (uint32)alamat;
                tabel[j] |= HALAMAN_ADA | HALAMAN_TULIS;
                alamat += HALAMAN_4KB;
            }
        }
    }

    /* 4. Muat alamat direktori ke CR3 */
    __asm__ volatile (
        "mov %0, %%cr3"
        :
        : "r"(direktori)
    );

    /* 5. Aktifkan paging (set bit PG di CR0) */
    {
        uint32 cr0;
        __asm__ volatile ("mov %%cr0, %0" : "=r"(cr0));
        cr0 |= 0x80000000;     /* Bit 31: Paging Enable */
        __asm__ volatile ("mov %0, %%cr0" : : "r"(cr0));
    }

    /* 6. Tandai halaman yang digunakan oleh kernel */
    {
        ukuran_t ram_kb;
        ukuran_t halaman_kernel;

        /* Asumsi kernel menggunakan 2 MB pertama */
        halaman_kernel = (2UL * 1024UL * 1024UL) / HALAMAN_4KB;
        for (i = 0; i < halaman_kernel; i++) {
            tandai_halaman(i);
        }

        /* Hitung total halaman fisik dari RAM yang terdeteksi */
        ram_kb = mesin_deteksi_ram();
        jumlah_halaman_fisik = (ram_kb * 1024UL) / HALAMAN_4KB;
    }

    paging_aktif = BENAR;
}

/* ================================================================
 * ALOKATOR HALAMAN FISIK
 * ================================================================ */

/*
 * penyedia_alokasi_memori — Alokasi sejumlah halaman memori fisik.
 * Mengembalikan alamat fisik halaman pertama, atau NULL jika gagal.
 *
 * Parameter:
 *   ukuran — jumlah byte yang dibutuhkan (dibulatkan ke atas ke
 *            kelipatan HALAMAN_4KB)
 */
void *penyedia_alokasi_memori(unsigned long long ukuran)
{
    ukuran_t halaman_dibutuhkan;
    ukuran_t halaman_beruntun = 0;
    ukuran_t halaman_mulai = 0;
    ukuran_t i;

    /* Bulatkan ukuran ke atas ke kelipatan 4 KB */
    halaman_dibutuhkan = (ukuran_t)((ukuran + HALAMAN_4KB - 1) / HALAMAN_4KB);
    if (halaman_dibutuhkan == 0) {
        halaman_dibutuhkan = 1;
    }

    /* Cari run halaman berurutan yang bebas */
    for (i = 0; i < jumlah_halaman_fisik; i++) {
        if (cek_halaman_bebas(i)) {
            if (halaman_beruntun == 0) {
                halaman_mulai = i;
            }
            halaman_beruntun++;
            if (halaman_beruntun >= halaman_dibutuhkan) {
                /* Ditemukan! Tandai semua halaman sebagai terpakai */
                ukuran_t j;
                for (j = halaman_mulai; j < halaman_mulai + halaman_dibutuhkan; j++) {
                    tandai_halaman(j);
                }
                return (void *)(ukuran_ptr)(halaman_mulai * HALAMAN_4KB);
            }
        } else {
            halaman_beruntun = 0;
        }
    }

    /* Tidak cukup memori fisik bebas */
    return NULL;
}

/*
 * penyedia_bebaskan_memori — Bebaskan halaman memori fisik.
 *
 * Parameter:
 *   alamat — alamat awal yang akan dibebaskan
 *   ukuran — jumlah byte yang dibebaskan
 */
void penyedia_bebaskan_memori(void *alamat, unsigned long long ukuran)
{
    ukuran_t halaman_mulai;
    ukuran_t halaman_jumlah;
    ukuran_t i;

    if (alamat == NULL) {
        return;
    }

    halaman_mulai = (ukuran_t)((ukuran_ptr)alamat / HALAMAN_4KB);
    halaman_jumlah = (ukuran_t)((ukuran + HALAMAN_4KB - 1) / HALAMAN_4KB);

    for (i = halaman_mulai; i < halaman_mulai + halaman_jumlah; i++) {
        if (i < jumlah_halaman_fisik) {
            bebaskan_halaman(i);
        }
    }
}
