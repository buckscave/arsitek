/*
 * penyedia.c
 *
 * Implementasi Penyedia — penyedia sumber daya kernel.
 * Penyedia bertugas mengumpulkan sumber daya yang dibutuhkan
 * untuk setiap perangkat yang sudah diperiksa oleh Peninjau.
 *
 * Sumber daya yang disediakan:
 *   - Tipe data sesuai arsitektur (32-bit atau 64-bit)
 *   - Alamat memori dan I/O
 *   - Jalur interupsi
 *   - Jalur bus (PCI, USB, I2C, SPI)
 *   - Ruang memori untuk kendali/driver
 */

#include "../lampiran/arsitek.h"
#include "../lampiran/mesin.h"

/* Penanda global */
int penyedia_siap = SALAH;

/* ================================================================
 * FUNGSI UTAMA
 * ================================================================ */

void penyedia_kumpulkan_semua(DataPerangkat daftar[], int jumlah)
{
    int i;
    for (i = 0; i < jumlah; i++) {
        penyedia_kumpulkan(&daftar[i], 1);
    }
    penyedia_siap = BENAR;
}

void penyedia_kumpulkan(DataPerangkat *perangkat, int jumlah)
{
    int i;
    for (i = 0; i < jumlah; i++) {
        penyedia_sediakan_tipedata(&perangkat[i]);
    }
}

void penyedia_sediakan_tipedata(DataPerangkat *perangkat)
{
    if (perangkat == NULL) return;

    /* Sesuaikan ukuran tipe data berdasarkan arsitektur */
    perangkat->data_khusus[64] = (uint8)ARSITEK_LEBAR;   /* 32 atau 64 */
    perangkat->data_khusus[65] = (uint8)(sizeof(void *)); /* Ukuran pointer */
    perangkat->data_khusus[66] = (uint8)(sizeof(int));    /* Ukuran int */
    perangkat->data_khusus[67] = (uint8)(sizeof(long));   /* Ukuran long */
}

int penyedia_sediakan_bus(DataPerangkat *perangkat, const char *jenis_bus)
{
    if (perangkat == NULL || jenis_bus == NULL) return STATUS_PARAMETAR_SALAH;

    /* Identifikasi tipe bus dari string */
    if (bandingkan_string(jenis_bus, "PCI") == 0) {
        perangkat->tipe_bus = BUS_PCI;
    } else if (bandingkan_string(jenis_bus, "USB") == 0) {
        perangkat->tipe_bus = BUS_USB;
    } else if (bandingkan_string(jenis_bus, "I2C") == 0) {
        perangkat->tipe_bus = BUS_I2C;
    } else if (bandingkan_string(jenis_bus, "SPI") == 0) {
        perangkat->tipe_bus = BUS_SPI;
    } else if (bandingkan_string(jenis_bus, "ISA") == 0) {
        perangkat->tipe_bus = BUS_ISA;
    } else {
        perangkat->tipe_bus = BUS_LAIN;
    }

    return STATUS_OK;
}

int penyedia_sediakan_alamat(DataPerangkat *perangkat, unsigned long long ukuran)
{
    void *alamat;

    if (perangkat == NULL) return STATUS_PARAMETAR_SALAH;

    alamat = penyedia_alokasi_memori(ukuran);
    if (alamat == NULL) return STATUS_GAGAL;

    perangkat->alamat = (ukuran_ptr)alamat;
    perangkat->ukuran = (ukuran_t)ukuran;

    return STATUS_OK;
}

int penyedia_sediakan_interupsi(DataPerangkat *perangkat, unsigned int nomor_interupsi)
{
    if (perangkat == NULL) return STATUS_PARAMETAR_SALAH;

    perangkat->interupsi = nomor_interupsi;
    return STATUS_OK;
}

/* Alokasi memori — didelegasikan ke konstruksi/memori.c */
/* Implementasi ada di konstruksi/x86/memori.c dan seterusnya */
