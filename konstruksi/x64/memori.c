/*
 * x64/memori.c
 *
 * Pengelolaan memori untuk arsitektur x64 (AMD64).
 * Menyiapkan pemetaan identitas menggunakan paging 4-level:
 *   PML4 (Page Map Level 4) → PDPT (Page Directory Pointer Table)
 *   → PD (Page Directory) → PT (Page Table)
 *
 * Untuk tahap awal, kita menggunakan halaman besar 2 MB
 * pada level PD sehingga hanya perlu PML4 + PDPT + PD.
 * Ini memetakan 1 GB pertama secara identitas.
 *
 * Struktur paging x64:
 *   - PML4: 512 entri × 8 byte = 4 KB
 *   - PDPT: 512 entri × 8 byte = 4 KB
 *   - PD:   512 entri × 8 byte = 4 KB (setiap entri = 2 MB)
 *   - Total pemetaan: 512 × 2 MB = 1 GB
 */

#include "../../lampiran/arsitek.h"
#include "../../lampiran/mesin.h"

/* ================================================================
 * KONSTANTA PAGING x64
 * ================================================================ */

/* Jumlah entri per tabel (512 untuk x64) */
#define HALAMAN_ENTRI_MAKS      512

/* Bit flag entri halaman 64-bit */
#define HALAMAN_ADA             0x001ULL   /* Bit 0: Present */
#define HALAMAN_TULIS           0x002ULL   /* Bit 1: Read/Write */
#define HALAMAN_PENGGUNA        0x004ULL   /* Bit 2: User/Supervisor */
#define HALAMAN_AKSES           0x020ULL   /* Bit 5: Accessed */
#define HALAMAN_BESAR           0x080ULL   /* Bit 7: Page Size (2MB at PD) */
#define HALAMAN_GLOBAL          0x100ULL   /* Bit 8: Global */
#define HALAMAN_NX              0x8000000000000000ULL  /* Bit 63: No Execute */

/* Mask alamat fisik — bit 12-51 */
#define MASK_ALAMAT_FISIK       0x000FFFFFFFFFF000ULL

/* ================================================================
 * VARIABEL GLOBAL
 * ================================================================ */

/* Penanda paging */
static int paging_aktif = SALAH;

/* Bitmap alokasi halaman fisik — mendukung hingga 4 GB */
#define BITMAP_UKURAN  32768   /* 4 GB / 4 KB / 8 bit = 131072 → cukup 128 KB */
static uint8 bitmap_halaman[BITMAP_UKURAN];

static ukuran_t jumlah_halaman_fisik = 0;

/* ================================================================
 * BITMAP HALAMAN
 * ================================================================ */

static void tandai_halaman(ukuran_t indeks)
{
    ukuran_t byte_posisi = indeks / 8;
    uint8 bit_posisi = (uint8)(indeks % 8);
    if (byte_posisi < BITMAP_UKURAN) {
        bitmap_halaman[byte_posisi] |= (uint8)(1 << bit_posisi);
    }
}

static void bebaskan_halaman(ukuran_t indeks)
{
    ukuran_t byte_posisi = indeks / 8;
    uint8 bit_posisi = (uint8)(indeks % 8);
    if (byte_posisi < BITMAP_UKURAN) {
        bitmap_halaman[byte_posisi] &= (uint8)~(1 << bit_posisi);
    }
}

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
 * INISIALISASI PAGING x64
 * ================================================================ */

/*
 * mesin_siapkan_paging — Siapkan pemetaan identitas untuk x64.
 *
 * PML4 di 0x70000, PDPT di 0x71000, PD di 0x72000.
 * Pemicu sudah menyiapkan ini, tapi kita pastikan lagi.
 * Memetakan 1 GB pertama dengan halaman 2 MB.
 */
void mesin_siapkan_paging(void)
{
    uint64 *pml4;
    uint64 *pdpt;
    uint64 *pd;
    ukuran_t i;

    /* Alamat struktur paging (sudah disiapkan oleh pemicu_awal.asm) */
    pml4 = (uint64 *)0x70000;
    pdpt = (uint64 *)0x71000;
    pd   = (uint64 *)0x72000;

    /* Bersihkan PML4 */
    for (i = 0; i < HALAMAN_ENTRI_MAKS; i++) {
        pml4[i] = 0;
    }

    /* Bersihkan PDPT */
    for (i = 0; i < HALAMAN_ENTRI_MAKS; i++) {
        pdpt[i] = 0;
    }

    /* PML4[0] menunjuk ke PDPT */
    pml4[0] = ((uint64)(ukuran_ptr)pdpt & MASK_ALAMAT_FISIK) | HALAMAN_ADA | HALAMAN_TULIS;

    /* PDPT[0] menunjuk ke PD */
    pdpt[0] = ((uint64)(ukuran_ptr)pd & MASK_ALAMAT_FISIK) | HALAMAN_ADA | HALAMAN_TULIS;

    /* Isi PD: 512 entri halaman 2 MB = 1 GB identitas */
    for (i = 0; i < HALAMAN_ENTRI_MAKS; i++) {
        uint64 alamat = i * HALAMAN_2MB;
        pd[i] = (alamat & MASK_ALAMAT_FISIK) | HALAMAN_ADA | HALAMAN_TULIS | HALAMAN_BESAR;
    }

    /* Pastikan CR3 menunjuk ke PML4 */
    __asm__ volatile (
        "mov %0, %%cr3"
        :
        : "r"((ukuran_ptr)pml4)
    );

    /* Hitung jumlah halaman fisik */
    {
        ukuran_t ram_kb = mesin_deteksi_ram();
        jumlah_halaman_fisik = (ram_kb * 1024UL) / HALAMAN_4KB;
    }

    /* Tandai halaman kernel (4 MB pertama) sebagai terpakai */
    {
        ukuran_t halaman_kernel = (4UL * 1024UL * 1024UL) / HALAMAN_4KB;
        for (i = 0; i < halaman_kernel && i < jumlah_halaman_fisik; i++) {
            tandai_halaman(i);
        }
    }

    paging_aktif = BENAR;
}

/* ================================================================
 * ALOKATOR HALAMAN FISIK
 * ================================================================ */

void *penyedia_alokasi_memori(unsigned long long ukuran)
{
    ukuran_t halaman_dibutuhkan;
    ukuran_t halaman_beruntun = 0;
    ukuran_t halaman_mulai = 0;
    ukuran_t i;

    halaman_dibutuhkan = (ukuran_t)((ukuran + HALAMAN_4KB - 1) / HALAMAN_4KB);
    if (halaman_dibutuhkan == 0) {
        halaman_dibutuhkan = 1;
    }

    for (i = 0; i < jumlah_halaman_fisik; i++) {
        if (cek_halaman_bebas(i)) {
            if (halaman_beruntun == 0) {
                halaman_mulai = i;
            }
            halaman_beruntun++;
            if (halaman_beruntun >= halaman_dibutuhkan) {
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

    return NULL;
}

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
