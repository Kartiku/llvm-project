; NOTE: Assertions have been autogenerated by utils/update_llc_test_checks.py
; RUN: llc -mtriple=x86_64-unknown-unknown -mattr=+avx < %s | FileCheck %s

define void @PR32957(<2 x float>* %in, <8 x float>* %out) {
; CHECK-LABEL: PR32957:
; CHECK:       # %bb.0:
; CHECK-NEXT:    vmovsd {{.*#+}} xmm0 = mem[0],zero
; CHECK-NEXT:    vmovaps %ymm0, (%rsi)
; CHECK-NEXT:    vzeroupper
; CHECK-NEXT:    retq
  %ld = load <2 x float>, <2 x float>* %in, align 8
  %ext = extractelement <2 x float> %ld, i64 0
  %ext2 = extractelement <2 x float> %ld, i64 1
  %ins = insertelement <8 x float> <float undef, float undef, float 0.0, float 0.0, float 0.0, float 0.0, float 0.0, float 0.0>, float %ext, i64 0
  %ins2 = insertelement <8 x float> %ins, float %ext2, i64 1
  store <8 x float> %ins2, <8 x float>* %out, align 32
  ret void
}

declare { i8, double } @fun()

; Check that this does not fail to combine concat_vectors of a value from
; merge_values through a bitcast.
define void @d(i1 %cmp) {
; CHECK-LABEL: d:
; CHECK:       # %bb.0: # %bar
; CHECK-NEXT:    pushq %rax
; CHECK-NEXT:    .cfi_def_cfa_offset 16
; CHECK-NEXT:    callq fun
bar:
  %val = call { i8, double } @fun()
  %extr = extractvalue { i8, double } %val, 1
  %bc = bitcast double %extr to <2 x float>
  br label %baz

baz:
  %extr1 = extractelement <2 x float> %bc, i64 0
  unreachable
}
