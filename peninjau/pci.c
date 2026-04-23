/*
 * pci.c
 *
 * Pemeriksaan bus PCI/PCIe — inti dari modul Peninjau.
 * Melakukan enumerasi lengkap bus/device/function pada
 * ruang konfigurasi PCI menggunakan port I/O 0xCF8/0xCFC.
 *
 * Fungsi utama:
 *   peninjau_cek_pci()  — enumerasi seluruh perangkat PCI
 *   peninjau_pci_baca() — baca register konfigurasi PCI
 *   peninjau_pci_tulis()— tulis register konfigurasi PCI
 *
 * Format alamat konfigurasi PCI (32-bit):
 *   Bit 31    : Aktifkan (1)
 *   Bit 24-30 : Nomor bus (0-255)
 *   Bit 16-23 : Nomor perangkat (0-31)
 *   Bit  8-15 : Nomor fungsi (0-7)
 *   Bit  2-7  : Nomor register (dikalikan 4)
 *   Bit  0-1  : Selalu 0 (sejajar 32-bit)
 */

#include "../lampiran/arsitek.h"
#include "../lampiran/mesin.h"
#include "../lampiran/peninjau.h"

/* ================================================================
 * KONSTANTA INTERNAL PCI
 * ================================================================ */

/* Batas enumerasi PCI */
#define PCI_BUS_MAKS        256     /* Jumlah bus maksimum */
#define PCI_PERANGKAT_MAKS  32      /* Perangkat per bus */
#define PCI_FUNGSI_MAKS     8       /* Fungsi per perangkat */

/* Bit header type multi-fungsi */
#define PCI_HEADER_MULTI    0x80

/* Offset tambahan untuk pembacaan PCI */
#define PCI_OFFSET_REVISI       0x08
#define PCI_OFFSET_PROG_IF      0x09
#define PCI_OFFSET_HEADER_TIPE  0x0E
#define PCI_OFFSET_BAR1         0x14
#define PCI_OFFSET_BAR2         0x18
#define PCI_OFFSET_BAR3         0x1C
#define PCI_OFFSET_BAR4         0x20
#define PCI_OFFSET_BAR5         0x24
#define PCI_OFFSET_SUBSYSTEM    0x2C
#define PCI_OFFSET_EXPANSION    0x34
#define PCI_OFFSET_LATENCY      0x0D

/* ================================================================
 * TABEL IC CHIP YANG DIKENALI
 *
 * Deteksi berbasis IC/chip, bukan berdasarkan vendor/brand.
 * ================================================================ */

/*
 * Tabel fabrikkan IC — mengidentifikasi IC chip berdasarkan
 * kombinasi vendor_id dan perangkat_id PCI.
 *
 * Deteksi berbasis IC/chip, bukan berdasarkan vendor/brand.
 * Banyak perangkat dari pabrikan berbeda menggunakan IC yang sama,
 * sehingga identifikasi IC lebih akurat daripada nama vendor.
 */
