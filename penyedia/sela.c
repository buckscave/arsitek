/*
 * sela.c
 *
 * Penyediaan jalur interupsi (sela) untuk perangkat.
 * Mengelola alokasi dan konfigurasi nomor IRQ untuk
 * setiap perangkat yang membutuhkan jalur interupsi.
 *
 * Pada arsitektur x86/x64, interupsi dikelola oleh PIC 8259A
 * (atau APIC pada sistem modern) dengan 16 jalur IRQ (0-15).
 * Pada arsitektur ARM, interupsi dikelola oleh GIC (Generic
 * Interrupt Controller) dengan jumlah interupsi yang lebih banyak.
 *
 * Fungsi utama:
 *   penyedia_sela_alokasi()   - alokasi IRQ untuk perangkat
 *   penyedia_sela_bebaskan()  - bebaskan IRQ yang dipakai
 *   penyedia_sela_cari_bebas()- cari nomor IRQ yang tersedia
 *   penyedia_sela_konfigurasi()- konfigurasi routing IRQ
 *   penyedia_sela_mask()      - mask (nonaktifkan) IRQ
 *   penyedia_sela_unmask()    - unmask (aktifkan) IRQ
 */

#include "../lampiran/arsitek.h"
#include "../lampiran/mesin.h"
#include "../lampiran/peninjau.h"

/* ================================================================
 * KONSTANTA INTERNAL
 * ================================================================ */

/* Jumlah maksimum jalur IRQ — berbeda per arsitektur */
#if defined(ARSITEK_X86) || defined(ARSITEK_X64)
#define IRQ_MAKSIMUM        16      /* PIC 8259A: IRQ 0-15 */
#define IRQ_PIC_BAWAH       0       /* PIC master: IRQ 0-7 */
#define IRQ_PIC_ATAS        7       /* Batas PIC master */
#define IRQ_CASCADE         2       /* IRQ cascade (tidak bisa dipakai) */
#elif defined(ARSITEK_ARM32) || defined(ARSITEK_ARM64)
#define IRQ_MAKSIMUM        256     /* GIC: mendukung hingga 256 IRQ */
#else
#define IRQ_MAKSIMUM        16
#endif

/* IRQ yang dicadangkan dan tidak bisa dialokasikan */
#if defined(ARSITEK_X86) || defined(ARSITEK_X64)
/* IRQ 0 (timer), 2 (cascade), 13 (FPU) dicadangkan */
#define IRQ_DICADANGKAN_COUNT 3
static const uint8 daftar_irq_dicadangkan[IRQ_DICADANGKAN_COUNT] = { 0, 2, 13 };
#endif

/* Jumlah maksimum perangkat yang dapat tercatat dalam tabel IRQ */
#define IRQ_PERANGKAT_MAKSIMUM  64

/* ================================================================
 * STRUKTUR DATA INTERNAL
 * ================================================================ */

/* Entri alokasi IRQ — mencatat perangkat pemilik IRQ */
typedef struct {
    uint8   nomor_irq;           /* Nomor IRQ (0-15 pada x86) */
    uint32  nomor_perangkat;     /* Nomor perangkat pemilik */
    char    nama_perangkat[32];  /* Nama perangkat pemilik */
    logika  terpakai;            /* BENAR jika IRQ ini teralokasi */
    logika  termask;             /* BENAR jika IRQ di-mask */
    uint8   tipe_pemicu;         /* 0=edge, 1=level */
    uint8   prioritas;           /* Prioritas IRQ (0=paling tinggi) */
} EntriIRQ;

/* Tabel routing IRQ untuk PCI */
typedef struct {
    uint8   irq_asli;            /* IRQ dari PCI config */
    uint8   irq_dirute;          /* IRQ yang dirutekan */
    uint8   bus;                 /* Nomor bus PCI */
    uint8   perangkat;           /* Nomor perangkat PCI */
    logika   aktif;              /* BENAR jika routing aktif */
} RoutingIRQPCI;

/* ================================================================
 * DATA INTERNAL
 * ================================================================ */

/* Tabel alokasi IRQ — satu entri per nomor IRQ */
static EntriIRQ tabel_irq[IRQ_MAKSIMUM];

/* Tabel routing IRQ PCI */
static RoutingIRQPCI tabel_routing[PCI_MAKSIMUM];

