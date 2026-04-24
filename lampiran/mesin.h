/*
 * mesin.h
 *
 * Definisi khusus mesin/arsitektur CPU.
 * Berisi konstanta register, port I/O, dan struktur
 * yang bergantung pada arsitektur CPU yang digunakan.
 *
 * Setiap arsitektur memiliki konfigurasi berbeda untuk:
 *   - Alamat memori dan port I/O
 *   - Struktur tabel deskriptor (GDT/IDT pada x86)
 *   - Konfigurasi MMU dan paging
 *   - Model interupsi/eksepsi
 */

#ifndef MESIN_H
#define MESIN_H

#include "arsitek.h"

/* ================================================================
 * KONSTANTA PCI — BERSAMA SEMUA ARSITEKTUR
 *
 * Format ruang konfigurasi PCI standar sama di semua arsitektur.
 * Hanya metode akses (port I/O vs MMIO/ECAM) yang berbeda.
 * ================================================================ */

/* Vendor ID perangkat kosong */
#define PCI_VENDOR_TIDAK_ADA  0xFFFF

/* Kelas PCI */
#define PCI_KELAS_TIDAK_DIKENAL  0x00
#define PCI_KELAS_STORAGE        0x01
#define PCI_KELAS_JARINGAN       0x02
#define PCI_KELAS_TAMPILAN       0x03
#define PCI_KELAS_MULTIMEDIA     0x04
#define PCI_KELAS_MEMORI         0x05
#define PCI_KELAS_BRIDGE         0x06
#define PCI_KELAS_KOMUNIKASI     0x07
#define PCI_KELAS_SISTEM         0x08
#define PCI_KELAS_INPUT          0x09
#define PCI_KELAS_DOCKING        0x0A
#define PCI_KELAS_PROSESOR       0x0B
#define PCI_KELAS_SERIAL         0x0C
#define PCI_KELAS_NIRKABEL       0x0D
#define PCI_KELAS_INTELIJEN      0x0E
#define PCI_KELAS_SATELIT        0x0F
#define PCI_KELAS_ENKRIPSI       0x10
#define PCI_KELAS_PENGAMBILAN    0x11

/* Offset register PCI — format standar */
#define PCI_OFFSET_VENDOR        0x00
#define PCI_OFFSET_PERANGKAT     0x02
#define PCI_OFFSET_PERINTAH      0x04
#define PCI_OFFSET_STATUS        0x06
#define PCI_OFFSET_KELAS         0x09
#define PCI_OFFSET_LATENCY       0x0D
#define PCI_OFFSET_HEADER        0x0E
#define PCI_OFFSET_BAR0          0x10
#define PCI_OFFSET_BAR1          0x14
#define PCI_OFFSET_BAR2          0x18
#define PCI_OFFSET_BAR3          0x1C
#define PCI_OFFSET_BAR4          0x20
#define PCI_OFFSET_BAR5          0x24
#define PCI_OFFSET_IRQ           0x3C

/* ================================================================
 * x86 / x64 — ARSITEKTUR INTEL/AMD
 * ================================================================ */
#if defined(ARSITEK_X86) || defined(ARSITEK_X64)

/* ---- Alamat memori penting ---- */
#define VGA_ALAMAT          0x000B8000  /* Memori teks VGA */
#define VGA_LEBAR           80          /* Kolom teks */
#define VGA_TINGGI          25          /* Baris teks */
#define VGA_TOTAL_SEL       (VGA_LEBAR * VGA_TINGGI)
#define VGA_BYTES_PER_SEL   2           /* 2 byte per sel (char + warna) */

#define BDA_ALAMAT          0x00000400  /* BIOS Data Area */
#define BDA_RAM_BAWAH       0x00000413  /* Ukuran RAM bawah (KB) */

#define KERNEL_ALAMAT_DASAR 0x00100000  /* Kernel dimuat di 1 MB */
#define KERNEL_TUMPUAN      0x00090000  /* Stack awal kernel */

/* ---- Port I/O VGA ---- */
#define VGA_PELABUHAN_KURSOR  0x03D4   /* Port indeks kursor */
#define VGA_PELABUHAN_DATA    0x03D5   /* Port data kursor */
#define VGA_KURSOR_TINGGI     0x0E     /* Register byte tinggi posisi kursor */
#define VGA_KURSOR_RENDah     0x0F     /* Register byte rendah posisi kursor */

