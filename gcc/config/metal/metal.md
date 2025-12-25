;; Machine description for Apple Metal target.
;; Copyright (C) 2025 Free Software Foundation, Inc.
;;
;; This file is part of GCC.
;;
;; GCC is free software; you can redistribute it and/or modify
;; it under the terms of the GNU General Public License as published by
;; the Free Software Foundation; either version 3, or (at your option)
;; any later version.
;;
;; GCC is distributed in the hope that it will be useful,
;; but WITHOUT ANY WARRANTY; without even the implied warranty of
;; MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
;; GNU General Public License for more details.
;;
;; You should have received a copy of the GNU General Public License
;; along with GCC; see the file COPYING3.  If not see
;; <http://www.gnu.org/licenses/>.

;; Minimal machine description for Metal target.
;; This is a skeleton - actual MSL code generation requires expansion.

;; Predicates

(define_predicate "metal_register_operand"
  (match_code "reg")
{
  return register_operand (op, mode);
})

(define_predicate "metal_nonmemory_operand"
  (ior (match_operand 0 "metal_register_operand")
       (match_code "const_int")))

;; Constraints

(define_constraint "R"
  "A pseudo register."
  (match_code "reg"))

;; Unspec constants

(define_c_enum "unspec" [
   UNSPEC_COPYSIGN
   UNSPEC_ATOMIC_CAS
   UNSPEC_ATOMIC_EXCH
   UNSPEC_ATOMIC_ADD
   UNSPEC_ATOMIC_AND
   UNSPEC_ATOMIC_OR
   UNSPEC_LOG2
   UNSPEC_EXP2
   UNSPEC_SIN
   UNSPEC_COS
   UNSPEC_RSQRT
])

;; Attributes

(define_attr "type"
  "unknown,move,load,store,alu,fpu,branch,call,nop"
  (const_string "unknown"))

;; Move patterns

(define_expand "movsi"
  [(set (match_operand:SI 0 "nonimmediate_operand")
	(match_operand:SI 1 "general_operand"))]
  ""
  "")

(define_insn "*movsi_insn"
  [(set (match_operand:SI 0 "metal_register_operand" "=R")
	(match_operand:SI 1 "metal_nonmemory_operand" "Ri"))]
  ""
  "// mov.s32 %0, %1"
  [(set_attr "type" "move")])

(define_expand "movdi"
  [(set (match_operand:DI 0 "nonimmediate_operand")
	(match_operand:DI 1 "general_operand"))]
  ""
  "")

(define_insn "*movdi_insn"
  [(set (match_operand:DI 0 "metal_register_operand" "=R")
	(match_operand:DI 1 "metal_nonmemory_operand" "Ri"))]
  ""
  "// mov.s64 %0, %1"
  [(set_attr "type" "move")])

(define_expand "movsf"
  [(set (match_operand:SF 0 "nonimmediate_operand")
	(match_operand:SF 1 "general_operand"))]
  ""
  "")

(define_insn "*movsf_insn"
  [(set (match_operand:SF 0 "metal_register_operand" "=R")
	(match_operand:SF 1 "metal_register_operand" "R"))]
  ""
  "// mov.f32 %0, %1"
  [(set_attr "type" "move")])

(define_expand "movdf"
  [(set (match_operand:DF 0 "nonimmediate_operand")
	(match_operand:DF 1 "general_operand"))]
  ""
  "")

(define_insn "*movdf_insn"
  [(set (match_operand:DF 0 "metal_register_operand" "=R")
	(match_operand:DF 1 "metal_register_operand" "R"))]
  ""
  "// mov.f64 %0, %1"
  [(set_attr "type" "move")])

;; Arithmetic patterns

(define_insn "addsi3"
  [(set (match_operand:SI 0 "metal_register_operand" "=R")
	(plus:SI (match_operand:SI 1 "metal_register_operand" "R")
		 (match_operand:SI 2 "metal_nonmemory_operand" "Ri")))]
  ""
  "// add.s32 %0, %1, %2"
  [(set_attr "type" "alu")])

(define_insn "adddi3"
  [(set (match_operand:DI 0 "metal_register_operand" "=R")
	(plus:DI (match_operand:DI 1 "metal_register_operand" "R")
		 (match_operand:DI 2 "metal_nonmemory_operand" "Ri")))]
  ""
  "// add.s64 %0, %1, %2"
  [(set_attr "type" "alu")])

(define_insn "subsi3"
  [(set (match_operand:SI 0 "metal_register_operand" "=R")
	(minus:SI (match_operand:SI 1 "metal_register_operand" "R")
		  (match_operand:SI 2 "metal_nonmemory_operand" "Ri")))]
  ""
  "// sub.s32 %0, %1, %2"
  [(set_attr "type" "alu")])

(define_insn "subdi3"
  [(set (match_operand:DI 0 "metal_register_operand" "=R")
	(minus:DI (match_operand:DI 1 "metal_register_operand" "R")
		  (match_operand:DI 2 "metal_nonmemory_operand" "Ri")))]
  ""
  "// sub.s64 %0, %1, %2"
  [(set_attr "type" "alu")])

(define_insn "mulsi3"
  [(set (match_operand:SI 0 "metal_register_operand" "=R")
	(mult:SI (match_operand:SI 1 "metal_register_operand" "R")
		 (match_operand:SI 2 "metal_register_operand" "R")))]
  ""
  "// mul.s32 %0, %1, %2"
  [(set_attr "type" "alu")])

(define_insn "muldi3"
  [(set (match_operand:DI 0 "metal_register_operand" "=R")
	(mult:DI (match_operand:DI 1 "metal_register_operand" "R")
		 (match_operand:DI 2 "metal_register_operand" "R")))]
  ""
  "// mul.s64 %0, %1, %2"
  [(set_attr "type" "alu")])

;; Division and modulo

(define_insn "divsi3"
  [(set (match_operand:SI 0 "metal_register_operand" "=R")
	(div:SI (match_operand:SI 1 "metal_register_operand" "R")
		(match_operand:SI 2 "metal_register_operand" "R")))]
  ""
  "// div.s32 %0, %1, %2"
  [(set_attr "type" "alu")])

(define_insn "udivsi3"
  [(set (match_operand:SI 0 "metal_register_operand" "=R")
	(udiv:SI (match_operand:SI 1 "metal_register_operand" "R")
		 (match_operand:SI 2 "metal_register_operand" "R")))]
  ""
  "// div.u32 %0, %1, %2"
  [(set_attr "type" "alu")])

