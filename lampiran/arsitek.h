/*
 * arsitek.h
 *
 * Berkas utama kernel Arsitek.
 * Berisi semua definisi tipe data, makro, dan struktur
 * yang digunakan oleh seluruh komponen kernel.
 *
 * Arsitek adalah kernel orisinal yang mendukung empat arsitektur:
 *   - x86 (32-bit IA-32)
 *   - x64 (64-bit AMD64/Intel 64)
 *   - ARM32 (ARMv7)
 *   - ARM64 (ARMv8/AArch64)
 *
 * Alur kernel:
 *   Pemicu → Konstruksi → Peninjau → Notulen → Penyedia
 *   → Pengembang → Pigura → Kontraktor (OS)
 */

#ifndef ARSITEK_H
#define ARSITEK_H

/* ================================================================
 * DETEKSI ARSITEKTUR
 *
 * Prioritas:
 *   1. Definisi eksplisit via -D (ARSITEK_X86, ARSITEK_X64,
 *      ARSITEK_ARM32, ARSITEK_ARM64) — meng-override deteksi compiler
 *   2. Deteksi otomatis dari makro bawaan compiler
 *      (__i386__, __x86_64__, __arm__, __aarch64__)
 * ================================================================ */

#if defined(ARSITEK_X86) && ARSITEK_X86
    /* Sudah didefinisikan secara eksplisit */
    #ifndef ARSITEK_LEBAR
        #define ARSITEK_LEBAR 32
    #endif
    #ifndef ARSITEK_NAMA
        #define ARSITEK_NAMA  "x86"
    #endif
#elif defined(ARSITEK_X64) && ARSITEK_X64
    /* Sudah didefinisikan secara eksplisit */
    #ifndef ARSITEK_LEBAR
        #define ARSITEK_LEBAR 64
    #endif
    #ifndef ARSITEK_NAMA
        #define ARSITEK_NAMA  "x64"
    #endif
#elif defined(ARSITEK_ARM32) && ARSITEK_ARM32
    /* Sudah didefinisikan secara eksplisit */
    #ifndef ARSITEK_LEBAR
        #define ARSITEK_LEBAR 32
    #endif
    #ifndef ARSITEK_NAMA
        #define ARSITEK_NAMA  "arm32"
    #endif
#elif defined(ARSITEK_ARM64) && ARSITEK_ARM64
    /* Sudah didefinisikan secara eksplisit */
    #ifndef ARSITEK_LEBAR
        #define ARSITEK_LEBAR 64
    #endif
    #ifndef ARSITEK_NAMA
        #define ARSITEK_NAMA  "arm64"
    #endif
#elif defined(__i386__) || defined(__i686__) || defined(_M_IX86)
    #define ARSITEK_X86   1
    #define ARSITEK_LEBAR 32
    #define ARSITEK_NAMA  "x86"
#elif defined(__x86_64__) || defined(_M_X64)
    #define ARSITEK_X64   1
    #define ARSITEK_LEBAR 64
    #define ARSITEK_NAMA  "x64"
#elif defined(__arm__) || defined(_M_ARM)
    #define ARSITEK_ARM32 1
    #define ARSITEK_LEBAR 32
    #define ARSITEK_NAMA  "arm32"
#elif defined(__aarch64__) || defined(_M_ARM64)
    #define ARSITEK_ARM64 1
    #define ARSITEK_LEBAR 64
    #define ARSITEK_NAMA  "arm64"
#else
    #error "Arsitektur CPU tidak dikenali oleh kernel Arsitek"
#endif

/* ================================================================
 * TIPE DATA DASAR
 * ================================================================ */

/* Bilangan bulat tanpa tanda */
typedef unsigned char      uint8;
typedef unsigned short     uint16;
typedef unsigned int       uint32;
typedef unsigned long long uint64;

/* Bilangan bulat bertanda */
typedef signed char        int8;
typedef signed short       int16;
typedef signed int         int32;
typedef signed long long   int64;

/* Tipe berukuran pointer — mengikuti lebar arsitektur */
#if ARSITEK_LEBAR == 32
    typedef uint32 ukuran_ptr;
#elif ARSITEK_LEBAR == 64
    typedef uint64 ukuran_ptr;
#endif

/* Tipe ukuran umum (serupa size_t / ssize_t pada POSIX) */
typedef unsigned long ukuran_t;
typedef long          selisih_t;

/* Tipe logika boolean */
typedef int logika;
#define BENAR  1
#define SALAH  0

