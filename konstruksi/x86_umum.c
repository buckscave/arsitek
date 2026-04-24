/*
 * x86_umum.c
 *
 * Implementasi fungsi-fungsi umum x86/x64.
 * Modul ini berisi kode yang dibagikan antara
 * arsitektur x86 (IA-32) dan x64 (AMD64/Intel 64),
 * sehingga menghilangkan duplikasi yang sebelumnya
 * ada di x86/interupsi.c dan x64/interupsi.c.
 *
 * Fungsi yang disediakan:
 *   - Pengisian entri GDT (tabel deskriptor global)
 *   - Pengisian entri IDT 32-bit dan 64-bit
 *   - Inisialisasi PIC 8259A (remap IRQ)
 *   - Inisialisasi PIT 8254 (timer sistem)
 *   - Pengiriman EOI (End of Interrupt)
 */

#include "../lampiran/arsitek.h"
#include "../lampiran/mesin.h"
#include "../lampiran/x86_umum.h"

#if defined(ARSITEK_X86) || defined(ARSITEK_X64)

/* ================================================================
 * BANTUAN GDT
 * ================================================================ */

/*
 * x86_umum_isi_entri_gdt - Isi satu entri GDT.
 *
 * Entri GDT mengenkode alamat dasar, batas, hak akses,
 * dan flag granularitas ke dalam format 8-byte yang
 * dipahami oleh prosesor x86/x64.
 *
 * Batas segmen dienkode dalam 20 bit:
 *   - Bit 0-15  berada di field batas_bawah
 *   - Bit 16-19 berada di field bendera_batas
 * Dasar segmen dienkode dalam 32 bit:
 *   - Bit 0-15  berada di field dasar_bawah
 *   - Bit 16-23 berada di field dasar_tengah
 *   - Bit 24-31 berada di field dasar_atas
 */
void x86_umum_isi_entri_gdt(EntriGDT *entri,
    uint32 dasar, uint32 batas,
    uint8 akses, uint8 bendera_batas)
{
    entri->batas_bawah = (uint16)(batas & 0xFFFF);
    entri->dasar_bawah = (uint16)(dasar & 0xFFFF);
    entri->dasar_tengah = (uint8)((dasar >> 16) & 0xFF);
    entri->akses = akses;
    entri->bendera_batas =
        (uint8)((batas >> 16) & 0x0F);
    entri->bendera_batas |= bendera_batas;
    entri->dasar_atas = (uint8)((dasar >> 24) & 0xFF);
}

/* ================================================================
 * BANTUAN IDT 32-BIT (x86)
 * ================================================================ */

/*
 * x86_umum_isi_entri_idt32 - Isi satu entri IDT 32-bit.
 *
 * Pada x86, setiap entri IDT berukuran 8 byte.
 * Alamat handler terbagi menjadi:
 *   - Bit 0-15  berada di field batas_bawah
 *   - Bit 16-31 berada di field dasar_atas
 * Field dasar_bawah digunakan ulang untuk bagian
 * bawah handler (sama dengan batas_bawah).
 */
void x86_umum_isi_entri_idt32(EntriIDT *entri,
    ukuran_ptr handler, uint16 selektor,
    uint8 tipe)
{
    entri->batas_bawah =
        (uint16)(handler & 0xFFFF);
    entri->dasar_bawah =
        (uint16)(handler & 0xFFFF);
    entri->dasar_tengah =
        (uint8)((handler >> 16) & 0xFF);
    entri->akses = tipe;
    entri->bendera = 0x00;
    entri->dasar_atas =
        (uint8)((handler >> 24) & 0xFF);
}

/* ================================================================
 * BANTUAN IDT 64-BIT (x64)
 * ================================================================ */

/*
 * x86_umum_isi_entri_idt64 - Isi satu entri IDT 64-bit.
 *
 * Pada x64, setiap entri IDT berukuran 16 byte.
 * Alamat handler 64-bit terbagi menjadi:
 *   - Bit 0-15  berada di field batas_bawah
 *   - Bit 16-31 berada di field dasar_atas
 *   - Bit 32-63 berada di field dasar_lanjutan
 * Field IST menentukan Interrupt Stack Table.
 */