(define_insn "modsi3"
  [(set (match_operand:SI 0 "metal_register_operand" "=R")
	(mod:SI (match_operand:SI 1 "metal_register_operand" "R")
		(match_operand:SI 2 "metal_register_operand" "R")))]
  ""
  "// rem.s32 %0, %1, %2"
  [(set_attr "type" "alu")])

(define_insn "umodsi3"
  [(set (match_operand:SI 0 "metal_register_operand" "=R")
	(umod:SI (match_operand:SI 1 "metal_register_operand" "R")
		 (match_operand:SI 2 "metal_register_operand" "R")))]
  ""
  "// rem.u32 %0, %1, %2"
  [(set_attr "type" "alu")])

(define_insn "divdi3"
  [(set (match_operand:DI 0 "metal_register_operand" "=R")
	(div:DI (match_operand:DI 1 "metal_register_operand" "R")
		(match_operand:DI 2 "metal_register_operand" "R")))]
  ""
  "// div.s64 %0, %1, %2"
  [(set_attr "type" "alu")])

(define_insn "udivdi3"
  [(set (match_operand:DI 0 "metal_register_operand" "=R")
	(udiv:DI (match_operand:DI 1 "metal_register_operand" "R")
		 (match_operand:DI 2 "metal_register_operand" "R")))]
  ""
  "// div.u64 %0, %1, %2"
  [(set_attr "type" "alu")])

(define_insn "moddi3"
  [(set (match_operand:DI 0 "metal_register_operand" "=R")
	(mod:DI (match_operand:DI 1 "metal_register_operand" "R")
		(match_operand:DI 2 "metal_register_operand" "R")))]
  ""
  "// rem.s64 %0, %1, %2"
  [(set_attr "type" "alu")])

(define_insn "umoddi3"
  [(set (match_operand:DI 0 "metal_register_operand" "=R")
	(umod:DI (match_operand:DI 1 "metal_register_operand" "R")
		 (match_operand:DI 2 "metal_register_operand" "R")))]
  ""
  "// rem.u64 %0, %1, %2"
  [(set_attr "type" "alu")])

;; Unary arithmetic

(define_insn "negsi2"
  [(set (match_operand:SI 0 "metal_register_operand" "=R")
	(neg:SI (match_operand:SI 1 "metal_register_operand" "R")))]
  ""
  "// neg.s32 %0, %1"
  [(set_attr "type" "alu")])

(define_insn "negdi2"
  [(set (match_operand:DI 0 "metal_register_operand" "=R")
	(neg:DI (match_operand:DI 1 "metal_register_operand" "R")))]
  ""
  "// neg.s64 %0, %1"
  [(set_attr "type" "alu")])

(define_insn "one_cmplsi2"
  [(set (match_operand:SI 0 "metal_register_operand" "=R")
	(not:SI (match_operand:SI 1 "metal_register_operand" "R")))]
  ""
  "// not.b32 %0, %1"
  [(set_attr "type" "alu")])

(define_insn "one_cmpldi2"
  [(set (match_operand:DI 0 "metal_register_operand" "=R")
	(not:DI (match_operand:DI 1 "metal_register_operand" "R")))]
  ""
  "// not.b64 %0, %1"
  [(set_attr "type" "alu")])

;; Logical operations

(define_insn "andsi3"
  [(set (match_operand:SI 0 "metal_register_operand" "=R")
	(and:SI (match_operand:SI 1 "metal_register_operand" "R")
		(match_operand:SI 2 "metal_register_operand" "R")))]
  ""
  "// and.b32 %0, %1, %2"
  [(set_attr "type" "alu")])

(define_insn "anddi3"
  [(set (match_operand:DI 0 "metal_register_operand" "=R")
	(and:DI (match_operand:DI 1 "metal_register_operand" "R")
		(match_operand:DI 2 "metal_register_operand" "R")))]
  ""
  "// and.b64 %0, %1, %2"
  [(set_attr "type" "alu")])

(define_insn "iorsi3"
  [(set (match_operand:SI 0 "metal_register_operand" "=R")
	(ior:SI (match_operand:SI 1 "metal_register_operand" "R")
		(match_operand:SI 2 "metal_register_operand" "R")))]
  ""
  "// or.b32 %0, %1, %2"
  [(set_attr "type" "alu")])

(define_insn "iordi3"
  [(set (match_operand:DI 0 "metal_register_operand" "=R")
	(ior:DI (match_operand:DI 1 "metal_register_operand" "R")
		(match_operand:DI 2 "metal_register_operand" "R")))]
  ""
  "// or.b64 %0, %1, %2"
  [(set_attr "type" "alu")])

(define_insn "xorsi3"
  [(set (match_operand:SI 0 "metal_register_operand" "=R")
	(xor:SI (match_operand:SI 1 "metal_register_operand" "R")
		(match_operand:SI 2 "metal_register_operand" "R")))]
  ""
  "// xor.b32 %0, %1, %2"
  [(set_attr "type" "alu")])

(define_insn "xordi3"
  [(set (match_operand:DI 0 "metal_register_operand" "=R")
	(xor:DI (match_operand:DI 1 "metal_register_operand" "R")
		(match_operand:DI 2 "metal_register_operand" "R")))]
  ""
  "// xor.b64 %0, %1, %2"
  [(set_attr "type" "alu")])

;; Shift operations

(define_insn "ashlsi3"
  [(set (match_operand:SI 0 "metal_register_operand" "=R")
	(ashift:SI (match_operand:SI 1 "metal_register_operand" "R")
		   (match_operand:SI 2 "metal_nonmemory_operand" "Ri")))]
  ""
  "// shl.b32 %0, %1, %2"
  [(set_attr "type" "alu")])

(define_insn "ashldi3"
  [(set (match_operand:DI 0 "metal_register_operand" "=R")
	(ashift:DI (match_operand:DI 1 "metal_register_operand" "R")
		   (match_operand:SI 2 "metal_nonmemory_operand" "Ri")))]
  ""
  "// shl.b64 %0, %1, %2"
  [(set_attr "type" "alu")])

(define_insn "ashrsi3"
  [(set (match_operand:SI 0 "metal_register_operand" "=R")
	(ashiftrt:SI (match_operand:SI 1 "metal_register_operand" "R")
		     (match_operand:SI 2 "metal_nonmemory_operand" "Ri")))]
  ""
  "// shr.s32 %0, %1, %2"
  [(set_attr "type" "alu")])