/* Pointer kosong */
#ifndef NULL
    #define NULL ((void *)0)
#endif

/* ================================================================
 * KONSTANTA KERNEL
 * ================================================================ */

/* Jumlah maksimum perangkat yang dapat dicatat Notulen */
#define PERANGKAT_MAKSIMUM         256

/* Jumlah maksimum entri interupsi */
#define INTERUPSI_MAKSIMUM         256

/* Jumlah maksimum entri GDT */
#define GDT_MAKSIMUM              8

/* Ukuran halaman memori */
#define HALAMAN_4KB    4096UL
#define HALAMAN_2MB    (2UL * 1024UL * 1024UL)
#define HALAMAN_4MB    (4UL * 1024UL * 1024UL)
#define HALAMAN_1GB    (1024UL * 1024UL * 1024UL)

/* Ukuran sektor disk (standar BIOS) */
#define SEKTOR_UKURAN  512

/* Panjang string nama/pabrikan perangkat */
#define NAMA_PANJANG         64
#define PABRIKAN_PANJANG     64
#define BUS_PANJANG          16
#define DATA_KHUSUS_PANJANG  128

/* Batas notulen (log kernel) */
#define NOTULEN_BARIS_MAKSIMUM  1024
#define NOTULEN_PANJANG_BARIS   256

/* Jumlah maksimum entri PCI */
#define PCI_MAKSIMUM            32

/* Jumlah maksimum entri penyimpanan */
#define PENYIMPANAN_MAKSIMUM    16

/* ================================================================
 * KONSTANTA IC (INTEGRATED CIRCUIT) / CHIP
 *
 * Deteksi perangkat keras berdasarkan IC/chip, bukan
 * berdasarkan vendor/merk. Banyak perangkat dari pabrikan
 * berbeda menggunakan IC chip yang sama, sehingga
 * pendeteksian berbasis IC lebih akurat dan memungkinkan
 * pembuatan driver generik per IC.
 * ================================================================ */

/* Panjang nama IC */
#define IC_NAMA_PANJANG         32
#define IC_FABRIKAN_PANJANG     32
#define IC_FUNGSI_PANJANG      64

/* Jumlah maksimum IC yang dikenali */
#define IC_MAKSIMUM            128

/* Tipe fungsi IC */
typedef enum {
    IC_FUNGSI_ETERNET         = 0,   /* IC pengendali Eternet */
    IC_FUNGSI_WIFI            = 1,   /* IC pengendali WiFi */
    IC_FUNGSI_STORAGE_IDE     = 2,   /* IC pengendali IDE/PATA */
    IC_FUNGSI_STORAGE_AHCI    = 3,   /* IC pengendali SATA AHCI */
    IC_FUNGSI_STORAGE_NVME    = 4,   /* IC pengendali NVMe */
    IC_FUNGSI_USB_UHCI        = 5,   /* IC pengendali USB UHCI */
    IC_FUNGSI_USB_OHCI        = 6,   /* IC pengendali USB OHCI */
    IC_FUNGSI_USB_EHCI        = 7,   /* IC pengendali USB EHCI */
    IC_FUNGSI_USB_XHCI        = 8,   /* IC pengendali USB xHCI */
    IC_FUNGSI_TAMPILAN_VGA    = 9,   /* IC pengendali tampilan VGA */
    IC_FUNGSI_TAMPILAN_GPU    = 10,  /* IC pengendali GPU */
    IC_FUNGSI_AUDIO           = 11,  /* IC pengendali audio */
    IC_FUNGSI_UART            = 12,  /* IC UART/serial */
    IC_FUNGSI_DMA             = 13,  /* IC DMA controller */
    IC_FUNGSI_PIC             = 14,  /* IC interrupt controller */
    IC_FUNGSI_PIT             = 15,  /* IC timer */
    IC_FUNGSI_RTC             = 16,  /* IC real-time clock */
    IC_FUNGSI_BRIDGE          = 17,  /* IC bus bridge */
    IC_FUNGSI_BLUETOOTH       = 18,  /* IC Bluetooth */
    IC_FUNGSI_LAIN            = 99   /* IC fungsi lainnya */
} TipeFungsiIC;

/* ================================================================
 * MAKRO UTILITAS
 * ================================================================ */