/* Jumlah IRQ yang sedang teralokasi */
static int jumlah_irq_teralokasi = 0;

/* Mask IRQ global — bit yang set berarti IRQ di-mask */
#if defined(ARSITEK_X86) || defined(ARSITEK_X64)
static uint16 mask_irq_global = 0x0000; /* Semua unmask awalnya */
#endif

/* Tanda apakah modul sela sudah diinisialisasi */
static logika sela_siap = SALAH;

/* ================================================================
 * FUNGSI BANTUAN INTERNAL
 * ================================================================ */

/*
 * Inisialisasi modul sela.
 * Menandai IRQ yang dicadangkan dan mengatur PIC/GIC.
 */
static void inisialisasi_sela(void)
{
    int i;

    isi_memori(tabel_irq, 0, sizeof(tabel_irq));
    isi_memori(tabel_routing, 0, sizeof(tabel_routing));
    jumlah_irq_teralokasi = 0;

#if defined(ARSITEK_X86) || defined(ARSITEK_X64)
    /* Tandai IRQ yang dicadangkan sebagai terpakai */
    for (i = 0; i < IRQ_DICADANGKAN_COUNT; i++) {
        uint8 irq = daftar_irq_dicadangkan[i];
        tabel_irq[irq].nomor_irq = irq;
        tabel_irq[irq].terpakai = BENAR;
        tabel_irq[irq].termask = BENAR; /* Dicadangkan, tidak bisa diubah */
        salin_string(tabel_irq[irq].nama_perangkat, "Sistem");
        tabel_irq[irq].prioritas = 0; /* Prioritas tertinggi */
        jumlah_irq_teralokasi++;
    }

    /* Mask semua IRQ terlebih dahulu selama inisialisasi */
    mask_irq_global = 0xFFFF;

    /* Kirim ICW1 ke PIC master dan slave untuk inisialisasi */
    tulis_port(PIC1_PERINTAH, PIC_ICW1_INIT | PIC_ICW1_ICW4);
    tunda_io();
    tulis_port(PIC2_PERINTAH, PIC_ICW1_INIT | PIC_ICW1_ICW4);
    tunda_io();

    /* ICW2: tentukan offset vektor interupsi */
    tulis_port(PIC1_DATA, 0x20); /* Master: INT 0x20-0x27 */
    tunda_io();
    tulis_port(PIC2_DATA, 0x28); /* Slave: INT 0x28-0x2F */
    tunda_io();

    /* ICW3: konfigurasi cascade */
    tulis_port(PIC1_DATA, 0x04); /* Master: slave di IRQ2 */
    tunda_io();
    tulis_port(PIC2_DATA, 0x02); /* Slave: identitas cascade */
    tunda_io();

    /* ICW4: mode 8086, auto-EOI */
    tulis_port(PIC1_DATA, PIC_ICW4_8086);
    tunda_io();
    tulis_port(PIC2_DATA, PIC_ICW4_8086);
    tunda_io();

    /* Unmask IRQ yang dicadangkan saja, sisanya tetap di-mask */
    mask_irq_global = (uint16)(0xFFFB); /* Mask semua kecuali cascade */

    tulis_port(PIC1_DATA, (uint8)(mask_irq_global & 0xFF));
    tunda_io();
    tulis_port(PIC2_DATA, (uint8)((mask_irq_global >> 8) & 0xFF));
    tunda_io();

#elif defined(ARSITEK_ARM32) || defined(ARSITEK_ARM64)
    /* Inisialisasi GIC — aktifkan distributor dan antarmuka CPU */
    volatile uint32 *gicd_ctrl = (volatile uint32 *)
        (GIC_DISTRIBUTOR_BASE + GICD_CTRL);
    volatile uint32 *gicc_ctrl = (volatile uint32 *)
        (GIC_CPU_INTERFACE_BASE + GICC_CTRL);
    volatile uint32 *gicc_pmr = (volatile uint32 *)
        (GIC_CPU_INTERFACE_BASE + GICC_PMR);

    /* Aktifkan distributor GIC */
    *gicd_ctrl = 0x00000001;

    /* Set priority mask ke level terendah (semua interupsi diizinkan) */
    *gicc_pmr = 0x000000F0;

    /* Aktifkan antarmuka CPU GIC */
    *gicc_ctrl = 0x00000001;

    /* Pada GIC, semua IRQ dimulai dalam keadaan non-aktif */
    for (i = 0; i < IRQ_MAKSIMUM; i++) {
        tabel_irq[i].nomor_irq = (uint8)i;
        tabel_irq[i].terpakai = SALAH;
        tabel_irq[i].termask = BENAR; /* Default: di-mask */
        tabel_irq[i].prioritas = 0;
    }
#endif

    sela_siap = BENAR;
}