(define_insn "ashrdi3"
  [(set (match_operand:DI 0 "metal_register_operand" "=R")
	(ashiftrt:DI (match_operand:DI 1 "metal_register_operand" "R")
		     (match_operand:SI 2 "metal_nonmemory_operand" "Ri")))]
  ""
  "// shr.s64 %0, %1, %2"
  [(set_attr "type" "alu")])

(define_insn "lshrsi3"
  [(set (match_operand:SI 0 "metal_register_operand" "=R")
	(lshiftrt:SI (match_operand:SI 1 "metal_register_operand" "R")
		     (match_operand:SI 2 "metal_nonmemory_operand" "Ri")))]
  ""
  "// shr.u32 %0, %1, %2"
  [(set_attr "type" "alu")])

(define_insn "lshrdi3"
  [(set (match_operand:DI 0 "metal_register_operand" "=R")
	(lshiftrt:DI (match_operand:DI 1 "metal_register_operand" "R")
		     (match_operand:SI 2 "metal_nonmemory_operand" "Ri")))]
  ""
  "// shr.u64 %0, %1, %2"
  [(set_attr "type" "alu")])

;; Bit manipulation operations

(define_insn "bitrevsi2"
  [(set (match_operand:SI 0 "metal_register_operand" "=R")
	(bitreverse:SI (match_operand:SI 1 "metal_register_operand" "R")))]
  ""
  "// brev.b32 %0, %1"
  [(set_attr "type" "alu")])

(define_insn "bitrevdi2"
  [(set (match_operand:DI 0 "metal_register_operand" "=R")
	(bitreverse:DI (match_operand:DI 1 "metal_register_operand" "R")))]
  ""
  "// brev.b64 %0, %1"
  [(set_attr "type" "alu")])

(define_insn "clzsi2"
  [(set (match_operand:SI 0 "metal_register_operand" "=R")
	(clz:SI (match_operand:SI 1 "metal_register_operand" "R")))]
  ""
  "// clz.b32 %0, %1"
  [(set_attr "type" "alu")])

(define_insn "clzdi2"
  [(set (match_operand:SI 0 "metal_register_operand" "=R")
	(clz:SI (match_operand:DI 1 "metal_register_operand" "R")))]
  ""
  "// clz.b64 %0, %1"
  [(set_attr "type" "alu")])

(define_expand "ctzsi2"
  [(set (match_operand:SI 0 "metal_register_operand" "")
	(ctz:SI (match_operand:SI 1 "metal_register_operand" "")))]
  ""
{
  rtx tmpreg = gen_reg_rtx (SImode);
  emit_insn (gen_bitrevsi2 (tmpreg, operands[1]));
  emit_insn (gen_clzsi2 (operands[0], tmpreg));
  DONE;
})

(define_expand "ctzdi2"
  [(set (match_operand:SI 0 "metal_register_operand" "")
	(ctz:SI (match_operand:DI 1 "metal_register_operand" "")))]
  ""
{
  rtx tmpreg = gen_reg_rtx (DImode);
  emit_insn (gen_bitrevdi2 (tmpreg, operands[1]));
  emit_insn (gen_clzdi2 (operands[0], tmpreg));
  DONE;
})

(define_insn "popcountsi2"
  [(set (match_operand:SI 0 "metal_register_operand" "=R")
	(popcount:SI (match_operand:SI 1 "metal_register_operand" "R")))]
  ""
  "// popc.b32 %0, %1"
  [(set_attr "type" "alu")])

(define_insn "popcountdi2"
  [(set (match_operand:SI 0 "metal_register_operand" "=R")
	(truncate:SI
	  (popcount:DI (match_operand:DI 1 "metal_register_operand" "R"))))]
  ""
  "// popc.b64 %0, %1"
  [(set_attr "type" "alu")])

;; Floating point arithmetic

(define_insn "addsf3"
  [(set (match_operand:SF 0 "metal_register_operand" "=R")
	(plus:SF (match_operand:SF 1 "metal_register_operand" "R")
		 (match_operand:SF 2 "metal_register_operand" "R")))]
  ""
  "// add.f32 %0, %1, %2"
  [(set_attr "type" "fpu")])

(define_insn "adddf3"
  [(set (match_operand:DF 0 "metal_register_operand" "=R")
	(plus:DF (match_operand:DF 1 "metal_register_operand" "R")
		 (match_operand:DF 2 "metal_register_operand" "R")))]
  ""
  "// add.f64 %0, %1, %2"
  [(set_attr "type" "fpu")])

(define_insn "subsf3"
  [(set (match_operand:SF 0 "metal_register_operand" "=R")
	(minus:SF (match_operand:SF 1 "metal_register_operand" "R")
		  (match_operand:SF 2 "metal_register_operand" "R")))]
  ""
  "// sub.f32 %0, %1, %2"
  [(set_attr "type" "fpu")])

(define_insn "subdf3"
  [(set (match_operand:DF 0 "metal_register_operand" "=R")
	(minus:DF (match_operand:DF 1 "metal_register_operand" "R")
		  (match_operand:DF 2 "metal_register_operand" "R")))]
  ""
  "// sub.f64 %0, %1, %2"
  [(set_attr "type" "fpu")])

(define_insn "mulsf3"
  [(set (match_operand:SF 0 "metal_register_operand" "=R")
	(mult:SF (match_operand:SF 1 "metal_register_operand" "R")
		 (match_operand:SF 2 "metal_register_operand" "R")))]
  ""
  "// mul.f32 %0, %1, %2"
  [(set_attr "type" "fpu")])

(define_insn "muldf3"
  [(set (match_operand:DF 0 "metal_register_operand" "=R")
	(mult:DF (match_operand:DF 1 "metal_register_operand" "R")
		 (match_operand:DF 2 "metal_register_operand" "R")))]
  ""
  "// mul.f64 %0, %1, %2"
  [(set_attr "type" "fpu")])

(define_insn "divsf3"
  [(set (match_operand:SF 0 "metal_register_operand" "=R")
	(div:SF (match_operand:SF 1 "metal_register_operand" "R")
		(match_operand:SF 2 "metal_register_operand" "R")))]
  ""
  "// div.f32 %0, %1, %2"
  [(set_attr "type" "fpu")])

