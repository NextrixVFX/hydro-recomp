; x87_sincos - retail's own x87 sequence, executed on this machine's FPU
; (memory/audit.md "Batch 9e", design (A)). VuMatrix_fromEuler @0x435640 runs,
; per angle:
;     fld     dword ptr [x]
;     fsincos
;     fstp    dword ptr [c]      ; ST0 = cos
;     fstp    dword ptr [s]      ; ST1 = sin
; The precision-control field does not apply to FSINCOS (Intel SDM: PC
; affects only FADD/FSUB/FMUL/FDIV/FSQRT), so the result is the hardware's
; extended one rounded once by FSTP under the rounding control - nearest in
; both processes. For |x| >= 2^63 FSINCOS sets C2 and leaves x in ST0: the
; first FSTP stores x and the second pops an EMPTY register - the masked
; stack underflow stores the indefinite NaN, as retail's does (checked against
; tools/x86run, the native 32-bit runner of retail's code).
;
; The FPU environment (control / status / tag words, and TOP, which the
; underflow pop leaves at 1) is saved and restored around the sequence: the
; x64 ABI hands the x87 stack over empty, and nothing reads that state here.
;
;     void x87_sincos( float x /* xmm0 */, float* c /* rdx */, float* s /* r8 */ );

.code
x87_sincos PROC
    sub     rsp, 40
    fnstenv [rsp + 4]
    movss   dword ptr [rsp], xmm0
    fld     dword ptr [rsp]
    fsincos
    fstp    dword ptr [rdx]
    fstp    dword ptr [r8]
    fldenv  [rsp + 4]
    add     rsp, 40
    ret
x87_sincos ENDP
END
