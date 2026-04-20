# Dokumentasi Arsitektur Kernel Arsitek

## 1. Ringkasan

Kernel Arsitek adalah kernel orisinal yang ditulis dalam bahasa C89 dan NASM assembly,
dengan seluruh penamaan dalam Bahasa Indonesia. Kernel ini dirancang untuk mendukung
empat arsitektur CPU: x86, x64, ARM32, dan ARM64.

Tujuan kernel: dari awal booting hingga tahap framebuffer (Pigura) atau console,
setelah itu menyerahkan kendali kepada OS (Kontraktor).

## 2. Arsitektur Sistem

### 2.1 Alur Eksekusi

```
                    ┌─────────┐
                    │  Pemicu  │  (boot code, assembly)
                    └────┬────┘
                         │
                    ┌────▼────┐
                    │Konstruksi│  (GDT, IDT, PIC, PIT, MMU)
                    └────┬────┘
                         │
                    ┌────▼────┐
                    │  Pigura  │  (framebuffer, konsol)
                    └────┬────┘
                         │
          ┌──────────────▼──────────────┐
          │        Peninjau              │
          │  (inspeksi perangkat keras)  │
          └──────────────┬──────────────┘
                         │ laporan via Notulen
                    ┌────▼────┐
                    │ Notulen  │  (pencatat & database)
                    └────┬────┘
                         │
          ┌──────────────▼──────────────┐
          │        Penyedia              │
          │  (alokasi sumber daya)       │
          └──────────────┬──────────────┘
                         │ laporan via Notulen
                    ┌────▼────┐
                    │Pengembang│  (bangun driver)
                    └────┬────┘
                         │ laporan via Notulen
                    ┌────▼────┐
                    │  Idle    │  (menunggu OS/Kontraktor)
                    └─────────┘
```

### 2.2 Komponen Utama

#### Pemicu (Boot Code)
- **x86**: Boot sector 512 byte → mode terlindung 32-bit → lompat ke kernel
- **x64**: Boot sector → mode terlindung → mode panjang 64-bit → lompat ke kernel
- **ARM32/64**: Dipanggil oleh U-Boot, inisialisasi CPU dan MMU

#### Konstruksi (Machine Setup)
- **x86/x64**: GDT (segmen), IDT (interupsi), PIC (8259A), PIT (timer), paging
- **ARM32**: Section mapping 1 MB, vektor eksepsi, GIC, timer
- **ARM64**: 4-level page tables, vektor eksepsi, GICv3, generic timer

#### Peninjau (Hardware Inspector)
Mendeteksi: CPU, RAM, PCI, USB, layar, papan ketik, tetikus, penyimpanan, jaringan, DMA

#### Notulen (Logger/Database)
- Log kernel dengan 4 tingkat: INFO, PERINGATAN, KESALAHAN, FATAL
- Database perangkat: simpan, tambah, hapus, baca
- Output ke serial console (COM1) dan konsol Pigura

#### Penyedia (Resource Provider)
- Tipe data per arsitektur
- Alokasi alamat memori (bitmap allocator)
- Jalur bus dan interupsi

#### Pengembang (Driver Builder)
- Koneksi bus (PCI config, USB, dll.)
- Driver per perangkat
- Antarmuka untuk lapisan atas

#### Pigura (Enhanced Framebuffer)
- Mode teks VGA (80×25, alamat 0xB8000)
- Mode grafis (linear framebuffer)
- Font bitmap 8×16 standar
- Primitif grafis: piksel, garis, kotak, karakter

## 3. Konvensi Kode

- **Bahasa**: C89 + ekstensi GCC inline assembly, NASM untuk x86/x64
- **Penamaan**: Seluruhnya dalam Bahasa Indonesia
  - Fungsi: `peninjau_cek_ram()`, `mesin_siapkan_gdt()`
  - Tipe: `DataPerangkat`, `KondisiPerangkat`, `ukuran_t`
  - Makro: `HALAMAN_4KB`, `PIC_EOI`, `VGA_ALAMAT`
  - Komentar: Bahasa Indonesia
- **Modularitas**: Satu file per komponen/fungsi
- **Freestanding**: Tidak menggunakan stdlib — semua utilitas ditulis sendiri
