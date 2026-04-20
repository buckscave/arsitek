# Kernel Arsitek

Kernel orisinal (bukan fork) yang mendukung empat arsitektur CPU:
- **x86** (IA-32, 32-bit)
- **x64** (AMD64, 64-bit)
- **ARM32** (ARMv7)
- **ARM64** (AArch64/ARMv8)

## Filosofi

Arsitek menggunakan **analogi organisasi** — setiap komponen kernel adalah "bawahan" yang menjalankan tugas spesifik dan melapor melalui Notulen (pencatat):

| Bawahan | Tugas |
|---------|-------|
| **Peninjau** | Memeriksa perangkat keras yang terpasang |
| **Notulen** | Mencatat semua laporan dan data perangkat |
| **Penyedia** | Menyediakan sumber daya untuk perangkat |
| **Pengembang** | Membangun driver dan antarmuka perangkat |
| **Pigura** | Enhanced framebuffer (lapisan tampilan) |

## Alur Kernel

```
Pemicu → Konstruksi → Pigura → Peninjau → Notulen → Penyedia → Pengembang → Idle
```

1. **Pemicu**: Kode boot (assembly) yang memuat kernel ke memori
2. **Konstruksi**: Inisialisasi CPU — GDT, IDT, PIC, PIT, paging
3. **Pigura**: Menyiapkan konsol tampilan (VGA teks / grafis)
4. **Peninjau**: Memeriksa CPU, RAM, PCI, USB, layar, papan ketik, dll.
5. **Notulen**: Mencatat semua temuan ke log kernel
6. **Penyedia**: Mengalokasikan sumber daya untuk setiap perangkat
7. **Pengembang**: Membangun driver dan antarmuka
8. **Idle**: Menunggu Kontraktor (OS) mengambil alih

## Membangun

```bash
# Untuk arsitektur x86 (default)
make arch=x86

# Untuk arsitektur x64
make arch=x64

# Untuk arsitektur ARM32 (memerlukan cross-compiler)
make arch=arm32 CROSS_COMPILE=arm-none-eabi-

# Untuk arsitektur ARM64 (memerlukan cross-compiler)
make arch=arm64 CROSS_COMPILE=aarch64-none-elf-

# Bersihkan
make clean

# Info build
make info
```

## Struktur Direktori

```
arsitek/
├── arsitek.c              Titik masuk kernel & utilitas dasar
├── makefile               Sistem build multi-arsitektur
├── baca.md                Berkas ini
├── lampiran/              Header files
│   ├── arsitek.h          Definisi tipe data, makro, struktur utama
│   ├── mesin.h            Konstanta dan deklarasi khusus arsitektur
│   ├── pigura.h           Deklarasi API framebuffer
│   ├── peninjau.h         Deklarasi API inspeksi perangkat
│   ├── notulen.h          Deklarasi API pencatatan
│   ├── penyedia.h         Deklarasi API penyedia sumber daya
│   └── pengembang.h       Deklarasi API pembangun driver
├── pemicu/                Kode boot (assembly)
│   ├── x86/pemicu.asm     Boot sector x86 16→32 bit
│   ├── x64/pemicu_awal.asm  Boot x64 16→64 bit
│   ├── x64/pemicu_lanjutan.asm  Lanjutan x64 64-bit
│   ├── arm32/pemicu.c     Boot ARM32 (dipanggil U-Boot)
│   └── arm64/pemicu.c     Boot ARM64 (dipanggil U-Boot)
├── konstruksi/            Inisialisasi mesin per arsitektur
│   ├── mesin.c            Koordinator inisialisasi
│   ├── x86/               GDT, IDT, PIC, PIT, paging x86
│   ├── x64/               GDT, IDT, paging x64
│   ├── arm32/             MMU, vektor, GIC ARM32
│   └── arm64/             MMU, vektor, GIC ARM64
├── peninjau/              Inspeksi perangkat keras
│   ├── peninjau.c         Koordinator inspeksi
│   ├── cpu.c              Deteksi CPU (CPUID/MIDR)
│   ├── ram.c              Deteksi RAM (BDA/CMOS/ATAG)
│   ├── pci.c              Enumerasi bus PCI
│   ├── usb.c              Deteksi pengendali USB
│   ├── layar.c            Deteksi tampilan VGA
│   ├── papan_ketik.c      Deteksi papan ketik PS/2
│   ├── tetikus.c          Deteksi tetikus PS/2
│   ├── penyimpanan.c      Deteksi penyimpanan (IDE/AHCI/NVMe)
│   ├── jaringan.c         Deteksi jaringan (Ethernet)
│   └── dma.c              Deteksi DMA 8237
├── notulen/               Pencatatan dan logging
│   └── notulen.c          Implementasi log + database perangkat
├── penyedia/              Penyedia sumber daya
│   ├── penyedia.c         Koordinator penyediaan
│   ├── tipedata.c         Tipe data per arsitektur
│   ├── alamat.c           Alokasi alamat memori
│   ├── jalur.c            Konfigurasi bus
│   └── sela.c             Alokasi interupsi
├── pengembang/            Pembangun driver
│   ├── pengembang.c       Koordinator pembangunan
│   ├── koneksi.c          Koneksi bus perangkat
│   ├── kendali.c          Driver perangkat
│   └── antarmuka.c        API untuk lapisan atas
├── pigura/                Enhanced framebuffer
│   └── pigura.c           Konsol VGA + mode grafis
└── penghubung/            Linker scripts
    ├── tautan_x86.ld
    ├── tautan_x64.ld
    ├── tautan_arm32.ld
    └── tautan_arm64.ld
```

## Lisensi

Proyek orisinal — semua kode ditulis dari nol.
