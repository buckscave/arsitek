/*
 * pigura.h
 *
 * Pigura adalah framebuffer yang ditingkatkan.
 * Diolah oleh CPU + GPU, berfungsi sebagai lapisan
 * antara kernel Arsitek dan OS (Kontraktor).
 *
 * Konsep serupa: DRM/KMS pada Unix/Linux.
 *
 * Pigura mendukung dua mode operasi:
 *   1. Mode Teks — menggunakan memori VGA standar (0xB8000)
 *   2. Mode Grafis — menggunakan linear framebuffer VBE
 *
 * Mode teks digunakan pada tahap awal booting,
 * mode grafis digunakan setelah VBE diinisialisasi.
 */

#ifndef PIGURA_H
#define PIGURA_H

#include "arsitek.h"

/* ================================================================
 * KONSTANTA PIGURA
 * ================================================================ */

/* Mode tampilan */
#define PIGURA_MODE_TEKS      0
#define PIGURA_MODE_GRAFIS    1

/* Warna teks VGA (4-bit) */
#define VGA_HITAM          0
#define VGA_BIRU           1
#define VGA_HIJAU          2
#define VGA_CYAN           3
#define VGA_MERAH          4
#define VGA_MAGENTA        5
#define VGA_COKLAT         6
#define VGA_ABU_TERANG     7
#define VGA_ABU_GELAP      8
#define VGA_BIRU_TERANG    9
#define VGA_HIJAU_TERANG   10
#define VGA_CYAN_TERANG    11
#define VGA_MERAH_TERANG   12
#define VGA_MAGENTA_TERANG 13
#define VGA_KUNING         14
#define VGA_PUTIH          15

/* Ukuran font bitmap standar */
#define FONT_LEBAR         8
#define FONT_TINGGI        16
#define FONT_BYTES_PER_GLYPH  16   /* 16 baris per glyph, 1 byte per baris */

/* Jumlah karakter ASCII yang didukung font */
#define FONT_JUMLAH_GLYPH  128

/* Batas layar konsol */
#define KONSOLE_LEBAR      80
#define KONSOLE_TINGGI     25

/* ================================================================
 * STRUKTUR PIGURA
 * ================================================================ */

/* Entri VGA teks — satu sel karakter */
typedef struct DIKEMAS {
    uint8 karakter;       /* Kode ASCII karakter */
    uint8 warna;          /* Byte warna (depan | latar << 4) */
} EntriVGA;

/* Status konsol pigura */
typedef struct {
    uint32     kursor_x;         /* Posisi kursor kolom (0..lebar-1) */
    uint32     kursor_y;         /* Posisi kursor baris (0..tinggi-1) */
    uint32     lebar;            /* Lebar layar dalam kolom */
    uint32     tinggi;           /* Tinggi layar dalam baris */
    uint8      warna_depan;      /* Warna teks aktif */
    uint8      warna_latar;      /* Warna latar belakang aktif */
    uint8      mode;             /* Mode tampilan (teks/grafis) */
    ukuran_ptr alamat;           /* Alamat framebuffer */
    logika     kursor_nyala;     /* Apakah kursor ditampilkan */
} KonsolPigura;

/* ================================================================
 * FUNGSI PIGURA
 * ================================================================ */

/* Inisialisasi pigura — dipanggil sekali saat kernel mulai */
int  pigura_mulai(void);

/* ---- Operasi Konsol Teks ---- */

/* Tulis satu karakter ke konsol */
void pigura_tulis_char(char c);

/* Tulis string null-terminated ke konsol */
void pigura_tulis_string(const char *str);

/* Tulis sejumlah karakter dari buffer */
void pigura_tulis_baris(const char *str, ukuran_t panjang);

/* Hapus layar (isi dengan spasi) */
void pigura_hapus_layar(void);

/* Gulir layar ke atas satu baris */
void pigura_gulir_atas(void);

/* Perbarui posisi kursor hardware */
void pigura_perbarui_kursor(void);

/* Atur warna teks dan latar belakang */
void pigura_set_warna(uint8 depan, uint8 latar);

/* Atur posisi kursor secara manual */
void pigura_set_posisi(uint32 x, uint32 y);

/* ---- Operasi Mode Grafis ---- */

/* Masuk ke mode grafis dengan resolusi tertentu */
int  pigura_masuk_grafis(uint32 lebar, uint32 tinggi, uint32 bpp);

/* Gambar satu piksel pada koordinat tertentu */
void pigura_gambar_piksel(uint32 x, uint32 y, WarnaRGBA warna);

/* Gambar kotak berwarna penuh */
void pigura_gambar_kotak(uint32 x, uint32 y, uint32 lebar, uint32 tinggi,
                         WarnaRGBA warna);

/* Gambar garis dari titik (x1,y1) ke (x2,y2) — algoritma Bresenham */
void pigura_gambar_garis(uint32 x1, uint32 y1, uint32 x2, uint32 y2,
                         WarnaRGBA warna);

/* Hapus seluruh layar dengan warna tertentu */
void pigura_hapus_layar_warna(WarnaRGBA warna);

/* Gambar satu karakter bitmap pada posisi piksel tertentu */
void pigura_gambar_char(char c, uint32 x, uint32 y,
                        WarnaRGBA depan, WarnaRGBA latar);

/* Tukar buffer (jika menggunakan double buffering) */
void pigura_perbarui(void);

/* ---- Fungsi Kueri ---- */

/* Dapatkan mode tampilan aktif */
ModePigura *pigura_dapatkan_mode(void);

/* Dapatkan status konsol */
KonsolPigura *pigura_dapatkan_konsol(void);

/* ---- Font Bitmap ---- */

/* Data font bitmap 8x16 (ASCII 0-127) */
extern const uint8 font_bitmap[FONT_JUMLAH_GLYPH][FONT_BYTES_PER_GLYPH];

#endif /* PIGURA_H */
