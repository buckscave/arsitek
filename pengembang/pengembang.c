/*
 * pengembang.c
 *
 * Modul koordinator utama Pengembang — pembangun driver
 * dan antarmuka perangkat keras.
 *
 * Setelah Penyedia selesai mengumpulkan sumber daya,
 * Pengembang menggunakan sumber daya itu untuk:
 *   - Membangun koneksi ke perangkat via bus yang sesuai
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

/* ================================================================
 * PENANDA GLOBAL
 * ================================================================ */

int pengembang_siap = SALAH;

/* ================================================================
 * TABEL KENDALI INTERNAL
 *
 * Menyimpan AntarmukaPerangkat yang sudah dibangun untuk
 * setiap perangkat yang berhasil diinisialisasi.
 * ================================================================ */

#define KENDALI_MAKSIMUM  64

static AntarmukaPerangkat tabel_antarmuka_internal[KENDALI_MAKSIMUM];
static int jumlah_antarmuka_internal = 0;

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
extern int pengembang_koneksi_i2c(DataPerangkat *perangkat);
extern int pengembang_koneksi_spi(DataPerangkat *perangkat);
extern int pengembang_koneksi_uart(DataPerangkat *perangkat);
extern int pengembang_koneksi_generik(DataPerangkat *perangkat);

/* Dari kendali.c */
extern DataIC *pengembang_kendali_identifikasi_ic(uint16 vendor_id,
    uint16 perangkat_id, uint8 kelas, uint8 subkelas);
extern int pengembang_kendali_inisialisasi_ic(DataIC *ic,
    DataPerangkat *perangkat);
extern logika pengembang_kendali_verifikasi_ic(DataIC *ic,
    DataPerangkat *perangkat);
extern int pengembang_kendali_set_parameter(DataIC *ic,
    DataPerangkat *perangkat);
extern int pengembang_kendali_reset_ic(DataIC *ic,
    DataPerangkat *perangkat);
extern int pengembang_kendali_diagnostik(DataIC *ic,
    DataPerangkat *perangkat);

/* Dari antarmuka.c */
extern int pengembang_antarmuka_buat(DataPerangkat *perangkat,
    DataIC *ic, AntarmukaPerangkat *antarmuka);

/* ================================================================
 * pengembang_bangun_semua — Bangun semua driver untuk semua perangkat
 *
 * Iterasi melalui daftar perangkat. Untuk setiap perangkat,
 * jalankan tiga tahap:
 *   1. bangun_koneksi  — siapkan jalur bus ke perangkat
 *   2. bangun_kendali  — identifikasi IC dan inisialisasi driver
 *   3. bangun_antarmuka — buat antarmuka untuk lapisan atas
 *
 * Setiap tahap dicatat ke notulen. Jika salah satu tahap gagal,
 * perangkat tersebut dilewati dan dilanjutkan ke perangkat berikutnya.
 *
 * Parameter:
 *   daftar  — array DataPerangkat yang akan dibangun
 *   jumlah  — jumlah entri dalam array daftar
 * ================================================================ */
