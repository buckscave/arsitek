/*
 * pengembang.c
 *
 * Implementasi Pengembang — pembangun driver dan antarmuka.
 * Setelah Penyedia selesai mengumpulkan sumber daya,
 * Pengembang menggunakan sumber daya itu untuk:
 *   - Membangun koneksi ke perangkat
 *   - Mengidentifikasi IC chip perangkat
 *   - Membangun kendali (driver) generik berdasarkan IC
 *   - Membangun antarmuka agar lapisan atas bisa berbicara
 *
 * Filosofi: Deteksi berdasarkan IC, bukan vendor/brand.
 * Driver generik dibuat per IC chip, sehingga perangkat
 * dari pabrikan berbeda yang menggunakan IC yang sama
 * dapat menggunakan driver yang sama pula.
 */

#include "../lampiran/arsitek.h"
#include "../lampiran/mesin.h"
#include "../lampiran/peninjau.h"
#include "../lampiran/pengembang.h"

/* Penanda global */
int pengembang_siap = SALAH;

/* Tabel kendali (driver) yang sudah dibangun */
#define KENDALI_MAKSIMUM  64

static struct {
    DataPerangkat *perangkat;
    DataIC         ic;           /* IC yang teridentifikasi */
    int            terpasang;
} tabel_kendali[KENDALI_MAKSIMUM];

static int jumlah_kendali = 0;

/* ================================================================
 * DEKLARASI FUNGSI EKSTERN (dari koneksi.c, kendali.c, antarmuka.c)
 * ================================================================ */

/* Dari koneksi.c */
extern int pengembang_koneksi_pci(DataPerangkat *perangkat);
extern int pengembang_koneksi_usb_uhci(DataPerangkat *perangkat);
extern int pengembang_koneksi_usb_ohci(DataPerangkat *perangkat);
extern int pengembang_koneksi_usb_ehci(DataPerangkat *perangkat);
extern int pengembang_koneksi_usb_xhci(DataPerangkat *perangkat);
extern int pengembang_koneksi_ahci(DataPerangkat *perangkat);
extern int pengembang_koneksi_nvme(DataPerangkat *perangkat);
extern int pengembang_koneksi_generik(DataPerangkat *perangkat);

/* Dari kendali.c */
extern int pengembang_kendali_identifikasi_ic(uint16 vendor_id, uint16 perangkat_id,
                                               uint8 kelas, uint8 subkelas,
                                               DataIC *hasil);
extern int pengembang_kendali_inisialisasi_ic(DataIC *ic, DataPerangkat *perangkat);
extern int pengembang_kendali_verifikasi_ic(DataPerangkat *perangkat, DataIC *ic);
extern int pengembang_kendali_diagnostik(DataPerangkat *perangkat, DataIC *ic);

/* Dari antarmuka.c */
extern int pengembang_antarmuka_buat(DataPerangkat *perangkat, DataIC *ic);

/* ================================================================
 * FUNGSI UTAMA
 * ================================================================ */

/*
 * pengembang_bangun_semua — Bangun semua driver untuk semua perangkat.
 * Iterasi melalui daftar perangkat, identifikasi IC chip,
 * bangun koneksi, kendali, dan antarmuka untuk setiap perangkat.
 */