/* Manipulasi bit */
#define BIT(n)              (1U << (n))
#define BIT_SET(reg, n)     ((reg) |= BIT(n))
#define BIT_HAPUS(reg, n)   ((reg) &= ~BIT(n))
#define BIT_CEK(reg, n)     (((reg) & BIT(n)) != 0)
#define BIT_BALIK(reg, n)   ((reg) ^= BIT(n))

/* Mask bit */
#define MASK_LEBAR(n)       ((1U << (n)) - 1)

/* Perataan (alignment) */
#define RATA_ATAS(n, a)     (((n) + ((a) - 1)) & ~((a) - 1))
#define RATA_BAWAH(n, a)    ((n) & ~((a) - 1))

/* Minimum dan Maksimum */
#define MIN(a, b)           (((a) < (b)) ? (a) : (b))
#define MAKS(a, b)          (((a) > (b)) ? (a) : (b))

/* Ukuran array */
#define UKURAN_ARRAY(arr)   (sizeof(arr) / sizeof((arr)[0]))

/* Penggabung token preprosesor */
#define GABUNG(a, b)        a##b
#define GABUNG3(a, b, c)    a##b##c

/* Penanda parameter yang tidak digunakan */
#define TIDAK_DIGUNAKAN(x)  ((void)(x))

/* Atribut khusus GCC/Clang */
#define BAGIAN(s)           __attribute__((section(s)))
#define SEJAJAR(n)          __attribute__((aligned(n)))
#define DIKEMAS              __attribute__((packed))
#define LEMAH                __attribute__((weak))
#define NORETURN             __attribute__((noreturn))
#define MURNI                __attribute__((pure))
#define SELALU_SEJAJAR      __attribute__((always_inline))

/* Penanda fungsi ekspor untuk tabel kendali */
#define KENDALI_EKSPOR      __attribute__((used))

/* ================================================================
 * ENUMERASI
 * ================================================================ */

/* Kondisi perangkat keras */
typedef enum {
    KONDISI_BAIK            = 0,   /* Perangkat berfungsi baik */
    KONDISI_RUSAK           = 1,   /* Perangkat rusak */
    KONDISI_TIDAK_DIKENAL  = 2,   /* Perangkat tidak dikenali */
    KONDISI_TERPUTUS       = 3,   /* Perangkat terputus */
    KONDISI_SIBUK          = 4,   /* Perangkat sedang sibuk */
    KONDISI_SIAGA          = 5    /* Perangkat dalam mode siaga */
} KondisiPerangkat;

/* Tipe perangkat */
typedef enum {
    PERANGKAT_CPU           = 0,
    PERANGKAT_RAM           = 1,
    PERANGKAT_USB           = 2,
    PERANGKAT_PCI           = 3,
    PERANGKAT_LAYAR         = 4,
    PERANGKAT_PAPAN_KETIK   = 5,
    PERANGKAT_TETIKUS       = 6,
    PERANGKAT_PENYIMPANAN   = 7,
    PERANGKAT_JARINGAN      = 8,
    PERANGKAT_DMA           = 9,
    PERANGKAT_PIC           = 10,  /* Programmable Interrupt Controller */
    PERANGKAT_PIT           = 11,  /* Programmable Interval Timer */
    PERANGKAT_RTC           = 12,  /* Real-Time Clock */
    PERANGKAT_ACPI          = 13,  /* Advanced Configuration & Power Interface */
    PERANGKAT_APIC          = 14,  /* Advanced PIC */
    PERANGKAT_GPU           = 15,  /* Graphics Processing Unit */
    PERANGKAT_SERIAL        = 16,  /* Port serial */
    PERANGKAT_LAIN          = 99
} TipePerangkat;

/* Tipe bus */
typedef enum {
    BUS_PCI    = 0,
    BUS_USB    = 1,
    BUS_I2C    = 2,
    BUS_SPI    = 3,
    BUS_ISA    = 4,
    BUS_SATA   = 5,
    BUS_NVME   = 6,
    BUS_LAIN   = 99
} TipeBus;

/* Tingkat notulen (log level) */
typedef enum {
    NOTULEN_INFO       = 0,   /* Informasi umum */
    NOTULEN_PERINGATAN = 1,   /* Peringatan */
    NOTULEN_KESALAHAN  = 2,   /* Kesalahan */
    NOTULEN_FATAL      = 3    /* Kesalahan fatal — sistem berhenti */
} TingkatNotulen;

