global _start
extern main

section .text
_start:
    call main
    mov ebx, eax
    mov eax, 14
    jmp  $