/* ---- Port I/O PIC (8259A Programmable Interrupt Controller) ---- */
#define PIC1_PERINTAH   0x0020         /* PIC master — port perintah */
#define PIC1_DATA       0x0021         /* PIC master — port data */
#define PIC2_PERINTAH   0x00A0         /* PIC slave — port perintah */
#define PIC2_DATA       0x00A1         /* PIC slave — port data */
#define PIC_EOI         0x20           /* End of Interrupt */

#define PIC_ICW1_ICW4   0x01           /* ICW4 diperlukan */
#define PIC_ICW1_SINGLE  0x02          /* Mode tunggal (cascade) */
#define PIC_ICW1_INTERVAL4 0x04        /* Interval panggilan 4 */
#define PIC_ICW1_LEVEL   0x08          /* Mode pemicuan level */
#define PIC_ICW1_INIT    0x10          /* Inisialisasi */

#define PIC_ICW4_8086    0x01          /* Mode 8086 */
#define PIC_ICW4_AUTO    0x02          /* Auto EOI */
#define PIC_ICW4_BUF_SLAVE 0x08        /* Mode buffer — slave */
#define PIC_ICW4_BUF_MASTER 0x0C       /* Mode buffer — master */
#define PIC_ICW4_SFNM    0x10          /* Special fully nested */

/* Pemetaan IRQ PIC master */
#define IRQ_PIT          0             /* Timer (IRQ 0) */
#define IRQ_PAPAN_KETIK  1             /* Papan ketik (IRQ 1) */
#define IRQ_CASCADE      2             /* Cascade ke PIC2 (IRQ 2) */
#define IRQ_COM2         3             /* COM2 (IRQ 3) */
#define IRQ_COM1         4             /* COM1 (IRQ 4) */
#define IRQ_LPT2         5             /* LPT2 (IRQ 5) */
#define IRQ_FLOPPY       6             /* Floppy disk (IRQ 6) */
#define IRQ_LPT1         7             /* LPT1 (IRQ 7) */

/* Pemetaan IRQ PIC slave */
#define IRQ_RTC          8             /* Real-Time Clock (IRQ 8) */
#define IRQ_ACPI         9             /* ACPI (IRQ 9) */
#define IRQ_PERCOBAAN    10            /* Tersedia (IRQ 10) */
#define IRQ_PERCOBAAN2   11            /* Tersedia (IRQ 11) */
#define IRQ_PAPAN_KETIK_PS2 12        /* PS/2 mouse (IRQ 12) */
#define IRQ_FPU          13            /* Coprocessor (IRQ 13) */
#define IRQ_IDE_UTAMA    14            /* IDE primary (IRQ 14) */
#define IRQ_IDE_SEKUNDER 15            /* IDE secondary (IRQ 15) */

/* ---- Port I/O PIT (8253/8254 Programmable Interval Timer) ---- */
#define PIT_SALURAN0     0x0040        /* Saluran 0 — timer */
#define PIT_SALURAN1     0x0041        /* Saluran 1 — RAM refresh */
#define PIT_SALURAN2     0x0042        /* Saluran 2 — speaker */
#define PIT_PERINTAH     0x0043        /* Port perintah PIT */
#define PIT_FREKUENSI    1193182UL     /* Frekuensi dasar PIT (Hz) */

#define PIT_MODE_BINARY   0x00         /* Penghitung biner 16-bit */
#define PIT_MODE_BCD      0x01         /* Penghitung BCD */
#define PIT_MODE_LOBYTE   0x10         /* Akses byte rendah saja */
#define PIT_MODE_HIBYTE   0x20         /* Akses byte tinggi saja */
#define PIT_MODE_LOHI     0x30         /* Akses byte rendah lalu tinggi */
#define PIT_MODE_RATE     0x00         /* Mode rate generator */
#define PIT_MODE_SQUARE   0x06         /* Mode gelombang kotak */
#define PIT_MODE_ONESHOT  0x02         /* Mode satu tembakan */
#define PIT_MODE_SW_STROBE 0x04        /* Mode strobe perangkat lunak */

/* ---- Port I/O Papan Ketik (8042) ---- */
#define PAPAN_KETIK_DATA    0x0060     /* Port data papan ketik */
#define PAPAN_KETIK_STATUS  0x0064     /* Port status perangkat ketik */
#define PAPAN_KETIK_PERINTAH 0x0064    /* Port perintah perangkat ketik */

