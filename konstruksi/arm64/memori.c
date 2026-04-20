/*
 * arm64/memori.c
 *
 * Pengelolaan memori untuk arsitektur ARM64 (AArch64).
 * Menyiapkan pemetaan identitas menggunakan paging 4-level:
 *   PGD (Page Global Directory) → PUD (Page Upper Directory)
 *   → PMD (Page Middle Directory) → PTE (Page Table Entry)
 *
 * Struktur paging ARM64 ( dengan 4 KB granule):
 *   - PGD: 512 entri × 8 byte = 4 KB
 *   - PUD: 512 entri × 8 byte = 4 KB
 *   - PMD: 512 entri × 8 byte = 4 KB (mendukung block 2 MB)
 *   - PTE: 512 entri × 8 byte = 4 KB (halaman 4 KB)
 *
 * Untuk tahap awal, kita menggunakan block mapping pada PMD
 * untuk 1 GB pertama, dan fine-grained mapping untuk 32 MB pertama.
 */

#include "../../lampiran/arsitek.h"
#include "../../lampiran/mesin.h"

/* ================================================================
 * FALLBACK DEFINISI ARM64
 *
 * Jika kompilasi dilakukan tanpa flag -DARSITEK_ARM64=1,
 * definisi SCTLR dari mesin.h tidak akan tersedia.
 * Berikan fallback agar kode tetap dapat dikompilasi.
 * ================================================================ */
#ifndef SCTLR_M
#define SCTLR_M     BIT(0)             /* MMU Enable */
#define SCTLR_A     BIT(1)             /* Alignment Check */
#define SCTLR_C     BIT(2)             /* Data Cache Enable */
#define SCTLR_I     BIT(12)            /* Instruction Cache Enable */
#define SCTLR_Z     BIT(11)            /* Branch Prediction */
#endif

/* ================================================================
 * KONSTANTA PAGING ARM64
 * ================================================================ */

#define ENTRI_PER_TABEL         512
#define UKURAN_TABEL            (ENTRI_PER_TABEL * 8)   /* 4 KB */

/* Bit flag deskriptor tabel */
#define VALID                   0x0000000000000001ULL
#define TABLE                   0x0000000000000002ULL   /* Menunjuk ke tabel berikutnya */
#define BLOCK                   0x0000000000000001ULL   /* Block entry (bukan table) */

/* Attribute fields */
#define ATTR_AF                 0x0000000000000100ULL   /* Access Flag */
#define ATTR_nG                 0x0000000000000200ULL   /* Non-Global */
#define ATTR_AP_RW              0x0000000000000000ULL   /* Read/Write (EL1) */
#define ATTR_AP_RO              0x0000000000000080ULL   /* Read-Only */
#define ATTR_AP_USER            0x0000000000000400ULL   /* Accessible dari EL0 */
#define ATTR_SH_INNER           0x0000000000080000ULL   /* Shareability: Inner */
#define ATTR_SH_OUTER           0x0000000000100000ULL   /* Shareability: Outer */

/* MAIR index */
#define ATTR_NORMAL             0x0000000000000000ULL   /* MAIR[0] = Normal */
#define ATTR_DEVICE             0x0000000000000400ULL   /* MAIR[1] = Device */

/* Block flag lengkap */
#define BLOK_NORMAL             (VALID | ATTR_AF | ATTR_AP_RW | ATTR_SH_INNER | ATTR_NORMAL)
#define BLOK_PERANGKAT          (VALID | ATTR_AF | ATTR_AP_RW | ATTR_DEVICE)

/* Mask alamat fisik (bit 12-47 untuk 4 KB granule) */
#define MASK_ALAMAT_FISIK       0x0000FFFFFFFFF000ULL

/* ================================================================
 * INISIALISASI PAGING ARM64
 * ================================================================ */