/*
 * Periksa apakah suatu nomor IRQ dicadangkan.
 */
static logika apakah_irq_dicadangkan(uint8 nomor_irq)
{
#if defined(ARSITEK_X86) || defined(ARSITEK_X64)
    int i;
    for (i = 0; i < IRQ_DICADANGKAN_COUNT; i++) {
        if (daftar_irq_dicadangkan[i] == nomor_irq) {
            return BENAR;
        }
    }
    return SALAH;
#else
    /* Pada ARM, tidak ada IRQ khusus yang dicadangkan */
    if (nomor_irq == 0) return BENAR; /* IRQ 0 biasanya untuk timer */
    return SALAH;
#endif
}

/*
 * Perbarui mask PIC/GIC berdasarkan status tabel IRQ.
 * Mengirim data mask baru ke PIC master dan slave.
 */
static void perbarui_mask_pic(void)
{
#if defined(ARSITEK_X86) || defined(ARSITEK_X64)
    uint8 mask_master;
    uint8 mask_slave;
    int i;

    /* Hitung mask: bit=1 berarti IRQ di-mask (nonaktif) */
    mask_irq_global = 0x0000;

    /* Selalu mask IRQ cascade (2) pada master — diatur oleh PIC */
    BIT_SET(mask_irq_global, IRQ_CASCADE);

    for (i = 0; i < IRQ_MAKSIMUM; i++) {
        if (tabel_irq[i].termask) {
            BIT_SET(mask_irq_global, i);
        }
    }

    /* Kirim ke PIC */
    mask_master = (uint8)(mask_irq_global & 0xFF);
    mask_slave = (uint8)((mask_irq_global >> 8) & 0xFF);

    tulis_port(PIC1_DATA, mask_master);
    tunda_io();
    tulis_port(PIC2_DATA, mask_slave);
    tunda_io();
#endif
}

/*
 * Cari slot kosong dalam tabel routing PCI.
 * Mengembalikan indeks slot, atau -1 jika penuh.
 */
static int cari_slot_routing(void)
{
    int i;
    for (i = 0; i < PCI_MAKSIMUM; i++) {
        if (!tabel_routing[i].aktif) {
            return i;
        }
    }
    return -1;
}

/* ================================================================
 * FUNGSI PUBLIK
 * ================================================================ */

/*
 * penyedia_sela_cari_bebas
 *
 * Mencari nomor IRQ yang tersedia (belum dialokasikan).
 * Mengembalikan nomor IRQ bebas pertama yang ditemukan,
 * atau -1 jika semua IRQ sudah terpakai.
 *
 * Mengembalikan: nomor IRQ bebas, atau -1 jika tidak ada.
 */
int penyedia_sela_cari_bebas(void)
{
    int i;

    /* Inisialisasi jika belum */
    if (!sela_siap) {
        inisialisasi_sela();
    }

    for (i = 0; i < IRQ_MAKSIMUM; i++) {
        if (!tabel_irq[i].terpakai && !apakah_irq_dicadangkan((uint8)i)) {
            return i;
        }
    }

    return -1;
}

/*
 * penyedia_sela_alokasi
 *
 * Mengalokasikan nomor IRQ untuk perangkat.
 * Jika nomor_interupsi = 0xFF, maka akan dicari IRQ bebas
 * secara otomatis. Jika nomor spesifik diminta, akan
 * diperiksa apakah IRQ tersebut tersedia.
 *
 * Parameter:
 *   perangkat        - struktur perangkat yang meminta IRQ
 *   nomor_interupsi  - nomor IRQ yang diminta, atau 0xFF untuk otomatis
 *
 * Mengembalikan: nomor IRQ yang dialokasikan, atau -1 jika gagal.
 */
