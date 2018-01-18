model compact
.386c
.data
tsc_diff	dd ?,?
irq8addr	dw 8*4, 0
.code
jumps
locals @@

pitvalue = 11
pitrate  = 108471
; pitvalue * pitrate = 1234DDh


; _fastcall double CPUinfo(void);
;                  Measures and returns CPU speed in Hz
public @CPUinfo
@CPUinfo:
	push si
	push di
	 cli
	  mov cx, pitvalue
	  call ProgramPIT
	  les di, dword ptr [irq8addr]
	  mov eax, dword ptr es:[di]
	  mov dword ptr cs:[oldi08], eax
	  db 66h, 0B8h        ; mov eax, address TempIrq0
	  dw offset TempIrq0
	  dw seg    TempIrq0
	  mov dword ptr es:[di], eax
	  xor eax, eax
	  mov dword ptr cs:[mycount], eax
	  db 0Fh,31h ;rdtsc
	  ; ^ OLD TSC -- produces EDX:EAX, but move to ESI:EDI
	  xchg eax, edi
	  xchg edx, esi
@@needsmoreloops:
	 sti
	 mov cx, 20000
@@loop1: dec cx
	 jnz @@loop1 ; some looping.
	 cli ; stop counting, and read TSC as soon as possible
	  mov eax, cs:[mycount]
	  cmp eax, 120
	  jb @@needsmoreloops ; If we did not get enough precision, loop more
	  ; We are aiming to catch at least 120/pitrate seconds (1.1 milliseconds)

	  db 0Fh,31h ;rdtsc
	  ; ^ NEW TSC -- produces EDX:EAX
	  sub eax, edi ; sub low parts
	  sbb edx, esi ; sub high parts
	  xchg eax, esi
	   ; ^ EDX:ESI is now TSC difference.
	   xor cx, cx
	   call ProgramPIT
	   ;les di, dword ptr [irq8addr]
	   mov di, word ptr [irq8addr]
	   mov eax, dword ptr cs:[oldi08]
	   mov dword ptr es:[di], eax
	 sti
	 xchg eax, esi
	 ; ^ EDX:EAX is now TSC difference.
	 ; Now CPU speed = TSCdifference * (pitrate / mycount)
	 imul edi, edx, pitrate
	 mov ecx, pitrate
	 mul ecx
	 add edx, edi
	 mov dword ptr [tsc_diff+0], eax
	 mov dword ptr [tsc_diff+4], edx
	 fild qword ptr [tsc_diff]
	 fild dword ptr cs:[mycount]
	 fdivp st(1), st
	pop di
	pop si
IFDEF   __LARGE__               ; Large Code - Large Data
        retf
ELSE
IFDEF   __MEDIUM__
	retf
ELSE
IFDEF   __HUGE__
	retf
ELSE
	ret
ENDIF
ELSE
	ret
ENDIF
ELSE
	ret
ENDIF


TempIrq0:
	inc dword ptr cs:[mycount]
	add word ptr cs:[irq0counter], pitvalue
	jc @@callold
	push ax
	 mov al, 20h
	 out 20h, al
	pop ax
	iret
@@callold:
	;jmp dword ptr [cs:oldi08]
	db 0EAh
	oldi08 dd 0

ProgramPIT:
	mov al, 34h
	out 43h, al
	mov ax, cx
	out 40h, al
	mov al, ah
	out 40h, al
	ret
	
irq0counter dw 0
mycount     dd 0
END
