/*
 * tetikus.c
 *
 * Pemeriksaan tetikus (mouse) — deteksi melalui port aux 8042.
 * Tetikus PS/2 terhubung ke pengendali 8042 pada port aux,
 * dan menggunakan IRQ 12.
 *
 * Metode deteksi:
 *   1. Aktifkan port aux pada pengendali 8042
 *   2. Kirim perintah identifikasi ke tetikus
 *   3. Baca respons untuk menentukan tipe tetikus
 */

#include "../lampiran/arsitek.h"
#include "../lampiran/mesin.h"

/* Perintah pengendali 8042 untuk tetikus — khusus x86/x64 */
#if defined(ARSITEK_X86) || defined(ARSITEK_X64)
#define PERINTAH_AUX_AKTIFKAN    0xA8    /* Aktifkan port aux */
#define PERINTAH_AUX_NONAKTIFKAN 0xA7    /* Nonaktifkan port aux */
#define PERINTAH_AUX_UJI         0xA9    /* Uji port aux */
#define PERINTAH_TULIS_AUX       0xD4    /* Tulis ke port aux */
#endif

/* ================================================================
 * FUNGSI BANTUAN 8042 — didefinisikan sebelum pemanggilan
 * ================================================================ */

#if defined(ARSITEK_X86) || defined(ARSITEK_X64)

/* Tunggu sampai input buffer pengendali 8042 kosong */
static logika tunggu_input_bebas(void)
{
    uint8 ulangi = 255;
    while (ulangi-- > 0) {
        if ((baca_port(PAPAN_KETIK_STATUS) & PAPAN_KETIK_STATUS_INPUT) == 0)
            return BENAR;
        tunda_io();
    }
    return SALAH;
}

/* Tunggu sampai output buffer pengendali 8042 berisi data */
static logika tunggu_output_siap(void)
{
    uint8 ulangi = 255;
    while (ulangi-- > 0) {
        if (baca_port(PAPAN_KETIK_STATUS) & PAPAN_KETIK_STATUS_OUTPUT)
            return BENAR;
        tunda_io();
    }
    return SALAH;
}

#endif /* ARSITEK_X86 || ARSITEK_X64 */

/* ================================================================
 * FUNGSI UTAMA
 * ================================================================ */

int peninjau_cek_tetikus(DataPerangkat *hasil)
{
    if (hasil == NULL) {
        return STATUS_PARAMETAR_SALAH;
    }

    isi_memori(hasil, 0, sizeof(DataPerangkat));
    hasil->tipe = PERANGKAT_TETIKUS;

#if defined(ARSITEK_X86) || defined(ARSITEK_X64)
    hasil->tipe_bus = BUS_ISA;
    {
        uint8 respons;

        /* 1. Aktifkan port aux */
        if (!tunggu_input_bebas()) goto gagal;
        tulis_port(PAPAN_KETIK_PERINTAH, PERINTAH_AUX_AKTIFKAN);
        tunda_io();

        /* 2. Uji port aux */
        if (!tunggu_input_bebas()) goto gagal;
        tulis_port(PAPAN_KETIK_PERINTAH, PERINTAH_AUX_UJI);
        tunda_io();

        if (!tunggu_output_siap()) goto gagal;
        respons = baca_port(PAPAN_KETIK_DATA);
        if (respons != 0x00) {
            /* Port aux tidak berfungsi — mungkin tidak ada tetikus */
            goto gagal;
        }

        /* 3. Kirim perintah reset ke tetikus */
        if (!tunggu_input_bebas()) goto gagal;
        tulis_port(PAPAN_KETIK_PERINTAH, PERINTAH_TULIS_AUX);
        if (!tunggu_input_bebas()) goto gagal;
        tulis_port(PAPAN_KETIK_DATA, 0xFF);    /* Reset */

        if (!tunggu_output_siap()) goto gagal;
        respons = baca_port(PAPAN_KETIK_DATA);
        if (respons != 0xFA && respons != 0xAA) {
            goto gagal;
        }

        /* Baca ID tetikus */
        if (tunggu_output_siap()) {
            uint8 id = baca_port(PAPAN_KETIK_DATA);
            if (id == 0x00) {
                salin_string(hasil->nama, "Tetikus PS/2 Standar");
            } else if (id == 0x03) {
                salin_string(hasil->nama, "Tetikus PS/2 dengan Scroll");
            } else if (id == 0x04) {
                salin_string(hasil->nama, "Tetikus PS/2 5 Tombol");
            } else {
                salin_string(hasil->nama, "Tetikus PS/2");
            }
        } else {
            salin_string(hasil->nama, "Tetikus PS/2");
        }

        hasil->kondisi = KONDISI_BAIK;
        hasil->interupsi = IRQ_PAPAN_KETIK_PS2;    /* IRQ 12 */
        hasil->port = PAPAN_KETIK_DATA;

        notulen_catat(NOTULEN_INFO, "Peninjau: Tetikus PS/2 terdeteksi");
        return STATUS_OK;

gagal:
        hasil->kondisi = KONDISI_TIDAK_DIKENAL;
        salin_string(hasil->nama, "Tetikus (tidak terdeteksi)");
        return STATUS_TIDAK_ADA;
    }
#else
    salin_string(hasil->nama, "Tetikus (tidak tersedia)");
    hasil->kondisi = KONDISI_TIDAK_DIKENAL;
    return STATUS_TIDAK_ADA;
#endif
}