int penyedia_sela_alokasi(DataPerangkat *perangkat,
                           unsigned int nomor_interupsi)
{
    int irq_dipilih;

    /* Inisialisasi jika belum */
    if (!sela_siap) {
        inisialisasi_sela();
    }

    if (perangkat == NULL) return -1;

    /* Tentukan nomor IRQ */
    if (nomor_interupsi == 0xFF) {
        /* Cari IRQ bebas secara otomatis */
        irq_dipilih = penyedia_sela_cari_bebas();
        if (irq_dipilih < 0) {
            notulen_catat(NOTULEN_KESALAHAN,
                          "Penyedia: Tidak ada IRQ bebas untuk dialokasi");
            return -1;
        }
    } else {
        /* Gunakan nomor IRQ spesifik yang diminta */
        if (nomor_interupsi >= (unsigned int)IRQ_MAKSIMUM) {
            notulen_catat(NOTULEN_KESALAHAN,
                          "Penyedia: Nomor IRQ di luar jangkauan");
            return -1;
        }
        if (apakah_irq_dicadangkan((uint8)nomor_interupsi)) {
            notulen_catat(NOTULEN_KESALAHAN,
                          "Penyedia: IRQ dicadangkan, tidak dapat dialokasi");
            return -1;
        }
        if (tabel_irq[nomor_interupsi].terpakai) {
            notulen_catat(NOTULEN_KESALAHAN,
                          "Penyedia: IRQ sudah teralokasi");
            return -1;
        }
        irq_dipilih = (int)nomor_interupsi;
    }

    /* Alokasi IRQ */
    tabel_irq[irq_dipilih].nomor_irq = (uint8)irq_dipilih;
    tabel_irq[irq_dipilih].nomor_perangkat = perangkat->nomor;
    tabel_irq[irq_dipilih].terpakai = BENAR;
    tabel_irq[irq_dipilih].termask = SALAH;
    tabel_irq[irq_dipilih].tipe_pemicu = 1; /* Level-triggered default */
    tabel_irq[irq_dipilih].prioritas =
        (uint8)(irq_dipilih < 8 ? irq_dipilih : irq_dipilih - 8);

    /* Salin nama perangkat */
    {
        int j;
        const char *sumber = perangkat->nama;
        for (j = 0; j < 31 && sumber[j] != '\0'; j++) {
            tabel_irq[irq_dipilih].nama_perangkat[j] = sumber[j];
        }
        tabel_irq[irq_dipilih].nama_perangkat[j] = '\0';
    }

    /* Perbarui struktur perangkat */
    perangkat->interupsi = (uint32)irq_dipilih;

    /* Unmask IRQ yang baru dialokasi */
    penyedia_sela_unmask((uint8)irq_dipilih);

    jumlah_irq_teralokasi++;

    {
        char pesan[80];
        salin_string(pesan, "Penyedia: IRQ ");
        konversi_uint_ke_string((uint32)irq_dipilih,
                                pesan + panjang_string(pesan), 10);
        salin_string(pesan + panjang_string(pesan), " dialokasi untuk ");
        salin_string(pesan + panjang_string(pesan),
                     tabel_irq[irq_dipilih].nama_perangkat);
        notulen_catat(NOTULEN_INFO, pesan);
    }

    return irq_dipilih;
}

/*
 * penyedia_sela_bebaskan
 *
 * Membebaskan nomor IRQ yang sebelumnya dialokasikan.
 * IRQ yang dibebaskan akan di-mask dan ditandai sebagai
 * tersedia kembali untuk perangkat lain.
 *
 * Parameter:
 *   nomor_irq - nomor IRQ yang akan dibebaskan
 *
 * Mengembalikan: STATUS_OK jika berhasil,
 *                STATUS_TIDAK_ADA jika IRQ tidak teralokasi,
 *                STATUS_PARAMETAR_SALAH jika nomor IRQ tidak valid.
 */
