;; ================================================================
;; pemicu_lanjutan.asm — Lanjutan kode pemicu x64
;;
;; Bagian ini dipanggil setelah CPU berada di mode panjang 64-bit.
;; Menyiapkan lingkungan eksekusi final sebelum menyerahkan
;; kendali ke kernel Arsitek.
;;
;; Tugas:
;;   1. Verifikasi CPU berada di mode panjang 64-bit
;;   2. Atur segmen dan tumpukan final
;;   3. Inisialisasi register SSE (jika tersedia)
;;   4. Lompat ke titik masuk kernel arsitek_mulai()
;;
;; Dapat dikompilasi sebagai ELF64 (ditautkan dengan kernel)
;; maupun biner mentah (flat binary). Melompat langsung
;; ke alamat kernel 0x200000 sesuai tautan_x64.ld.
;; ================================================================

[BITS 64]

pemicu_lanjutan_mulai:
    ;; ------ Verifikasi mode panjang ------
    ;; Cek bit LMA di MSR EFER (bit 10)
    mov ecx, 0xC0000080         ; MSR EFER
    rdmsr
    test eax, (1 << 10)         ; Bit LMA aktif?
    jz .kesalahan_mode          ; Tidak? Berhenti

    ;; ------ Atur segmen data ------
    xor ax, ax
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax
    mov ss, ax

    ;; ------ Atur tumpukan final ------
    ;; Tumpukan kernel di 0x90000, tumbuh ke bawah
    mov rsp, 0x90000
    mov rbp, 0

    ;; ------ Inisialisasi SSE (jika CPU mendukung) ------
    ;; Cek fitur SSE via CPUID
    mov eax, 1
    cpuid
    test edx, (1 << 25)         ; Bit SSE
    jz .tanpa_sse

    ;; Aktifkan SSE via CR0 dan CR4
    mov rax, cr0
    and ax, 0xFFFB              ; Clear bit EM (CR0.2)
    or ax, 0x2                  ; Set bit MP (CR0.1)
    mov cr0, rax

    mov rax, cr4
    or ax, (3 << 9)             ; Set bit OSFXSR dan OSXMMEXCPT
    mov cr4, rax

.tanpa_sse:
    ;; ------ Bersihkan register umum ------
    xor rax, rax
    xor rbx, rbx
    xor rcx, rcx
    xor rdx, rdx
    xor rsi, rsi
    xor rdi, rdi
    xor r8, r8
    xor r9, r9
    xor r10, r10
    xor r11, r11
    xor r12, r12
    xor r13, r13
    xor r14, r14
    xor r15, r15

    ;; ------ Lompat ke titik masuk kernel ------
    ;; Kernel Arsitek dimuat di alamat 0x200000 (2 MB)
    ;; sesuai tautan_x64.ld. Fungsi pertama: arsitek_mulai()
    ;; Menggunakan alamat absolut agar kompatibel dengan
    ;; format biner maupun ELF64.
    jmp 0x200000

    ;; ------ Jika kernel kembali (seharusnya tidak) ------
    ;; Hentikan CPU
.kesalahan_mode:
    cli
    hlt
    jmp .kesalahan_mode
