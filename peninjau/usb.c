/*
 * usb.c
 *
 * Pemeriksaan USB — deteksi pengendali USB via bus PCI.
 * Memindai kelas PCI 0x0C03 (USB Controller) dan mengidentifikasi
 * tipe pengendali: UHCI, OHCI, EHCI, xHCI.
 *
 * Kelas USB pada PCI:
 *   Subkelas 0x00: UHCI (Universal Host Controller Interface)
 *   Subkelas 0x10: OHCI (Open Host Controller Interface)
 *   Subkelas 0x20: EHCI (Enhanced Host Controller Interface)
 *   Subkelas 0x30: xHCI (eXtensible Host Controller Interface)
 */

#include "../lampiran/arsitek.h"
#include "../lampiran/mesin.h"
#include "../lampiran/peninjau.h"

int peninjau_cek_usb(unsigned int nomor_port, DataPerangkat *hasil)
{
    if (hasil == NULL) {
        return STATUS_PARAMETAR_SALAH;
    }

    isi_memori(hasil, 0, sizeof(DataPerangkat));

#if defined(ARSITEK_X86) || defined(ARSITEK_X64)
    {
        /* Scan PCI untuk pengendali USB (kelas 0x0C, subkelas 0x03) */
        uint8 bus, dev, fn;

        for (bus = 0; bus < 255; bus++) {
            for (dev = 0; dev < 32; dev++) {
                for (fn = 0; fn < 8; fn++) {
                    uint32 data_kls;
                    uint16 vendor;
                    uint8 kelas, subkelas;

                    vendor = peninjau_pci_baca_vendor(bus, dev, fn);
                    if (vendor == PCI_VENDOR_TIDAK_ADA) continue;

                    data_kls = peninjau_pci_baca(bus, dev, fn, 0x08);
                    kelas = (uint8)((data_kls >> 24) & 0xFF);
                    subkelas = (uint8)((data_kls >> 16) & 0xFF);

                    if (kelas == 0x0C && subkelas == 0x03) {
                        /* Ditemukan pengendali USB */
                        hasil->tipe = PERANGKAT_USB;
                        hasil->tipe_bus = BUS_PCI;
                        hasil->vendor_id = vendor;
                        hasil->perangkat_id = peninjau_pci_baca_perangkat(bus, dev, fn);
                        hasil->kelas = kelas;
                        hasil->subkelas = subkelas;
                        hasil->kondisi = KONDISI_BAIK;
                        hasil->port = ((uint32)bus << 16) | ((uint32)dev << 8) | fn;
                        hasil->alamat = (ukuran_ptr)peninjau_pci_baca_bar(bus, dev, fn, 0);

                        /* Identifikasi tipe USB controller */
                        switch (subkelas) {
                        case 0x00:
                            salin_string(hasil->nama, "Pengendali USB UHCI");
                            break;
                        case 0x10:
                            salin_string(hasil->nama, "Pengendali USB OHCI");
                            break;
                        case 0x20:
                            salin_string(hasil->nama, "Pengendali USB EHCI");
                            break;
                        case 0x30:
                            salin_string(hasil->nama, "Pengendali USB xHCI");
                            break;
                        default:
                            salin_string(hasil->nama, "Pengendali USB");
                            break;
                        }

                        TIDAK_DIGUNAKAN(nomor_port);
                        notulen_catat(NOTULEN_INFO, "Peninjau: Pengendali USB terdeteksi");
                        return STATUS_OK;
                    }
                }
            }
        }
    }
#else
    TIDAK_DIGUNAKAN(nomor_port);
#endif

    return STATUS_TIDAK_ADA;
}