void pengembang_bangun_semua(DataPerangkat daftar[], int jumlah)
{
    int i;
    char pesan[128];

    notulen_catat(NOTULEN_INFO,
        "Pengembang: Memulai pembangunan driver berbasis IC...");

    /* Bersihkan tabel antarmuka internal */
    isi_memori(tabel_antarmuka_internal, 0,
               sizeof(tabel_antarmuka_internal));
    jumlah_antarmuka_internal = 0;

    /* Iterasi semua perangkat */
    for (i = 0; i < jumlah; i++) {
        /* Tahap 1: Bangun koneksi ke perangkat */
        if (pengembang_bangun_koneksi(&daftar[i]) != STATUS_OK) {
            notulen_catat(NOTULEN_KESALAHAN,
                "Pengembang: Gagal membangun koneksi, lewati perangkat");
            continue;
        }

        /* Tahap 2: Bangun kendali (driver) untuk perangkat */
        if (pengembang_bangun_kendali(&daftar[i]) != STATUS_OK) {
            notulen_catat(NOTULEN_KESALAHAN,
                "Pengembang: Gagal membangun kendali, lewati perangkat");
            continue;
        }

        /* Tahap 3: Bangun antarmuka untuk lapisan atas */
        if (pengembang_bangun_antarmuka(&daftar[i]) != STATUS_OK) {
            notulen_catat(NOTULEN_KESALAHAN,
                "Pengembang: Gagal membangun antarmuka, lewati perangkat");
            continue;
        }

        notulen_catat_perangkat(NOTULEN_INFO,
            "Pengembang: Perangkat berhasil dibangun", &daftar[i]);
    }

    /* Tandai pengembang siap */
    pengembang_siap = BENAR;

    /* Catat ringkasan */
    salin_string(pesan, "Pengembang: Selesai — ");
    konversi_uint_ke_string((uint32)jumlah_antarmuka_internal,
                            pesan + panjang_string(pesan), 10);
    salin_string(pesan + panjang_string(pesan),
                 " antarmuka perangkat berhasil dibangun");
    notulen_catat(NOTULEN_INFO, pesan);
}

/* ================================================================
 * pengembang_bangun_koneksi — Bangun koneksi ke satu perangkat
 *
 * Memilih fungsi koneksi yang sesuai berdasarkan tipe bus
 * dan subkelas PCI. Koneksi dipilih berdasarkan jenis IC,
 * bukan berdasarkan vendor/brand.
 *
 * Parameter:
 *   perangkat — pointer ke DataPerangkat
 *
 * Mengembalikan:
 *   STATUS_OK jika koneksi berhasil,
 *   STATUS_PARAMETAR_SALAH jika perangkat NULL
 * ================================================================ */