static const struct {
    uint16 vendor_id;
    uint16 perangkat_id;
    const char *nama_ic;        /* Nama IC (mis. "RTL8139C+") */
    const char *fabrikkan;      /* Fabrikkan IC (mis. "Realtek") */
} tabel_ic_pci[] = {
    /* IC Jaringan */
    { 0x10EC, 0x8139, "RTL8139C+",     "Realtek" },
    { 0x10EC, 0x8169, "RTL8169SC",     "Realtek" },
    { 0x10EC, 0x8125, "RTL8125B",      "Realtek" },
    { 0x8086, 0x10D3, "I82574L",       "Intel" },
    { 0x8086, 0x10EA, "I82577LM",      "Intel" },
    { 0x8086, 0x1502, "I82579V",       "Intel" },
    { 0x1969, 0x1073, "AR8151V2",      "Atheros" },
    { 0x1969, 0x1091, "AR8161",        "Atheros" },
    { 0x14E4, 0x165F, "BCM5720",       "Broadcom" },
    { 0x14E4, 0x168E, "BCM57810",      "Broadcom" },
    /* IC Storage */
    { 0x8086, 0x2922, "ICH9 AHCI",     "Intel" },
    { 0x8086, 0x3A22, "ICH10 AHCI",    "Intel" },
    { 0x8086, 0x1C02, "PCH SATA",      "Intel" },
    { 0x8086, 0x8C02, "Lynx Pt AHCI",  "Intel" },
    { 0x8086, 0x7010, "PIIX3 IDE",     "Intel" },
    { 0x8086, 0x2820, "ICH8 IDE",      "Intel" },
    { 0x8086, 0xF1A5, "PCH NVMe",      "Intel" },
    { 0x144D, 0xA808, "PM981 NVMe",    "Samsung" },
    /* IC USB */
    { 0x8086, 0x7020, "PIIX3 UHCI",    "Intel" },
    { 0x8086, 0x24D2, "ICH5 UHCI",     "Intel" },
    { 0x8086, 0x24DD, "ICH5 EHCI",     "Intel" },
    { 0x8086, 0x293A, "ICH9 EHCI",     "Intel" },
    { 0x8086, 0x1E31, "PP xHCI",       "Intel" },
    { 0x8086, 0x9CB1, "WP xHCI",       "Intel" },
    /* IC Tampilan */
    { 0x1234, 0x1111, "Bochs VBE",     "Bochs" },
    { 0x8086, 0x0046, "HD Graphics",   "Intel" },
    { 0x10DE, 0x0FC6, "GK107 GPU",     "NVIDIA" },
    { 0x1AF4, 0x1050, "VirtIO-GPU",    "Red Hat" },
    /* IC Audio */
    { 0x8086, 0x2415, "ICH2 AC97",     "Intel" },
    { 0x8086, 0x293E, "ICH9 HDA",      "Intel" },
    /* IC Virtual/Emulasi */
    { 0x1AF4, 0x1000, "VirtIO-Net",    "Red Hat" },
    { 0x1AF4, 0x1001, "VirtIO-Blk",    "Red Hat" },
    { 0x1AF4, 0x1003, "VirtIO-Con",    "Red Hat" },
    { 0x15AD, 0x0405, "VMware SVGA",   "VMware" },
    { 0x1B36, 0x000D, "QXL GPU",       "QEMU" },
    /* Penanda akhir */
    { 0x0000, 0x0000, NULL,            NULL }
};

/* ================================================================
 * FUNGSI BANTUAN INTERNAL
 * ================================================================ */

/*
 * Cari IC chip berdasarkan vendor ID dan perangkat ID.
 * Mengembalikan nama IC dan fabrikkan, atau "Tidak Dikenal"
 * jika IC tidak ditemukan dalam tabel.
 *
 * Deteksi berbasis IC/chip: dua perangkat dari vendor berbeda
 * yang menggunakan IC chip yang sama akan terdeteksi sebagai
 * IC yang sama, bukan sebagai perangkat yang berbeda.
 */
static const char *cari_nama_ic(uint16 vendor_id, uint16 perangkat_id,
                                 const char **fabrikkan_hasil)
{
    int i;
    for (i = 0; tabel_ic_pci[i].nama_ic != NULL; i++) {
        if (tabel_ic_pci[i].vendor_id == vendor_id &&
            tabel_ic_pci[i].perangkat_id == perangkat_id) {
            if (fabrikkan_hasil != NULL) {
                *fabrikkan_hasil = tabel_ic_pci[i].fabrikkan;
            }
            return tabel_ic_pci[i].nama_ic;
        }
    }
    if (fabrikkan_hasil != NULL) {
        *fabrikkan_hasil = "Tidak Dikenal";
    }
    return "Tidak Dikenal";
}

/*
 * Buat alamat konfigurasi PCI 32-bit.
 * Parameter:
 *   bus      — nomor bus (0-255)
 *   perangkat — nomor perangkat (0-31)
 *   fungsi   — nomor fungsi (0-7)
 *   offset   — offset register (harus kelipatan 4)
 * Mengembalikan alamat 32-bit untuk port 0xCF8.
 */
#if defined(ARSITEK_X86) || defined(ARSITEK_X64)
static uint32 buat_alamat_pci(uint8 bus, uint8 perangkat,
                               uint8 fungsi, uint8 offset)
{
    return (uint32)(
        PCI_AKTIF |
        ((uint32)bus << 16) |
        ((uint32)perangkat << 11) |
        ((uint32)fungsi << 8) |
        ((uint32)offset & 0xFC)
    );
}
#endif

/* ================================================================
 * FUNGSI PUBLIK — AKSES PCI
 * ================================================================ */

/*
 * Baca register konfigurasi PCI 32-bit.
 * Parameter:
 *   bus      — nomor bus
 *   perangkat — nomor perangkat
 *   fungsi   — nomor fungsi
 *   offset   — offset register (kelipatan 4)
 * Mengembalikan nilai register 32-bit.
 */