(define_insn "divdf3"
  [(set (match_operand:DF 0 "metal_register_operand" "=R")
	(div:DF (match_operand:DF 1 "metal_register_operand" "R")
		(match_operand:DF 2 "metal_register_operand" "R")))]
  ""
  "// div.f64 %0, %1, %2"
  [(set_attr "type" "fpu")])

;; Floating point unary operations

(define_insn "negsf2"
  [(set (match_operand:SF 0 "metal_register_operand" "=R")
	(neg:SF (match_operand:SF 1 "metal_register_operand" "R")))]
  ""
  "// neg.f32 %0, %1"
  [(set_attr "type" "fpu")])

(define_insn "negdf2"
  [(set (match_operand:DF 0 "metal_register_operand" "=R")
	(neg:DF (match_operand:DF 1 "metal_register_operand" "R")))]
  ""
  "// neg.f64 %0, %1"
  [(set_attr "type" "fpu")])

(define_insn "abssf2"
  [(set (match_operand:SF 0 "metal_register_operand" "=R")
	(abs:SF (match_operand:SF 1 "metal_register_operand" "R")))]
  ""
  "// abs.f32 %0, %1"
  [(set_attr "type" "fpu")])

(define_insn "absdf2"
  [(set (match_operand:DF 0 "metal_register_operand" "=R")
	(abs:DF (match_operand:DF 1 "metal_register_operand" "R")))]
  ""
  "// abs.f64 %0, %1"
  [(set_attr "type" "fpu")])

(define_insn "sqrtsf2"
  [(set (match_operand:SF 0 "metal_register_operand" "=R")
	(sqrt:SF (match_operand:SF 1 "metal_register_operand" "R")))]
  ""
  "// sqrt.f32 %0, %1"
  [(set_attr "type" "fpu")])

(define_insn "sqrtdf2"
  [(set (match_operand:DF 0 "metal_register_operand" "=R")
	(sqrt:DF (match_operand:DF 1 "metal_register_operand" "R")))]
  ""
  "// sqrt.f64 %0, %1"
  [(set_attr "type" "fpu")])

;; Fused multiply-add

(define_insn "fmasf4"
  [(set (match_operand:SF 0 "metal_register_operand" "=R")
	(fma:SF (match_operand:SF 1 "metal_register_operand" "R")
		(match_operand:SF 2 "metal_register_operand" "R")
		(match_operand:SF 3 "metal_register_operand" "R")))]
  ""
  "// fma.f32 %0, %1, %2, %3"
  [(set_attr "type" "fpu")])

(define_insn "fmadf4"
  [(set (match_operand:DF 0 "metal_register_operand" "=R")
	(fma:DF (match_operand:DF 1 "metal_register_operand" "R")
		(match_operand:DF 2 "metal_register_operand" "R")
		(match_operand:DF 3 "metal_register_operand" "R")))]
  ""
  "// fma.f64 %0, %1, %2, %3"
  [(set_attr "type" "fpu")])

;; Floating-point special functions

(define_insn "log2sf2"
  [(set (match_operand:SF 0 "metal_register_operand" "=R")
	(unspec:SF [(match_operand:SF 1 "metal_register_operand" "R")]
		   UNSPEC_LOG2))]
  ""
  "// lg2.approx.f32 %0, %1"
  [(set_attr "type" "fpu")])

(define_insn "log2df2"
  [(set (match_operand:DF 0 "metal_register_operand" "=R")
	(unspec:DF [(match_operand:DF 1 "metal_register_operand" "R")]
		   UNSPEC_LOG2))]
  ""
  "// lg2.approx.f64 %0, %1"
  [(set_attr "type" "fpu")])

(define_insn "exp2sf2"
  [(set (match_operand:SF 0 "metal_register_operand" "=R")
	(unspec:SF [(match_operand:SF 1 "metal_register_operand" "R")]
		   UNSPEC_EXP2))]
  ""
  "// ex2.approx.f32 %0, %1"
  [(set_attr "type" "fpu")])

(define_insn "exp2df2"
  [(set (match_operand:DF 0 "metal_register_operand" "=R")
	(unspec:DF [(match_operand:DF 1 "metal_register_operand" "R")]
		   UNSPEC_EXP2))]
  ""
  "// ex2.approx.f64 %0, %1"
  [(set_attr "type" "fpu")])

(define_insn "sinsf2"
  [(set (match_operand:SF 0 "metal_register_operand" "=R")
	(unspec:SF [(match_operand:SF 1 "metal_register_operand" "R")]
		   UNSPEC_SIN))]
  ""
  "// sin.approx.f32 %0, %1"
  [(set_attr "type" "fpu")])

(define_insn "sindf2"
  [(set (match_operand:DF 0 "metal_register_operand" "=R")
	(unspec:DF [(match_operand:DF 1 "metal_register_operand" "R")]
		   UNSPEC_SIN))]
  ""
  "// sin.approx.f64 %0, %1"
  [(set_attr "type" "fpu")])

(define_insn "cossf2"
  [(set (match_operand:SF 0 "metal_register_operand" "=R")
	(unspec:SF [(match_operand:SF 1 "metal_register_operand" "R")]
		   UNSPEC_COS))]
  ""
  "// cos.approx.f32 %0, %1"
  [(set_attr "type" "fpu")])

(define_insn "cosdf2"
  [(set (match_operand:DF 0 "metal_register_operand" "=R")
	(unspec:DF [(match_operand:DF 1 "metal_register_operand" "R")]
		   UNSPEC_COS))]
  ""
  "// cos.approx.f64 %0, %1"
  [(set_attr "type" "fpu")])

(define_insn "rsqrtsf2"
  [(set (match_operand:SF 0 "metal_register_operand" "=R")
	(unspec:SF [(match_operand:SF 1 "metal_register_operand" "R")]
		   UNSPEC_RSQRT))]
  ""
  "// rsqrt.approx.f32 %0, %1"
  [(set_attr "type" "fpu")])

(define_insn "rsqrtdf2"
  [(set (match_operand:DF 0 "metal_register_operand" "=R")
	(unspec:DF [(match_operand:DF 1 "metal_register_operand" "R")]
		   UNSPEC_RSQRT))]
  ""
  "// rsqrt.approx.f64 %0, %1"
  [(set_attr "type" "fpu")])

;; Integer min/max

(define_insn "sminsi3"
  [(set (match_operand:SI 0 "metal_register_operand" "=R")
	(smin:SI (match_operand:SI 1 "metal_register_operand" "R")
		 (match_operand:SI 2 "metal_nonmemory_operand" "Ri")))]
  ""
  "// min.s32 %0, %1, %2"
  [(set_attr "type" "alu")])