/* Status operasi */
typedef enum {
    STATUS_OK             =  0,
    STATUS_GAGAL          = -1,
    STATUS_TIDAK_ADA      = -2,
    STATUS_SIBUK          = -3,
    STATUS_TIDAK_DIKENAL  = -4,
    STATUS_PARAMETAR_SALAH = -5
} StatusOperasi;

/* ================================================================
 * STRUKTUR DATA PERANGKAT
 * ================================================================ */

/* Data perangkat umum — dicatat oleh Notulen */
typedef struct {
    uint32           nomor;                /* Nomor urut perangkat */
    uint32           port;                 /* Nomor port/bus */
    TipePerangkat    tipe;                 /* Jenis perangkat */
    KondisiPerangkat kondisi;              /* Kondisi perangkat saat ini */
    char             nama[NAMA_PANJANG];            /* Nama perangkat */
    char             pabrikan[PABRIKAN_PANJANG];    /* Nama pabrikan */
    ukuran_ptr       alamat;               /* Alamat memori/IO dasar */
    ukuran_t         ukuran;               /* Ukuran memori perangkat */
    uint32           interupsi;            /* Nomor interupsi (IRQ) */
    TipeBus          tipe_bus;             /* Jenis bus koneksi */
    uint16           vendor_id;            /* ID vendor (PCI/USB) */
    uint16           perangkat_id;         /* ID perangkat (PCI/USB) */
    uint8            kelas;                /* Kelas perangkat */
    uint8            subkelas;             /* Sub-kelas perangkat */
    uint8            data_khusus[DATA_KHUSUS_PANJANG]; /* Data spesifik */
} DataPerangkat;

/* Data CPU — hasil pemeriksaan oleh Peninjau */
typedef struct {
    char     pabrikan[PABRIKAN_PANJANG];
    char     nama[NAMA_PANJANG];
    uint32   kecepatan_mhz;
    uint32   family;
    uint32   model;
    uint32   stepping;
    uint32   jumlah_inti;
    uint32   cache_l1;          /* Ukuran cache L1 dalam KB */
    uint32   cache_l2;          /* Ukuran cache L2 dalam KB */
    uint32   cache_l3;          /* Ukuran cache L3 dalam KB */
    uint32   fitur_ecx;         /* CPUID ecx feature flags */
    uint32   fitur_edx;         /* CPUID edx feature flags */
} DataCPU;

/* Data RAM — hasil pemeriksaan oleh Peninjau */
typedef struct {
    ukuran_t total_kb;          /* Total RAM terdeteksi (KB) */
    ukuran_t tersedia_kb;       /* RAM tersedia (KB) */
    ukuran_t terpakai_kb;       /* RAM terpakai (KB) */
    uint32   jumlah_bank;       /* Jumlah bank/modul RAM */
    ukuran_t ukuran_bank[8];    /* Ukuran tiap bank (KB) */
} DataRAM;

/* Entri PCI — satu perangkat pada bus PCI */
typedef struct {
    uint8    bus;               /* Nomor bus */
    uint8    perangkat;         /* Nomor perangkat */
    uint8    fungsi;            /* Nomor fungsi */
    uint16   vendor_id;         /* ID vendor */
    uint16   perangkat_id;      /* ID perangkat */
    uint8    kelas;             /* Kelas perangkat */
    uint8    subkelas;          /* Sub-kelas perangkat */
    uint8    prog_if;           /* Programming interface */
    uint32   bar[6];            /* Base Address Register */
    uint8    irq;               /* Nomor IRQ */
} EntriPCI;

/* Data layar/tampilan */
typedef struct {
    uint32     lebar;            /* Resolusi horizontal (piksel) */
    uint32     tinggi;           /* Resolusi vertikal (piksel) */
    uint32     kedalaman_bit;    /* Kedalaman warna (bit per piksel) */
    ukuran_ptr alamat_framebuffer; /* Alamat linear framebuffer */
    ukuran_t   ukuran_framebuffer; /* Ukuran buffer framebuffer */
    uint8      tipe;             /* 0=Teks VGA, 1=Grafis VBE */
} DataLayar;

/* Data penyimpanan (HDD/SSD/NVMe) */
typedef struct {
    char     nama[NAMA_PANJANG];
    uint64   kapasitas_sektor;   /* Jumlah sektor total */
    uint32   ukuran_sektor;      /* Ukuran sektor dalam byte */
    uint8    tipe;               /* 0=HDD, 1=SSD, 2=NVMe */
    TipeBus  bus;                /* Jenis bus koneksi */
} DataPenyimpanan;