int pengembang_bangun_koneksi(DataPerangkat *perangkat)
{
    if (perangkat == NULL) {
        return STATUS_PARAMETAR_SALAH;
    }

    /* Pilih koneksi berdasarkan tipe bus dan kelas PCI */
    switch (perangkat->tipe_bus) {
    case BUS_PCI:
        /* Identifikasi berdasarkan kelas dan subkelas PCI */
        switch (perangkat->kelas) {
        case 0x01:
            /* Kelas penyimpanan */
            if (perangkat->subkelas == 0x06) {
                return pengembang_koneksi_ahci(perangkat);
            } else if (perangkat->subkelas == 0x08) {
                return pengembang_koneksi_nvme(perangkat);
            }
            /* IDE atau lainnya */
            return pengembang_koneksi_pci(perangkat);

        case 0x07:
            /* Kelas komunikasi serial */
            return pengembang_koneksi_uart(perangkat);

        case 0x0C:
            /* Kelas bus serial */
            if (perangkat->subkelas == 0x03) {
                /* USB — tentukan tipe dari prog_if */
                uint8 prog_if = perangkat->data_khusus[24];
                switch (prog_if) {
                case 0x00:
                    return pengembang_koneksi_usb_uhci(perangkat);
                case 0x10:
                    return pengembang_koneksi_usb_ohci(perangkat);
                case 0x20:
                    return pengembang_koneksi_usb_ehci(perangkat);
                case 0x30:
                    return pengembang_koneksi_usb_xhci(perangkat);
                default:
                    return pengembang_koneksi_usb_xhci(perangkat);
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
        return pengembang_koneksi_i2c(perangkat);

    case BUS_SPI:
        return pengembang_koneksi_spi(perangkat);

    case BUS_ISA:
        /* ISA tidak memerlukan koneksi khusus */
        notulen_catat(NOTULEN_INFO,
            "Pengembang: Perangkat ISA, koneksi tidak diperlukan");
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
 * pengembang_bangun_kendali — Bangun kendali (driver) untuk perangkat
 *
 * Mengidentifikasi IC chip, menginisialisasi driver generik,
 * mengatur parameter optimal, dan menjalankan diagnostik.
 *
 * Parameter:
 *   perangkat — pointer ke DataPerangkat
 *
 * Mengembalikan:
 *   STATUS_OK jika kendali berhasil dibangun
 * ================================================================ */
int pengembang_bangun_kendali(DataPerangkat *perangkat)
{
    DataIC *ic;
    int hasil;

    if (perangkat == NULL) {
        return STATUS_PARAMETAR_SALAH;
    }

    /* 1. Identifikasi IC chip */
    ic = pengembang_kendali_identifikasi_ic(
        perangkat->vendor_id,
        perangkat->perangkat_id,
        perangkat->kelas,
        perangkat->subkelas
    );

    if (ic == NULL) {
        notulen_catat(NOTULEN_PERINGATAN,
            "Pengembang: IC tidak dikenali, driver generik digunakan");
        return STATUS_GAGAL;
    }

    /* 2. Inisialisasi driver IC */
    hasil = pengembang_kendali_inisialisasi_ic(ic, perangkat);
    if (hasil != STATUS_OK) {
        notulen_catat(NOTULEN_KESALAHAN,
            "Pengembang: Gagal menginisialisasi IC");
        return STATUS_GAGAL;
    }

    /* 3. Atur parameter ke nilai optimal */
    hasil = pengembang_kendali_set_parameter(ic, perangkat);
    if (hasil != STATUS_OK) {
        notulen_catat(NOTULEN_PERINGATAN,
            "Pengembang: Gagal mengatur parameter IC, gunakan default");
        /* Bukan kesalahan fatal — lanjutkan */
    }

    /* 4. Verifikasi IC merespons dengan benar */
    if (!pengembang_kendali_verifikasi_ic(ic, perangkat)) {
        notulen_catat(NOTULEN_PERINGATAN,
            "Pengembang: Verifikasi IC gagal, IC mungkin tidak merespons");
    }

    /* 5. Jalankan diagnostik ringan */
    hasil = pengembang_kendali_diagnostik(ic, perangkat);
    if (hasil != STATUS_OK) {
        notulen_catat(NOTULEN_PERINGATAN,
            "Pengembang: Diagnostik IC menemukan masalah");
    }

    return STATUS_OK;
}

/* ================================================================
 * pengembang_pasang_kendali — Pasang kendali ke perangkat
 *
 * Mengaktifkan antarmuka perangkat pada nomor tertentu
 * sehingga dapat digunakan oleh lapisan atas.
 *
 * Parameter:
 *   nomor — nomor urut antarmuka dalam tabel internal
 *
 * Mengembalikan:
 *   STATUS_OK jika berhasil,
 *   STATUS_TIDAK_ADA jika nomor di luar jangkauan
 * ================================================================ */
int pengembang_pasang_kendali(unsigned int nomor)
{
    if (nomor >= (unsigned int)jumlah_antarmuka_internal) {
        return STATUS_TIDAK_ADA;
    }

    /* Tandai antarmuka sebagai aktif */
    tabel_antarmuka_internal[nomor].terkunci = 0;
    tabel_antarmuka_internal[nomor].kondisi = KONDISI_BAIK;

    {
        char pesan[80];
        salin_string(pesan, "Pengembang: Kendali dipasang #");
        konversi_uint_ke_string(nomor,
                                pesan + panjang_string(pesan), 10);
        notulen_catat(NOTULEN_INFO, pesan);
    }

    return STATUS_OK;
}

/* ================================================================
 * pengembang_copot_kendali — Copot kendali dari perangkat
 *
 * Menonaktifkan antarmuka perangkat pada nomor tertentu.
 * Perangkat tidak dapat diakses oleh lapisan atas
 * sampai dipasang kembali.
 *
 * Parameter:
 *   nomor — nomor urut antarmuka dalam tabel internal
 * ================================================================ */
void pengembang_copot_kendali(unsigned int nomor)
{
    if (nomor >= (unsigned int)jumlah_antarmuka_internal) {
        return;
    }

    /* Tandai antarmuka sebagai tidak aktif */
    tabel_antarmuka_internal[nomor].terkunci = 1;
    tabel_antarmuka_internal[nomor].kondisi = KONDISI_TERPUTUS;

    {
        char pesan[80];
        salin_string(pesan, "Pengembang: Kendali dicopot #");
        konversi_uint_ke_string(nomor,
                                pesan + panjang_string(pesan), 10);
        notulen_catat(NOTULEN_INFO, pesan);
    }
}

/* ================================================================
 * pengembang_bersihkan_kendali — Bersihkan kendali ketika
 * perangkat dicabut (hot-unplug)
 *
 * Mencari antarmuka yang terkait dengan nomor port tertentu
 * dan menghapus seluruh datanya dari tabel internal.
 *
 * Parameter:
 *   nomor_port — nomor port perangkat yang dicabut
 * ================================================================ */
void pengembang_bersihkan_kendali(unsigned int nomor_port)
{
    int i;

    for (i = 0; i < jumlah_antarmuka_internal; i++) {
        if (tabel_antarmuka_internal[i].perangkat != NULL &&
            tabel_antarmuka_internal[i].perangkat->port == nomor_port) {
            /* Bersihkan entri antarmuka */
            isi_memori(&tabel_antarmuka_internal[i], 0,
                       sizeof(AntarmukaPerangkat));
            tabel_antarmuka_internal[i].kondisi = KONDISI_TERPUTUS;

            {
                char pesan[80];
                salin_string(pesan,
                    "Pengembang: Kendali dibersihkan untuk port ");
                konversi_uint_ke_string(nomor_port,
                                        pesan + panjang_string(pesan), 10);
                notulen_catat(NOTULEN_INFO, pesan);
            }
            return;
        }
    }
}

/* ================================================================
 * pengembang_bangun_antarmuka — Bangun antarmuka untuk lapisan atas
 *
 * Mengidentifikasi IC chip dan membuat struktur AntarmukaPerangkat
 * yang menghubungkan perangkat dengan IC teridentifikasi.
 * Antarmuka ini akan digunakan oleh lapisan Kontraktor/OS.
 *
 * Parameter:
 *   perangkat — pointer ke DataPerangkat
 *
 * Mengembalikan:
 *   STATUS_OK jika antarmuka berhasil dibuat,
 *   STATUS_GAGAL jika tabel penuh atau IC tidak dikenali
 * ================================================================ */
int pengembang_bangun_antarmuka(DataPerangkat *perangkat)
{
    DataIC *ic;
    int hasil;

    if (perangkat == NULL) {
        return STATUS_PARAMETAR_SALAH;
    }

    /* Pastikan tabel tidak penuh */
    if (jumlah_antarmuka_internal >= KENDALI_MAKSIMUM) {
        notulen_catat(NOTULEN_KESALAHAN,
            "Pengembang: Tabel antarmuka penuh");
        return STATUS_GAGAL;
    }

    /* Identifikasi IC untuk antarmuka */
    ic = pengembang_kendali_identifikasi_ic(
        perangkat->vendor_id,
        perangkat->perangkat_id,
        perangkat->kelas,
        perangkat->subkelas
    );

    if (ic == NULL) {
        notulen_catat(NOTULEN_KESALAHAN,
            "Pengembang: Tidak dapat mengidentifikasi IC untuk antarmuka");
        return STATUS_GAGAL;
    }

    /* Buat antarmuka menggunakan fungsi dari antarmuka.c */
    hasil = pengembang_antarmuka_buat(perangkat, ic,
        &tabel_antarmuka_internal[jumlah_antarmuka_internal]);

    if (hasil == STATUS_OK) {
        jumlah_antarmuka_internal++;

        {
            char pesan[128];
            salin_string(pesan, "Pengembang: Antarmuka dibuat untuk ");
            salin_string(pesan + panjang_string(pesan), ic->nama_ic);
            notulen_catat(NOTULEN_INFO, pesan);
        }
    }

    return hasil;
}