(define_insn "smaxsi3"
  [(set (match_operand:SI 0 "metal_register_operand" "=R")
	(smax:SI (match_operand:SI 1 "metal_register_operand" "R")
		 (match_operand:SI 2 "metal_nonmemory_operand" "Ri")))]
  ""
  "// max.s32 %0, %1, %2"
  [(set_attr "type" "alu")])

(define_insn "uminsi3"
  [(set (match_operand:SI 0 "metal_register_operand" "=R")
	(umin:SI (match_operand:SI 1 "metal_register_operand" "R")
		 (match_operand:SI 2 "metal_nonmemory_operand" "Ri")))]
  ""
  "// min.u32 %0, %1, %2"
  [(set_attr "type" "alu")])

(define_insn "umaxsi3"
  [(set (match_operand:SI 0 "metal_register_operand" "=R")
	(umax:SI (match_operand:SI 1 "metal_register_operand" "R")
		 (match_operand:SI 2 "metal_nonmemory_operand" "Ri")))]
  ""
  "// max.u32 %0, %1, %2"
  [(set_attr "type" "alu")])

(define_insn "smindi3"
  [(set (match_operand:DI 0 "metal_register_operand" "=R")
	(smin:DI (match_operand:DI 1 "metal_register_operand" "R")
		 (match_operand:DI 2 "metal_nonmemory_operand" "Ri")))]
  ""
  "// min.s64 %0, %1, %2"
  [(set_attr "type" "alu")])

(define_insn "smaxdi3"
  [(set (match_operand:DI 0 "metal_register_operand" "=R")
	(smax:DI (match_operand:DI 1 "metal_register_operand" "R")
		 (match_operand:DI 2 "metal_nonmemory_operand" "Ri")))]
  ""
  "// max.s64 %0, %1, %2"
  [(set_attr "type" "alu")])

(define_insn "umindi3"
  [(set (match_operand:DI 0 "metal_register_operand" "=R")
	(umin:DI (match_operand:DI 1 "metal_register_operand" "R")
		 (match_operand:DI 2 "metal_nonmemory_operand" "Ri")))]
  ""
  "// min.u64 %0, %1, %2"
  [(set_attr "type" "alu")])

(define_insn "umaxdi3"
  [(set (match_operand:DI 0 "metal_register_operand" "=R")
	(umax:DI (match_operand:DI 1 "metal_register_operand" "R")
		 (match_operand:DI 2 "metal_nonmemory_operand" "Ri")))]
  ""
  "// max.u64 %0, %1, %2"
  [(set_attr "type" "alu")])

;; Floating point min/max

(define_insn "sminsf3"
  [(set (match_operand:SF 0 "metal_register_operand" "=R")
	(smin:SF (match_operand:SF 1 "metal_register_operand" "R")
		 (match_operand:SF 2 "metal_register_operand" "R")))]
  ""
  "// min.f32 %0, %1, %2"
  [(set_attr "type" "fpu")])

(define_insn "smaxsf3"
  [(set (match_operand:SF 0 "metal_register_operand" "=R")
	(smax:SF (match_operand:SF 1 "metal_register_operand" "R")
		 (match_operand:SF 2 "metal_register_operand" "R")))]
  ""
  "// max.f32 %0, %1, %2"
  [(set_attr "type" "fpu")])

(define_insn "smindf3"
  [(set (match_operand:DF 0 "metal_register_operand" "=R")
	(smin:DF (match_operand:DF 1 "metal_register_operand" "R")
		 (match_operand:DF 2 "metal_register_operand" "R")))]
  ""
  "// min.f64 %0, %1, %2"
  [(set_attr "type" "fpu")])

(define_insn "smaxdf3"
  [(set (match_operand:DF 0 "metal_register_operand" "=R")
	(smax:DF (match_operand:DF 1 "metal_register_operand" "R")
		 (match_operand:DF 2 "metal_register_operand" "R")))]
  ""
  "// max.f64 %0, %1, %2"
  [(set_attr "type" "fpu")])

;; Integer absolute value

(define_insn "abssi2"
  [(set (match_operand:SI 0 "metal_register_operand" "=R")
	(abs:SI (match_operand:SI 1 "metal_register_operand" "R")))]
  ""
  "// abs.s32 %0, %1"
  [(set_attr "type" "alu")])

(define_insn "absdi2"
  [(set (match_operand:DI 0 "metal_register_operand" "=R")
	(abs:DI (match_operand:DI 1 "metal_register_operand" "R")))]
  ""
  "// abs.s64 %0, %1"
  [(set_attr "type" "alu")])

;; Wide multiply (SI x SI -> DI)

(define_insn "mulsidi3"
  [(set (match_operand:DI 0 "metal_register_operand" "=R")
	(mult:DI (sign_extend:DI
		  (match_operand:SI 1 "metal_register_operand" "R"))
		 (sign_extend:DI
		  (match_operand:SI 2 "metal_register_operand" "R"))))]
  ""
  "// mul.wide.s32 %0, %1, %2"
  [(set_attr "type" "alu")])

(define_insn "umulsidi3"
  [(set (match_operand:DI 0 "metal_register_operand" "=R")
	(mult:DI (zero_extend:DI
		  (match_operand:SI 1 "metal_register_operand" "R"))
		 (zero_extend:DI
		  (match_operand:SI 2 "metal_register_operand" "R"))))]
  ""
  "// mul.wide.u32 %0, %1, %2"
  [(set_attr "type" "alu")])

;; High-part multiply

(define_insn "smulsi3_highpart"
  [(set (match_operand:SI 0 "metal_register_operand" "=R")
	(smul_highpart:SI
	  (match_operand:SI 1 "metal_register_operand" "R")
	  (match_operand:SI 2 "metal_nonmemory_operand" "Ri")))]
  ""
  "// mul.hi.s32 %0, %1, %2"
  [(set_attr "type" "alu")])

(define_insn "umulsi3_highpart"
  [(set (match_operand:SI 0 "metal_register_operand" "=R")
	(umul_highpart:SI
	  (match_operand:SI 1 "metal_register_operand" "R")
	  (match_operand:SI 2 "metal_nonmemory_operand" "Ri")))]
  ""
  "// mul.hi.u32 %0, %1, %2"
  [(set_attr "type" "alu")])