uint32 peninjau_pci_baca(uint8 bus, uint8 perangkat,
                          uint8 fungsi, uint8 offset)
{
    uint32 data;

#if defined(ARSITEK_X86) || defined(ARSITEK_X64)
    uint32 alamat;
    alamat = buat_alamat_pci(bus, perangkat, fungsi, offset);
    tulis_port32(PCI_ALAMAT_CONFIG, alamat);
    tunda_io();
    data = baca_port32(PCI_DATA_CONFIG);
#else
    /* Arsitektur non-x86: akses PCI via MMIO atau tidak tersedia */
    (void)bus;
    (void)perangkat;
    (void)fungsi;
    (void)offset;
    data = 0xFFFFFFFF;
#endif

    return data;
}

/*
 * Tulis register konfigurasi PCI 32-bit.
 * Parameter:
 *   bus      — nomor bus
 *   perangkat — nomor perangkat
 *   fungsi   — nomor fungsi
 *   offset   — offset register (kelipatan 4)
 *   nilai    — nilai yang akan ditulis
 */
void peninjau_pci_tulis(uint8 bus, uint8 perangkat,
                         uint8 fungsi, uint8 offset, uint32 nilai)
{
#if defined(ARSITEK_X86) || defined(ARSITEK_X64)
    uint32 alamat;
    alamat = buat_alamat_pci(bus, perangkat, fungsi, offset);
    tulis_port32(PCI_ALAMAT_CONFIG, alamat);
    tunda_io();
    tulis_port32(PCI_DATA_CONFIG, nilai);
#else
    /* Arsitektur non-x86: belum didukung */
    (void)bus;
    (void)perangkat;
    (void)fungsi;
    (void)offset;
    (void)nilai;
#endif
}

/*
 * Baca vendor ID dari perangkat PCI.
 * Mengembalikan 0xFFFF jika slot kosong.
 */
uint16 peninjau_pci_baca_vendor(uint8 bus, uint8 perangkat, uint8 fungsi)
{
    uint32 data;
    data = peninjau_pci_baca(bus, perangkat, fungsi, PCI_OFFSET_VENDOR);
    return (uint16)(data & 0xFFFF);
}

/*
 * Baca perangkat ID dari perangkat PCI.
 */
uint16 peninjau_pci_baca_perangkat(uint8 bus, uint8 perangkat, uint8 fungsi)
{
    uint32 data;
    data = peninjau_pci_baca(bus, perangkat, fungsi, PCI_OFFSET_PERANGKAT);
    return (uint16)(data & 0xFFFF);
}

/*
 * Baca kelas dan subkelas PCI (1 byte masing-masing, dalam 1 word).
 * Byte rendah = subkelas, byte tinggi = kelas.
 */
uint16 peninjau_pci_baca_kelas(uint8 bus, uint8 perangkat, uint8 fungsi)
{
    uint32 data;
    data = peninjau_pci_baca(bus, perangkat, fungsi, PCI_OFFSET_KELAS);
    /* Offset 0x09 berada di byte ketiga (bit 16-23 = kelas, bit 8-15 = subkelas) */
    /* Sebenarnya offset 0x08 berisi: byte[0]=revisi, byte[1]=prog_if,
       byte[2]=subkelas, byte[3]=kelas */
    /* Baca dari offset 0x08 untuk mendapatkan seluruh word kelas */
    data = peninjau_pci_baca(bus, perangkat, fungsi, 0x08);
    return (uint16)((data >> 16) & 0xFFFF);
}

/*
 * Baca nomor IRQ dari perangkat PCI.
 */
uint8 peninjau_pci_baca_irq(uint8 bus, uint8 perangkat, uint8 fungsi)
{
    uint32 data;
    data = peninjau_pci_baca(bus, perangkat, fungsi, PCI_OFFSET_IRQ);
    return (uint8)(data & 0xFF);
}

/*
 * Baca salah satu BAR (Base Address Register) dari perangkat PCI.
 * Parameter nomor_bar: 0-5
 */
uint32 peninjau_pci_baca_bar(uint8 bus, uint8 perangkat,
                              uint8 fungsi, int nomor_bar)
{
    uint8 offset;
    offset = (uint8)(PCI_OFFSET_BAR0 + (nomor_bar * 4));
    return peninjau_pci_baca(bus, perangkat, fungsi, offset);
}

/*
 * Baca tipe header dari perangkat PCI.
 */
uint8 peninjau_pci_baca_header_tipe(uint8 bus, uint8 perangkat, uint8 fungsi)
{
    uint32 data;
    data = peninjau_pci_baca(bus, perangkat, fungsi, PCI_OFFSET_HEADER_TIPE);
    return (uint8)((data >> 16) & 0xFF);
}

/*
 * Baca prog_if dari perangkat PCI.
 */