#define PAPAN_KETIK_STATUS_OUTPUT    0x01  /* Data siap dibaca */
#define PAPAN_KETIK_STATUS_INPUT     0x02  /* Buffer perintah penuh */
#define PAPAN_KETIK_STATUS_SYSTEM    0x04  /* Post selesai */
#define PAPAN_KETIK_STATUS_CMD_DATA  0x08  /* Data adalah perintah */
#define PAPAN_KETIK_STATUS_LOCK      0x10  /* Keyboard lock */
#define PAPAN_KETIK_STATUS_AUX_BUF   0x20  /* Data dari tetikus */
#define PAPAN_KETIK_STATUS_TIMEOUT   0x40  /* Timeout */
#define PAPAN_KETIK_STATUS_PARITY    0x80  /* Kesalahan paritas */

/* Perintah perangkat ketik 8042 */
#define PAPAN_KETIK_CMD_LED           0xED
#define PAPAN_KETIK_CMD_ECHO          0xEE
#define PAPAN_KETIK_CMD_SCANCODE      0xF0
#define PAPAN_KETIK_CMD_IDENTIFIKASI  0xF2
#define PAPAN_KETIK_CMD_RATE          0xF3
#define PAPAN_KETIK_CMD_AKTIFKAN      0xF4
#define PAPAN_KETIK_CMD_BAWAAN        0xF6
#define PAPAN_KETIK_CMD_NONAKTIFKAN   0xF5
#define PAPAN_KETIK_CMD_RESET         0xFF

#define PAPAN_KETIK_BALASAN_OK        0xAA
#define PAPAN_KETIK_BALASAN_ECHO      0xEE
#define PAPAN_KETIK_BALASAN_KEY       0xFA
#define PAPAN_KETIK_BALASAN_ULANG     0xFE
#define PAPAN_KETIK_BALASAN_KESALAHAN 0xFC

/* ---- Port I/O PCI (Configuration Space) — khusus x86/x64 ---- */
#define PCI_ALAMAT_CONFIG  0x0CF8      /* Port alamat konfigurasi */
#define PCI_DATA_CONFIG    0x0CFC      /* Port data konfigurasi */
#define PCI_AKTIF          0x80000000  /* Bit aktifkan akses PCI */

/* ---- Port I/O Serial (UART 8250/16550) ---- */
#define SERIAL_COM1       0x03F8
#define SERIAL_COM2       0x02F8
#define SERIAL_COM3       0x03E8
#define SERIAL_COM4       0x02E8

/* ---- A20 Line ---- */
#define A20_PERINTAH      0x0064
#define A20_DATA          0x0060
#define A20_BENDERAS_OUTPUT 0x02

/* ---- Selektor GDT ---- */
#define GDT_NULL         0x00          /* Selektor null */
#define GDT_KODE         0x08          /* Kode kernel (ring 0) */
#define GDT_DATA         0x10          /* Data kernel (ring 0) */
#define GDT_KODE_USER    0x18          /* Kode pengguna (ring 3) */
#define GDT_DATA_USER    0x20          /* Data pengguna (ring 3) */
#define GDT_TSS          0x28          /* Task State Segment */

/* ---- Bit akses GDT ---- */
#define GDT_AKSES_P      0x80          /* Segment Present */
#define GDT_AKSES_DPL0   0x00          /* Descriptor Privilege Level 0 */
#define GDT_AKSES_DPL3   0x60          /* Descriptor Privilege Level 3 */
#define GDT_AKSES_S      0x10          /* Deskriptor kode/data */
#define GDT_AKSES_KODE   0x08          /* Segmen kode */
#define GDT_AKSES_DATA   0x00          /* Segmen data */
#define GDT_AKSES_EKSEKUSI 0x08        /* Dapat dieksekusi */
#define GDT_AKSES_BACA   0x02          /* Dapat dibaca */
#define GDT_AKSES_TULIS  0x02          /* Dapat ditulis */
#define GDT_AKSES_AKSES  0x01          /* Accessed bit */

/* ---- Bit akses IDT ---- */
#define IDT_PINTU_INTERUPSI  0x8E      /* 32-bit interrupt gate */
#define IDT_PINTU_PERANGKAP  0x8F      /* 32-bit trap gate */
#define IDT_PINTU_TUGAS      0x85      /* 32-bit task gate */