(define_insn "smuldi3_highpart"
  [(set (match_operand:DI 0 "metal_register_operand" "=R")
	(smul_highpart:DI
	  (match_operand:DI 1 "metal_register_operand" "R")
	  (match_operand:DI 2 "metal_nonmemory_operand" "Ri")))]
  ""
  "// mul.hi.s64 %0, %1, %2"
  [(set_attr "type" "alu")])

(define_insn "umuldi3_highpart"
  [(set (match_operand:DI 0 "metal_register_operand" "=R")
	(umul_highpart:DI
	  (match_operand:DI 1 "metal_register_operand" "R")
	  (match_operand:DI 2 "metal_nonmemory_operand" "Ri")))]
  ""
  "// mul.hi.u64 %0, %1, %2"
  [(set_attr "type" "alu")])
;; Copysign

(define_insn "copysignsf3"
  [(set (match_operand:SF 0 "metal_register_operand" "=R")
	(unspec:SF [(match_operand:SF 1 "metal_register_operand" "R")
		    (match_operand:SF 2 "metal_register_operand" "R")]
		   UNSPEC_COPYSIGN))]
  ""
  "// copysign.f32 %0, %1, %2"
  [(set_attr "type" "fpu")])

(define_insn "copysigndf3"
  [(set (match_operand:DF 0 "metal_register_operand" "=R")
	(unspec:DF [(match_operand:DF 1 "metal_register_operand" "R")
		    (match_operand:DF 2 "metal_register_operand" "R")]
		   UNSPEC_COPYSIGN))]
  ""
  "// copysign.f64 %0, %1, %2"
  [(set_attr "type" "fpu")])

;; Floating point type conversions

(define_insn "extendsfdf2"
  [(set (match_operand:DF 0 "metal_register_operand" "=R")
	(float_extend:DF (match_operand:SF 1 "metal_register_operand" "R")))]
  ""
  "// cvt.f64.f32 %0, %1"
  [(set_attr "type" "fpu")])

(define_insn "truncdfsf2"
  [(set (match_operand:SF 0 "metal_register_operand" "=R")
	(float_truncate:SF (match_operand:DF 1 "metal_register_operand" "R")))]
  ""
  "// cvt.f32.f64 %0, %1"
  [(set_attr "type" "fpu")])

(define_insn "floatsisf2"
  [(set (match_operand:SF 0 "metal_register_operand" "=R")
	(float:SF (match_operand:SI 1 "metal_register_operand" "R")))]
  ""
  "// cvt.f32.s32 %0, %1"
  [(set_attr "type" "fpu")])

(define_insn "floatsidf2"
  [(set (match_operand:DF 0 "metal_register_operand" "=R")
	(float:DF (match_operand:SI 1 "metal_register_operand" "R")))]
  ""
  "// cvt.f64.s32 %0, %1"
  [(set_attr "type" "fpu")])

(define_insn "floatdisf2"
  [(set (match_operand:SF 0 "metal_register_operand" "=R")
	(float:SF (match_operand:DI 1 "metal_register_operand" "R")))]
  ""
  "// cvt.f32.s64 %0, %1"
  [(set_attr "type" "fpu")])

(define_insn "floatdidf2"
  [(set (match_operand:DF 0 "metal_register_operand" "=R")
	(float:DF (match_operand:DI 1 "metal_register_operand" "R")))]
  ""
  "// cvt.f64.s64 %0, %1"
  [(set_attr "type" "fpu")])

(define_insn "fix_truncsfsi2"
  [(set (match_operand:SI 0 "metal_register_operand" "=R")
	(fix:SI (match_operand:SF 1 "metal_register_operand" "R")))]
  ""
  "// cvt.s32.f32 %0, %1"
  [(set_attr "type" "fpu")])

(define_insn "fix_truncdfsi2"
  [(set (match_operand:SI 0 "metal_register_operand" "=R")
	(fix:SI (match_operand:DF 1 "metal_register_operand" "R")))]
  ""
  "// cvt.s32.f64 %0, %1"
  [(set_attr "type" "fpu")])

(define_insn "fix_truncsfdi2"
  [(set (match_operand:DI 0 "metal_register_operand" "=R")
	(fix:DI (match_operand:SF 1 "metal_register_operand" "R")))]
  ""
  "// cvt.s64.f32 %0, %1"
  [(set_attr "type" "fpu")])

(define_insn "fix_truncdfdi2"
  [(set (match_operand:DI 0 "metal_register_operand" "=R")
	(fix:DI (match_operand:DF 1 "metal_register_operand" "R")))]
  ""
  "// cvt.s64.f64 %0, %1"
  [(set_attr "type" "fpu")])

;; Unsigned integer to float conversions

(define_insn "floatunssisf2"
  [(set (match_operand:SF 0 "metal_register_operand" "=R")
	(unsigned_float:SF (match_operand:SI 1 "metal_register_operand" "R")))]
  ""
  "// cvt.f32.u32 %0, %1"
  [(set_attr "type" "fpu")])

(define_insn "floatunssidf2"
  [(set (match_operand:DF 0 "metal_register_operand" "=R")
	(unsigned_float:DF (match_operand:SI 1 "metal_register_operand" "R")))]
  ""
  "// cvt.f64.u32 %0, %1"
  [(set_attr "type" "fpu")])

(define_insn "floatunsdisf2"
  [(set (match_operand:SF 0 "metal_register_operand" "=R")
	(unsigned_float:SF (match_operand:DI 1 "metal_register_operand" "R")))]
  ""
  "// cvt.f32.u64 %0, %1"
  [(set_attr "type" "fpu")])

(define_insn "floatunsdidf2"
  [(set (match_operand:DF 0 "metal_register_operand" "=R")
	(unsigned_float:DF (match_operand:DI 1 "metal_register_operand" "R")))]
  ""
  "// cvt.f64.u64 %0, %1"
  [(set_attr "type" "fpu")])

;; Float to unsigned integer conversions

(define_insn "fixuns_truncsfsi2"
  [(set (match_operand:SI 0 "metal_register_operand" "=R")
	(unsigned_fix:SI (match_operand:SF 1 "metal_register_operand" "R")))]
  ""
  "// cvt.u32.f32 %0, %1"
  [(set_attr "type" "fpu")])

(define_insn "fixuns_truncdfsi2"
  [(set (match_operand:SI 0 "metal_register_operand" "=R")
	(unsigned_fix:SI (match_operand:DF 1 "metal_register_operand" "R")))]
  ""
  "// cvt.u32.f64 %0, %1"
  [(set_attr "type" "fpu")])