uint8 peninjau_pci_baca_prog_if(uint8 bus, uint8 perangkat, uint8 fungsi)
{
    uint32 data;
    data = peninjau_pci_baca(bus, perangkat, fungsi, PCI_OFFSET_REVISI);
    return (uint8)((data >> 8) & 0xFF);
}

/* ================================================================
 * FUNGSI BANTUAN KONVERSI DATA
 * ================================================================ */

/*
 * Isi struktur DataPerangkat dari informasi PCI yang ditemukan.
 */
static void isi_data_pci(DataPerangkat *hasil, uint8 bus, uint8 perangkat,
                          uint8 fungsi, uint16 vendor_id, uint16 perangkat_id)
{
    uint16 kelas_subkelas;
    uint8 kelas, subkelas, prog_if, irq, header_tipe;
    int i;

    /* Bersihkan struktur terlebih dahulu */
    isi_memori(hasil, 0, sizeof(DataPerangkat));

    /* Isi informasi dasar */
    hasil->tipe      = PERANGKAT_PCI;
    hasil->tipe_bus  = BUS_PCI;

    /* Enkode alamat port sebagai bus/perangkat/fungsi */
    hasil->port      = ((uint32)bus << 16) |
                       ((uint32)perangkat << 8) |
                       ((uint32)fungsi);

    /* ID vendor dan perangkat */
    hasil->vendor_id    = vendor_id;
    hasil->perangkat_id = perangkat_id;

    /* Baca kelas dan subkelas */
    kelas_subkelas = peninjau_pci_baca_kelas(bus, perangkat, fungsi);
    kelas    = (uint8)((kelas_subkelas >> 8) & 0xFF);
    subkelas = (uint8)(kelas_subkelas & 0xFF);
    hasil->kelas    = kelas;
    hasil->subkelas = subkelas;

    /* Baca prog_if — simpan ke data_khusus */
    prog_if = peninjau_pci_baca_prog_if(bus, perangkat, fungsi);
    hasil->data_khusus[24] = prog_if;

    /* Baca IRQ */
    irq = peninjau_pci_baca_irq(bus, perangkat, fungsi);
    hasil->interupsi = (uint32)irq;

    /* Baca BAR0 sebagai alamat dasar */
    hasil->alamat = (ukuran_ptr)peninjau_pci_baca_bar(bus, perangkat, fungsi, 0);

    /* Baca semua BAR ke data_khusus */
    for (i = 0; i < 6; i++) {
        uint32 bar;
        bar = peninjau_pci_baca_bar(bus, perangkat, fungsi, i);
        salin_memori(hasil->data_khusus + (i * 4), &bar, 4);
    }

    /* Baca header tipe — simpan ke data_khusus */
    header_tipe = peninjau_pci_baca_header_tipe(bus, perangkat, fungsi);
    hasil->data_khusus[25] = header_tipe;

    /* Nama IC chip dari tabel IC (bukan nama vendor) */
    {
        const char *fabrikkan_ic = "Tidak Dikenal";
        const char *nama_ic;
        nama_ic = cari_nama_ic(vendor_id, perangkat_id, &fabrikkan_ic);

        /* Isi nama perangkat dengan nama IC chip */
        salin_string(hasil->pabrikan, fabrikkan_ic);

        /* Buat nama deskriptif berdasarkan kelas + nama IC */
        {
            const char *nama_kelas = "Perangkat PCI";
            switch (kelas) {
            case 0x00: nama_kelas = "Perangkat Lama"; break;
            case 0x01: nama_kelas = "Pengendali Penyimpanan"; break;
            case 0x02: nama_kelas = "Pengendali Jaringan"; break;
            case 0x03: nama_kelas = "Pengendali Tampilan"; break;
            case 0x04: nama_kelas = "Pengendali Multimedia"; break;
            case 0x05: nama_kelas = "Pengendali Memori"; break;
            case 0x06: nama_kelas = "Jembatan Bus"; break;
            case 0x07: nama_kelas = "Pengendali Komunikasi"; break;
            case 0x08: nama_kelas = "Periferal Sistem"; break;
            case 0x09: nama_kelas = "Pengendali Input"; break;
            case 0x0A: nama_kelas = "Stasiun Dock"; break;
            case 0x0B: nama_kelas = "Prosesor"; break;
            case 0x0C: nama_kelas = "Pengendali Serial"; break;
            case 0x0D: nama_kelas = "Pengendali Nirkabel"; break;
            case 0x0E: nama_kelas = "Pengendali Intelijen"; break;
            case 0x0F: nama_kelas = "Pengendali Satelit"; break;
            case 0x10: nama_kelas = "Pengendali Enkripsi"; break;
            case 0x11: nama_kelas = "Pengendali Pengambilan Data"; break;
            default:   nama_kelas = "Perangkat PCI Tidak Dikenal"; break;
            }

            /* Jika IC dikenali, gunakan nama IC; jika tidak, nama kelas */
            if (bandingkan_string(nama_ic, "Tidak Dikenal") != 0) {
                salin_string(hasil->nama, nama_ic);
            } else {
                salin_string(hasil->nama, nama_kelas);
            }
        }
    }

    /* Tentukan kondisi perangkat */
    if (vendor_id == PCI_VENDOR_TIDAK_ADA) {
        hasil->kondisi = KONDISI_TIDAK_DIKENAL;
    } else {
        hasil->kondisi = KONDISI_BAIK;
    }
}