/* Data jaringan */
typedef struct {
    char     nama[NAMA_PANJANG];
    uint8    alamat_mac[6];     /* Alamat MAC */
    uint8    tipe;              /* 0=Eternet, 1=WiFi */
    uint16   kecepatan_mbps;    /* Kecepatan nominal */
    TipeBus  bus;               /* Jenis bus koneksi */
} DataJaringan;

/* Data IC (Integrated Circuit) — identifikasi chip perangkat */
typedef struct {
    uint16        vendor_id;           /* Vendor ID PCI/USB */
    uint16        perangkat_id;        /* Device ID PCI/USB */
    uint8         kelas;               /* Kelas PCI */
    uint8         subkelas;           /* Sub-kelas PCI */
    char          nama_ic[IC_NAMA_PANJANG];       /* Nama IC (mis. "RTL8139") */
    char          fabrikkan[IC_FABRIKAN_PANJANG];  /* Fabrikan IC (mis. "Realtek") */
    char          fungsi[IC_FUNGSI_PANJANG];      /* Deskripsi fungsi IC */
    TipeFungsiIC  tipe_fungsi;        /* Tipe fungsi IC */
    TipePerangkat tipe_perangkat;     /* Tipe perangkat yang menggunakan IC */
    TipeBus       tipe_bus;           /* Jenis bus koneksi IC */
    uint32        clock_mhz;          /* Kecepatan clock IC (MHz) */
    uint32        alamat_basis;        /* Alamat basis register IC */
    uint32        ukuran_register;     /* Ukuran ruang register IC */
    uint32        interupsi_default;   /* Nomor interupsi default IC */
    uint32        parameter_maks[8];   /* Parameter nilai maksimum untuk uji */
    uint32        parameter_optimal[8]; /* Parameter nilai optimal (stabil) */
    uint8         jumlah_parameter;    /* Jumlah parameter yang digunakan */
} DataIC;

/* ================================================================
 * STRUKTUR ARSITEKTUR x86/x64
 * ================================================================ */

#if defined(ARSITEK_X86) || defined(ARSITEK_X64)

/* Entri Global Descriptor Table (GDT) */
typedef struct DIKEMAS {
    uint16 batas_bawah;          /* Bit 0-15 batas segmen */
    uint16 dasar_bawah;          /* Bit 0-15 alamat dasar */
    uint8  dasar_tengah;         /* Bit 16-23 alamat dasar */
    uint8  akses;                /* Byte akses (type, DPL, P) */
    uint8  bendera_batas;        /* Flag + bit 16-19 batas */
    uint8  dasar_atas;           /* Bit 24-31 alamat dasar */
} EntriGDT;

/* Entri Interrupt Descriptor Table (IDT) untuk x86 (32-bit) */
#if defined(ARSITEK_X86)
typedef struct DIKEMAS {
    uint16 batas_bawah;          /* Bit 0-15 alamat handler */
    uint16 dasar_bawah;          /* Bit 0-15 alamat handler */
    uint8  dasar_tengah;         /* Bit 16-23 alamat handler */
    uint8  akses;                /* Byte akses (type, DPL, P) */
    uint8  bendera;              /* Flag + reserved */
    uint8  dasar_atas;           /* Bit 24-31 alamat handler */
} EntriIDT;
#endif

/* Entri IDT untuk x64 (64-bit) — format 16 byte */
#if defined(ARSITEK_X64)
typedef struct DIKEMAS {
    uint16 batas_bawah;          /* Bit 0-15 alamat handler */
    uint16 dasar_bawah;          /* Bit 0-15 alamat handler */
    uint8  dasar_tengah;         /* Bit 16-23 alamat handler */
    uint8  akses;                /* Byte akses */
    uint8  bendera;              /* Flag */
    uint8  dasar_atas;           /* Bit 24-31 alamat handler */
    uint32 dasar_lanjutan;       /* Bit 32-63 alamat handler */
    uint32 dicadangkan;          /* Harus nol */
} EntriIDT;
#endif

/* Register deskriptor (untuk instruksi LGDT/LIDT) */
typedef struct DIKEMAS {
    uint16    batas;             /* Ukuran tabel - 1 */
    ukuran_ptr dasar;            /* Alamat dasar tabel */
} RegisterDeskriptor;

