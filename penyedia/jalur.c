/*
 * jalur.c
 *
 * Penyediaan jalur bus untuk perangkat.
 * Mengelola konfigurasi bus PCI, USB, I2C, SPI
 * dan menyediakan fungsi akses bus untuk pengendali.
 *
 * Fungsi utama:
 *   penyedia_jalur_konfigurasi_pci() - konfigurasi bus PCI perangkat
 *   penyedia_jalur_konfigurasi_usb() - konfigurasi pengendali USB
 *   penyedia_jalur_konfigurasi_i2c() - konfigurasi bus I2C
 *   penyedia_jalur_konfigurasi_spi() - konfigurasi bus SPI
 *   penyedia_jalur_baca_pci()        - baca register konfigurasi PCI
 *   penyedia_jalur_tulis_pci()       - tulis register konfigurasi PCI
 *   penyedia_jalur_status_bus()      - dapatkan status bus perangkat
 */

#include "../lampiran/arsitek.h"
#include "../lampiran/mesin.h"
#include "../lampiran/peninjau.h"

/* ================================================================
 * KONSTANTA INTERNAL
 * ================================================================ */

/* Bit dalam register perintah PCI (offset 0x04) */
#define PCI_PERINTAH_IO_SPACE       0x0001  /* Aktifkan akses I/O */
#define PCI_PERINTAH_MEM_SPACE      0x0002  /* Aktifkan akses memori */
#define PCI_PERINTAH_BUS_MASTER     0x0004  /* Aktifkan bus mastering */
#define PCI_PERINTAH_SPECIAL        0x0008  /* Siklus khusus */
#define PCI_PERINTAH_MEM_WRITE_INV  0x0010  /* Memory write & invalidate */
#define PCI_PERINTAH_VGA_PALETTESNOOP 0x0020 /* VGA palette snoop */
#define PCI_PERINTAH_PARITY         0x0040  /* Respon kesalahan paritas */
#define PCI_PERINTAH_SERR           0x0100  /* Aktifkan SERR */
#define PCI_PERINTAH_FAST_B2B       0x0200  /* Fast back-to-back */

/* Bit dalam register status PCI (offset 0x06) */
#define PCI_STATUS_CAPABILITIES     0x0010  /* Daftar kapabilitas ada */
#define PCI_STATUS_66MHZ           0x0020  /* Mendukung 66 MHz */
#define PCI_STATUS_FAST_B2B        0x0080  /* Fast back-to-back diterima */
#define PCI_STATUS_MASTER_PARITY   0x0100  /* Kesalahan paritas master */
#define PCI_STATUS_DEVSEL_MASK     0x0600  /* Mask DEVSEL timing */
#define PCI_STATUS_TARGET_ABORT    0x1000  /* Target abort diterima */
#define PCI_STATUS_MASTER_ABORT    0x2000  /* Master abort diterima */
#define PCI_STATUS_SERR            0x4000  /* SERR ditekan */
#define PCI_STATUS_PARITY          0x8000  /* Kesalahan paritas terdeteksi */

/* Jumlah maksimum perangkat bus yang dilacak */
#define BUS_PERANGKAT_MAKSIMUM  64

/* Nilai konfigurasi USB optimal */
#define USB_TIMING_DEFAULT    16    /* Frame interval default (ms) */
#define USB_MAX_PACKET_FS     64    /* Max packet size Full Speed */

/* Nilai konfigurasi I2C optimal */
#define I2C_KECEPATAN_STANDAR  100  /* 100 kHz mode standar */
#define I2C_KECEPATAN_CEPAT    400  /* 400 kHz mode cepat */
#define I2C_ALAMAT_7BIT_MASK   0x7F /* Mask alamat 7-bit I2C */

/* Nilai konfigurasi SPI optimal */
#define SPI_KECEPATAN_DEFAULT  1000 /* 1 MHz clock default */
#define SPI_MODE_0             0     /* CPOL=0, CPHA=0 */
#define SPI_BIT_PER_KATA_8     8     /* 8 bit per kata */

/* ================================================================
 * STRUKTUR DATA INTERNAL
 * ================================================================ */