/* ---- Bit CR0 ---- */
#define CR0_PE    BIT(0)               /* Protected Mode Enable */
#define CR0_MP    BIT(1)               /* Monitor Coprocessor */
#define CR0_EM    BIT(2)               /* Emulate FPU */
#define CR0_TS    BIT(3)               /* Task Switched */
#define CR0_ET    BIT(4)               /* Extension Type */
#define CR0_NE    BIT(5)               /* Numeric Error */
#define CR0_WP    BIT(16)              /* Write Protect */
#define CR0_AM    BIT(18)              /* Alignment Mask */
#define CR0_NW    BIT(29)              /* Not Write-through */
#define CR0_CD    BIT(30)              /* Cache Disable */
#define CR0_PG    BIT(31)              /* Paging Enable */

/* ---- Bit CR4 ---- */
#define CR4_VME        BIT(0)          /* Virtual 8086 Mode Extensions */
#define CR4_PVI        BIT(1)          /* Protected-mode Virtual Interrupts */
#define CR4_TSD        BIT(2)          /* Time Stamp Disable */
#define CR4_DE         BIT(3)          /* Debugging Extensions */
#define CR4_PSE        BIT(4)          /* Page Size Extension */
#define CR4_PAE        BIT(5)          /* Physical Address Extension */
#define CR4_MCE        BIT(6)          /* Machine Check Exception */
#define CR4_PGE        BIT(7)          /* Page Global Enabled */
#define CR4_PCE        BIT(8)          /* Performance-Monitoring Counter */
#define CR4_OSFXSR     BIT(9)          /* OS support for FXSAVE/FXRSTOR */
#define CR4_OSXMMEXCPT BIT(10)         /* OS support for unmasked SIMD */

/* ---- MSR (Model Specific Register) ---- */
#define MSR_EFER       0xC0000080      /* Extended Feature Enable */
#define MSR_LME        BIT(8)          /* Long Mode Enable */
#define MSR_STAR       0xC0000081      /* Syscall target */
#define MSR_LSTAR      0xC0000082      /* Long mode syscall target */
#define MSR_CSTAR      0xC0000083      /* Compatibility mode syscall */

/* ---- Eksepsi CPU x86 ---- */
#define EKSEPSI_PEMBAGIAN_NOL       0
#define EKSEPSI_DEBUG               1
#define EKSEPSI_NMI                 2
#define EKSEPSI_BREAKPOINT          3
#define EKSEPSI_OVERFLOW            4
#define EKSEPSI_BATAS               5
#define EKSEPSI_OPCODE_INVALID      6
#define EKSEPSI_FPU_TIDAK_ADA      7
#define EKSEPSI_GANDA_KESALAHAN     8
#define EKSEPSI_FPU_OVERFLOW       9
#define EKSEPSI_TSS_INVALID        10
#define EKSEPSI_SEGEMEN_TIDAK_ADA  11
#define EKSEPSI_SEGEMEN_STACK       12
#define EKSEPSI_PELANGGARAN_UMUM   13
#define EKSEPSI_HALAMAN            14
#define EKSEPSI_FPU_KESALAHAN      16
#define EKSEPSI_ALIGNMEN           17
#define EKSEPSI_PEMERIKSAAN_MESIN  18
#define EKSEPSI_SIMD               19

/* ---- CPUID ---- */
#define CPUID_VENDOR       0x00000000
#define CPUID_FITUR        0x00000001
#define CPUID_CACHE        0x00000002
#define CPUID_SERIAL       0x00000003
#define CPUID_PERKIRAAN    0x80000000
#define CPUID_NAMA_MEREK   0x80000002
#define CPUID_NAMA_LANJUT  0x80000003
#define CPUID_NAMA_AKHIR   0x80000004

#endif /* ARSITEK_X86 || ARSITEK_X64 */

/* ================================================================
 * ARM32 / ARM64 — ARSITEKTUR ARM
 * ================================================================ */
#if defined(ARSITEK_ARM32) || defined(ARSITEK_ARM64)

/* ---- Alamat memori penting ---- */
#define KERNEL_ALAMAT_DASAR_ARM32  0x00010000  /* Kernel dimuat di 64 KB (ARM32) */
#define KERNEL_ALAMAT_DASAR_ARM64  0x00080000  /* Kernel dimuat di 512 KB (ARM64) */
#define KERNEL_TUMPUKAN_ARM32      0x00014000  /* Stack awal kernel (ARM32) */
#define KERNEL_TUMPUKAN_ARM64      0x00084000  /* Stack awal kernel (ARM64) */

/* Pilih alamat dasar sesuai arsitektur */
#if defined(ARSITEK_ARM32)
    #define KERNEL_ALAMAT_DASAR  KERNEL_ALAMAT_DASAR_ARM32
    #define KERNEL_TUMPUKAN      KERNEL_TUMPUKAN_ARM32
