/*
 * cpu.c
 *
 * Pemeriksaan CPU — deteksi informasi prosesor.
 * Pada arsitektur x86/x64, menggunakan instruksi CPUID
 * untuk mendapatkan vendor, nama merek, fitur, dan cache.
 * Pada arsitektur ARM, membaca register MIDR.
 *
 * Informasi yang dikumpulkan:
 *   - Nama pabrikan (vendor string)
 *   - Nama merek prosesor (brand string)
 *   - Kecepatan (estimasi)
 *   - Jumlah inti (core)
 *   - Ukuran cache L1/L2/L3
 *   - Fitur CPU (FPU, SSE, APIC, dll.)
 */

#include "../lampiran/arsitek.h"
#include "../lampiran/mesin.h"

/* ================================================================
 * DETEKSI CPU x86/x64 VIA CPUID
 * ================================================================ */

#if defined(ARSITEK_X86) || defined(ARSITEK_X64)

/*
 * Ekstrak string vendor dari register CPUID.
 * CPUID daun 0 menghasilkan vendor string 12 karakter
 * dalam register EBX, EDX, ECX.
 */
static void ekstrak_vendor_string(char *buffer, uint32 ebx, uint32 edx, uint32 ecx)
{
    uint32 *p = (uint32 *)buffer;
    p[0] = ebx;
    p[1] = edx;
    p[2] = ecx;
    buffer[12] = '\0';
}

/*
 * Ekstrak nama merek (brand string) dari CPUID.
 * CPUID daun 0x80000002, 0x80000003, 0x80000004
 * masing-masing menghasilkan 16 karakter.
 */
static void ekstrak_nama_merek(char *buffer)
{
    uint32 eax, ebx, ecx, edx;
    uint32 *p = (uint32 *)buffer;

    /* Daun 0x80000002 */
    mesin_cpuid(0x80000002, &eax, &ebx, &ecx, &edx);
    p[0] = eax; p[1] = ebx; p[2] = ecx; p[3] = edx;

    /* Daun 0x80000003 */
    mesin_cpuid(0x80000003, &eax, &ebx, &ecx, &edx);
    p[4] = eax; p[5] = ebx; p[6] = ecx; p[7] = edx;

    /* Daun 0x80000004 */
    mesin_cpuid(0x80000004, &eax, &ebx, &ecx, &edx);
    p[8] = eax; p[9] = ebx; p[10] = ecx; p[11] = edx;

    buffer[48] = '\0';
}

/*
 * Perkiraan kecepatan CPU menggunakan metode sederhana.
 * Pada x86, kita bisa menggunakan TSC (Time Stamp Counter)
 * atau PIT untuk mengukur frekuensi.
 */
static uint32 perkiraan_kecepatan_cpu(void)
{
    /* Untuk saat ini, kembalikan estimasi.
     * Metode yang lebih akurat memerlukan timer yang berjalan. */
    return 1000;    /* 1000 MHz = 1 GHz (estimasi konservatif) */
}

#endif /* ARSITEK_X86 || ARSITEK_X64 */

/* ================================================================
 * FUNGSI PUBLIK
 * ================================================================ */

/*
 * peninjau_cek_cpu — Periksa dan identifikasi CPU.
 *
 * Pada x86/x64: menggunakan CPUID untuk mendapatkan
 * vendor string, brand string, fitur, dan informasi cache.
 *
 * Pada ARM: membaca register MIDR (Main ID Register)
 * untuk mendapatkan implementer, variant, dan part number.
 *
 * Parameter:
 *   hasil — pointer ke struktur DataPerangkat untuk menampung hasil
 * Mengembalikan STATUS_OK jika berhasil, STATUS_GAGAL jika gagal.
 */