/* Status bus satu perangkat */
typedef struct {
    uint8    bus;              /* Nomor bus */
    uint8    perangkat;        /* Nomor perangkat */
    uint8    fungsi;           /* Nomor fungsi */
    TipeBus  tipe_bus;         /* Jenis bus */
    uint16   perintah;         /* Register perintah PCI terakhir */
    uint16   status;           /* Register status PCI */
    logika   terkonfigurasi;   /* BENAR jika sudah dikonfigurasi */
} StatusBusPerangkat;

/* Konfigurasi I2C */
typedef struct {
    uint32  kecepatan_khz;     /* Kecepatan clock dalam kHz */
    uint8   alamat_7bit;       /* Alamat perangkat 7-bit */
    uint8   aktif;             /* BENAR jika bus I2C aktif */
} KonfigurasiI2C;

/* Konfigurasi SPI */
typedef struct {
    uint32  kecepatan_khz;     /* Kecepatan clock dalam kHz */
    uint8   mode;              /* Mode SPI (0-3) */
    uint8   bit_per_kata;      /* Bit per kata transfer */
    uint8   cs_aktif_rendah;   /* BENAR jika CS aktif rendah */
    uint8   aktif;             /* BENAR jika bus SPI aktif */
} KonfigurasiSPI;

/* Konfigurasi USB */
typedef struct {
    uint32  alamat_mmio;       /* Alamat MMIO pengendali USB */
    uint8   tipe_pengendali;   /* 0=UHCI, 1=OHCI, 2=EHCI, 3=xHCI */
    uint8   aktif;             /* BENAR jika pengendali USB aktif */
} KonfigurasiUSB;

/* ================================================================
 * DATA INTERNAL
 * ================================================================ */

/* Tabel status bus per perangkat */
static StatusBusPerangkat tabel_bus[BUS_PERANGKAT_MAKSIMUM];

/* Tabel konfigurasi I2C (mendukung hingga 4 bus I2C) */
static KonfigurasiI2C tabel_i2c[4];

/* Tabel konfigurasi SPI (mendukung hingga 4 bus SPI) */
static KonfigurasiSPI tabel_spi[4];

/* Tabel konfigurasi USB (mendukung hingga 4 pengendali) */
static KonfigurasiUSB tabel_usb[4];

/* Jumlah perangkat bus yang tercatat */
static int jumlah_bus_tercatat = 0;

/* ================================================================
 * FUNGSI BANTUAN INTERNAL
 * ================================================================ */

/*
 * Cari slot kosong dalam tabel bus.
 * Mengembalikan indeks slot, atau -1 jika penuh.
 */
static int cari_slot_bus(void)
{
    int i;
    for (i = 0; i < BUS_PERANGKAT_MAKSIMUM; i++) {
        if (!tabel_bus[i].terkonfigurasi) {
            return i;
        }
    }
    return -1;
}

/*
 * Cari slot bus berdasarkan koordinat bus/perangkat/fungsi.
 * Mengembalikan indeks slot, atau -1 jika tidak ditemukan.
 */
static int cari_bus_berdasarkan_bpf(uint8 bus, uint8 perangkat, uint8 fungsi)
{
    int i;
    for (i = 0; i < BUS_PERANGKAT_MAKSIMUM; i++) {
        if (tabel_bus[i].terkonfigurasi &&
            tabel_bus[i].bus == bus &&
            tabel_bus[i].perangkat == perangkat &&
            tabel_bus[i].fungsi == fungsi) {
            return i;
        }
    }
    return -1;
}

/*
 * Ekstrak koordinat bus/perangkat/fungsi dari port DataPerangkat.
 * Port dienkode sebagai: (bus << 16) | (perangkat << 8) | fungsi
 */
static void ekstrak_bpf(uint32 port, uint8 *bus, uint8 *perangkat, uint8 *fungsi)
{
    *bus = (uint8)((port >> 16) & 0xFF);
    *perangkat = (uint8)((port >> 8) & 0xFF);
    *fungsi = (uint8)(port & 0xFF);
}

/*
 * Inisialisasi tabel internal saat pertama kali digunakan.
 */
static void inisialisasi_jalur(void)
{
    isi_memori(tabel_bus, 0, sizeof(tabel_bus));
    isi_memori(tabel_i2c, 0, sizeof(tabel_i2c));
    isi_memori(tabel_spi, 0, sizeof(tabel_spi));
    isi_memori(tabel_usb, 0, sizeof(tabel_usb));
    jumlah_bus_tercatat = 0;
}