#elif defined(ARSITEK_ARM64)
    #define KERNEL_ALAMAT_DASAR  KERNEL_ALAMAT_DASAR_ARM64
    #define KERNEL_TUMPUKAN      KERNEL_TUMPUKAN_ARM64
#endif

/* ---- Register kontrol ARM ---- */
#define SCTLR_M     BIT(0)             /* MMU Enable */
#define SCTLR_A     BIT(1)             /* Alignment Check */
#define SCTLR_C     BIT(2)             /* Data Cache Enable */
#define SCTLR_I     BIT(12)            /* Instruction Cache Enable */
#define SCTLR_Z     BIT(11)            /* Branch Prediction */

/* ---- Exception Vector Base ---- */
#define VBAR_ALAMAT 0x0000             /* Vector Base Address */

/* ---- GIC (Generic Interrupt Controller) ---- */
#define GIC_DISTRIBUTOR_BASE   0x2C001000
#define GIC_CPU_INTERFACE_BASE 0x2C002000

/* GIC Distributor offset */
#define GICD_CTRL              0x0000   /* Distributor Control */
#define GICD_TYPER             0x0004   /* Interrupt Controller Type */
#define GICD_ISENABLE          0x0100   /* Interrupt Set-Enable */
#define GICD_ICENABLE          0x0180   /* Interrupt Clear-Enable */
#define GICD_ISPEND            0x0200   /* Interrupt Set-Pending */
#define GICD_ICPEND            0x0280   /* Interrupt Clear-Pending */
#define GICD_ISACTIVE          0x0300   /* Interrupt Set-Active */
#define GICD_ICACTIVE          0x0380   /* Interrupt Clear-Active */
#define GICD_IPRIORITY         0x0400   /* Interrupt Priority */
#define GICD_ITARGETS          0x0800   /* Interrupt Targets */
#define GICD_ICFG              0x0C00   /* Interrupt Configuration */
#define GICD_SGIR              0x0F00   /* Software Generated Interrupt */

/* GIC CPU Interface offset */
#define GICC_CTRL              0x0000   /* CPU Interface Control */
#define GICC_PMR               0x0004   /* Interrupt Priority Mask */
#define GICC_IAR               0x000C   /* Interrupt Acknowledge */
#define GICC_EOIR              0x0010   /* End of Interrupt */

/* ---- Eksepsi ARM ---- */
#define EKSEPSI_RESET              0
#define EKSEPSI_UNDEFINED          1
#define EKSEPSI_SVC                2
#define EKSEPSI_PREFETCH_ABORT     3
#define EKSEPSI_DATA_ABORT         4
#define EKSEPSI_HYP                5
#define EKSEPSI_IRQ                6
#define EKSEPSI_FIQ                7

#endif /* ARSITEK_ARM32 || ARSITEK_ARM64 */

/* ================================================================
 * DEKLARASI FUNGSI MESIN
 * ================================================================ */

/* Inisialisasi mesin per arsitektur */
void mesin_siapkan(void);

/* Inisialisasi GDT (hanya x86/x64) */
void mesin_siapkan_gdt(void);

/* Inisialisasi IDT (hanya x86/x64) */
void mesin_siapkan_idt(void);

/* Inisialisasi PIC (hanya x86/x64) */
void mesin_siapkan_pic(void);

/* Inisialisasi PIT/timer */
void mesin_siapkan_pit(uint32 frekuensi_hz);

/* Inisialisasi paging */
void mesin_siapkan_paging(void);

/* Deteksi memori fisik */
ukuran_t mesin_deteksi_ram(void);

/* Baca CPUID (hanya x86/x64) */
void mesin_cpuid(uint32 daun, uint32 *eax, uint32 *ebx,
                 uint32 *ecx, uint32 *edx);

/* Penanganan interupsi bersifat spesifik per arsitektur;
 * tidak ada fungsi handler umum yang berdiri sendiri.
 * Setiap arsitektur mendefinisikan handler-nya sendiri
 * melalui IDT (x86/x64) atau tabel vektor (ARM). */

/* Kirim End-of-Interrupt ke PIC */
void mesin_kirim_eoi(uint8 irq);

/* Hentikan sistem secara fatal */
void mesin_hentikan(void) NORETURN;

/* Baca nomor interupsi saat ini */
uint8 mesin_baca_irq(void);

#endif /* MESIN_H */