int peninjau_cek_cpu(DataPerangkat *hasil)
{
    if (hasil == NULL) {
        return STATUS_PARAMETAR_SALAH;
    }

    isi_memori(hasil, 0, sizeof(DataPerangkat));
    hasil->tipe = PERANGKAT_CPU;
    hasil->kondisi = KONDISI_BAIK;
    salin_string(hasil->nama, "Prosesor");

#if defined(ARSITEK_X86) || defined(ARSITEK_X64)
    {
        uint32 eax, ebx, ecx, edx;
        uint32 daun_maks, daun_maks_perluasan;
        DataCPU data_cpu;

        isi_memori(&data_cpu, 0, sizeof(DataCPU));

        /* Cek dukungan CPUID — daun 0 */
        mesin_cpuid(CPUID_VENDOR, &eax, &ebx, &ecx, &edx);
        daun_maks = eax;

        /* Ekstrak vendor string */
        ekstrak_vendor_string(data_cpu.pabrikan, ebx, edx, ecx);
        salin_string(hasil->pabrikan, data_cpu.pabrikan);

        /* Cek daun perluasan (extended) */
        mesin_cpuid(0x80000000, &daun_maks_perluasan, &ebx, &ecx, &edx);

        /* Ekstrak nama merek jika didukung */
        if (daun_maks_perluasan >= 0x80000004) {
            ekstrak_nama_merek(data_cpu.nama);
            salin_string(hasil->nama, data_cpu.nama);
        }

        /* Baca fitur CPU — daun 1 */
        if (daun_maks >= 1) {
            mesin_cpuid(CPUID_FITUR, &eax, &ebx, &ecx, &edx);

            data_cpu.family   = (eax >> 8) & 0x0F;
            data_cpu.model    = (eax >> 4) & 0x0F;
            data_cpu.stepping = eax & 0x0F;

            /* Extended family dan model */
            if (data_cpu.family == 0x0F) {
                data_cpu.family += (eax >> 20) & 0xFF;
                data_cpu.model  += ((eax >> 16) & 0x0F) << 4;
            }

            data_cpu.fitur_ecx = ecx;
            data_cpu.fitur_edx = edx;

            /* Simpan informasi fitur ke data_khusus */
            salin_memori(hasil->data_khusus, &data_cpu, MIN(sizeof(DataCPU), DATA_KHUSUS_PANJANG));
        }

        /* Perkiraan kecepatan CPU */
        data_cpu.kecepatan_mhz = perkiraan_kecepatan_cpu();

        /* Informasi cache — daun 2 (jika didukung) */
        if (daun_maks >= 2) {
            mesin_cpuid(2, &eax, &ebx, &ecx, &edx);
            /* Parse cache descriptors — implementasi sederhana */
            data_cpu.cache_l1 = 32;     /* Estimasi 32 KB */
            data_cpu.cache_l2 = 256;    /* Estimasi 256 KB */
            data_cpu.cache_l3 = 8192;   /* Estimasi 8 MB */
        }

        /* Jumlah inti — daun 0x80000008 (AMD) atau daun 4 (Intel) */
        data_cpu.jumlah_inti = 1;   /* Default: 1 inti */

        notulen_catat(NOTULEN_INFO, "Peninjau: CPU terdeteksi");
    }

#elif defined(ARSITEK_ARM32) || defined(ARSITEK_ARM64)
    {
        uint32 midr;

        /* Baca MIDR (Main ID Register) */
#if defined(ARSITEK_ARM32)
        __asm__ volatile ("mrc p15, 0, %0, c0, c0, 0" : "=r"(midr));
#else
        uint64 midr64;
        __asm__ volatile ("mrs %0, midr_el1" : "=r"(midr64));
        midr = (uint32)midr64;
#endif

        /* Ekstrak informasi dari MIDR */
        {
            uint8 implementer = (uint8)((midr >> 24) & 0xFF);
            uint16 variant    = (uint16)((midr >> 20) & 0x0F);
            uint16 part       = (uint16)((midr >> 4) & 0xFFF);
            uint8 revisi      = (uint8)(midr & 0x0F);

            /* Identifikasi vendor */
            switch (implementer) {
            case 0x41: salin_string(hasil->pabrikan, "ARM"); break;
            case 0x42: salin_string(hasil->pabrikan, "Broadcom"); break;
            case 0x43: salin_string(hasil->pabrikan, "Cavium"); break;
            case 0x44: salin_string(hasil->pabrikan, "DEC"); break;
            case 0x51: salin_string(hasil->pabrikan, "Qualcomm"); break;
            case 0x56: salin_string(hasil->pabrikan, "Marvell"); break;
            case 0x69: salin_string(hasil->pabrikan, "Intel"); break;
            default:   salin_string(hasil->pabrikan, "Tidak Dikenal"); break;
            }

            /* Simpan ke data_khusus */
            hasil->data_khusus[0] = implementer;
            hasil->data_khusus[1] = (uint8)variant;
            hasil->data_khusus[2] = (uint8)(part >> 8);
            hasil->data_khusus[3] = (uint8)(part & 0xFF);
            hasil->data_khusus[4] = revisi;
        }

        notulen_catat(NOTULEN_INFO, "Peninjau: CPU ARM terdeteksi");
    }
#endif

    return STATUS_OK;
}
