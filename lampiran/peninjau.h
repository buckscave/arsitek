/*
 * peninjau.h
 *
 * Peninjau bertugas memeriksa semua perangkat keras.
 * Ia melihat apa yang terpasang, mengidentifikasi tipe,
 * dan memverifikasi apakah perangkat berfungsi baik.
 *
 * Fungsi-fungsi Peninjau:
 *   cek_cpu()        -> "Prosesor apa yang terpasang?"
 *   cek_ram()        -> "Ada berapa RAM? Apakah berfungsi?"
 *   cek_usb()        -> "Ada USB di port berapa? Tipe apa?"
 *   cek_pci()        -> "Ada kartu PCI apa saja?"
 *   cek_layar()      -> "Layar terhubung? Resolusi berapa?"
 *   cek_papan_ketik()-> "Papan ketik terhubung?"
 *   cek_tetikus()    -> "Tetikus terhubung?"
 *   cek_penyimpanan()-> "Ada disk apa saja?"
 *   cek_jaringan()   -> "Ada kartu jaringan?"
 *   cek_dma()        -> "Pengendali DMA tersedia?"
 */

#ifndef PENINJAU_H
#define PENINJAU_H

#include "arsitek.h"

/* === Pemeriksaan Umum === */

/* Periksa semua perangkat yang terpasang */
void peninjau_periksa_semua(void);

/* Periksa perangkat di satu port tertentu (untuk hot-plug) */
int  peninjau_periksa_port(unsigned int nomor_port, DataPerangkat *hasil);

/* === Pemeriksaan Perangkat Spesifik === */

/* Periksa CPU yang terpasang */
int  peninjau_cek_cpu(DataPerangkat *hasil);

/* Periksa RAM yang terpasang */
int  peninjau_cek_ram(DataPerangkat *hasil);

/* Periksa perangkat USB */
int  peninjau_cek_usb(unsigned int nomor_port, DataPerangkat *hasil);

/* Periksa bus PCI/PCIe */
int  peninjau_cek_pci(DataPerangkat daftar[], int maks);

/* Periksa layar/tampilan */
int  peninjau_cek_layar(DataPerangkat *hasil);

/* Periksa papan ketik */
int  peninjau_cek_papan_ketik(DataPerangkat *hasil);

/* Periksa tetikus (mouse) */
int  peninjau_cek_tetikus(DataPerangkat *hasil);

/* Periksa penyimpanan (HDD/SSD) */
int  peninjau_cek_penyimpanan(DataPerangkat daftar[], int maks);

/* Periksa jaringan (Eternet/WiFi) */
int  peninjau_cek_jaringan(DataPerangkat daftar[], int maks);

/* Periksa pengendali DMA */
int  peninjau_cek_dma(DataPerangkat *hasil);

/* Verifikasi apakah perangkat berfungsi dengan baik */
KondisiPerangkat peninjau_verifikasi(DataPerangkat *perangkat);

/* === Akses Langsung PCI (low-level) === */

/* Baca register konfigurasi PCI 32-bit */
uint32 peninjau_pci_baca(uint8 bus, uint8 perangkat, uint8 fungsi, uint8 offset);

/* Tulis register konfigurasi PCI 32-bit */
void   peninjau_pci_tulis(uint8 bus, uint8 perangkat, uint8 fungsi,
                           uint8 offset, uint32 nilai);

/* Baca vendor ID dari perangkat PCI */
uint16 peninjau_pci_baca_vendor(uint8 bus, uint8 perangkat, uint8 fungsi);

/* Baca perangkat ID dari perangkat PCI */
uint16 peninjau_pci_baca_perangkat(uint8 bus, uint8 perangkat, uint8 fungsi);

/* Baca kelas dan subkelas PCI (byte tinggi=kelas, byte rendah=subkelas) */
uint16 peninjau_pci_baca_kelas(uint8 bus, uint8 perangkat, uint8 fungsi);

/* Baca nomor IRQ dari perangkat PCI */
uint8  peninjau_pci_baca_irq(uint8 bus, uint8 perangkat, uint8 fungsi);

/* Baca BAR (Base Address Register) dari perangkat PCI; nomor_bar: 0-5 */
uint32 peninjau_pci_baca_bar(uint8 bus, uint8 perangkat, uint8 fungsi,
                              int nomor_bar);

/* Baca tipe header dari perangkat PCI */
uint8  peninjau_pci_baca_header_tipe(uint8 bus, uint8 perangkat, uint8 fungsi);

/* Baca prog_if dari perangkat PCI */
uint8  peninjau_pci_baca_prog_if(uint8 bus, uint8 perangkat, uint8 fungsi);

#endif
