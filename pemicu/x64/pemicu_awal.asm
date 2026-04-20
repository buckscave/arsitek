;; ================================================================
;; pemicu_awal.asm — Kode pemicu awal untuk arsitektur x64 (AMD64)
;;
;; Dimulai dari mode nyata 16-bit (BIOS),
;; beralih melalui mode terlindung 32-bit
;; menuju mode panjang 64-bit.
;;
;; Alur:
;;   1. Inisialisasi segmen dan tumpukan (mode nyata 16-bit)
;;   2. Aktifkan garis A20
;;   3. Muat kernel dari disk ke memori
;;   4. Siapkan tabel halaman (PML4 → PDPT → PD, identitas 1 GB)
;;   5. Muat GDT (termasuk segmen 64-bit)
;;   6. Beralih ke mode terlindung 32-bit
;;   7. Aktifkan PAE, atur MSR EFER untuk mode panjang
;;   8. Aktifkan paging
;;   9. Lompat ke kode 64-bit
;; ================================================================

[BITS 16]
[ORG 0x7C00]

;; ================================================================
;; BAGIAN 1: MODE NYATA 16-BIT
;; ================================================================

pemicu_x64_mulai:
    ;; ------ Inisialisasi segmen ------
    cli
    xor ax, ax
    mov ds, ax
    mov es, ax
    mov ss, ax
    mov sp, 0x7C00

    ;; ------ Simpan nomor drive ------
    mov [nomor_drive], dl

    ;; ------ Tampilkan pesan ------
    mov si, pesan_boot_x64
    call tampilkan_string

    ;; ------ Aktifkan A20 ------
    call aktifkan_a20_x64

    ;; ------ Muat kernel dari disk ------
    call muat_kernel_x64

    ;; ------ Siapkan tabel halaman ------
    call siapkan_tabel_halaman

    ;; ------ Matikan interupsi ------
    cli

    ;; ------ Muat GDT ------
    lgdt [penuding_gdt_x64]

    ;; ------ Beralih ke mode terlindung 32-bit ------
    mov eax, cr0
    or eax, 0x00000001          ; Set bit PE
    mov cr0, eax

    ;; ------ Lompat ke kode 32-bit ------
    jmp 0x08:mode_terlindung_32

;; ================================================================
;; Fungsi: Aktifkan A20 (sama seperti x86)
;; ================================================================
aktifkan_a20_x64:
    call a64_tunggu_perintah
    mov al, 0xAD
    out 0x64, al

    call a64_tunggu_perintah
    mov al, 0xD0
    out 0x64, al

    call a64_tunggu_data
    in al, 0x60
    push ax

    call a64_tunggu_perintah
    mov al, 0xD1
    out 0x64, al

    call a64_tunggu_perintah
    pop ax
    or al, 0x02
    out 0x60, al

    call a64_tunggu_perintah
    mov al, 0xAE
    out 0x64, al
    ret

a64_tunggu_perintah:
    in al, 0x64
    test al, 0x02
    jnz a64_tunggu_perintah
    ret

a64_tunggu_data:
    in al, 0x64
    test al, 0x01
    jz a64_tunggu_data
    ret

;; ================================================================
;; Fungsi: Muat kernel dari disk
;; ================================================================
muat_kernel_x64:
    mov si, pesan_muat_x64
    call tampilkan_string

    ;; Muat kernel ke alamat sementara (di bawah 1 MB)
    ;; Lalu akan dipindahkan ke alamat tinggi oleh kode 32-bit
    mov ax, 0x1000
    mov es, ax
    xor bx, bx

    mov ah, 0x02
    mov al, 64                  ; 64 sektor (32 KB)
    mov ch, 0
    mov cl, 2
    mov dh, 0
    mov dl, [nomor_drive]
    int 0x13
    jc .kesalahan

    ;; Lanjutan
    mov ax, 0x2000
    mov es, ax
    xor bx, bx
    mov ah, 0x02
    mov al, 64
    mov ch, 0
    mov cl, 66
    mov dh, 0
    mov dl, [nomor_drive]
    int 0x13
    jc .kesalahan

    ret
.kesalahan:
    mov si, pesan_kesalahan_x64
    call tampilkan_string
    jmp $

;; ================================================================
;; Fungsi: Siapkan tabel halaman untuk mode panjang
;; ================================================================
;; Struktur:
;;   PML4  di 0x70000 (1 entri → PDPT)
;;   PDPT  di 0x71000 (1 entri → PD)
;;   PD    di 0x72000 (512 entri → 512 halaman 2MB = 1GB)
siapkan_tabel_halaman:
    ;; ------ Bersihkan area tabel halaman ------
    pusha
    mov edi, 0x70000
    xor eax, eax
    mov ecx, 0x3000 / 4        ; 3 tabel × 4KB / 4 byte
