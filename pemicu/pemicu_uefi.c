/*
 * pemicu_uefi.c
 *
 * Titik masuk kernel untuk boot UEFI pada x86/x64.
 * Berbeda dengan boot BIOS legacy yang menggunakan
 * interrupt 0x10/0x13, UEFI menyediakan layanan
 * melalui tabel EFI_SYSTEM_TABLE yang diteruskan
 * oleh firmware ke titik masuk aplikasi EFI.
 *
 * UEFI boot memberikan keuntungan:
 *   1. CPU sudah dalam mode terlindung/long mode
 *      saat kernel dimuat (tidak perlu transisi
 *      dari mode real)
 *   2. Identitas pemetaan memori (identity mapping)
 *      sudah aktif dengan paging yang disiapkan
 *      oleh firmware
 *   3. Akses ke GOP (Graphics Output Protocol)
 *      untuk mode grafis tanpa VBE BIOS
 *   4. Dukungan GPT (GUID Partition Table)
 *      untuk disk besar (> 2 TB)
 *   5. Layanan EFI Boot Services tersedia
 *      untuk deteksi perangkat sebelum kernel
 *      mengambil alih kendali
 *
 * Alur boot UEFI:
 *   1. Firmware UEFI memuat file EFI dari
 *      partisi EFI System Partition (ESP)
 *   2. Firmware memanggil EfiMain() dengan
 *      ImageHandle dan SystemTable
 *   3. Kernel mengakses informasi memori,
 *      framebuffer, dan perangkat dari
 *      SystemTable
 *   4. Kernel memanggil ExitBootServices()
 *      untuk mengambil kendali penuh
 *   5. Kernel melanjutkan inisialisasi
 *      normal via arsitek_mulai()
 */

#include "../lampiran/arsitek.h"
#include "../lampiran/mesin.h"

/* ================================================================
 * TIPE DATA UEFI
 *
 * Definisi tipe dasar UEFI sesuai spesifikasi
 * UEFI 2.9. Semua tipe menggunakan konvensi
 * penamaan bahasa Indonesia.
 * ================================================================ */

/* Tipe dasar UEFI */
typedef unsigned long long efi_status;
typedef void *efi_handle;
typedef unsigned long long efi_uintn;

/* Konstanta status UEFI */
#define EFI_BERHASIL             0
#define EFI_GAGAL_MEMUAT        2
#define EFI_TIDAK_ADA           14
#define EFI_SUDAH_KELUAR        17

/* Struktur lokasi framebuffer dari GOP */
typedef struct {
    uint32 versi;            /* Versi framebuffer */
    uint32 lebar;            /* Resolusi horizontal */
    uint32 tinggi;           /* Resolusi vertikal */
    uint32 piksel_per_baris; /* Piksel per baris scan */
    uint32 format_piksel;    /* Format piksel */
    ukuran_ptr alamat;       /* Alamat framebuffer */
    ukuran_t ukuran;         /* Ukuran framebuffer */
} EfiGopModeInfo;

/* Format piksel GOP */
#define EFI_GOP_PIXEL_RGB       0  /* 32-bit RGBX */
#define EFI_GOP_PIXEL_BGR       1  /* 32-bit BGRX */
#define EFI_GOP_PIXEL_BITMASK   2  /* Custom bitmask */
#define EFI_GOP_PIXEL_BLT       3  /* Blt-only */

/* Struktur deskriptor memori UEFI */
typedef struct {
    uint32 tipe;             /* Tipe region memori */
    ukuran_ptr awal;         /* Alamat awal fisik */
    ukuran_ptr akhir;        /* Alamat akhir fisik */
    uint64 atribut;          /* Atribut region */
} EfiDeskriptorMemori;

/* Tipe memori UEFI */
#define EFI_MEMORI_LOADER_CODE     1
#define EFI_MEMORI_LOADER_DATA     2
#define EFI_MEMORI_BOOT_CODE       3
#define EFI_MEMORI_BOOT_DATA       4
#define EFI_MEMORI_RAM             7
#define EFI_MEMORI_RESERVED        0
#define EFI_MEMORI_UNUSABLE        5
#define EFI_MEMORI_ACPI_RECLAIM    9
#define EFI_MEMORI_ACPI_NVS        10
#define EFI_MEMORI_MMIO            11
#define EFI_MEMORI_MMIO_PORT       12
#define EFI_MEMORI_PAL_CODE        13