/* ================================================================
 * FUNGSI PUBLIK
 * ================================================================ */

/*
 * penyedia_jalur_konfigurasi_pci
 *
 * Mengkonfigurasi bus PCI untuk perangkat tertentu.
 * Mengaktifkan bus mastering, memory space, dan I/O space
 * sesuai kebutuhan perangkat. Menulis ke register perintah
 * PCI (offset 0x04) untuk mengaktifkan fitur yang diminta.
 *
 * Parameter:
 *   perangkat    - struktur perangkat yang akan dikonfigurasi
 *   aktifkan_io  - BENAR untuk mengaktifkan akses I/O space
 *   aktifkan_mem - BENAR untuk mengaktifkan akses memory space
 *   aktifkan_master - BENAR untuk mengaktifkan bus mastering
 *
 * Mengembalikan: STATUS_OK jika berhasil,
 *                STATUS_GAGAL jika perangkat bukan PCI,
 *                STATUS_PARAMETAR_SALAH jika parameter salah.
 */
int penyedia_jalur_konfigurasi_pci(DataPerangkat *perangkat,
                                    logika aktifkan_io,
                                    logika aktifkan_mem,
                                    logika aktifkan_master)
{
    uint8 bus, dev, fn;
    uint16 perintah_sekarang;
    uint16 perintah_baru;
    int slot;

    if (perangkat == NULL) return STATUS_PARAMETAR_SALAH;

    /* Inisialisasi jika belum */
    if (jumlah_bus_tercatat == 0) {
        inisialisasi_jalur();
    }

    /* Perangkat harus berada di bus PCI */
    if (perangkat->tipe_bus != BUS_PCI) {
        return STATUS_GAGAL;
    }

    /* Ekstrak koordinat BDF dari port */
    ekstrak_bpf(perangkat->port, &bus, &dev, &fn);

    /* Baca register perintah saat ini */
    perintah_sekarang = (uint16)peninjau_pci_baca(bus, dev, fn,
                                                   PCI_OFFSET_PERINTAH);

    /* Susun perintah baru berdasarkan flag yang diminta */
    perintah_baru = perintah_sekarang;
    if (aktifkan_io) {
        perintah_baru |= PCI_PERINTAH_IO_SPACE;
    } else {
        perintah_baru &= ~PCI_PERINTAH_IO_SPACE;
    }

    if (aktifkan_mem) {
        perintah_baru |= PCI_PERINTAH_MEM_SPACE;
    } else {
        perintah_baru &= ~PCI_PERINTAH_MEM_SPACE;
    }

    if (aktifkan_master) {
        perintah_baru |= PCI_PERINTAH_BUS_MASTER;
    } else {
        perintah_baru &= ~PCI_PERINTAH_BUS_MASTER;
    }

    /* Aktifkan respons paritas dan SERR untuk stabilitas */
    perintah_baru |= PCI_PERINTAH_PARITY;
    perintah_baru |= PCI_PERINTAH_SERR;

    /* Tulis register perintah baru */
    peninjau_pci_tulis(bus, dev, fn, PCI_OFFSET_PERINTAH,
                       (uint32)perintah_baru);

    /* Catat dalam tabel bus */
    slot = cari_bus_berdasarkan_bpf(bus, dev, fn);
    if (slot < 0) {
        slot = cari_slot_bus();
    }
    if (slot >= 0) {
        tabel_bus[slot].bus = bus;
        tabel_bus[slot].perangkat = dev;
        tabel_bus[slot].fungsi = fn;
        tabel_bus[slot].tipe_bus = BUS_PCI;
        tabel_bus[slot].perintah = perintah_baru;
        tabel_bus[slot].status = (uint16)peninjau_pci_baca(bus, dev, fn,
                                                            PCI_OFFSET_STATUS);
        tabel_bus[slot].terkonfigurasi = BENAR;
        jumlah_bus_tercatat++;
    }

    {
        char pesan[80];
        salin_string(pesan, "Penyedia: PCI BDF ");
        konversi_uint_ke_string((uint32)bus,
                                pesan + panjang_string(pesan), 16);
        salin_string(pesan + panjang_string(pesan), ":");
        konversi_uint_ke_string((uint32)dev,
                                pesan + panjang_string(pesan), 16);
        salin_string(pesan + panjang_string(pesan), ".");
        konversi_uint_ke_string((uint32)fn,
                                pesan + panjang_string(pesan), 16);
        salin_string(pesan + panjang_string(pesan), " dikonfigurasi");
        notulen_catat(NOTULEN_INFO, pesan);
    }

    return STATUS_OK;
}

