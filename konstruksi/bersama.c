/*
 * bersama.c
 *
 * Fungsi bersama arsitektur x86/x64.
 * Berisi implementasi PIC 8259A dan PIT 8253/8254
 * yang identik antara x86 dan x64, sehingga tidak
 * perlu diduplikasi di kedua file interupsi.c.
 *
 * Fungsi-fungsi ini dipanggil oleh mesin_siapkan()
 * dan oleh modul interupsi per arsitektur.
 *
 * Catatan: File ini dikompilasi untuk x86 ATAU x64,
 * bukan keduanya sekaligus. Flag ARSITEK_X86 atau
 * ARSITEK_X64 harus didefinisikan saat kompilasi.
 */

#include "../lampiran/arsitek.h"
#include "../lampiran/mesin.h"

/* Hanya untuk arsitektur x86/x64 */
#if defined(ARSITEK_X86) || defined(ARSITEK_X64)

/* ================================================================
 * INISIALISASI PIC 8259A
 *
 * Pemetaan ulang PIC agar IRQ tidak bertabrakan
 * dengan vektor eksepsi CPU:
 *   Master: IRQ 0-7  -> INT 32-39
 *   Slave:  IRQ 8-15 -> INT 40-47
 *
 * Proses inisialisasi (ICW1-ICW4):
 *   ICW1: Mulai inisialisasi mode cascade
 *   ICW2: Alamat vektor dasar
 *   ICW3: Konfigurasi cascade
 *   ICW4: Mode prosesor (8086)
 * ================================================================ */

void mesin_siapkan_pic(void)
{
    uint8 mask_master;
    uint8 mask_slave;

    /* Simpan mask interupsi saat ini */
    mask_master = baca_port(PIC1_DATA);
    mask_slave  = baca_port(PIC2_DATA);

    /* ICW1: Mulai inisialisasi mode cascade */
    tulis_port(PIC1_PERINTAH, PIC_ICW1_INIT | PIC_ICW1_ICW4);
    tunda_io();
    tulis_port(PIC2_PERINTAH, PIC_ICW1_INIT | PIC_ICW1_ICW4);
    tunda_io();

    /* ICW2: Alamat vektor dasar */
    tulis_port(PIC1_DATA, 32);      /* Master: vektor 32-39 */
    tunda_io();
    tulis_port(PIC2_DATA, 40);      /* Slave: vektor 40-47 */
    tunda_io();

    /* ICW3: Konfigurasi cascade */
    tulis_port(PIC1_DATA, 0x04);    /* Master: slave pada IRQ2 */
    tunda_io();
    tulis_port(PIC2_DATA, 0x02);    /* Slave: identitas cascade */
    tunda_io();

    /* ICW4: Mode 8086 */
    tulis_port(PIC1_DATA, PIC_ICW4_8086);
    tunda_io();
    tulis_port(PIC2_DATA, PIC_ICW4_8086);
    tunda_io();

    /* Kembalikan mask interupsi yang disimpan */
    tulis_port(PIC1_DATA, mask_master);
    tulis_port(PIC2_DATA, mask_slave);
}

/* ================================================================
 * INISIALISASI PIT 8253/8254
 *
 * Timer sistem yang menghasilkan interupsi periodik.
 * Digunakan untuk penjadwalan tugas dan penghitungan
 * waktu sistem.
 *
 * Parameter:
 *   frekuensi_hz — frekuensi interupsi (Hz)
 *                  Nilai umum: 100 (10 ms per tick)
 *
 * Rumus pembagi:
 *   pembagi = FREKUENSI_PIT / frekuensi_hz
 * ================================================================ */

void mesin_siapkan_pit(uint32 frekuensi_hz)
{
    uint16 pembagi;

    if (frekuensi_hz == 0) {
        frekuensi_hz = 100;     /* Default: 100 Hz */
    }

    pembagi = (uint16)(PIT_FREKUENSI / frekuensi_hz);
    if (pembagi == 0) {
        pembagi = 1;
    }

    /* Kirim perintah ke PIT:
     * - Saluran 0 (timer)
     * - Mode lo/hi byte
     * - Mode gelombang kotak (rate generator) */
    tulis_port(PIT_PERINTAH, 0x36);
    tunda_io();

    /* Tulis byte rendah pembagi */
    tulis_port(PIT_SALURAN0, (uint8)(pembagi & 0xFF));
    tunda_io();

    /* Tulis byte tinggi pembagi */
    tulis_port(PIT_SALURAN0, (uint8)((pembagi >> 8) & 0xFF));
}

#endif /* ARSITEK_X86 || ARSITEK_X64 */