/* ================================================================
 * VARIABEL GLOBAL UEFI
 * ================================================================ */

/* Informasi framebuffer dari GOP */
static EfiGopModeInfo gop_info;

/* Jumlah total RAM terdeteksi (KB) */
static ukuran_t ram_total_uefi = 0;

/* Penanda apakah boot services sudah keluar */
static int boot_services_keluar = SALAH;

/* ================================================================
 * FUNGSI AKSES GOP
 * ================================================================ */

/*
 * pemicu_uefi_dapatkan_gop — Dapatkan informasi
 * framebuffer dari GOP (Graphics Output Protocol).
 * Dipanggil sebelum ExitBootServices untuk mendapatkan
 * alamat dan ukuran framebuffer.
 *
 * Mengembalikan pointer ke info GOP, atau NULL
 * jika GOP tidak tersedia.
 */
EfiGopModeInfo *pemicu_uefi_dapatkan_gop(void)
{
    return &gop_info;
}

/*
 * pemicu_uefi_dapatkan_ram — Dapatkan total RAM
 * yang terdeteksi dari peta memori UEFI.
 * Harus dipanggil sebelum ExitBootServices.
 */
ukuran_t pemicu_uefi_dapatkan_ram(void)
{
    return ram_total_uefi;
}

/*
 * pemicu_uefi_apakah_tersedia — Periksa apakah
 * kernel di-boot melalui UEFI.
 * Mengembalikan BENAR jika boot UEFI aktif.
 */
int pemicu_uefi_apakah_tersedia(void)
{
    return boot_services_keluar;
}

/* ================================================================
 * TITIK MASUK UEFI
 * ================================================================ */

/*
 * EfiMain — Titik masuk aplikasi UEFI.
 * Dipanggil oleh firmware UEFI setelah memuat
 * kernel dari ESP (EFI System Partition).
 *
 * Parameter:
 *   gambar      — handle gambar EFI
 *   tabel_sistem — pointer ke EFI_SYSTEM_TABLE
 *
 * Catatan: Fungsi ini menggunakan konvensi
 * pemanggilan UEFI (EFIAPI = __attribute__((ms_abi))
 * pada x64). Pada implementasi ini, kita menggunakan
 * tanda tangan standar C karena toolchain mungkin
 * tidak mendukung ms_abi.
 *
 * Alur:
 *   1. Simpan informasi dari SystemTable
 *   2. Dapatkan info framebuffer dari GOP
 *   3. Dapatkan peta memori dari Boot Services
 *   4. Panggil ExitBootServices
 *   5. Setelah keluar, siapkan paging sendiri
 *   6. Panggil arsitek_mulai()
 */