/* ================================================================
 * FUNGSI UTAMA — ENUMERASI PCI
 * ================================================================ */

/*
 * Enumerasi seluruh perangkat pada bus PCI.
 * Iterasi melalui semua bus/perangkat/fungsi dan mencatat
 * setiap perangkat yang memiliki vendor ID valid.
 *
 * Parameter:
 *   daftar — array penampung hasil enumerasi
 *   maks   — jumlah maksimum entri yang dapat ditampung
 * Mengembalikan jumlah perangkat yang ditemukan.
 */
int peninjau_cek_pci(DataPerangkat daftar[], int maks)
{
#if defined(ARSITEK_X86) || defined(ARSITEK_X64)
    uint8 bus, perangkat, fungsi;
    uint16 vendor_id;
    uint16 perangkat_id;
    uint8 header_tipe;
    int jumlah;
    uint32 nomor_urut;

    jumlah = 0;
    nomor_urut = 0;

    notulen_catat(NOTULEN_INFO, "Peninjau: Memulai enumerasi bus PCI...");

    /* Iterasi seluruh bus */
    for (bus = 0; bus < PCI_BUS_MAKS && jumlah < maks; bus++) {
        /* Iterasi seluruh perangkat pada bus ini */
        for (perangkat = 0; perangkat < PCI_PERANGKAT_MAKS && jumlah < maks; perangkat++) {
            /* Baca vendor ID fungsi 0 */
            vendor_id = peninjau_pci_baca_vendor(bus, perangkat, 0);

            /* Periksa apakah slot kosong */
            if (vendor_id == PCI_VENDOR_TIDAK_ADA) {
                continue;
            }

            /* Baca header tipe untuk menentukan multi-fungsi */
            header_tipe = peninjau_pci_baca_header_tipe(bus, perangkat, 0);

            /* Iterasi fungsi (0 saja jika bukan multi-fungsi) */
            {
                uint8 fungsi_maks;
                if (header_tipe & PCI_HEADER_MULTI) {
                    fungsi_maks = PCI_FUNGSI_MAKS;
                } else {
                    fungsi_maks = 1;
                }

                for (fungsi = 0; fungsi < fungsi_maks && jumlah < maks; fungsi++) {
                    /* Untuk fungsi > 0, baca ulang vendor ID */
                    if (fungsi > 0) {
                        vendor_id = peninjau_pci_baca_vendor(bus, perangkat, fungsi);
                        if (vendor_id == PCI_VENDOR_TIDAK_ADA) {
                            continue;
                        }
                    }

                    /* Baca perangkat ID */
                    perangkat_id = peninjau_pci_baca_perangkat(bus, perangkat, fungsi);

                    /* Isi struktur DataPerangkat */
                    isi_data_pci(&daftar[jumlah], bus, perangkat, fungsi,
                                 vendor_id, perangkat_id);

                    /* Tetapkan nomor urut */
                    daftar[jumlah].nomor = nomor_urut;
                    nomor_urut++;

                    /* Catat ke notulen */
                    notulen_tambah(&daftar[jumlah]);

                    jumlah++;
                }
            }
        }
    }

    {
        char pesan[80];
        /* Buat pesan ringkas hasil enumerasi */
        salin_string(pesan, "Peninjau: Ditemukan ");
        konversi_uint_ke_string((uint32)jumlah,
                                pesan + panjang_string(pesan), 10);
        salin_string(pesan + panjang_string(pesan), " perangkat PCI");
        notulen_catat(NOTULEN_INFO, pesan);
    }

    return jumlah;

#else
    /* Arsitektur non-x86: PCI enumeration berbeda */
    notulen_catat(NOTULEN_PERINGATAN,
                  "Peninjau: Enumerasi PCI belum didukung pada arsitektur ini");
    (void)daftar;
    (void)maks;
    return 0;
#endif
}