/*
 * penyedia_jalur_konfigurasi_usb
 *
 * Mengkonfigurasi pengendali USB untuk akses.
 * Mengatur parameter pengendali USB berdasarkan tipe
 * (UHCI, OHCI, EHCI, xHCI) dan mengaktifkan akses
 * ke register pengendali.
 *
 * Parameter:
 *   perangkat      - struktur perangkat USB
 *   tipe_pengendali - tipe pengendali: 0=UHCI, 1=OHCI, 2=EHCI, 3=xHCI
 *   nomor_bus      - indeks bus USB (0-3)
 *
 * Mengembalikan: STATUS_OK jika berhasil,
 *                STATUS_PARAMETAR_SALAH jika parameter salah.
 */
int penyedia_jalur_konfigurasi_usb(DataPerangkat *perangkat,
                                    uint8 tipe_pengendali,
                                    int nomor_bus)
{
    if (perangkat == NULL) return STATUS_PARAMETAR_SALAH;

    /* Inisialisasi jika belum */
    if (jumlah_bus_tercatat == 0) {
        inisialisasi_jalur();
    }

    if (nomor_bus < 0 || nomor_bus >= 4) return STATUS_PARAMETAR_SALAH;

    /* Simpan konfigurasi pengendali USB */
    tabel_usb[nomor_bus].tipe_pengendali = tipe_pengendali;
    tabel_usb[nomor_bus].alamat_mmio = (uint32)perangkat->alamat;
    tabel_usb[nomor_bus].aktif = BENAR;

    /* Konfigurasi PCI jika perangkat berada di bus PCI */
    if (perangkat->tipe_bus == BUS_PCI) {
        penyedia_jalur_konfigurasi_pci(perangkat, BENAR, BENAR, BENAR);
    }

    perangkat->tipe_bus = BUS_USB;

    {
        char pesan[80];
        salin_string(pesan, "Penyedia: USB pengendali tipe ");
        konversi_uint_ke_string((uint32)tipe_pengendali,
                                pesan + panjang_string(pesan), 10);
        salin_string(pesan + panjang_string(pesan), " pada bus ");
        konversi_uint_ke_string((uint32)nomor_bus,
                                pesan + panjang_string(pesan), 10);
        salin_string(pesan + panjang_string(pesan), " dikonfigurasi");
        notulen_catat(NOTULEN_INFO, pesan);
    }

    return STATUS_OK;
}

/*
 * penyedia_jalur_konfigurasi_i2c
 *
 * Mengkonfigurasi parameter bus I2C.
 * Mengatur kecepatan clock dan alamat perangkat slave
 * pada bus I2C tertentu.
 *
 * Parameter:
 *   nomor_bus     - indeks bus I2C (0-3)
 *   kecepatan_khz - kecepatan clock dalam kHz
 *                    (100=standar, 400=cepat, 3400=kecepatan tinggi)
 *   alamat_7bit   - alamat perangkat slave 7-bit
 *
 * Mengembalikan: STATUS_OK jika berhasil,
 *                STATUS_PARAMETAR_SALAH jika parameter salah.
 */
int penyedia_jalur_konfigurasi_i2c(int nomor_bus,
                                    uint32 kecepatan_khz,
                                    uint8 alamat_7bit)
{
    /* Inisialisasi jika belum */
    if (jumlah_bus_tercatat == 0) {
        inisialisasi_jalur();
    }

    if (nomor_bus < 0 || nomor_bus >= 4) return STATUS_PARAMETAR_SALAH;

    /* Batasi kecepatan ke nilai yang wajar */
    if (kecepatan_khz == 0) {
        kecepatan_khz = I2C_KECEPATAN_STANDAR;
    } else if (kecepatan_khz > I2C_KECEPATAN_CEPAT) {
        kecepatan_khz = I2C_KECEPATAN_CEPAT;
    }

    /* Mask alamat ke 7 bit */
    tabel_i2c[nomor_bus].alamat_7bit = alamat_7bit & I2C_ALAMAT_7BIT_MASK;
    tabel_i2c[nomor_bus].kecepatan_khz = kecepatan_khz;
    tabel_i2c[nomor_bus].aktif = BENAR;

    {
        char pesan[80];
        salin_string(pesan, "Penyedia: I2C bus ");
        konversi_uint_ke_string((uint32)nomor_bus,
                                pesan + panjang_string(pesan), 10);
        salin_string(pesan + panjang_string(pesan), " dikonfigurasi, kecepatan ");
        konversi_uint_ke_string(kecepatan_khz,
                                pesan + panjang_string(pesan), 10);
        salin_string(pesan + panjang_string(pesan), " kHz");
        notulen_catat(NOTULEN_INFO, pesan);
    }

    return STATUS_OK;
}