/* Frame stack interupsi — disimpan oleh handler */
typedef struct {
    uint32 gs, fs, es, ds;       /* Segmen yang disimpan */
    uint32 edi, esi, ebp, esp;
    uint32 ebx, edx, ecx, eax;   /* Register umum */
    uint32 nomor_int;            /* Nomor interupsi */
    uint32 kode_kesalahan;       /* Kode kesalahan (jika ada) */
    uint32 eip, cs, eflags;      /* Ringkasan prosesor */
    uint32 esp_pengguna, ss_pengguna; /* Hanya jika privilege change */
} BingkaiInterupsi;

#endif /* ARSITEK_X86 || ARSITEK_X64 */

/* ================================================================
 * STRUKTUR PIGURA (FRAMEBUFFER)
 * ================================================================ */

/* Warna RGBA — 4 byte per piksel */
typedef struct DIKEMAS {
    uint8 merah;
    uint8 hijau;
    uint8 biru;
    uint8 alfa;
} WarnaRGBA;

/* Mode tampilan pigura */
typedef struct {
    uint32     lebar;            /* Lebar layar (piksel) */
    uint32     tinggi;           /* Tinggi layar (piksel) */
    uint32     bit_per_piksel;   /* Kedalaman warna */
    uint32     piksel_per_baris; /* Jarak antar baris (stride) */
    ukuran_ptr alamat;           /* Alamat linear framebuffer */
    ukuran_t   ukuran_buffer;    /* Total ukuran buffer */
} ModePigura;

/* Penyangga pigura — status konsol utama */
typedef struct {
    ModePigura  mode;            /* Mode tampilan aktif */
    WarnaRGBA   warna_latar;     /* Warna latar belakang */
    uint32      kursor_x;        /* Posisi kursor kolom */
    uint32      kursor_y;        /* Posisi kursor baris */
    uint32      font_lebar;      /* Lebar satu karakter font */
    uint32      font_tinggi;     /* Tinggi satu karakter font */
} PenyanggaPigura;

/* ================================================================
 * DEKLARASI FUNGSI KERNEL
 * ================================================================ */

/* --- Titik masuk kernel --- */
void arsitek_mulai(void);

/* --- Konstruksi mesin --- */
void mesin_siapkan(void);

/* --- Peninjau (inspector hardware) --- */
void peninjau_periksa_semua(void);
int  peninjau_periksa_port(unsigned int nomor_port, DataPerangkat *hasil);
int  peninjau_cek_ram(DataPerangkat *hasil);
int  peninjau_cek_usb(unsigned int nomor_port, DataPerangkat *hasil);
int  peninjau_cek_pci(DataPerangkat daftar[], int maks);
int  peninjau_cek_layar(DataPerangkat *hasil);
int  peninjau_cek_papan_ketik(DataPerangkat *hasil);
int  peninjau_cek_tetikus(DataPerangkat *hasil);
int  peninjau_cek_penyimpanan(DataPerangkat daftar[], int maks);
int  peninjau_cek_cpu(DataPerangkat *hasil);
int  peninjau_cek_jaringan(DataPerangkat daftar[], int maks);
int  peninjau_cek_dma(DataPerangkat *hasil);
KondisiPerangkat peninjau_verifikasi(DataPerangkat *perangkat);

/* --- Notulen (pencatat/log) --- */
void notulen_catat(TingkatNotulen tingkat, const char *pesan);
void notulen_catat_perangkat(TingkatNotulen tingkat, const char *pesan,
                             DataPerangkat *perangkat);
void notulen_simpan(DataPerangkat daftar[], int jumlah);
void notulen_tambah(DataPerangkat *perangkat);
void notulen_hapus(unsigned int nomor_port);
int  notulen_baca_semua(DataPerangkat daftar[], int maks);
int  notulen_baca_satu(unsigned int nomor, DataPerangkat *hasil);
int  notulen_hitung(void);
void notulen_perbarui_kondisi(unsigned int nomor, KondisiPerangkat kondisi);
void notulen_tampilkan(void);

/* --- Penyedia (resource provider) --- */
void  penyedia_kumpulkan_semua(DataPerangkat daftar[], int jumlah);
void  penyedia_kumpulkan(DataPerangkat *perangkat, int jumlah);
void  penyedia_sediakan_tipedata(DataPerangkat *perangkat);
int   penyedia_sediakan_bus(DataPerangkat *perangkat, const char *jenis_bus);
int   penyedia_sediakan_alamat(DataPerangkat *perangkat,
                               unsigned long long ukuran);
int   penyedia_sediakan_interupsi(DataPerangkat *perangkat,
                                  unsigned int nomor_interupsi);