#if defined(ARSITEK_X64)
__attribute__((ms_abi))
#endif
efi_status EfiMain(efi_handle gambar, void *tabel_sistem)
{
    void *tabel_boot;
    ukuran_t ukuran_peta_memori;
    ukuran_t kunci_peta_memori;

    TIDAK_DIGUNAKAN(gambar);
    TIDAK_DIGUNAKAN(tabel_sistem);
    TIDAK_DIGUNAKAN(tabel_boot);
    TIDAK_DIGUNAKAN(ukuran_peta_memori);
    TIDAK_DIGUNAKAN(kunci_peta_memori);

    /* ------------------------------------------------
     * Tahap 1: Akses EFI System Table
     *
     * Pada implementasi penuh, kita akan:
     *   - Mendapatkan EFI_BOOT_SERVICES dari
     *     SystemTable->BootServices
     *   - Memanggil LocateProtocol() untuk
     *     mendapatkan GOP
     *   - Memanggil GetMemoryMap() untuk
     *     mendapatkan peta memori
     *   - Memanggil ExitBootServices()
     *
     * Namun karena kita menggunakan konvensi
     * C standar dan tidak bisa memanggil
     * layanan UEFI secara langsung tanpa
     * ms_abi, kita menyiapkan struktur data
     * yang diperlukan dan mendokumentasikan
     * alur lengkap untuk integrasi di masa
     * depan dengan toolchain yang tepat.
     * ------------------------------------------------ */

    /* ------------------------------------------------
     * Tahap 2: Dapatkan info framebuffer GOP
     *
     * Pada implementasi penuh:
     *   status = BS->LocateProtocol(
     *       &gEfiGraphicsOutputProtocolGuid,
     *       NULL, &gop);
     *   gop_info.lebar = gop->Mode->Info->HorizontalResolution;
     *   gop_info.tinggi = gop->Mode->Info->VerticalResolution;
     *   gop_info.piksel_per_baris =
     *       gop->Mode->Info->PixelsPerScanLine;
     *   gop_info.format_piksel =
     *       gop->Mode->Info->PixelFormat;
     *   gop_info.alamat =
     *       gop->Mode->FrameBufferBase;
     *   gop_info.ukuran =
     *       gop->Mode->FrameBufferSize;
     * ------------------------------------------------ */
    isi_memori(&gop_info, 0, sizeof(EfiGopModeInfo));

    /* ------------------------------------------------
     * Tahap 3: Dapatkan peta memori
     *
     * Pada implementasi penuh:
     *   BS->GetMemoryMap(&ukuran, peta, &kunci,
     *                   &deskriptor_ukuran, &versi);
     *   for (setiap deskriptor) {
     *       if (tipe == EFI_MEMORI_RAM) {
     *           ram_total_uefi += (akhir - awal);
     *       }
     *   }
     * ------------------------------------------------ */
    ram_total_uefi = 0;

    /* ------------------------------------------------
     * Tahap 4: ExitBootServices
     *
     * Pada implementasi penuh:
     *   status = BS->ExitBootServices(gambar,
     *                                 kunci);
     * ------------------------------------------------ */
    boot_services_keluar = BENAR;

    /* ------------------------------------------------
     * Tahap 5: Siapkan lingkungan kernel
     *
     * Setelah ExitBootServices, kita tidak boleh
     * menggunakan layanan firmware lagi. Kita perlu:
     *   1. Set IDT sendiri (bukan IDT firmware)
     *   2. Set GDT sendiri
     *   3. Siapkan paging sendiri (jika perlu
     *      mengubah pemetaan yang disediakan UEFI)
     *   4. Aktifkan interupsi
     * ------------------------------------------------ */

    /* ------------------------------------------------
     * Tahap 6: Panggil kernel
     *
     * Setelah semua siap, panggil titik masuk
     * kernel utama yang sama dengan boot BIOS.
     * ------------------------------------------------ */
    arsitek_mulai();

    /* Tidak boleh kembali */
    return EFI_BERHASIL;
}

/* ================================================================
 * FUNGSI UTILITAS UEFI
 * ================================================================ */

/*
 * pemicu_uefi_siapkan_framebuffer — Siapkan
 * framebuffer berdasarkan informasi GOP yang
 * diperoleh selama boot UEFI.
 *
 * Dipanggil oleh pigura_masuk_grafis() jika
 * boot UEFI terdeteksi, menggantikan VBE BIOS.
 */
int pemicu_uefi_siapkan_framebuffer(uint32 *lebar,
                                      uint32 *tinggi,
                                      uint32 *bpp,
                                      ukuran_ptr *alamat,
                                      ukuran_t *ukuran)
{
    if (lebar == NULL || tinggi == NULL ||
        bpp == NULL || alamat == NULL ||
        ukuran == NULL) {
        return STATUS_PARAMETAR_SALAH;
    }

    if (gop_info.alamat == 0) {
        return STATUS_TIDAK_ADA;
    }

    *lebar = gop_info.lebar;
    *tinggi = gop_info.tinggi;
    *bpp = 32;  /* UEFI GOP selalu 32-bit */
    *alamat = gop_info.alamat;
    *ukuran = gop_info.ukuran;

    return STATUS_OK;
}
