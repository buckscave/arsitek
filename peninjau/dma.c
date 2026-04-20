/*
 * dma.c
 *
 * Pemeriksaan pengendali DMA (Direct Memory Access) 8237.
 * Pengendali DMA pada x86 berada di port I/O 0x00-0x0F
 * (channel 0-3) dan 0xC0-0xDF (channel 4-7).
 *
 * Pada arsitektur lain, DMA mungkin tidak tersedia
 * atau menggunakan mekanisme berbeda.
 */

#include "../lampiran/arsitek.h"
#include "../lampiran/mesin.h"

/* Port I/O DMA 8237 — khusus x86/x64 */
#if defined(ARSITEK_X86) || defined(ARSITEK_X64)
#define DMA1_PERINTAH    0x08    /* Channel 0-3: Command register */
#define DMA1_STATUS      0x08    /* Channel 0-3: Status register */
#define DMA1_MAKS        0x0D    /* Channel 0-3: Mask register */
#endif

int peninjau_cek_dma(DataPerangkat *hasil)
{
    if (hasil == NULL) {
        return STATUS_PARAMETAR_SALAH;
    }

    isi_memori(hasil, 0, sizeof(DataPerangkat));
    hasil->tipe = PERANGKAT_DMA;

#if defined(ARSITEK_X86) || defined(ARSITEK_X64)
    {
        uint8 status;

        /* Baca register status DMA — jika dapat dibaca,
         * pengendali DMA kemungkinan ada */
        tulis_port(DMA1_MAKS, 0x0F);    /* Mask semua channel */
        tunda_io();

        status = baca_port(DMA1_STATUS);
        tunda_io();

        /* DMA controller selalu ada pada PC standar */
        hasil->kondisi = KONDISI_BAIK;
        hasil->port = 0x00;
        hasil->ukuran = 16;    /* 8 channel × 2 byte */
        salin_string(hasil->nama, "Pengendali DMA 8237");
        salin_string(hasil->pabrikan, "Intel");

        /* Simpan status DMA ke data_khusus */
        hasil->data_khusus[0] = status;

        notulen_catat(NOTULEN_INFO, "Peninjau: Pengendali DMA terdeteksi");
        return STATUS_OK;
    }
#else
    salin_string(hasil->nama, "DMA (tidak tersedia)");
    hasil->kondisi = KONDISI_TIDAK_DIKENAL;
    return STATUS_TIDAK_ADA;
#endif
}
