/*
 * arm32/memori.c
 *
 * Pengelolaan memori untuk arsitektur ARM32 (ARMv7).
 * Menyiapkan section mapping (1 MB per seksi) menggunakan
 * MMU ARM32 dengan Tabel Halaman Tingkat 1 (First-Level).
 *
 * Struktur paging ARM32:
 *   - Tabel Tingkat 1 (First-Level Table): 4096 entri, masing-masing
 *     memetakan 1 MB section. Total: 4 GB ruang alamat.
 *   - Untuk tahap awal, kita menggunakan section mapping
 *     tanpa tabel tingkat 2 (coarse page table).
 *
 * Register MMU ARM32:
 *   - TTBR0: Alamat dasar tabel terjemahan
 *   - TTBCR: Konfigurasi terjemahan
 *   - DACR:  Domain Access Control Register
 *   - SCTLR: System Control Register (mengaktifkan MMU)
 */

#include "../../lampiran/arsitek.h"
#include "../../lampiran/mesin.h"

/* ================================================================
 * KONSTANTA PAGING ARM32
 *
 * Konstanta SCTLR_M, SCTLR_A, SCTLR_C, SCTLR_I, dan SCTLR_Z
 * sudah didefinisikan di mesin.h yang di-include di atas.
 * Tidak perlu fallback #ifndef lagi.
 * ================================================================ */

/* Jumlah entri tabel tingkat 1 */
#define TABEL1_ENTRI_MAKS       4096

/* Ukuran section (1 MB) */
#define SEKSI_UKURAN            0x00100000UL

/* Bit flag deskriptor section */
#define SEKSI_ADA               0x00000002UL   /* Bit 1: Section valid */
#define SEKSI_BUFFER            0x00000004UL   /* Bit 2: Bufferable */
#define SEKSI_CACHE             0x00000008UL   /* Bit 3: Cacheable */
#define SEKSI_AP_KELOLA         0x00000400UL   /* Bit 10: AP = 01 (kernel RW) */
#define SEKSI_AP_PENUH          0x00000C00UL   /* Bit 10-11: AP = 11 (full access) */
#define SEKSI_DOMAIN_0          0x00000000UL   /* Domain 0 */
#define SEKSI_NORMAL            (SEKSI_ADA | SEKSI_BUFFER | SEKSI_CACHE | SEKSI_AP_PENUH)
#define SEKSI_PERANGKAT         (SEKSI_ADA | SEKSI_AP_PENUH)   /* Non-cacheable untuk I/O */

/* Alamat penempatan tabel halaman */
#define TABEL_HALAMAN_ALAMAT    0x04000000UL   /* 64 MB — setelah kernel */

/* ================================================================
 * INISIALISASI PAGING ARM32
 * ================================================================ */

/*
 * mesin_siapkan_paging — Siapkan section mapping untuk ARM32.
 * Memetakan 1 GB pertama secara identitas:
 *   - Bagian RAM (0x00000000 - 0x3FFFFFFF): cacheable, bufferable
 *   - Bagian perangkat (0x10000000 - 0x1FFFFFFF): non-cacheable
 */
void mesin_siapkan_paging(void)
{
    uint32 *tabel;
    ukuran_t i;

    tabel = (uint32 *)TABEL_HALAMAN_ALAMAT;

    /* Bersihkan seluruh tabel (4096 entri × 4 byte = 16 KB) */
    for (i = 0; i < TABEL1_ENTRI_MAKS; i++) {
        tabel[i] = 0;
    }

    /* Isi section mapping — identitas */
    for (i = 0; i < TABEL1_ENTRI_MAKS; i++) {
        uint32 alamat_fisik = (uint32)(i * SEKSI_UKURAN);
        uint32 entri;

        /* Area perangkat I/O pada VersatilePB: 0x10000000 - 0x1FFFFFFF */
        if (alamat_fisik >= 0x10000000UL && alamat_fisik < 0x20000000UL) {
            entri = alamat_fisik | SEKSI_PERANGKAT;
        } else {
            entri = alamat_fisik | SEKSI_NORMAL;
        }

        tabel[i] = entri;
    }

    /* Atur Domain Access Control — Domain 0: manajer (full access) */
    {
        uint32 dacr = 0x01;     /* Domain 0 = manajer */
        __asm__ volatile ("mcr p15, 0, %0, c3, c0, 0" : : "r"(dacr));
    }

    /* Atur TTBR0 — alamat dasar tabel terjemahan */
    {
        uint32 ttbr0 = (uint32)(ukuran_ptr)tabel;
        __asm__ volatile ("mcr p15, 0, %0, c2, c0, 0" : : "r"(ttbr0));
    }

    /* Aktifkan MMU — set bit M di SCTLR */
    {
        uint32 sctlr;
        __asm__ volatile ("mrc p15, 0, %0, c1, c0, 0" : "=r"(sctlr));
        sctlr |= SCTLR_M;     /* Aktifkan MMU */
        sctlr |= SCTLR_C;     /* Aktifkan data cache */
        sctlr |= SCTLR_I;     /* Aktifkan instruction cache */
        sctlr |= SCTLR_Z;     /* Aktifkan branch prediction */
        __asm__ volatile ("mcr p15, 0, %0, c1, c0, 0" : : "r"(sctlr));
    }
}

