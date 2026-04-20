/*
 * layar.c
 *
 * Pemeriksaan layar/tampilan — deteksi adapter VGA dan framebuffer.
 * Pada x86/x64, memeriksa keberadaan VGA di alamat 0xB8000
 * dan mencoba mendapatkan informasi VBE (VESA BIOS Extensions).
 *
 * Mode tampilan yang dideteksi:
 *   - Mode Teks VGA: 80x25, alamat 0xB8000
 *   - Mode Grafis VBE: resolusi dan kedalaman warna bervariasi
 */

#include "../lampiran/arsitek.h"
#include "../lampiran/mesin.h"

int peninjau_cek_layar(DataPerangkat *hasil)
{
    if (hasil == NULL) {
        return STATUS_PARAMETAR_SALAH;
    }

    isi_memori(hasil, 0, sizeof(DataPerangkat));
    hasil->tipe = PERANGKAT_LAYAR;

#if defined(ARSITEK_X86) || defined(ARSITEK_X64)
    {
        volatile uint16 *vga_mem;
        uint16 uji;

        /* Periksa apakah memori VGA dapat diakses */
        vga_mem = (volatile uint16 *)VGA_ALAMAT;

        /* Coba tulis dan baca kembali untuk verifikasi */
        uji = *vga_mem;         /* Simpan nilai asli */
        *vga_mem = 0x5A5A;      /* Tulis pola uji */
        tunda_io();

        if (*vga_mem == 0x5A5A) {
            /* VGA terdeteksi — kembalikan nilai asli */
            *vga_mem = uji;

            hasil->kondisi = KONDISI_BAIK;
            hasil->alamat = VGA_ALAMAT;
            hasil->ukuran = (ukuran_t)(VGA_LEBAR * VGA_TINGGI * 2);
            salin_string(hasil->nama, "Pengendali Tampilan VGA");
            salin_string(hasil->pabrikan, "Standar VGA");

            /* Isi data layar ke data_khusus */
            {
                DataLayar data_layar;
                isi_memori(&data_layar, 0, sizeof(DataLayar));
                data_layar.lebar = VGA_LEBAR;
                data_layar.tinggi = VGA_TINGGI;
                data_layar.kedalaman_bit = 4;   /* 4 bit per piksel (16 warna) */
                data_layar.alamat_framebuffer = VGA_ALAMAT;
                data_layar.ukuran_framebuffer = hasil->ukuran;
                data_layar.tipe = 0;    /* Mode Teks */
                salin_memori(hasil->data_khusus, &data_layar,
                             MIN(sizeof(DataLayar), DATA_KHUSUS_PANJANG));
            }

            notulen_catat(NOTULEN_INFO, "Peninjau: Layar VGA terdeteksi");
            return STATUS_OK;
        }

        /* VGA tidak terdeteksi — mungkin headless */
        *vga_mem = uji;
    }
#endif

    hasil->kondisi = KONDISI_TIDAK_DIKENAL;
    salin_string(hasil->nama, "Tidak ada layar terdeteksi");

    notulen_catat(NOTULEN_PERINGATAN, "Peninjau: Tidak ada layar terdeteksi");
    return STATUS_TIDAK_ADA;
}