.salinitas:
    mov [edi], eax
    add edi, 4
    loop .salinitas

    ;; ------ PML4: entri 0 menunjuk ke PDPT ------
    ;; a32 diperlukan karena alamat 0x70000 melampaui batas 16-bit
    a32 mov dword [0x70000], 0x71003    ; Present + Writable

    ;; ------ PDPT: entri 0 menunjuk ke PD ------
    a32 mov dword [0x71000], 0x72003    ; Present + Writable

    ;; ------ PD: 512 entri, masing-masing 2MB (halaman besar) ------
    ;; Setiap entri: alamat 2MB-aligned | flag: Present + Writable + PageSize
    mov edi, 0x72000
    mov eax, 0x000083              ; Present + Writable + PageSize (2MB)
    mov ecx, 512
.isi_pd:
    mov [edi], eax
    add eax, 0x00200000            ; Tambah 2 MB
    add edi, 8                     ; Entri berikutnya
    loop .isi_pd

    popa
    ret

;; ================================================================
;; Fungsi: Tampilkan string (mode nyata)
;; ================================================================
tampilkan_string:
    pusha
.ulangi:
    lodsb
    test al, al
    jz .selesai
    mov ah, 0x0E
    int 0x10
    jmp .ulangi
.selesai:
    popa
    ret

;; ================================================================
;; DATA
;; ================================================================
nomor_drive:        db 0
pesan_boot_x64:     db 'Arsitek: Pemicu x64 dimulai...', 13, 10, 0
pesan_muat_x64:     db 'Arsitek: Memuat kernel...', 13, 10, 0
pesan_kesalahan_x64: db 'Arsitek: KESALAHAN disk!', 13, 10, 0

;; ================================================================
;; GDT — termasuk segmen 64-bit
;; ================================================================
gdt_x64_mulai:
    ;; Entri 0: NULL
    dd 0, 0

    ;; Entri 1: Kode 32-bit (ring 0)
    dw 0xFFFF, 0x0000
    db 0x00, 10011010b, 11001111b, 0x00

    ;; Entri 2: Data 32-bit (ring 0)
    dw 0xFFFF, 0x0000
    db 0x00, 10010010b, 11001111b, 0x00

    ;; Entri 3: Kode 64-bit (ring 0)
    dw 0x0000, 0x0000
    db 0x00, 10011010b, 00100000b, 0x00

    ;; Entri 4: Data 64-bit (ring 0)
    dw 0x0000, 0x0000
    db 0x00, 10010010b, 00000000b, 0x00

    ;; Entri 5: Kode Pengguna 64-bit (ring 3)
    dw 0x0000, 0x0000
    db 0x00, 11111010b, 00100000b, 0x00

    ;; Entri 6: Data Pengguna 64-bit (ring 3)
    dw 0x0000, 0x0000
    db 0x00, 11110010b, 00000000b, 0x00
gdt_x64_akhir:

penuding_gdt_x64:
    dw gdt_x64_akhir - gdt_x64_mulai - 1
    dd gdt_x64_mulai

;; ================================================================
;; Padding dan tanda tangan boot sector
;; ================================================================
times 510 - ($ - $$) db 0
dw 0xAA55

;; ================================================================
;; BAGIAN 2: MODE TERLINDUNG 32-BIT
;; ================================================================
[BITS 32]

mode_terlindung_32:
    ;; ------ Atur segmen data ------
    mov ax, 0x10                ; Selektor data 32-bit
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax
    mov ss, ax
    mov esp, 0x90000

    ;; ------ Aktifkan PAE (Physical Address Extension) ------
    mov eax, cr4
    or eax, (1 << 5)            ; Set bit PAE
    mov cr4, eax

    ;; ------ Muat alamat PML4 ke CR3 ------
    mov eax, 0x70000            ; Alamat PML4
    mov cr3, eax

    ;; ------ Aktifkan mode panjang via MSR EFER ------
    mov ecx, 0xC0000080         ; MSR EFER
    rdmsr
    or eax, (1 << 8)            ; Set bit LME (Long Mode Enable)
    wrmsr

    ;; ------ Aktifkan paging ------
    mov eax, cr0
    or eax, (1 << 31)           ; Set bit PG (Paging)
    mov cr0, eax

    ;; ------ Lompat ke kode 64-bit ------
    ;; Selektor 0x18 = entri 3 GDT (Kode 64-bit)
    jmp 0x18:mode_panjang_64

;; Berhenti jika terjadi kesalahan
    cli
    hlt

;; ================================================================
;; BAGIAN 3: MODE PANJANG 64-BIT
;; ================================================================
[BITS 64]

mode_panjang_64:
    ;; ------ Atur segmen data 64-bit ------
    mov ax, 0x20                ; Selektor data 64-bit
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax
    mov ss, ax

    ;; ------ Atur tumpukan 64-bit ------
    mov rsp, 0x90000

    ;; ------ Lompat ke kernel ------
    ;; Kernel x64 dimuat di 0x200000 (2 MB) sesuai tautan_x64.ld
    jmp 0x200000

    ;; ------ Berhenti jika kernel kembali ------
    cli
    hlt