/* ================================================================
 * ALOKATOR HALAMAN FISIK
 * ================================================================ */

/* Bitmap alokasi seksi (1 MB per seksi) */
#define BITMAP_SEKSI_UKURAN  128   /* Mendukung hingga 128 seksi = 128 MB */
static uint8 bitmap_seksi[BITMAP_SEKSI_UKURAN];
static ukuran_t jumlah_seksi_fisik = 0;

static void tandai_seksi(ukuran_t indeks)
{
    ukuran_t byte_posisi = indeks / 8;
    uint8 bit_posisi = (uint8)(indeks % 8);
    if (byte_posisi < BITMAP_SEKSI_UKURAN) {
        bitmap_seksi[byte_posisi] |= (uint8)(1 << bit_posisi);
    }
}

static void bebaskan_seksi(ukuran_t indeks)
{
    ukuran_t byte_posisi = indeks / 8;
    uint8 bit_posisi = (uint8)(indeks % 8);
    if (byte_posisi < BITMAP_SEKSI_UKURAN) {
        bitmap_seksi[byte_posisi] &= (uint8)~(1 << bit_posisi);
    }
}

static logika cek_seksi_bebas(ukuran_t indeks)
{
    ukuran_t byte_posisi = indeks / 8;
    uint8 bit_posisi = (uint8)(indeks % 8);
    if (byte_posisi < BITMAP_SEKSI_UKURAN) {
        return (bitmap_seksi[byte_posisi] & (1 << bit_posisi)) ? SALAH : BENAR;
    }
    return SALAH;
}

void *penyedia_alokasi_memori(unsigned long long ukuran)
{
    ukuran_t seksi_dibutuhkan;
    ukuran_t seksi_beruntun = 0;
    ukuran_t seksi_mulai = 0;
    ukuran_t i;

    /* Asumsi 128 MB RAM jika belum terdeteksi */
    if (jumlah_seksi_fisik == 0) {
        jumlah_seksi_fisik = 128;
    }

    seksi_dibutuhkan = (ukuran_t)((ukuran + SEKSI_UKURAN - 1) / SEKSI_UKURAN);
    if (seksi_dibutuhkan == 0) {
        seksi_dibutuhkan = 1;
    }

    for (i = 0; i < jumlah_seksi_fisik; i++) {
        if (cek_seksi_bebas(i)) {
            if (seksi_beruntun == 0) {
                seksi_mulai = i;
            }
            seksi_beruntun++;
            if (seksi_beruntun >= seksi_dibutuhkan) {
                ukuran_t j;
                for (j = seksi_mulai; j < seksi_mulai + seksi_dibutuhkan; j++) {
                    tandai_seksi(j);
                }
                return (void *)(ukuran_ptr)(seksi_mulai * SEKSI_UKURAN);
            }
        } else {
            seksi_beruntun = 0;
        }
    }

    return NULL;
}

void penyedia_bebaskan_memori(void *alamat, unsigned long long ukuran)
{
    ukuran_t seksi_mulai;
    ukuran_t seksi_jumlah;
    ukuran_t i;

    if (alamat == NULL) {
        return;
    }

    seksi_mulai = (ukuran_t)((ukuran_ptr)alamat / SEKSI_UKURAN);
    seksi_jumlah = (ukuran_t)((ukuran + SEKSI_UKURAN - 1) / SEKSI_UKURAN);

    for (i = seksi_mulai; i < seksi_mulai + seksi_jumlah; i++) {
        if (i < jumlah_seksi_fisik) {
            bebaskan_seksi(i);
        }
    }
}