int penyedia_sela_bebaskan(uint8 nomor_irq)
{
    /* Inisialisasi jika belum */
    if (!sela_siap) {
        inisialisasi_sela();
    }

    if (nomor_irq >= IRQ_MAKSIMUM) return STATUS_PARAMETAR_SALAH;

    if (!tabel_irq[nomor_irq].terpakai) {
        return STATUS_TIDAK_ADA;
    }

    if (apakah_irq_dicadangkan(nomor_irq)) {
        notulen_catat(NOTULEN_KESALAHAN,
                      "Penyedia: IRQ dicadangkan, tidak dapat dibebaskan");
        return STATUS_GAGAL;
    }

    /* Mask IRQ sebelum membebaskan */
    penyedia_sela_mask(nomor_irq);

    /* Hapus catatan alokasi */
    tabel_irq[nomor_irq].nomor_perangkat = 0;
    tabel_irq[nomor_irq].nama_perangkat[0] = '\0';
    tabel_irq[nomor_irq].terpakai = SALAH;
    tabel_irq[nomor_irq].termask = BENAR;
    tabel_irq[nomor_irq].tipe_pemicu = 0;
    tabel_irq[nomor_irq].prioritas = 0;

    jumlah_irq_teralokasi--;

    {
        char pesan[80];
        salin_string(pesan, "Penyedia: IRQ ");
        konversi_uint_ke_string((uint32)nomor_irq,
                                pesan + panjang_string(pesan), 10);
        salin_string(pesan + panjang_string(pesan), " dibebaskan");
        notulen_catat(NOTULEN_INFO, pesan);
    }

    return STATUS_OK;
}

/*
 * penyedia_sela_konfigurasi
 *
 * Mengkonfigurasi routing IRQ untuk perangkat.
 * Pada perangkat PCI, ini berarti menulis nomor IRQ ke
 * register konfigurasi PCI (offset 0x3C) dan mencatat
 * routing dalam tabel internal.
 *
 * Parameter:
 *   perangkat   - struktur perangkat yang akan dikonfigurasi
 *   nomor_irq   - nomor IRQ yang akan dirutekan ke perangkat
 *   tipe_pemicu - tipe pemicu: 0=edge-triggered, 1=level-triggered
 *
 * Mengembalikan: STATUS_OK jika berhasil,
 *                STATUS_GAGAL jika gagal,
 *                STATUS_PARAMETAR_SALAH jika parameter salah.
 */
int penyedia_sela_konfigurasi(DataPerangkat *perangkat,
                               uint8 nomor_irq,
                               uint8 tipe_pemicu)
{
    int slot_routing;

    /* Inisialisasi jika belum */
    if (!sela_siap) {
        inisialisasi_sela();
    }

    if (perangkat == NULL) return STATUS_PARAMETAR_SALAH;
    if (nomor_irq >= IRQ_MAKSIMUM) return STATUS_PARAMETAR_SALAH;
    if (tipe_pemicu > 1) tipe_pemicu = 1; /* Default ke level-triggered */

    /* Untuk perangkat PCI, tulis IRQ ke register konfigurasi */
    if (perangkat->tipe_bus == BUS_PCI) {
        uint8 bus, dev, fn;
        uint32 data_irq;

        /* Ekstrak koordinat BDF */
        bus = (uint8)((perangkat->port >> 16) & 0xFF);
        dev = (uint8)((perangkat->port >> 8) & 0xFF);
        fn = (uint8)(perangkat->port & 0xFF);

        /* Baca register IRQ saat ini (offset 0x3C) */
        data_irq = peninjau_pci_baca(bus, dev, fn, PCI_OFFSET_IRQ);

        /* Ubah byte IRQ (byte rendah) dan pertahankan byte pin */
        data_irq = (data_irq & 0xFFFFFF00) | (uint32)nomor_irq;

        /* Tulis kembali ke PCI config space */
        peninjau_pci_tulis(bus, dev, fn, PCI_OFFSET_IRQ, data_irq);

        /* Catat routing dalam tabel */
        slot_routing = cari_slot_routing();
        if (slot_routing >= 0) {
            tabel_routing[slot_routing].irq_asli =
                (uint8)peninjau_pci_baca_irq(bus, dev, fn);
            tabel_routing[slot_routing].irq_dirute = nomor_irq;
            tabel_routing[slot_routing].bus = bus;
            tabel_routing[slot_routing].perangkat = dev;
            tabel_routing[slot_routing].aktif = BENAR;
        }
    }

    /* Perbarui tabel IRQ */
    tabel_irq[nomor_irq].tipe_pemicu = tipe_pemicu;

    /* Perbarui struktur perangkat */
    perangkat->interupsi = (uint32)nomor_irq;

    {
        char pesan[80];
        salin_string(pesan, "Penyedia: IRQ ");
        konversi_uint_ke_string((uint32)nomor_irq,
                                pesan + panjang_string(pesan), 10);
        salin_string(pesan + panjang_string(pesan),
                     " dikonfigurasi, pemicu ");
        if (tipe_pemicu == 0) {
            salin_string(pesan + panjang_string(pesan), "edge");
        } else {
            salin_string(pesan + panjang_string(pesan), "level");
        }
        notulen_catat(NOTULEN_INFO, pesan);
    }

    return STATUS_OK;
}