/*
 * penyedia_jalur_konfigurasi_spi
 *
 * Mengkonfigurasi parameter bus SPI.
 * Mengatur kecepatan clock, mode transfer, dan ukuran kata
 * pada bus SPI tertentu.
 *
 * Parameter:
 *   nomor_bus       - indeks bus SPI (0-3)
 *   kecepatan_khz   - kecepatan clock dalam kHz
 *   mode            - mode SPI (0-3: kombinasi CPOL dan CPHA)
 *   bit_per_kata    - jumlah bit per kata transfer (8 atau 16)
 *   cs_aktif_rendah - BENAR jika chip select aktif rendah
 *
 * Mengembalikan: STATUS_OK jika berhasil,
 *                STATUS_PARAMETAR_SALAH jika parameter salah.
 */
int penyedia_jalur_konfigurasi_spi(int nomor_bus,
                                    uint32 kecepatan_khz,
                                    uint8 mode,
                                    uint8 bit_per_kata,
                                    logika cs_aktif_rendah)
{
    /* Inisialisasi jika belum */
    if (jumlah_bus_tercatat == 0) {
        inisialisasi_jalur();
    }

    if (nomor_bus < 0 || nomor_bus >= 4) return STATUS_PARAMETAR_SALAH;

    /* Batasi mode ke rentang 0-3 */
    if (mode > 3) return STATUS_PARAMETAR_SALAH;

    /* Batasi bit per kata ke 8 atau 16 */
    if (bit_per_kata != 8 && bit_per_kata != 16) {
        bit_per_kata = SPI_BIT_PER_KATA_8;
    }

    /* Batasi kecepatan ke nilai yang wajar */
    if (kecepatan_khz == 0) {
        kecepatan_khz = SPI_KECEPATAN_DEFAULT;
    }

    /* Simpan konfigurasi */
    tabel_spi[nomor_bus].kecepatan_khz = kecepatan_khz;
    tabel_spi[nomor_bus].mode = mode;
    tabel_spi[nomor_bus].bit_per_kata = bit_per_kata;
    tabel_spi[nomor_bus].cs_aktif_rendah = (uint8)cs_aktif_rendah;
    tabel_spi[nomor_bus].aktif = BENAR;

    {
        char pesan[80];
        salin_string(pesan, "Penyedia: SPI bus ");
        konversi_uint_ke_string((uint32)nomor_bus,
                                pesan + panjang_string(pesan), 10);
        salin_string(pesan + panjang_string(pesan), " mode ");
        konversi_uint_ke_string((uint32)mode,
                                pesan + panjang_string(pesan), 10);
        salin_string(pesan + panjang_string(pesan), ", kecepatan ");
        konversi_uint_ke_string(kecepatan_khz,
                                pesan + panjang_string(pesan), 10);
        salin_string(pesan + panjang_string(pesan), " kHz");
        notulen_catat(NOTULEN_INFO, pesan);
    }

    return STATUS_OK;
}

/*
 * penyedia_jalur_baca_pci
 *
 * Membaca nilai dari ruang konfigurasi PCI untuk perangkat.
 * Merupakan pembungkus (wrapper) atas peninjau_pci_baca yang
 * menggunakan koordinat dari struktur DataPerangkat.
 *
 * Parameter:
 *   perangkat - struktur perangkat yang berisi koordinat bus
 *   offset    - offset register PCI yang akan dibaca (kelipatan 4)
 *
 * Mengembalikan: nilai register 32-bit, atau 0xFFFFFFFF jika gagal.
 */
