/*
 * pengembang.c
 *
 * Implementasi Pengembang — pembangun driver dan antarmuka.
 * Setelah Penyedia selesai mengumpulkan sumber daya,
 * Pengembang menggunakan sumber daya itu untuk:
 *   - Membangun koneksi ke perangkat
 *   - Membangun kendali (driver) agar perangkat bisa dipakai
 *   - Membangun antarmuka agar lapisan atas bisa berbicara
 */

#include "../lampiran/arsitek.h"
#include "../lampiran/mesin.h"
#include "../lampiran/peninjau.h"

/* Penanda global */
int pengembang_siap = SALAH;

/* Tabel kendali (driver) yang sudah dibangun */
#define KENDALI_MAKSIMUM  32

static struct {
    DataPerangkat *perangkat;
    int terpasang;
} tabel_kendali[KENDALI_MAKSIMUM];

static int jumlah_kendali = 0;

/* ================================================================
 * FUNGSI UTAMA
 * ================================================================ */

void pengembang_bangun_semua(DataPerangkat daftar[], int jumlah)
{
    int i;

    notulen_catat(NOTULEN_INFO, "Pengembang: Memulai pembangunan driver...");

    isi_memori(tabel_kendali, 0, sizeof(tabel_kendali));
    jumlah_kendali = 0;

    for (i = 0; i < jumlah && jumlah_kendali < KENDALI_MAKSIMUM; i++) {
        if (pengembang_bangun_kendali(&daftar[i]) == STATUS_OK) {
            if (pengembang_bangun_antarmuka(&daftar[i]) == STATUS_OK) {
                tabel_kendali[jumlah_kendali].perangkat = &daftar[i];
                tabel_kendali[jumlah_kendali].terpasang = 1;
                jumlah_kendali++;
            }
        }
    }

    pengembang_siap = BENAR;

    {
        char pesan[64];
        salin_string(pesan, "Pengembang: ");
        konversi_uint_ke_string((uint32)jumlah_kendali,
                                pesan + panjang_string(pesan), 10);
        salin_string(pesan + panjang_string(pesan), " driver berhasil dibangun");
        notulen_catat(NOTULEN_INFO, pesan);
    }
}

int pengembang_bangun_koneksi(DataPerangkat *perangkat)
{
    if (perangkat == NULL) return STATUS_PARAMETAR_SALAH;

    /* Koneksi ke perangkat bergantung pada tipe bus */
    switch (perangkat->tipe_bus) {
    case BUS_PCI:
        /* PCI sudah dikonfigurasi oleh peninjau —
         * cukup aktifkan bus mastering dan alamat memori */
#if defined(ARSITEK_X86) || defined(ARSITEK_X64)
        {
            uint8 bus = (uint8)((perangkat->port >> 16) & 0xFF);
            uint8 dev = (uint8)((perangkat->port >> 8) & 0xFF);
            uint8 fn  = (uint8)(perangkat->port & 0xFF);

            /* Aktifkan Bus Mastering, Memory Space, I/O Space */
            {
                uint32 perintah;
                perintah = peninjau_pci_baca(bus, dev, fn, PCI_OFFSET_PERINTAH);
                perintah |= 0x0007;   /* I/O + Memory + Bus Master */
                peninjau_pci_tulis(bus, dev, fn, PCI_OFFSET_PERINTAH, perintah);
            }
        }
#endif
        break;

    case BUS_USB:
    case BUS_I2C:
    case BUS_SPI:
    case BUS_ISA:
        /* Untuk tahap awal, koneksi ke bus ini belum diimplementasikan */
        break;

    default:
        break;
    }

    return STATUS_OK;
}

int pengembang_bangun_kendali(DataPerangkat *perangkat)
{
    if (perangkat == NULL) return STATUS_PARAMETAR_SALAH;

    /* Bangun koneksi terlebih dahulu */
    if (pengembang_bangun_koneksi(perangkat) != STATUS_OK) {
        return STATUS_GAGAL;
    }

    /* Bangun kendali berdasarkan tipe perangkat */
    switch (perangkat->tipe) {
    case PERANGKAT_LAYAR:
        /* Kendali tampilan — dikelola oleh Pigura */
        break;

    case PERANGKAT_PAPAN_KETIK:
        /* Kendali papan ketik — IRQ 1 sudah dipetakan */
        break;

    case PERANGKAT_TETIKUS:
        /* Kendali tetikus — IRQ 12 sudah dipetakan */
        break;

    case PERANGKAT_PENYIMPANAN:
        /* Kendali penyimpanan — akan diimplementasikan */
        break;

    case PERANGKAT_JARINGAN:
        /* Kendali jaringan — akan diimplementasikan */
        break;

    default:
        /* Perangkat lain — kendali generik */
        break;
    }

    return STATUS_OK;
}

int pengembang_pasang_kendali(unsigned int nomor)
{
    if (nomor >= (unsigned int)jumlah_kendali) return STATUS_TIDAK_ADA;
    tabel_kendali[nomor].terpasang = 1;
    return STATUS_OK;
}

void pengembang_copot_kendali(unsigned int nomor)
{
    if (nomor >= (unsigned int)jumlah_kendali) return;
    tabel_kendali[nomor].terpasang = 0;
}

void pengembang_bersihkan_kendali(unsigned int nomor_port)
{
    int i;
    for (i = 0; i < jumlah_kendali; i++) {
        if (tabel_kendali[i].perangkat != NULL &&
            tabel_kendali[i].perangkat->port == nomor_port) {
            tabel_kendali[i].terpasang = 0;
            return;
        }
    }
}

int pengembang_bangun_antarmuka(DataPerangkat *perangkat)
{
    if (perangkat == NULL) return STATUS_PARAMETAR_SALAH;

    /* Antarmuka disediakan melalui struktur DataPerangkat
     * yang sudah berisi semua informasi yang diperlukan.
     * Lapisan atas (OS/Kontraktor) dapat mengakses
     * perangkat melalui alamat, interupsi, dan data_khusus. */

    return STATUS_OK;
}