(define_insn "fixuns_truncsfdi2"
  [(set (match_operand:DI 0 "metal_register_operand" "=R")
	(unsigned_fix:DI (match_operand:SF 1 "metal_register_operand" "R")))]
  ""
  "// cvt.u64.f32 %0, %1"
  [(set_attr "type" "fpu")])

(define_insn "fixuns_truncdfdi2"
  [(set (match_operand:DI 0 "metal_register_operand" "=R")
	(unsigned_fix:DI (match_operand:DF 1 "metal_register_operand" "R")))]
  ""
  "// cvt.u64.f64 %0, %1"
  [(set_attr "type" "fpu")])

;; Comparison and branches

(define_expand "cbranchsi4"
  [(set (pc)
	(if_then_else (match_operator 0 "comparison_operator"
			[(match_operand:SI 1 "metal_register_operand")
			 (match_operand:SI 2 "metal_nonmemory_operand")])
		      (label_ref (match_operand 3 ""))
		      (pc)))]
  ""
  "")

(define_insn "*branch_si"
  [(set (pc)
	(if_then_else (match_operator 0 "comparison_operator"
			[(match_operand:SI 1 "metal_register_operand" "R")
			 (match_operand:SI 2 "metal_nonmemory_operand" "Ri")])
		      (label_ref (match_operand 3 "" ""))
		      (pc)))]
  ""
  "// bra.%c0 %1, %2, %l3"
  [(set_attr "type" "branch")])

;; Conditional select operations

;; Predicate to integer conversion
(define_insn "zero_extendbisi2"
  [(set (match_operand:SI 0 "metal_register_operand" "=R")
	(zero_extend:SI
	 (match_operand:BI 1 "metal_register_operand" "R")))]
  ""
  "// selp.s32 %0, 1, 0, %1"
  [(set_attr "type" "alu")])

(define_insn "zero_extendbidi2"
  [(set (match_operand:DI 0 "metal_register_operand" "=R")
	(zero_extend:DI
	 (match_operand:BI 1 "metal_register_operand" "R")))]
  ""
  "// selp.s64 %0, 1, 0, %1"
  [(set_attr "type" "alu")])

;; Conditional select - select if predicate true (ne 0)
(define_insn "*sel_truesi"
  [(set (match_operand:SI 0 "metal_register_operand" "=R")
	(if_then_else:SI
	  (ne (match_operand:BI 1 "metal_register_operand" "R")
	      (const_int 0))
	  (match_operand:SI 2 "metal_nonmemory_operand" "Ri")
	  (match_operand:SI 3 "metal_nonmemory_operand" "Ri")))]
  ""
  "// selp.s32 %0, %2, %3, %1"
  [(set_attr "type" "alu")])

(define_insn "*sel_truedi"
  [(set (match_operand:DI 0 "metal_register_operand" "=R")
	(if_then_else:DI
	  (ne (match_operand:BI 1 "metal_register_operand" "R")
	      (const_int 0))
	  (match_operand:DI 2 "metal_nonmemory_operand" "Ri")
	  (match_operand:DI 3 "metal_nonmemory_operand" "Ri")))]
  ""
  "// selp.s64 %0, %2, %3, %1"
  [(set_attr "type" "alu")])

(define_insn "*sel_truesf"
  [(set (match_operand:SF 0 "metal_register_operand" "=R")
	(if_then_else:SF
	  (ne (match_operand:BI 1 "metal_register_operand" "R")
	      (const_int 0))
	  (match_operand:SF 2 "metal_register_operand" "R")
	  (match_operand:SF 3 "metal_register_operand" "R")))]
  ""
  "// selp.f32 %0, %2, %3, %1"
  [(set_attr "type" "fpu")])

(define_insn "*sel_truedf"
  [(set (match_operand:DF 0 "metal_register_operand" "=R")
	(if_then_else:DF
	  (ne (match_operand:BI 1 "metal_register_operand" "R")
	      (const_int 0))
	  (match_operand:DF 2 "metal_register_operand" "R")
	  (match_operand:DF 3 "metal_register_operand" "R")))]
  ""
  "// selp.f64 %0, %2, %3, %1"
  [(set_attr "type" "fpu")])

;; Conditional select - select if predicate false (eq 0)
(define_insn "*sel_falsesi"
  [(set (match_operand:SI 0 "metal_register_operand" "=R")
	(if_then_else:SI
	  (eq (match_operand:BI 1 "metal_register_operand" "R")
	      (const_int 0))
	  (match_operand:SI 2 "metal_nonmemory_operand" "Ri")
	  (match_operand:SI 3 "metal_nonmemory_operand" "Ri")))]
  ""
  "// selp.s32 %0, %3, %2, %1"
  [(set_attr "type" "alu")])

(define_insn "*sel_falsedi"
  [(set (match_operand:DI 0 "metal_register_operand" "=R")
	(if_then_else:DI
	  (eq (match_operand:BI 1 "metal_register_operand" "R")
	      (const_int 0))
	  (match_operand:DI 2 "metal_nonmemory_operand" "Ri")
	  (match_operand:DI 3 "metal_nonmemory_operand" "Ri")))]
  ""
  "// selp.s64 %0, %3, %2, %1"
  [(set_attr "type" "alu")])

(define_insn "*sel_falsesf"
  [(set (match_operand:SF 0 "metal_register_operand" "=R")
	(if_then_else:SF
	  (eq (match_operand:BI 1 "metal_register_operand" "R")
	      (const_int 0))
	  (match_operand:SF 2 "metal_register_operand" "R")
	  (match_operand:SF 3 "metal_register_operand" "R")))]
  ""
  "// selp.f32 %0, %3, %2, %1"
  [(set_attr "type" "fpu")])

(define_insn "*sel_falsedf"
  [(set (match_operand:DF 0 "metal_register_operand" "=R")
	(if_then_else:DF
	  (eq (match_operand:BI 1 "metal_register_operand" "R")
	      (const_int 0))
	  (match_operand:DF 2 "metal_register_operand" "R")
	  (match_operand:DF 3 "metal_register_operand" "R")))]
  ""
  "// selp.f64 %0, %3, %2, %1"
  [(set_attr "type" "fpu")])

;; Jump

(define_insn "jump"
  [(set (pc)
	(label_ref (match_operand 0 "" "")))]
  ""
  "// bra %l0"
  [(set_attr "type" "branch")])

;; Indirect jump