void x86_umum_isi_entri_idt64(EntriIDT *entri,
    uint64 handler, uint16 selektor,
    uint8 tipe, uint8 ist)
{
    entri->batas_bawah =
        (uint16)(handler & 0xFFFF);
    entri->dasar_bawah =
        (uint16)(handler & 0xFFFF);
    entri->dasar_tengah =
        (uint8)((handler >> 16) & 0xFF);
    entri->akses = tipe;
    entri->bendera = (uint8)((ist & 0x07) << 0);
    entri->dasar_atas =
        (uint8)((handler >> 24) & 0xFF);
    entri->dasar_lanjutan =
        (uint32)((handler >> 32) & 0xFFFFFFFF);
    entri->dicadangkan = 0;
}

/* ================================================================
 * INISIALISASI PIC 8259A
 * ================================================================ */

/*
 * x86_umum_siapkan_pic - Pemetaan ulang PIC 8259A.
 *
 * Secara bawaan, PIC master menggunakan IRQ 8-15 dan
 * PIC slave menggunakan IRQ 0-7, yang bertabrakan
 * dengan vektor eksepsi CPU. Kita memetakan ulang:
 *   Master: IRQ 0-7  -> INT 32-39
 *   Slave:  IRQ 8-15 -> INT 40-47
 *
 * Proses inisialisasi PIC (ICW1-ICW4):
 *   ICW1: Mulai inisialisasi, ICW4 diperlukan
 *   ICW2: Alamat vektor dasar
 *   ICW3: Konfigurasi cascade
 *   ICW4: Mode prosesor (8086)
 */
void x86_umum_siapkan_pic(void)
{
    uint8 mask_master = baca_port(PIC1_DATA);
    uint8 mask_slave  = baca_port(PIC2_DATA);

    /* ICW1: Mulai inisialisasi mode cascade */
    tulis_port(PIC1_PERINTAH,
               PIC_ICW1_INIT | PIC_ICW1_ICW4);
    tunda_io();
    tulis_port(PIC2_PERINTAH,
               PIC_ICW1_INIT | PIC_ICW1_ICW4);
    tunda_io();

    /* ICW2: Alamat vektor dasar */
    tulis_port(PIC1_DATA, 32);
    tunda_io();
    tulis_port(PIC2_DATA, 40);
    tunda_io();

    /* ICW3: Konfigurasi cascade */
    tulis_port(PIC1_DATA, 0x04);
    tunda_io();
    tulis_port(PIC2_DATA, 0x02);
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
 * INISIALISASI PIT 8254
 * ================================================================ */

/*
 * x86_umum_siapkan_pit - Siapkan timer sistem.
 *
 * PIT menghasilkan interupsi periodik pada frekuensi
 * yang ditentukan. Frekuensi dasar PIT adalah
 * 1193182 Hz. Pembagi dihitung dari:
 *   pembagi = FREKUENSI_PIT / frekuensi_hz
 *
 * Parameter:
 *   frekuensi_hz - frekuensi interupsi (Hz).
 *                  Default jika 0: 100 Hz
 */
void x86_umum_siapkan_pit(uint32 frekuensi_hz)
{
    uint16 pembagi;

    if (frekuensi_hz == 0) {
        frekuensi_hz = 100;
    }

    pembagi = (uint16)(PIT_FREKUENSI / frekuensi_hz);
    if (pembagi == 0) {
        pembagi = 1;
    }

    /* Perintah PIT: saluran 0, lo/hi, gelombang kotak */
    tulis_port(PIT_PERINTAH, 0x36);

    /* Tulis byte rendah pembagi */
    tulis_port(PIT_SALURAN0,
               (uint8)(pembagi & 0xFF));

    /* Tulis byte tinggi pembagi */
    tulis_port(PIT_SALURAN0,
               (uint8)((pembagi >> 8) & 0xFF));
}

/* ================================================================
 * KIRIM EOI
 * ================================================================ */

/*
 * x86_umum_kirim_eoi - Kirim End-of-Interrupt ke PIC.
 *
 * Harus dipanggil di akhir setiap handler IRQ agar
 * PIC dapat menerima interupsi berikutnya.
 * Jika IRQ berasal dari slave (IRQ >= 8), EOI
 * dikirim ke kedua PIC.
 */
void x86_umum_kirim_eoi(uint8 irq)
{
    /* Kirim EOI ke PIC master */
    tulis_port(PIC1_PERINTAH, PIC_EOI);

    /* Jika dari slave, kirim juga ke slave */
    if (irq >= 8) {
        tulis_port(PIC2_PERINTAH, PIC_EOI);
    }
}

#endif /* ARSITEK_X86 || ARSITEK_X64 */