/*
 * penyedia_sela_mask
 *
 * Men-mask (menonaktifkan) jalur IRQ tertentu.
 * IRQ yang di-mask tidak akan menghasilkan interupsi
 * sampai di-unmask kembali.
 *
 * Pada x86/x64: men-set bit yang sesuai dalam register
 * data PIC (0x21 untuk master, 0xA1 untuk slave).
 *
 * Pada ARM: men-clear bit yang sesuai dalam register
 * GICD_ISENABLE (disable enable = mask).
 *
 * Parameter:
 *   nomor_irq - nomor IRQ yang akan di-mask
 *
 * Mengembalikan: STATUS_OK jika berhasil,
 *                STATUS_PARAMETAR_SALAH jika nomor IRQ tidak valid.
 */
int penyedia_sela_mask(uint8 nomor_irq)
{
    /* Inisialisasi jika belum */
    if (!sela_siap) {
        inisialisasi_sela();
    }

    if (nomor_irq >= IRQ_MAKSIMUM) return STATUS_PARAMETAR_SALAH;

    /* Tandai sebagai ter-mask dalam tabel */
    tabel_irq[nomor_irq].termask = BENAR;

#if defined(ARSITEK_X86) || defined(ARSITEK_X64)
    /* Perbarui mask PIC */
    perbarui_mask_pic();
#elif defined(ARSITEK_ARM32) || defined(ARSITEK_ARM64)
    /* Mask IRQ pada GIC Distributor — clear enable bit */
    {
        volatile uint32 *gicd_icenable = (volatile uint32 *)
            (GIC_DISTRIBUTOR_BASE + GICD_ICENABLE + (nomor_irq / 32) * 4);
        *gicd_icenable = (uint32)BIT(nomor_irq % 32);
    }
#endif

    return STATUS_OK;
}

/*
 * penyedia_sela_unmask
 *
 * Meng-unmask (mengaktifkan) jalur IRQ tertentu.
 * IRQ yang di-unmask akan menghasilkan interupsi
 * ketika perangkat memicunya.
 *
 * Pada x86/x64: meng-clear bit yang sesuai dalam register
 * data PIC (0x21 untuk master, 0xA1 untuk slave).
 *
 * Pada ARM: men-set bit yang sesuai dalam register
 * GICD_ISENABLE (set enable = unmask).
 *
 * Parameter:
 *   nomor_irq - nomor IRQ yang akan di-unmask
 *
 * Mengembalikan: STATUS_OK jika berhasil,
 *                STATUS_PARAMETAR_SALAH jika nomor IRQ tidak valid.
 */
int penyedia_sela_unmask(uint8 nomor_irq)
{
    /* Inisialisasi jika belum */
    if (!sela_siap) {
        inisialisasi_sela();
    }

    if (nomor_irq >= IRQ_MAKSIMUM) return STATUS_PARAMETAR_SALAH;

    /* Jangan unmask IRQ yang dicadangkan */
    if (apakah_irq_dicadangkan(nomor_irq)) {
        return STATUS_GAGAL;
    }

    /* Tandai sebagai tidak ter-mask dalam tabel */
    tabel_irq[nomor_irq].termask = SALAH;

#if defined(ARSITEK_X86) || defined(ARSITEK_X64)
    /* Perbarui mask PIC */
    perbarui_mask_pic();
#elif defined(ARSITEK_ARM32) || defined(ARSITEK_ARM64)
    /* Unmask IRQ pada GIC Distributor — set enable bit */
    {
        volatile uint32 *gicd_isenable = (volatile uint32 *)
            (GIC_DISTRIBUTOR_BASE + GICD_ISENABLE + (nomor_irq / 32) * 4);
        *gicd_isenable = (uint32)BIT(nomor_irq % 32);
    }
#endif

    return STATUS_OK;
}