uint32 penyedia_jalur_baca_pci(DataPerangkat *perangkat, uint8 offset)
{
    uint8 bus, dev, fn;

    if (perangkat == NULL) return 0xFFFFFFFF;
    if (perangkat->tipe_bus != BUS_PCI) return 0xFFFFFFFF;

    ekstrak_bpf(perangkat->port, &bus, &dev, &fn);

    return peninjau_pci_baca(bus, dev, fn, offset);
}

/*
 * penyedia_jalur_tulis_pci
 *
 * Menulis nilai ke ruang konfigurasi PCI untuk perangkat.
 * Merupakan pembungkus (wrapper) atas peninjau_pci_tulis yang
 * menggunakan koordinat dari struktur DataPerangkat.
 *
 * Parameter:
 *   perangkat - struktur perangkat yang berisi koordinat bus
 *   offset    - offset register PCI yang akan ditulis (kelipatan 4)
 *   nilai     - nilai 32-bit yang akan ditulis
 *
 * Mengembalikan: STATUS_OK jika berhasil,
 *                STATUS_PARAMETAR_SALAH jika parameter salah,
 *                STATUS_GAGAL jika perangkat bukan PCI.
 */
int penyedia_jalur_tulis_pci(DataPerangkat *perangkat, uint8 offset,
                              uint32 nilai)
{
    uint8 bus, dev, fn;

    if (perangkat == NULL) return STATUS_PARAMETAR_SALAH;
    if (perangkat->tipe_bus != BUS_PCI) return STATUS_GAGAL;

    ekstrak_bpf(perangkat->port, &bus, &dev, &fn);

    peninjau_pci_tulis(bus, dev, fn, offset, nilai);

    return STATUS_OK;
}

/*
 * penyedia_jalur_status_bus
 *
 * Mendapatkan status bus untuk perangkat tertentu.
 * Membaca register perintah dan status PCI saat ini
 * dan mengembalikan informasi konfigurasi bus.
 *
 * Parameter:
 *   perangkat - struktur perangkat yang diperiksa
 *   status    - pointer ke uint32 untuk menyimpan status bus
 *               Bit 0-15: register perintah PCI
 *               Bit 16-31: register status PCI
 *
 * Mengembalikan: STATUS_OK jika berhasil,
 *                STATUS_PARAMETAR_SALAH jika parameter salah,
 *                STATUS_TIDAK_ADA jika perangkat belum dikonfigurasi.
 */
int penyedia_jalur_status_bus(DataPerangkat *perangkat, uint32 *status)
{
    uint8 bus, dev, fn;
    uint16 perintah;
    uint16 status_pci;
    int slot;

    if (perangkat == NULL || status == NULL) return STATUS_PARAMETAR_SALAH;

    /* Inisialisasi jika belum */
    if (jumlah_bus_tercatat == 0) {
        inisialisasi_jalur();
    }

    /* Cek apakah perangkat PCI */
    if (perangkat->tipe_bus == BUS_PCI) {
        ekstrak_bpf(perangkat->port, &bus, &dev, &fn);

        /* Baca register langsung dari hardware */
        perintah = (uint16)peninjau_pci_baca(bus, dev, fn,
                                              PCI_OFFSET_PERINTAH);
        status_pci = (uint16)peninjau_pci_baca(bus, dev, fn,
                                                PCI_OFFSET_STATUS);

        /* Gabungkan perintah dan status */
        *status = ((uint32)status_pci << 16) | (uint32)perintah;

        /* Perbarui tabel internal */
        slot = cari_bus_berdasarkan_bpf(bus, dev, fn);
        if (slot >= 0) {
            tabel_bus[slot].perintah = perintah;
            tabel_bus[slot].status = status_pci;
        }

        return STATUS_OK;
    }

    /* Untuk bus non-PCI, kembalikan status dari tabel internal */
    if (perangkat->tipe_bus == BUS_USB) {
        *status = 0x00010000; /* Bit 16 set = USB aktif */
        return STATUS_OK;
    }

    if (perangkat->tipe_bus == BUS_I2C) {
        *status = 0x00020000; /* Bit 17 set = I2C aktif */
        return STATUS_OK;
    }

    if (perangkat->tipe_bus == BUS_SPI) {
        *status = 0x00040000; /* Bit 18 set = SPI aktif */
        return STATUS_OK;
    }

    *status = 0;
    return STATUS_TIDAK_ADA;
}