void pengembang_bangun_semua(DataPerangkat daftar[], int jumlah)
{
    int i;

    notulen_catat(NOTULEN_INFO, "Pengembang: Memulai pembangunan driver berbasis IC...");

    isi_memori(tabel_kendali, 0, sizeof(tabel_kendali));
    jumlah_kendali = 0;

    for (i = 0; i < jumlah && jumlah_kendali < KENDALI_MAKSIMUM; i++) {
        DataIC ic;
        int hasil_ic;

        /* 1. Identifikasi IC chip perangkat */
        hasil_ic = pengembang_kendali_identifikasi_ic(
            daftar[i].vendor_id,
            daftar[i].perangkat_id,
            daftar[i].kelas,
            daftar[i].subkelas,
            &ic
        );

        if (hasil_ic == STATUS_OK) {
            notulen_catat(NOTULEN_INFO, "Pengembang: IC dikenali");
        } else {
            notulen_catat(NOTULEN_PERINGATAN, "Pengembang: IC tidak dikenali — menggunakan driver generik");
        }

        /* 2. Bangun koneksi ke perangkat berdasarkan IC */
        if (pengembang_bangun_koneksi(&daftar[i]) != STATUS_OK) {
            notulen_catat(NOTULEN_KESALAHAN, "Pengembang: Gagal membangun koneksi");
            continue;
        }

        /* 3. Inisialisasi driver IC */
        if (pengembang_kendali_inisialisasi_ic(&ic, &daftar[i]) != STATUS_OK) {
            notulen_catat(NOTULEN_KESALAHAN, "Pengembang: Gagal menginisialisasi IC");
            continue;
        }

        /* 4. Verifikasi IC merespons */
        if (pengembang_kendali_verifikasi_ic(&daftar[i], &ic)) {
            notulen_catat(NOTULEN_INFO, "Pengembang: IC terverifikasi");
        } else {
            notulen_catat(NOTULEN_PERINGATAN, "Pengembang: Verifikasi IC gagal — IC mungkin tidak merespons");
        }

        /* 5. Jalankan diagnostik */
        if (pengembang_kendali_diagnostik(&daftar[i], &ic) == STATUS_OK) {
            notulen_catat(NOTULEN_INFO, "Pengembang: Diagnostik IC lulus");
        }

        /* 6. Bangun antarmuka untuk lapisan atas */
        if (pengembang_bangun_antarmuka(&daftar[i]) == STATUS_OK) {
            tabel_kendali[jumlah_kendali].perangkat = &daftar[i];
            tabel_kendali[jumlah_kendali].ic = ic;
            tabel_kendali[jumlah_kendali].terpasang = 1;
            jumlah_kendali++;
        }
    }

    pengembang_siap = BENAR;

    {
        char pesan[80];
        salin_string(pesan, "Pengembang: ");
        konversi_uint_ke_string((uint32)jumlah_kendali,
                                pesan + panjang_string(pesan), 10);
        salin_string(pesan + panjang_string(pesan), " driver IC berhasil dibangun");
        notulen_catat(NOTULEN_INFO, pesan);
    }
}

/* ================================================================
 * FUNGSI KONEKSI
 * ================================================================ */

/*
 * pengembang_bangun_koneksi — Bangun koneksi ke perangkat
 * berdasarkan tipe IC yang digunakan.
 *
 * Koneksi dipilih berdasarkan subkelas PCI (tipe IC)
 * dan bukan berdasarkan vendor/brand.
 */
int pengembang_bangun_koneksi(DataPerangkat *perangkat)
{
    if (perangkat == NULL) return STATUS_PARAMETAR_SALAH;

    /* Pilih koneksi berdasarkan tipe bus dan subkelas */
    switch (perangkat->tipe_bus) {
    case BUS_PCI:
        /* Identifikasi berdasarkan kelas dan subkelas PCI */
        switch (perangkat->kelas) {
        case 0x01: /* Storage */
            if (perangkat->subkelas == 0x06) {
                /* AHCI */
                return pengembang_koneksi_ahci(perangkat);
            } else if (perangkat->subkelas == 0x08) {
                /* NVMe */
                return pengembang_koneksi_nvme(perangkat);
            }
            /* IDE atau lainnya — gunakan PCI generik */
            return pengembang_koneksi_pci(perangkat);

        case 0x0C: /* Serial bus */
            if (perangkat->subkelas == 0x03) {
                /* USB — tentukan tipe dari data_khusus */
                uint8 prog_if = perangkat->data_khusus[24];
                switch (prog_if) {
                case 0x00: return pengembang_koneksi_usb_uhci(perangkat);
                case 0x10: return pengembang_koneksi_usb_ohci(perangkat);
                case 0x20: return pengembang_koneksi_usb_ehci(perangkat);
                case 0x30: return pengembang_koneksi_usb_xhci(perangkat);
                default:   return pengembang_koneksi_usb_xhci(perangkat);
                }
            }
            return pengembang_koneksi_pci(perangkat);

        default:
            /* Perangkat PCI lainnya */
            return pengembang_koneksi_pci(perangkat);
        }

    case BUS_USB:
        return pengembang_koneksi_usb_ehci(perangkat);

    case BUS_I2C:
        return pengembang_koneksi_generik(perangkat);

    case BUS_SPI:
        return pengembang_koneksi_generik(perangkat);

    case BUS_ISA:
        /* ISA tidak memerlukan koneksi khusus */
        return STATUS_OK;

    case BUS_SATA:
        return pengembang_koneksi_ahci(perangkat);

    case BUS_NVME:
        return pengembang_koneksi_nvme(perangkat);

    default:
        return pengembang_koneksi_generik(perangkat);
    }
}