void *penyedia_alokasi_memori(unsigned long long ukuran);
void  penyedia_bebaskan_memori(void *alamat, unsigned long long ukuran);

/* --- Pengembang (driver builder) --- */
void pengembang_bangun_semua(DataPerangkat daftar[], int jumlah);
int  pengembang_bangun_koneksi(DataPerangkat *perangkat);
int  pengembang_bangun_kendali(DataPerangkat *perangkat);
int  pengembang_pasang_kendali(unsigned int nomor);
void pengembang_copot_kendali(unsigned int nomor);
void pengembang_bersihkan_kendali(unsigned int nomor_port);
int  pengembang_bangun_antarmuka(DataPerangkat *perangkat);

/* --- Pigura (enhanced framebuffer) --- */
int  pigura_mulai(void);
void pigura_tulis_char(char c);
void pigura_tulis_string(const char *str);
void pigura_tulis_baris(const char *str, ukuran_t panjang);
void pigura_hapus_layar(void);
void pigura_gulir_atas(void);
void pigura_perbarui_kursor(void);
void pigura_set_warna(uint8 depan, uint8 latar);
void pigura_set_posisi(uint32 x, uint32 y);
int  pigura_masuk_grafis(uint32 lebar, uint32 tinggi, uint32 bpp);
void pigura_gambar_piksel(uint32 x, uint32 y, WarnaRGBA warna);
void pigura_gambar_kotak(uint32 x, uint32 y, uint32 lebar, uint32 tinggi,
                         WarnaRGBA warna);
void pigura_gambar_garis(uint32 x1, uint32 y1, uint32 x2, uint32 y2,
                         WarnaRGBA warna);
void pigura_hapus_layar_warna(WarnaRGBA warna);
void pigura_gambar_char(char c, uint32 x, uint32 y,
                        WarnaRGBA depan, WarnaRGBA latar);
void pigura_perbarui(void);
ModePigura   *pigura_dapatkan_mode(void);

/* --- Utilitas memori --- */
void     *salin_memori(void *tujuan, const void *sumber, ukuran_t ukuran);
void     *isi_memori(void *tujuan, int nilai, ukuran_t ukuran);
int       bandingkan_memori(const void *a, const void *b, ukuran_t ukuran);
ukuran_t  panjang_string(const char *str);
char     *salin_string(char *tujuan, const char *sumber);
int       bandingkan_string(const char *a, const char *b);

/* --- Utilitas konversi --- */
char *konversi_uint_ke_string(uint32 nilai, char *buffer, int basis);
char *konversi_int_ke_string(int32 nilai, char *buffer, int basis);

/* --- IC/Chip (deteksi dan identifikasi) --- */
DataIC       *ic_identifikasi(uint16 vendor_id, uint16 perangkat_id, uint8 kelas, uint8 subkelas);
const char   *ic_nama_fungsi(TipeFungsiIC tipe);
int           ic_cocok_perangkat(DataIC *ic, DataPerangkat *perangkat);
void          ic_isi_parameter_optimal(DataIC *ic, DataPerangkat *perangkat);

/* ================================================================
 * INLINE I/O PORT (x86/x64)
 * ================================================================ */

#if defined(ARSITEK_X86) || defined(ARSITEK_X64)

/* Tulis byte ke port I/O */
static __inline__ void tulis_port(uint16 port, uint8 nilai)
{
    __asm__ volatile ("outb %0, %1" : : "a"(nilai), "Nd"(port));
}

/* Baca byte dari port I/O */
static __inline__ uint8 baca_port(uint16 port)
{
    uint8 nilai;
    __asm__ volatile ("inb %1, %0" : "=a"(nilai) : "Nd"(port));
    return nilai;
}

/* Tulis word (16-bit) ke port I/O */
static __inline__ void tulis_port16(uint16 port, uint16 nilai)
{
    __asm__ volatile ("outw %0, %1" : : "a"(nilai), "Nd"(port));
}

/* Baca word (16-bit) dari port I/O */
static __inline__ uint16 baca_port16(uint16 port)
{
    uint16 nilai;
    __asm__ volatile ("inw %1, %0" : "=a"(nilai) : "Nd"(port));
    return nilai;
}

/* Tulis dword (32-bit) ke port I/O */
static __inline__ void tulis_port32(uint16 port, uint32 nilai)
{
    __asm__ volatile ("outl %0, %1" : : "a"(nilai), "Nd"(port));
}

