/*
 * papan_ketik.c
 *
 * Pemeriksaan papan ketik — deteksi melalui pengendali 8042.
 * Pengendali 8042 (Keyboard Controller) berada di port I/O 0x60/0x64
 * dan mengelola komunikasi dengan papan ketik PS/2.
 *
 * Metode deteksi:
 *   1. Kirim perintah uji self-test ke 8042
 *   2. Kirim perintah identifikasi ke papan ketik
 *   3. Baca respons untuk menentukan tipe
 */

#include "../lampiran/arsitek.h"
#include "../lampiran/mesin.h"

/* ================================================================
 * FUNGSI BANTUAN 8042 — hanya tersedia pada x86/x64
 * ================================================================ */

#if defined(ARSITEK_X86) || defined(ARSITEK_X64)

/*
 * tunggu_input_bebas — Tunggu sampai buffer input 8042 kosong.
 * Mengembalikan BENAR jika berhasil, SALAH jika timeout.
 */
static logika tunggu_input_bebas(void)
{
    uint8 ulangi = 255;
    while (ulangi-- > 0) {
        if ((baca_port(PAPAN_KETIK_STATUS) & PAPAN_KETIK_STATUS_INPUT) == 0) {
            return BENAR;
        }
        tunda_io();
    }
    return SALAH;
}

/*
 * tunggu_output_siap — Tunggu sampai data output siap dibaca.
 * Mengembalikan BENAR jika berhasil, SALAH jika timeout.
 */
static logika tunggu_output_siap(void)
{
    uint8 ulangi = 255;
    while (ulangi-- > 0) {
        if (baca_port(PAPAN_KETIK_STATUS) & PAPAN_KETIK_STATUS_OUTPUT) {
            return BENAR;
        }
        tunda_io();
    }
    return SALAH;
}

/*
 * kirim_perintah_papan_ketik — Kirim perintah ke papan ketik.
 * Mengembalikan BENAR jika ACK diterima.
 */
static logika kirim_perintah_papan_ketik(uint8 perintah)
{
    uint8 respons;

    if (!tunggu_input_bebas()) return SALAH;
    tulis_port(PAPAN_KETIK_DATA, perintah);

    if (!tunggu_output_siap()) return SALAH;
    respons = baca_port(PAPAN_KETIK_DATA);

    return (respons == PAPAN_KETIK_BALASAN_KEY) ? BENAR : SALAH;
}

#endif /* ARSITEK_X86 || ARSITEK_X64 */

/* ================================================================
 * FUNGSI PUBLIK
 * ================================================================ */

int peninjau_cek_papan_ketik(DataPerangkat *hasil)
{
    if (hasil == NULL) {
        return STATUS_PARAMETAR_SALAH;
    }

    isi_memori(hasil, 0, sizeof(DataPerangkat));
    hasil->tipe = PERANGKAT_PAPAN_KETIK;
    hasil->tipe_bus = BUS_ISA;

#if defined(ARSITEK_X86) || defined(ARSITEK_X64)
    {
        uint8 respons;

        /* Kirim perintah self-test ke pengendali 8042 */
        if (!tunggu_input_bebas()) {
            hasil->kondisi = KONDISI_RUSAK;
            salin_string(hasil->nama, "Papan Ketik (8042 tidak respons)");
            return STATUS_GAGAL;
        }
        tulis_port(PAPAN_KETIK_PERINTAH, 0xAA);    /* Self-test */

        if (!tunggu_output_siap()) {
            hasil->kondisi = KONDISI_RUSAK;
            return STATUS_GAGAL;
        }
        respons = baca_port(PAPAN_KETIK_DATA);

        if (respons != 0x55) {
            /* Self-test gagal */
            hasil->kondisi = KONDISI_RUSAK;
            salin_string(hasil->nama, "Papan Ketik (self-test gagal)");
            return STATUS_GAGAL;
        }

        /* Aktifkan papan ketik */
        if (!kirim_perintah_papan_ketik(PAPAN_KETIK_CMD_AKTIFKAN)) {
            hasil->kondisi = KONDISI_RUSAK;
            salin_string(hasil->nama, "Papan Ketik (aktifkan gagal)");
            return STATUS_GAGAL;
        }

        /* Kirim perintah identifikasi */
        if (kirim_perintah_papan_ketik(PAPAN_KETIK_CMD_IDENTIFIKASI)) {
            uint8 id_byte0 = 0, id_byte1 = 0;

            if (tunggu_output_siap()) {
                id_byte0 = baca_port(PAPAN_KETIK_DATA);
            }
            if (tunggu_output_siap()) {
                id_byte1 = baca_port(PAPAN_KETIK_DATA);
            }

            /* Identifikasi tipe papan ketik */
            if (id_byte0 == 0xAB && id_byte1 == 0x41) {
                salin_string(hasil->nama, "Papan Ketik MF2 (122 tombol)");
            } else if (id_byte0 == 0xAB && id_byte1 == 0x83) {
                salin_string(hasil->nama, "Papan Ketik MF2 (101/102 tombol)");
            } else if (id_byte0 == 0xAB && id_byte1 == 0xC1) {
                salin_string(hasil->nama, "Papan Ketik IBM Sun");
            } else {
                salin_string(hasil->nama, "Papan Ketik PS/2");
            }
        } else {
            salin_string(hasil->nama, "Papan Ketik PS/2");
        }

        hasil->kondisi = KONDISI_BAIK;
        hasil->interupsi = IRQ_PAPAN_KETIK;    /* IRQ 1 */
        hasil->port = PAPAN_KETIK_DATA;

        notulen_catat(NOTULEN_INFO, "Peninjau: Papan ketik terdeteksi");
        return STATUS_OK;
    }
#else
    salin_string(hasil->nama, "Papan Ketik (tidak tersedia pada platform ini)");
    hasil->kondisi = KONDISI_TIDAK_DIKENAL;
    return STATUS_TIDAK_ADA;
#endif
}