/* ================================================================
 * FUNGSI KENDALI
 * ================================================================ */

/*
 * pengembang_bangun_kendali — Bangun kendali (driver) untuk perangkat.
 * Mengidentifikasi IC chip dan menginisialisasi driver generik.
 */
int pengembang_bangun_kendali(DataPerangkat *perangkat)
{
    DataIC ic;

    if (perangkat == NULL) return STATUS_PARAMETAR_SALAH;

    /* Identifikasi IC chip */
    if (pengembang_kendali_identifikasi_ic(
            perangkat->vendor_id,
            perangkat->perangkat_id,
            perangkat->kelas,
            perangkat->subkelas,
            &ic) != STATUS_OK) {
        /* IC tidak dikenali — gunakan driver generik */
        notulen_catat(NOTULEN_PERINGATAN, "Pengembang: IC tidak dikenali, driver generik digunakan");
    }

    /* Inisialisasi driver IC */
    return pengembang_kendali_inisialisasi_ic(&ic, perangkat);
}

/*
 * pengembang_pasang_kendali — Pasang kendali ke perangkat.
 */
int pengembang_pasang_kendali(unsigned int nomor)
{
    if (nomor >= (unsigned int)jumlah_kendali) return STATUS_TIDAK_ADA;
    tabel_kendali[nomor].terpasang = 1;
    return STATUS_OK;
}

/*
 * pengembang_copot_kendali — Copot kendali dari perangkat.
 */
void pengembang_copot_kendali(unsigned int nomor)
{
    if (nomor >= (unsigned int)jumlah_kendali) return;
    tabel_kendali[nomor].terpasang = 0;
}

/*
 * pengembang_bersihkan_kendali — Bersihkan kendali ketika
 * perangkat dicabut.
 */
void pengembang_bersihkan_kendali(unsigned int nomor_port)
{
    int i;
    for (i = 0; i < jumlah_kendali; i++) {
        if (tabel_kendali[i].perangkat != NULL &&
            tabel_kendali[i].perangkat->port == nomor_port) {
            tabel_kendali[i].terpasang = 0;
            tabel_kendali[i].perangkat = NULL;
            return;
        }
    }
}

/* ================================================================
 * FUNGSI ANTARMUKA
 * ================================================================ */

/*
 * pengembang_bangun_antarmuka — Bangun antarmuka untuk lapisan atas.
 * Membuat struktur antarmuka yang menghubungkan perangkat
 * dengan IC yang sudah diidentifikasi.
 */
int pengembang_bangun_antarmuka(DataPerangkat *perangkat)
{
    DataIC ic;

    if (perangkat == NULL) return STATUS_PARAMETAR_SALAH;

    /* Identifikasi IC untuk antarmuka */
    pengembang_kendali_identifikasi_ic(
        perangkat->vendor_id,
        perangkat->perangkat_id,
        perangkat->kelas,
        perangkat->subkelas,
        &ic
    );

    /* Buat antarmuka */
    return pengembang_antarmuka_buat(perangkat, &ic);
}