/* Baca dword (32-bit) dari port I/O */
static __inline__ uint32 baca_port32(uint16 port)
{
    uint32 nilai;
    __asm__ volatile ("inl %1, %0" : "=a"(nilai) : "Nd"(port));
    return nilai;
}

/* Tunda I/O singkat (sekitar 1 mikrodetik) */
static __inline__ void tunda_io(void)
{
    tulis_port(0x80, 0);
}

/* Hentikan CPU sampai interupsi berikutnya */
static __inline__ void hentikan_cpu(void)
{
    __asm__ volatile ("hlt");
}

/* Jeda (hint untuk CPU — spin-wait) */
static __inline__ void jeda(void)
{
    __asm__ volatile ("pause");
}

/* Nonaktifkan interupsi */
static __inline__ void nonaktifkan_interupsi(void)
{
    __asm__ volatile ("cli");
}

/* Aktifkan interupsi */
static __inline__ void aktifkan_interupsi(void)
{
    __asm__ volatile ("sti");
}

#endif /* ARSITEK_X86 || ARSITEK_X64 */

/* ================================================================
 * INLINE CPU — ARM32
 * ================================================================ */

#if defined(ARSITEK_ARM32)

/* Nonaktifkan interupsi IRQ dan FIQ pada CPSR */
static __inline__ void nonaktifkan_interupsi(void)
{
    __asm__ volatile (
        "cpsid i\n\t"
        "cpsid f"
        : : : "memory"
    );
}

/* Aktifkan interupsi IRQ dan FIQ pada CPSR */
static __inline__ void aktifkan_interupsi(void)
{
    __asm__ volatile (
        "cpsie f\n\t"
        "cpsie i"
        : : : "memory"
    );
}

/* Hentikan CPU sampai interupsi berikutnya */
static __inline__ void hentikan_cpu(void)
{
    while (1) {
        __asm__ volatile ("wfe");
    }
}

#endif /* ARSITEK_ARM32 */

/* ================================================================
 * INLINE CPU — ARM64
 * ================================================================ */

#if defined(ARSITEK_ARM64)

/* Nonaktifkan semua interupsi (DAIF) */
static __inline__ void nonaktifkan_interupsi(void)
{
    __asm__ volatile ("msr daifset, #0xF" : : : "memory");
}

/* Aktifkan semua interupsi (DAIF) */
static __inline__ void aktifkan_interupsi(void)
{
    __asm__ volatile ("msr daifclr, #0xF" : : : "memory");
}

/* Hentikan CPU sampai interupsi berikutnya */
static __inline__ void hentikan_cpu(void)
{
    while (1) {
        __asm__ volatile ("wfe");
    }
}

#endif /* ARSITEK_ARM64 */

/* ================================================================
 * WARNA STANDAR PIGURA
 * ================================================================ */

#define WARNA_HITAM    ((WarnaRGBA){  0,   0,   0, 255})
#define WARNA_PUTIH    ((WarnaRGBA){255, 255, 255, 255})
#define WARNA_MERAH    ((WarnaRGBA){255,   0,   0, 255})
#define WARNA_HIJAU    ((WarnaRGBA){  0, 255,   0, 255})
#define WARNA_BIRU     ((WarnaRGBA){  0,   0, 255, 255})
#define WARNA_KUNING   ((WarnaRGBA){255, 255,   0, 255})
#define WARNA_CYAN     ((WarnaRGBA){  0, 255, 255, 255})
#define WARNA_MAGENTA  ((WarnaRGBA){255,   0, 255, 255})
#define WARNA_ABU_ABA  ((WarnaRGBA){128, 128, 128, 255})
#define WARNA_COKLAT   ((WarnaRGBA){139,  69,  19, 255})
#define WARNA_BIRU_GELAP ((WarnaRGBA){0,   0, 139, 255})
#define WARNA_HIJAU_GELAP ((WarnaRGBA){0, 100,   0, 255})

/* ================================================================
 * PENANDA KOMPILASI BERSYARAT
 * ================================================================ */

/* Tanda apakah konstruksi sudah diinisialisasi */
extern int konstruksi_siap;

/* Tanda apakah peninjau sudah selesai */
extern int peninjau_siap;

/* Tanda apakah penyedia sudah selesai */
extern int penyedia_siap;

/* Tanda apakah pengembang sudah selesai */
extern int pengembang_siap;

/* Tanda apakah pigura sudah siap */
extern int pigura_siap;

#endif /* ARSITEK_H */