void mesin_siapkan_paging(void)
{
    uint64 *pgd;
    uint64 *pud;
    uint64 *pmd;
    ukuran_t i;

    /* Alokasikan tabel dari memori statis setelah kernel */
    pgd = (uint64 *)0x080000;       /* 512 KB */
    pud = (uint64 *)0x081000;       /* 516 KB */
    pmd = (uint64 *)0x082000;       /* 520 KB */

    /* Bersihkan tabel */
    for (i = 0; i < ENTRI_PER_TABEL; i++) {
        pgd[i] = 0;
        pud[i] = 0;
        pmd[i] = 0;
    }

    /* PGD[0] → PUD */
    pgd[0] = ((uint64)(ukuran_ptr)pud & MASK_ALAMAT_FISIK) | VALID | TABLE;

    /* PUD[0] → PMD */
    pud[0] = ((uint64)(ukuran_ptr)pmd & MASK_ALAMAT_FISIK) | VALID | TABLE;

    /* PMD: Block mapping 2 MB per entri */
    for (i = 0; i < ENTRI_PER_TABEL; i++) {
        uint64 alamat = (uint64)(i * 0x200000);   /* 2 MB per entri */

        /* Area perangkat I/O pada QEMU virt: 0x08000000 - 0x0FFFFFFF */
        if (alamat >= 0x08000000ULL && alamat < 0x10000000ULL) {
            pmd[i] = (alamat & MASK_ALAMAT_FISIK) | BLOK_PERANGKAT;
        } else {
            pmd[i] = (alamat & MASK_ALAMAT_FISIK) | BLOK_NORMAL;
        }
    }

    /* Konfigurasi MAIR_EL1 (Memory Attribute Indirection Register) */
    {
        uint64 mair = 0x00FFULL;    /* Attr[0] = 0xFF (Normal), Attr[1] = 0x00 (Device) */
        __asm__ volatile ("msr mair_el1, %0" : : "r"(mair));
    }

    /* Konfigurasi TCR_EL1 (Translation Control Register) */
    {
        uint64 tcr;
        tcr  = (16ULL << 0);    /* T0SZ = 16 (48-bit VA) */
        tcr |= (0ULL << 6);     /* Reserved */
        tcr |= (0ULL << 7);     /* EPD0 = 0 (enable TTBR0 walks) */
        tcr |= (3ULL << 8);     /* IRGN0 = 3 (Inner write-back cacheable) */
        tcr |= (3ULL << 10);    /* ORGN0 = 3 (Outer write-back cacheable) */
        tcr |= (3ULL << 12);    /* SH0 = 3 (Inner shareable) */
        tcr |= (0ULL << 14);    /* TG0 = 0 (4 KB granule) */
        tcr |= (16ULL << 16);   /* T1SZ = 16 */
        tcr |= (1ULL << 23);    /* EPD1 = 1 (disable TTBR1 walks) */
        tcr |= (3ULL << 24);    /* IRGN1 */
        tcr |= (3ULL << 26);    /* ORGN1 */
        tcr |= (3ULL << 28);    /* SH1 */
        tcr |= (2ULL << 30);    /* TG1 = 2 (4 KB granule) */
        tcr |= (25ULL << 32);   /* IPS = 25 (36-bit PA, 64 GB) */
        __asm__ volatile ("msr tcr_el1, %0" : : "r"(tcr));
    }

    /* Atur TTBR0_EL1 — alamat PGD */
    {
        uint64 ttbr0 = (uint64)(ukuran_ptr)pgd;
        __asm__ volatile ("msr ttbr0_el1, %0" : : "r"(ttbr0));
    }

    /* Invalidasi TLB */
    __asm__ volatile (
        "tlbi vmalle1is\n\t"
        "dsb ish\n\t"
        "isb"
    );

    /* Aktifkan MMU — set bit M di SCTLR_EL1 */
    {
        uint64 sctlr;
        __asm__ volatile ("mrs %0, sctlr_el1" : "=r"(sctlr));
        sctlr |= SCTLR_M;      /* Aktifkan MMU */
        sctlr |= SCTLR_C;      /* Aktifkan data cache */
        sctlr |= SCTLR_I;      /* Aktifkan instruction cache */
        __asm__ volatile ("msr sctlr_el1, %0" : : "r"(sctlr));
        __asm__ volatile ("isb");
    }
}

/* ================================================================
 * ALOKATOR HALAMAN FISIK
 * ================================================================ */

#define BITMAP_HALAMAN_UKURAN  8192   /* Mendukung hingga 256 MB */
static uint8 bitmap_halaman_arm64[BITMAP_HALAMAN_UKURAN];
static ukuran_t jumlah_halaman_fisik_arm64 = 0;

static void tandai_halaman_arm64(ukuran_t indeks)
{
    ukuran_t byte_posisi = indeks / 8;
    uint8 bit_posisi = (uint8)(indeks % 8);
    if (byte_posisi < BITMAP_HALAMAN_UKURAN) {
        bitmap_halaman_arm64[byte_posisi] |= (uint8)(1 << bit_posisi);
    }
}

static void bebaskan_halaman_arm64(ukuran_t indeks)
{
    ukuran_t byte_posisi = indeks / 8;
    uint8 bit_posisi = (uint8)(indeks % 8);
    if (byte_posisi < BITMAP_HALAMAN_UKURAN) {
        bitmap_halaman_arm64[byte_posisi] &= (uint8)~(1 << bit_posisi);
    }
}

static logika cek_halaman_bebas_arm64(ukuran_t indeks)
{
    ukuran_t byte_posisi = indeks / 8;
    uint8 bit_posisi = (uint8)(indeks % 8);
    if (byte_posisi < BITMAP_HALAMAN_UKURAN) {
        return (bitmap_halaman_arm64[byte_posisi] & (1 << bit_posisi)) ? SALAH : BENAR;
    }
    return SALAH;
}

void *penyedia_alokasi_memori(unsigned long long ukuran)
{
    ukuran_t halaman_dibutuhkan;
    ukuran_t halaman_beruntun = 0;
    ukuran_t halaman_mulai = 0;
    ukuran_t i;

    if (jumlah_halaman_fisik_arm64 == 0) {
        jumlah_halaman_fisik_arm64 = 65536;  /* Asumsi 256 MB */
    }

    halaman_dibutuhkan = (ukuran_t)((ukuran + HALAMAN_4KB - 1) / HALAMAN_4KB);
    if (halaman_dibutuhkan == 0) {
        halaman_dibutuhkan = 1;
    }

    for (i = 0; i < jumlah_halaman_fisik_arm64; i++) {
        if (cek_halaman_bebas_arm64(i)) {
            if (halaman_beruntun == 0) {
                halaman_mulai = i;
            }
            halaman_beruntun++;
            if (halaman_beruntun >= halaman_dibutuhkan) {
                ukuran_t j;
                for (j = halaman_mulai; j < halaman_mulai + halaman_dibutuhkan; j++) {
                    tandai_halaman_arm64(j);
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
        if (i < jumlah_halaman_fisik_arm64) {
            bebaskan_halaman_arm64(i);
        }
    }
}