(define_insn "indirect_jump"
  [(set (pc)
	(match_operand:DI 0 "metal_register_operand" "R"))]
  ""
  "// bra.indirect %0"
  [(set_attr "type" "branch")])

;; Return

(define_insn "return"
  [(return)]
  ""
  "// ret"
  [(set_attr "type" "branch")])

;; No-op

(define_insn "nop"
  [(const_int 0)]
  ""
  "// nop"
  [(set_attr "type" "nop")])

;; Prologue and epilogue

(define_expand "prologue"
  [(const_int 0)]
  ""
{
  /* MSL functions have implicit stack management.  */
  DONE;
})

(define_expand "epilogue"
  [(const_int 0)]
  ""
{
  emit_jump_insn (gen_return ());
  DONE;
})

;; Function calls

(define_expand "call"
  [(call (match_operand:DI 0 "")
	 (match_operand 1 ""))]
  ""
{
  /* Metal/MSL function calls handled at source level.  */
  DONE;
})

(define_expand "call_value"
  [(set (match_operand 0 "")
	(call (match_operand:DI 1 "")
	      (match_operand 2 "")))]
  ""
{
  /* Metal/MSL function calls handled at source level.  */
  DONE;
})

;; Type conversions

(define_insn "zero_extendsidi2"
  [(set (match_operand:DI 0 "metal_register_operand" "=R")
	(zero_extend:DI (match_operand:SI 1 "metal_register_operand" "R")))]
  ""
  "// cvt.u64.u32 %0, %1"
  [(set_attr "type" "alu")])

(define_insn "extendsidi2"
  [(set (match_operand:DI 0 "metal_register_operand" "=R")
	(sign_extend:DI (match_operand:SI 1 "metal_register_operand" "R")))]
  ""
  "// cvt.s64.s32 %0, %1"
  [(set_attr "type" "alu")])

;; Atomic operations

(define_insn "atomic_compare_and_swapsi"
  [(set (match_operand:SI 0 "metal_register_operand" "=R")
	(unspec_volatile:SI
	  [(match_operand:SI 1 "memory_operand" "m")
	   (match_operand:SI 2 "metal_register_operand" "R")
	   (match_operand:SI 3 "metal_register_operand" "R")]
	  UNSPEC_ATOMIC_CAS))]
  ""
  "// atom.cas.b32 %0, [%1], %2, %3"
  [(set_attr "type" "alu")])

(define_insn "atomic_compare_and_swapdi"
  [(set (match_operand:DI 0 "metal_register_operand" "=R")
	(unspec_volatile:DI
	  [(match_operand:DI 1 "memory_operand" "m")
	   (match_operand:DI 2 "metal_register_operand" "R")
	   (match_operand:DI 3 "metal_register_operand" "R")]
	  UNSPEC_ATOMIC_CAS))]
  ""
  "// atom.cas.b64 %0, [%1], %2, %3"
  [(set_attr "type" "alu")])

(define_insn "atomic_exchangesi"
  [(set (match_operand:SI 0 "metal_register_operand" "=R")
	(unspec_volatile:SI
	  [(match_operand:SI 1 "memory_operand" "m")
	   (match_operand:SI 2 "metal_register_operand" "R")]
	  UNSPEC_ATOMIC_EXCH))]
  ""
  "// atom.exch.b32 %0, [%1], %2"
  [(set_attr "type" "alu")])

(define_insn "atomic_exchangedi"
  [(set (match_operand:DI 0 "metal_register_operand" "=R")
	(unspec_volatile:DI
	  [(match_operand:DI 1 "memory_operand" "m")
	   (match_operand:DI 2 "metal_register_operand" "R")]
	  UNSPEC_ATOMIC_EXCH))]
  ""
  "// atom.exch.b64 %0, [%1], %2"
  [(set_attr "type" "alu")])

(define_insn "atomic_fetch_addsi"
  [(set (match_operand:SI 0 "metal_register_operand" "=R")
	(unspec_volatile:SI
	  [(match_operand:SI 1 "memory_operand" "m")
	   (match_operand:SI 2 "metal_register_operand" "R")]
	  UNSPEC_ATOMIC_ADD))]
  ""
  "// atom.add.s32 %0, [%1], %2"
  [(set_attr "type" "alu")])

(define_insn "atomic_fetch_adddi"
  [(set (match_operand:DI 0 "metal_register_operand" "=R")
	(unspec_volatile:DI
	  [(match_operand:DI 1 "memory_operand" "m")
	   (match_operand:DI 2 "metal_register_operand" "R")]
	  UNSPEC_ATOMIC_ADD))]
  ""
  "// atom.add.s64 %0, [%1], %2"
  [(set_attr "type" "alu")])

(define_insn "atomic_fetch_andsi"
  [(set (match_operand:SI 0 "metal_register_operand" "=R")
	(unspec_volatile:SI
	  [(match_operand:SI 1 "memory_operand" "m")
	   (match_operand:SI 2 "metal_register_operand" "R")]
	  UNSPEC_ATOMIC_AND))]
  ""
  "// atom.and.b32 %0, [%1], %2"
  [(set_attr "type" "alu")])

(define_insn "atomic_fetch_anddi"
  [(set (match_operand:DI 0 "metal_register_operand" "=R")
	(unspec_volatile:DI
	  [(match_operand:DI 1 "memory_operand" "m")
	   (match_operand:DI 2 "metal_register_operand" "R")]
	  UNSPEC_ATOMIC_AND))]
  ""
  "// atom.and.b64 %0, [%1], %2"
  [(set_attr "type" "alu")])

(define_insn "atomic_fetch_orsi"
  [(set (match_operand:SI 0 "metal_register_operand" "=R")
	(unspec_volatile:SI
	  [(match_operand:SI 1 "memory_operand" "m")
	   (match_operand:SI 2 "metal_register_operand" "R")]
	  UNSPEC_ATOMIC_OR))]
  ""
  "// atom.or.b32 %0, [%1], %2"
  [(set_attr "type" "alu")])

(define_insn "atomic_fetch_ordi"
  [(set (match_operand:DI 0 "metal_register_operand" "=R")
	(unspec_volatile:DI
	  [(match_operand:DI 1 "memory_operand" "m")
	   (match_operand:DI 2 "metal_register_operand" "R")]
	  UNSPEC_ATOMIC_OR))]
  ""
  "// atom.or.b64 %0, [%1], %2"
  [(set_attr "type" "alu")])
