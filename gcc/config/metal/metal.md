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
  "%0 = %1;"
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
  "%0 = %1;"
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
  "%0 = %1;"
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
  "%0 = %1;"
  [(set_attr "type" "move")])

;; Half-precision floating-point operations

(define_expand "movhf"
  [(set (match_operand:HF 0 "nonimmediate_operand")
	(match_operand:HF 1 "general_operand"))]
  ""
  "")

(define_insn "*movhf_insn"
  [(set (match_operand:HF 0 "metal_register_operand" "=R")
	(match_operand:HF 1 "metal_register_operand" "R"))]
  ""
  "%0 = %1;"
  [(set_attr "type" "move")])

(define_insn "extendhfsf2"
  [(set (match_operand:SF 0 "metal_register_operand" "=R")
	(float_extend:SF (match_operand:HF 1 "metal_register_operand" "R")))]
  ""
  "%0 = static_cast<float>(%1);"
  [(set_attr "type" "fpu")])

(define_insn "extendhfdf2"
  [(set (match_operand:DF 0 "metal_register_operand" "=R")
	(float_extend:DF (match_operand:HF 1 "metal_register_operand" "R")))]
  ""
  "%0 = static_cast<double>(%1);"
  [(set_attr "type" "fpu")])

(define_insn "truncsfhf2"
  [(set (match_operand:HF 0 "metal_register_operand" "=R")
	(float_truncate:HF (match_operand:SF 1 "metal_register_operand" "R")))]
  ""
  "%0 = static_cast<half>(%1);"
  [(set_attr "type" "fpu")])

(define_insn "truncdfhf2"
  [(set (match_operand:HF 0 "metal_register_operand" "=R")
	(float_truncate:HF (match_operand:DF 1 "metal_register_operand" "R")))]
  ""
  "%0 = static_cast<half>(%1);"
  [(set_attr "type" "fpu")])

(define_insn "addhf3"
  [(set (match_operand:HF 0 "metal_register_operand" "=R")
	(plus:HF (match_operand:HF 1 "metal_register_operand" "R")
		 (match_operand:HF 2 "metal_register_operand" "R")))]
  ""
  "%0 = %1 + %2;"
  [(set_attr "type" "fpu")])

(define_insn "subhf3"
  [(set (match_operand:HF 0 "metal_register_operand" "=R")
	(minus:HF (match_operand:HF 1 "metal_register_operand" "R")
		  (match_operand:HF 2 "metal_register_operand" "R")))]
  ""
  "%0 = %1 - %2;"
  [(set_attr "type" "fpu")])

(define_insn "mulhf3"
  [(set (match_operand:HF 0 "metal_register_operand" "=R")
	(mult:HF (match_operand:HF 1 "metal_register_operand" "R")
		 (match_operand:HF 2 "metal_register_operand" "R")))]
  ""
  "%0 = %1 * %2;"
  [(set_attr "type" "fpu")])

(define_insn "neghf2"
  [(set (match_operand:HF 0 "metal_register_operand" "=R")
	(neg:HF (match_operand:HF 1 "metal_register_operand" "R")))]
  ""
  "%0 = -%1;"
  [(set_attr "type" "fpu")])

(define_insn "abshf2"
  [(set (match_operand:HF 0 "metal_register_operand" "=R")
	(abs:HF (match_operand:HF 1 "metal_register_operand" "R")))]
  ""
  "%0 = abs(%1);"
  [(set_attr "type" "fpu")])

(define_insn "fmahf4"
  [(set (match_operand:HF 0 "metal_register_operand" "=R")
	(fma:HF (match_operand:HF 1 "metal_register_operand" "R")
		(match_operand:HF 2 "metal_register_operand" "R")
		(match_operand:HF 3 "metal_register_operand" "R")))]
  ""
  "%0 = fma(%1, %2, %3);"
  [(set_attr "type" "fpu")])

;; Arithmetic patterns

(define_insn "addsi3"
  [(set (match_operand:SI 0 "metal_register_operand" "=R")
	(plus:SI (match_operand:SI 1 "metal_register_operand" "R")
		 (match_operand:SI 2 "metal_nonmemory_operand" "Ri")))]
  ""
  "%0 = %1 + %2;"
  [(set_attr "type" "alu")])

(define_insn "adddi3"
  [(set (match_operand:DI 0 "metal_register_operand" "=R")
	(plus:DI (match_operand:DI 1 "metal_register_operand" "R")
		 (match_operand:DI 2 "metal_nonmemory_operand" "Ri")))]
  ""
  "%0 = %1 + %2;"
  [(set_attr "type" "alu")])

(define_insn "subsi3"
  [(set (match_operand:SI 0 "metal_register_operand" "=R")
	(minus:SI (match_operand:SI 1 "metal_register_operand" "R")
		  (match_operand:SI 2 "metal_nonmemory_operand" "Ri")))]
  ""
  "%0 = %1 - %2;"
  [(set_attr "type" "alu")])

(define_insn "subdi3"
  [(set (match_operand:DI 0 "metal_register_operand" "=R")
	(minus:DI (match_operand:DI 1 "metal_register_operand" "R")
		  (match_operand:DI 2 "metal_nonmemory_operand" "Ri")))]
  ""
  "%0 = %1 - %2;"
  [(set_attr "type" "alu")])

(define_insn "mulsi3"
  [(set (match_operand:SI 0 "metal_register_operand" "=R")
	(mult:SI (match_operand:SI 1 "metal_register_operand" "R")
		 (match_operand:SI 2 "metal_register_operand" "R")))]
  ""
  "%0 = %1 * %2;"
  [(set_attr "type" "alu")])

(define_insn "muldi3"
  [(set (match_operand:DI 0 "metal_register_operand" "=R")
	(mult:DI (match_operand:DI 1 "metal_register_operand" "R")
		 (match_operand:DI 2 "metal_register_operand" "R")))]
  ""
  "%0 = %1 * %2;"
  [(set_attr "type" "alu")])

;; Multiply-accumulate operations

;; Integer MAD: (a * b) + c
(define_insn "*maddsi4"
  [(set (match_operand:SI 0 "metal_register_operand" "=R")
        (plus:SI (mult:SI (match_operand:SI 1 "metal_register_operand" "R")
                          (match_operand:SI 2 "metal_register_operand" "R"))
                 (match_operand:SI 3 "metal_register_operand" "R")))]
  ""
  "%0 = %1 * %2 + %3;"
  [(set_attr "type" "alu")])

(define_insn "*madddi4"
  [(set (match_operand:DI 0 "metal_register_operand" "=R")
        (plus:DI (mult:DI (match_operand:DI 1 "metal_register_operand" "R")
                          (match_operand:DI 2 "metal_register_operand" "R"))
                 (match_operand:DI 3 "metal_register_operand" "R")))]
  ""
  "%0 = %1 * %2 + %3;"
  [(set_attr "type" "alu")])

;; Unsigned MAD
(define_insn "*umaddsi4"
  [(set (match_operand:SI 0 "metal_register_operand" "=R")
        (plus:SI (mult:SI (match_operand:SI 1 "metal_register_operand" "R")
                          (match_operand:SI 2 "metal_register_operand" "R"))
                 (match_operand:SI 3 "metal_register_operand" "R")))]
  ""
  "%0 = %1 * %2 + %3;"
  [(set_attr "type" "alu")])

(define_insn "*umadddi4"
  [(set (match_operand:DI 0 "metal_register_operand" "=R")
        (plus:DI (mult:DI (match_operand:DI 1 "metal_register_operand" "R")
                          (match_operand:DI 2 "metal_register_operand" "R"))
                 (match_operand:DI 3 "metal_register_operand" "R")))]
  ""
  "%0 = %1 * %2 + %3;"
  [(set_attr "type" "alu")])

;; Multiply-subtract: (a * b) - c
(define_insn "*msubsi4"
  [(set (match_operand:SI 0 "metal_register_operand" "=R")
        (minus:SI (mult:SI (match_operand:SI 1 "metal_register_operand" "R")
                           (match_operand:SI 2 "metal_register_operand" "R"))
                  (match_operand:SI 3 "metal_register_operand" "R")))]
  ""
  "%0 = %1 * %2 - %3;"
  [(set_attr "type" "alu")])

(define_insn "*msubdi4"
  [(set (match_operand:DI 0 "metal_register_operand" "=R")
        (minus:DI (mult:DI (match_operand:DI 1 "metal_register_operand" "R")
                           (match_operand:DI 2 "metal_register_operand" "R"))
                  (match_operand:DI 3 "metal_register_operand" "R")))]
  ""
  "%0 = %1 * %2 - %3;"
  [(set_attr "type" "alu")])

;; Unsigned MSUB
(define_insn "*umsubsi4"
  [(set (match_operand:SI 0 "metal_register_operand" "=R")
        (minus:SI (mult:SI (match_operand:SI 1 "metal_register_operand" "R")
                           (match_operand:SI 2 "metal_register_operand" "R"))
                  (match_operand:SI 3 "metal_register_operand" "R")))]
  ""
  "%0 = %1 * %2 - %3;"
  [(set_attr "type" "alu")])

(define_insn "*umsubdi4"
  [(set (match_operand:DI 0 "metal_register_operand" "=R")
        (minus:DI (mult:DI (match_operand:DI 1 "metal_register_operand" "R")
                           (match_operand:DI 2 "metal_register_operand" "R"))
                  (match_operand:DI 3 "metal_register_operand" "R")))]
  ""
  "%0 = %1 * %2 - %3;"
  [(set_attr "type" "alu")])

;; Division and modulo

(define_insn "divsi3"
  [(set (match_operand:SI 0 "metal_register_operand" "=R")
	(div:SI (match_operand:SI 1 "metal_register_operand" "R")
		(match_operand:SI 2 "metal_register_operand" "R")))]
  ""
  "%0 = %1 / %2;"
  [(set_attr "type" "alu")])

(define_insn "udivsi3"
  [(set (match_operand:SI 0 "metal_register_operand" "=R")
	(udiv:SI (match_operand:SI 1 "metal_register_operand" "R")
		 (match_operand:SI 2 "metal_register_operand" "R")))]
  ""
  "%0 = %1 / %2;"
  [(set_attr "type" "alu")])

(define_insn "modsi3"
  [(set (match_operand:SI 0 "metal_register_operand" "=R")
	(mod:SI (match_operand:SI 1 "metal_register_operand" "R")
		(match_operand:SI 2 "metal_register_operand" "R")))]
  ""
  "%0 = %1 % %2;"
  [(set_attr "type" "alu")])

(define_insn "umodsi3"
  [(set (match_operand:SI 0 "metal_register_operand" "=R")
	(umod:SI (match_operand:SI 1 "metal_register_operand" "R")
		 (match_operand:SI 2 "metal_register_operand" "R")))]
  ""
  "%0 = %1 % %2;"
  [(set_attr "type" "alu")])

(define_insn "divdi3"
  [(set (match_operand:DI 0 "metal_register_operand" "=R")
	(div:DI (match_operand:DI 1 "metal_register_operand" "R")
		(match_operand:DI 2 "metal_register_operand" "R")))]
  ""
  "%0 = %1 / %2;"
  [(set_attr "type" "alu")])

(define_insn "udivdi3"
  [(set (match_operand:DI 0 "metal_register_operand" "=R")
	(udiv:DI (match_operand:DI 1 "metal_register_operand" "R")
		 (match_operand:DI 2 "metal_register_operand" "R")))]
  ""
  "%0 = %1 / %2;"
  [(set_attr "type" "alu")])

(define_insn "moddi3"
  [(set (match_operand:DI 0 "metal_register_operand" "=R")
	(mod:DI (match_operand:DI 1 "metal_register_operand" "R")
		(match_operand:DI 2 "metal_register_operand" "R")))]
  ""
  "%0 = %1 % %2;"
  [(set_attr "type" "alu")])

(define_insn "umoddi3"
  [(set (match_operand:DI 0 "metal_register_operand" "=R")
	(umod:DI (match_operand:DI 1 "metal_register_operand" "R")
		 (match_operand:DI 2 "metal_register_operand" "R")))]
  ""
  "%0 = %1 % %2;"
  [(set_attr "type" "alu")])

;; Unary arithmetic

(define_insn "negsi2"
  [(set (match_operand:SI 0 "metal_register_operand" "=R")
	(neg:SI (match_operand:SI 1 "metal_register_operand" "R")))]
  ""
  "%0 = -%1;"
  [(set_attr "type" "alu")])

(define_insn "negdi2"
  [(set (match_operand:DI 0 "metal_register_operand" "=R")
	(neg:DI (match_operand:DI 1 "metal_register_operand" "R")))]
  ""
  "%0 = -%1;"
  [(set_attr "type" "alu")])

(define_insn "one_cmplsi2"
  [(set (match_operand:SI 0 "metal_register_operand" "=R")
	(not:SI (match_operand:SI 1 "metal_register_operand" "R")))]
  ""
  "%0 = ~%1;"
  [(set_attr "type" "alu")])

(define_insn "one_cmpldi2"
  [(set (match_operand:DI 0 "metal_register_operand" "=R")
	(not:DI (match_operand:DI 1 "metal_register_operand" "R")))]
  ""
  "%0 = ~%1;"
  [(set_attr "type" "alu")])

;; Logical operations

(define_insn "andsi3"
  [(set (match_operand:SI 0 "metal_register_operand" "=R")
	(and:SI (match_operand:SI 1 "metal_register_operand" "R")
		(match_operand:SI 2 "metal_register_operand" "R")))]
  ""
  "%0 = %1 & %2;"
  [(set_attr "type" "alu")])

(define_insn "anddi3"
  [(set (match_operand:DI 0 "metal_register_operand" "=R")
	(and:DI (match_operand:DI 1 "metal_register_operand" "R")
		(match_operand:DI 2 "metal_register_operand" "R")))]
  ""
  "%0 = %1 & %2;"
  [(set_attr "type" "alu")])

(define_insn "iorsi3"
  [(set (match_operand:SI 0 "metal_register_operand" "=R")
	(ior:SI (match_operand:SI 1 "metal_register_operand" "R")
		(match_operand:SI 2 "metal_register_operand" "R")))]
  ""
  "%0 = %1 | %2;"
  [(set_attr "type" "alu")])

(define_insn "iordi3"
  [(set (match_operand:DI 0 "metal_register_operand" "=R")
	(ior:DI (match_operand:DI 1 "metal_register_operand" "R")
		(match_operand:DI 2 "metal_register_operand" "R")))]
  ""
  "%0 = %1 | %2;"
  [(set_attr "type" "alu")])

(define_insn "xorsi3"
  [(set (match_operand:SI 0 "metal_register_operand" "=R")
	(xor:SI (match_operand:SI 1 "metal_register_operand" "R")
		(match_operand:SI 2 "metal_register_operand" "R")))]
  ""
  "%0 = %1 ^ %2;"
  [(set_attr "type" "alu")])

(define_insn "xordi3"
  [(set (match_operand:DI 0 "metal_register_operand" "=R")
	(xor:DI (match_operand:DI 1 "metal_register_operand" "R")
		(match_operand:DI 2 "metal_register_operand" "R")))]
  ""
  "%0 = %1 ^ %2;"
  [(set_attr "type" "alu")])

;; Shift operations

(define_insn "ashlsi3"
  [(set (match_operand:SI 0 "metal_register_operand" "=R")
	(ashift:SI (match_operand:SI 1 "metal_register_operand" "R")
		   (match_operand:SI 2 "metal_nonmemory_operand" "Ri")))]
  ""
  "%0 = %1 << %2;"
  [(set_attr "type" "alu")])

(define_insn "ashldi3"
  [(set (match_operand:DI 0 "metal_register_operand" "=R")
	(ashift:DI (match_operand:DI 1 "metal_register_operand" "R")
		   (match_operand:SI 2 "metal_nonmemory_operand" "Ri")))]
  ""
  "%0 = %1 << %2;"
  [(set_attr "type" "alu")])

(define_insn "ashrsi3"
  [(set (match_operand:SI 0 "metal_register_operand" "=R")
	(ashiftrt:SI (match_operand:SI 1 "metal_register_operand" "R")
		     (match_operand:SI 2 "metal_nonmemory_operand" "Ri")))]
  ""
  "%0 = %1 >> %2;"
  [(set_attr "type" "alu")])

(define_insn "ashrdi3"
  [(set (match_operand:DI 0 "metal_register_operand" "=R")
	(ashiftrt:DI (match_operand:DI 1 "metal_register_operand" "R")
		     (match_operand:SI 2 "metal_nonmemory_operand" "Ri")))]
  ""
  "%0 = %1 >> %2;"
  [(set_attr "type" "alu")])

(define_insn "lshrsi3"
  [(set (match_operand:SI 0 "metal_register_operand" "=R")
	(lshiftrt:SI (match_operand:SI 1 "metal_register_operand" "R")
		     (match_operand:SI 2 "metal_nonmemory_operand" "Ri")))]
  ""
  "%0 = %1 >> %2;"
  [(set_attr "type" "alu")])

(define_insn "lshrdi3"
  [(set (match_operand:DI 0 "metal_register_operand" "=R")
	(lshiftrt:DI (match_operand:DI 1 "metal_register_operand" "R")
		     (match_operand:SI 2 "metal_nonmemory_operand" "Ri")))]
  ""
  "%0 = %1 >> %2;"
  [(set_attr "type" "alu")])

;; Bit manipulation operations

(define_insn "bitrevsi2"
  [(set (match_operand:SI 0 "metal_register_operand" "=R")
	(bitreverse:SI (match_operand:SI 1 "metal_register_operand" "R")))]
  ""
  "%0 = reverse_bits(%1);"
  [(set_attr "type" "alu")])

(define_insn "bitrevdi2"
  [(set (match_operand:DI 0 "metal_register_operand" "=R")
	(bitreverse:DI (match_operand:DI 1 "metal_register_operand" "R")))]
  ""
  "%0 = reverse_bits(%1);"
  [(set_attr "type" "alu")])

(define_insn "clzsi2"
  [(set (match_operand:SI 0 "metal_register_operand" "=R")
	(clz:SI (match_operand:SI 1 "metal_register_operand" "R")))]
  ""
  "%0 = clz(%1);"
  [(set_attr "type" "alu")])

(define_insn "clzdi2"
  [(set (match_operand:SI 0 "metal_register_operand" "=R")
	(clz:SI (match_operand:DI 1 "metal_register_operand" "R")))]
  ""
  "%0 = clz(%1);"
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
  "%0 = popcount(%1);"
  [(set_attr "type" "alu")])

(define_insn "popcountdi2"
  [(set (match_operand:SI 0 "metal_register_operand" "=R")
	(truncate:SI
	  (popcount:DI (match_operand:DI 1 "metal_register_operand" "R"))))]
  ""
  "%0 = popcount(%1);"
  [(set_attr "type" "alu")])

;; Rotate operations
;; Decomposed into shift and or operations since Metal may not have direct rotate

(define_expand "rotlsi3"
  [(set (match_operand:SI 0 "metal_register_operand")
	(rotate:SI (match_operand:SI 1 "metal_register_operand")
		   (match_operand:SI 2 "metal_nonmemory_operand")))]
  ""
{
  rtx temp = gen_reg_rtx (SImode);
  rtx shift_amount = operands[2];
  rtx inv_shift = gen_reg_rtx (SImode);

  emit_insn (gen_subsi3 (inv_shift, GEN_INT (32), shift_amount));
  emit_insn (gen_ashlsi3 (temp, operands[1], shift_amount));
  emit_insn (gen_lshrsi3 (operands[0], operands[1], inv_shift));
  emit_insn (gen_iorsi3 (operands[0], operands[0], temp));
  DONE;
})

(define_expand "rotldi3"
  [(set (match_operand:DI 0 "metal_register_operand")
	(rotate:DI (match_operand:DI 1 "metal_register_operand")
		   (match_operand:SI 2 "metal_nonmemory_operand")))]
  ""
{
  rtx temp = gen_reg_rtx (DImode);
  rtx shift_amount = operands[2];
  rtx inv_shift = gen_reg_rtx (SImode);

  emit_insn (gen_subsi3 (inv_shift, GEN_INT (64), shift_amount));
  emit_insn (gen_ashldi3 (temp, operands[1], shift_amount));
  emit_insn (gen_lshrdi3 (operands[0], operands[1], inv_shift));
  emit_insn (gen_iordi3 (operands[0], operands[0], temp));
  DONE;
})

(define_expand "rotrsi3"
  [(set (match_operand:SI 0 "metal_register_operand")
	(rotatert:SI (match_operand:SI 1 "metal_register_operand")
		     (match_operand:SI 2 "metal_nonmemory_operand")))]
  ""
{
  rtx temp = gen_reg_rtx (SImode);
  rtx shift_amount = operands[2];
  rtx inv_shift = gen_reg_rtx (SImode);

  emit_insn (gen_subsi3 (inv_shift, GEN_INT (32), shift_amount));
  emit_insn (gen_lshrsi3 (temp, operands[1], shift_amount));
  emit_insn (gen_ashlsi3 (operands[0], operands[1], inv_shift));
  emit_insn (gen_iorsi3 (operands[0], operands[0], temp));
  DONE;
})

(define_expand "rotrdi3"
  [(set (match_operand:DI 0 "metal_register_operand")
	(rotatert:DI (match_operand:DI 1 "metal_register_operand")
		     (match_operand:SI 2 "metal_nonmemory_operand")))]
  ""
{
  rtx temp = gen_reg_rtx (DImode);
  rtx shift_amount = operands[2];
  rtx inv_shift = gen_reg_rtx (SImode);

  emit_insn (gen_subsi3 (inv_shift, GEN_INT (64), shift_amount));
  emit_insn (gen_lshrdi3 (temp, operands[1], shift_amount));
  emit_insn (gen_ashldi3 (operands[0], operands[1], inv_shift));
  emit_insn (gen_iordi3 (operands[0], operands[0], temp));
  DONE;
})

;; Floating point arithmetic

(define_insn "addsf3"
  [(set (match_operand:SF 0 "metal_register_operand" "=R")
	(plus:SF (match_operand:SF 1 "metal_register_operand" "R")
		 (match_operand:SF 2 "metal_register_operand" "R")))]
  ""
  "%0 = %1 + %2;"
  [(set_attr "type" "fpu")])

(define_insn "adddf3"
  [(set (match_operand:DF 0 "metal_register_operand" "=R")
	(plus:DF (match_operand:DF 1 "metal_register_operand" "R")
		 (match_operand:DF 2 "metal_register_operand" "R")))]
  ""
  "%0 = %1 + %2;"
  [(set_attr "type" "fpu")])

(define_insn "subsf3"
  [(set (match_operand:SF 0 "metal_register_operand" "=R")
	(minus:SF (match_operand:SF 1 "metal_register_operand" "R")
		  (match_operand:SF 2 "metal_register_operand" "R")))]
  ""
  "%0 = %1 - %2;"
  [(set_attr "type" "fpu")])

(define_insn "subdf3"
  [(set (match_operand:DF 0 "metal_register_operand" "=R")
	(minus:DF (match_operand:DF 1 "metal_register_operand" "R")
		  (match_operand:DF 2 "metal_register_operand" "R")))]
  ""
  "%0 = %1 - %2;"
  [(set_attr "type" "fpu")])

(define_insn "mulsf3"
  [(set (match_operand:SF 0 "metal_register_operand" "=R")
	(mult:SF (match_operand:SF 1 "metal_register_operand" "R")
		 (match_operand:SF 2 "metal_register_operand" "R")))]
  ""
  "%0 = %1 * %2;"
  [(set_attr "type" "fpu")])

(define_insn "muldf3"
  [(set (match_operand:DF 0 "metal_register_operand" "=R")
	(mult:DF (match_operand:DF 1 "metal_register_operand" "R")
		 (match_operand:DF 2 "metal_register_operand" "R")))]
  ""
  "%0 = %1 * %2;"
  [(set_attr "type" "fpu")])

(define_insn "divsf3"
  [(set (match_operand:SF 0 "metal_register_operand" "=R")
	(div:SF (match_operand:SF 1 "metal_register_operand" "R")
		(match_operand:SF 2 "metal_register_operand" "R")))]
  ""
  "%0 = %1 / %2;"
  [(set_attr "type" "fpu")])

(define_insn "divdf3"
  [(set (match_operand:DF 0 "metal_register_operand" "=R")
	(div:DF (match_operand:DF 1 "metal_register_operand" "R")
		(match_operand:DF 2 "metal_register_operand" "R")))]
  ""
  "%0 = %1 / %2;"
  [(set_attr "type" "fpu")])

;; Floating point unary operations

(define_insn "negsf2"
  [(set (match_operand:SF 0 "metal_register_operand" "=R")
	(neg:SF (match_operand:SF 1 "metal_register_operand" "R")))]
  ""
  "%0 = -%1;"
  [(set_attr "type" "fpu")])

(define_insn "negdf2"
  [(set (match_operand:DF 0 "metal_register_operand" "=R")
	(neg:DF (match_operand:DF 1 "metal_register_operand" "R")))]
  ""
  "%0 = -%1;"
  [(set_attr "type" "fpu")])

(define_insn "abssf2"
  [(set (match_operand:SF 0 "metal_register_operand" "=R")
	(abs:SF (match_operand:SF 1 "metal_register_operand" "R")))]
  ""
  "%0 = abs(%1);"
  [(set_attr "type" "fpu")])

(define_insn "absdf2"
  [(set (match_operand:DF 0 "metal_register_operand" "=R")
	(abs:DF (match_operand:DF 1 "metal_register_operand" "R")))]
  ""
  "%0 = abs(%1);"
  [(set_attr "type" "fpu")])

(define_insn "sqrtsf2"
  [(set (match_operand:SF 0 "metal_register_operand" "=R")
	(sqrt:SF (match_operand:SF 1 "metal_register_operand" "R")))]
  ""
  "%0 = sqrt(%1);"
  [(set_attr "type" "fpu")])

(define_insn "sqrtdf2"
  [(set (match_operand:DF 0 "metal_register_operand" "=R")
	(sqrt:DF (match_operand:DF 1 "metal_register_operand" "R")))]
  ""
  "%0 = sqrt(%1);"
  [(set_attr "type" "fpu")])

;; Fused multiply-add

(define_insn "fmasf4"
  [(set (match_operand:SF 0 "metal_register_operand" "=R")
	(fma:SF (match_operand:SF 1 "metal_register_operand" "R")
		(match_operand:SF 2 "metal_register_operand" "R")
		(match_operand:SF 3 "metal_register_operand" "R")))]
  ""
  "%0 = fma(%1, %2, %3);"
  [(set_attr "type" "fpu")])

(define_insn "fmadf4"
  [(set (match_operand:DF 0 "metal_register_operand" "=R")
	(fma:DF (match_operand:DF 1 "metal_register_operand" "R")
		(match_operand:DF 2 "metal_register_operand" "R")
		(match_operand:DF 3 "metal_register_operand" "R")))]
  ""
  "%0 = fma(%1, %2, %3);"
  [(set_attr "type" "fpu")])

;; Floating-point special functions

(define_insn "log2sf2"
  [(set (match_operand:SF 0 "metal_register_operand" "=R")
	(unspec:SF [(match_operand:SF 1 "metal_register_operand" "R")]
		   UNSPEC_LOG2))]
  ""
  "%0 = log2(%1);"
  [(set_attr "type" "fpu")])

(define_insn "log2df2"
  [(set (match_operand:DF 0 "metal_register_operand" "=R")
	(unspec:DF [(match_operand:DF 1 "metal_register_operand" "R")]
		   UNSPEC_LOG2))]
  ""
  "%0 = log2(%1);"
  [(set_attr "type" "fpu")])

(define_insn "exp2sf2"
  [(set (match_operand:SF 0 "metal_register_operand" "=R")
	(unspec:SF [(match_operand:SF 1 "metal_register_operand" "R")]
		   UNSPEC_EXP2))]
  ""
  "%0 = exp2(%1);"
  [(set_attr "type" "fpu")])

(define_insn "exp2df2"
  [(set (match_operand:DF 0 "metal_register_operand" "=R")
	(unspec:DF [(match_operand:DF 1 "metal_register_operand" "R")]
		   UNSPEC_EXP2))]
  ""
  "%0 = exp2(%1);"
  [(set_attr "type" "fpu")])

(define_insn "sinsf2"
  [(set (match_operand:SF 0 "metal_register_operand" "=R")
	(unspec:SF [(match_operand:SF 1 "metal_register_operand" "R")]
		   UNSPEC_SIN))]
  ""
  "%0 = sin(%1);"
  [(set_attr "type" "fpu")])

(define_insn "sindf2"
  [(set (match_operand:DF 0 "metal_register_operand" "=R")
	(unspec:DF [(match_operand:DF 1 "metal_register_operand" "R")]
		   UNSPEC_SIN))]
  ""
  "%0 = sin(%1);"
  [(set_attr "type" "fpu")])

(define_insn "cossf2"
  [(set (match_operand:SF 0 "metal_register_operand" "=R")
	(unspec:SF [(match_operand:SF 1 "metal_register_operand" "R")]
		   UNSPEC_COS))]
  ""
  "%0 = cos(%1);"
  [(set_attr "type" "fpu")])

(define_insn "cosdf2"
  [(set (match_operand:DF 0 "metal_register_operand" "=R")
	(unspec:DF [(match_operand:DF 1 "metal_register_operand" "R")]
		   UNSPEC_COS))]
  ""
  "%0 = cos(%1);"
  [(set_attr "type" "fpu")])

(define_insn "rsqrtsf2"
  [(set (match_operand:SF 0 "metal_register_operand" "=R")
	(unspec:SF [(match_operand:SF 1 "metal_register_operand" "R")]
		   UNSPEC_RSQRT))]
  ""
  "%0 = rsqrt(%1);"
  [(set_attr "type" "fpu")])

(define_insn "rsqrtdf2"
  [(set (match_operand:DF 0 "metal_register_operand" "=R")
	(unspec:DF [(match_operand:DF 1 "metal_register_operand" "R")]
		   UNSPEC_RSQRT))]
  ""
  "%0 = rsqrt(%1);"
  [(set_attr "type" "fpu")])

;; Integer min/max

(define_insn "sminsi3"
  [(set (match_operand:SI 0 "metal_register_operand" "=R")
	(smin:SI (match_operand:SI 1 "metal_register_operand" "R")
		 (match_operand:SI 2 "metal_nonmemory_operand" "Ri")))]
  ""
  "%0 = min(%1, %2);"
  [(set_attr "type" "alu")])

(define_insn "smaxsi3"
  [(set (match_operand:SI 0 "metal_register_operand" "=R")
	(smax:SI (match_operand:SI 1 "metal_register_operand" "R")
		 (match_operand:SI 2 "metal_nonmemory_operand" "Ri")))]
  ""
  "%0 = max(%1, %2);"
  [(set_attr "type" "alu")])

(define_insn "uminsi3"
  [(set (match_operand:SI 0 "metal_register_operand" "=R")
	(umin:SI (match_operand:SI 1 "metal_register_operand" "R")
		 (match_operand:SI 2 "metal_nonmemory_operand" "Ri")))]
  ""
  "%0 = min(%1, %2);"
  [(set_attr "type" "alu")])

(define_insn "umaxsi3"
  [(set (match_operand:SI 0 "metal_register_operand" "=R")
	(umax:SI (match_operand:SI 1 "metal_register_operand" "R")
		 (match_operand:SI 2 "metal_nonmemory_operand" "Ri")))]
  ""
  "%0 = max(%1, %2);"
  [(set_attr "type" "alu")])

(define_insn "smindi3"
  [(set (match_operand:DI 0 "metal_register_operand" "=R")
	(smin:DI (match_operand:DI 1 "metal_register_operand" "R")
		 (match_operand:DI 2 "metal_nonmemory_operand" "Ri")))]
  ""
  "%0 = min(%1, %2);"
  [(set_attr "type" "alu")])

(define_insn "smaxdi3"
  [(set (match_operand:DI 0 "metal_register_operand" "=R")
	(smax:DI (match_operand:DI 1 "metal_register_operand" "R")
		 (match_operand:DI 2 "metal_nonmemory_operand" "Ri")))]
  ""
  "%0 = max(%1, %2);"
  [(set_attr "type" "alu")])

(define_insn "umindi3"
  [(set (match_operand:DI 0 "metal_register_operand" "=R")
	(umin:DI (match_operand:DI 1 "metal_register_operand" "R")
		 (match_operand:DI 2 "metal_nonmemory_operand" "Ri")))]
  ""
  "%0 = min(%1, %2);"
  [(set_attr "type" "alu")])

(define_insn "umaxdi3"
  [(set (match_operand:DI 0 "metal_register_operand" "=R")
	(umax:DI (match_operand:DI 1 "metal_register_operand" "R")
		 (match_operand:DI 2 "metal_nonmemory_operand" "Ri")))]
  ""
  "%0 = max(%1, %2);"
  [(set_attr "type" "alu")])

;; Floating point min/max

(define_insn "sminsf3"
  [(set (match_operand:SF 0 "metal_register_operand" "=R")
	(smin:SF (match_operand:SF 1 "metal_register_operand" "R")
		 (match_operand:SF 2 "metal_register_operand" "R")))]
  ""
  "%0 = min(%1, %2);"
  [(set_attr "type" "fpu")])

(define_insn "smaxsf3"
  [(set (match_operand:SF 0 "metal_register_operand" "=R")
	(smax:SF (match_operand:SF 1 "metal_register_operand" "R")
		 (match_operand:SF 2 "metal_register_operand" "R")))]
  ""
  "%0 = max(%1, %2);"
  [(set_attr "type" "fpu")])

(define_insn "smindf3"
  [(set (match_operand:DF 0 "metal_register_operand" "=R")
	(smin:DF (match_operand:DF 1 "metal_register_operand" "R")
		 (match_operand:DF 2 "metal_register_operand" "R")))]
  ""
  "%0 = min(%1, %2);"
  [(set_attr "type" "fpu")])

(define_insn "smaxdf3"
  [(set (match_operand:DF 0 "metal_register_operand" "=R")
	(smax:DF (match_operand:DF 1 "metal_register_operand" "R")
		 (match_operand:DF 2 "metal_register_operand" "R")))]
  ""
  "%0 = max(%1, %2);"
  [(set_attr "type" "fpu")])

;; Integer absolute value

(define_insn "abssi2"
  [(set (match_operand:SI 0 "metal_register_operand" "=R")
	(abs:SI (match_operand:SI 1 "metal_register_operand" "R")))]
  ""
  "%0 = abs(%1);"
  [(set_attr "type" "alu")])

(define_insn "absdi2"
  [(set (match_operand:DI 0 "metal_register_operand" "=R")
	(abs:DI (match_operand:DI 1 "metal_register_operand" "R")))]
  ""
  "%0 = abs(%1);"
  [(set_attr "type" "alu")])

;; Wide multiply (SI x SI -> DI)

(define_insn "mulsidi3"
  [(set (match_operand:DI 0 "metal_register_operand" "=R")
	(mult:DI (sign_extend:DI
		  (match_operand:SI 1 "metal_register_operand" "R"))
		 (sign_extend:DI
		  (match_operand:SI 2 "metal_register_operand" "R"))))]
  ""
  "%0 = static_cast<long>(%1) * static_cast<long>(%2);"
  [(set_attr "type" "alu")])

(define_insn "umulsidi3"
  [(set (match_operand:DI 0 "metal_register_operand" "=R")
	(mult:DI (zero_extend:DI
		  (match_operand:SI 1 "metal_register_operand" "R"))
		 (zero_extend:DI
		  (match_operand:SI 2 "metal_register_operand" "R"))))]
  ""
  "%0 = static_cast<ulong>(%1) * static_cast<ulong>(%2);"
  [(set_attr "type" "alu")])

;; High-part multiply

(define_insn "smulsi3_highpart"
  [(set (match_operand:SI 0 "metal_register_operand" "=R")
	(smul_highpart:SI
	  (match_operand:SI 1 "metal_register_operand" "R")
	  (match_operand:SI 2 "metal_nonmemory_operand" "Ri")))]
  ""
  "%0 = mulhi(%1, %2);"
  [(set_attr "type" "alu")])

(define_insn "umulsi3_highpart"
  [(set (match_operand:SI 0 "metal_register_operand" "=R")
	(umul_highpart:SI
	  (match_operand:SI 1 "metal_register_operand" "R")
	  (match_operand:SI 2 "metal_nonmemory_operand" "Ri")))]
  ""
  "%0 = mulhi(%1, %2);"
  [(set_attr "type" "alu")])

(define_insn "smuldi3_highpart"
  [(set (match_operand:DI 0 "metal_register_operand" "=R")
	(smul_highpart:DI
	  (match_operand:DI 1 "metal_register_operand" "R")
	  (match_operand:DI 2 "metal_nonmemory_operand" "Ri")))]
  ""
  "%0 = mulhi(%1, %2);"
  [(set_attr "type" "alu")])

(define_insn "umuldi3_highpart"
  [(set (match_operand:DI 0 "metal_register_operand" "=R")
	(umul_highpart:DI
	  (match_operand:DI 1 "metal_register_operand" "R")
	  (match_operand:DI 2 "metal_nonmemory_operand" "Ri")))]
  ""
  "%0 = mulhi(%1, %2);"
  [(set_attr "type" "alu")])
;; Copysign

(define_insn "copysignsf3"
  [(set (match_operand:SF 0 "metal_register_operand" "=R")
	(unspec:SF [(match_operand:SF 1 "metal_register_operand" "R")
		    (match_operand:SF 2 "metal_register_operand" "R")]
		   UNSPEC_COPYSIGN))]
  ""
  "%0 = copysign(%1, %2);"
  [(set_attr "type" "fpu")])

(define_insn "copysigndf3"
  [(set (match_operand:DF 0 "metal_register_operand" "=R")
	(unspec:DF [(match_operand:DF 1 "metal_register_operand" "R")
		    (match_operand:DF 2 "metal_register_operand" "R")]
		   UNSPEC_COPYSIGN))]
  ""
  "%0 = copysign(%1, %2);"
  [(set_attr "type" "fpu")])

;; Floating point type conversions

(define_insn "extendsfdf2"
  [(set (match_operand:DF 0 "metal_register_operand" "=R")
	(float_extend:DF (match_operand:SF 1 "metal_register_operand" "R")))]
  ""
  "%0 = static_cast<double>(%1);"
  [(set_attr "type" "fpu")])

(define_insn "truncdfsf2"
  [(set (match_operand:SF 0 "metal_register_operand" "=R")
	(float_truncate:SF (match_operand:DF 1 "metal_register_operand" "R")))]
  ""
  "%0 = static_cast<float>(%1);"
  [(set_attr "type" "fpu")])

(define_insn "floatsisf2"
  [(set (match_operand:SF 0 "metal_register_operand" "=R")
	(float:SF (match_operand:SI 1 "metal_register_operand" "R")))]
  ""
  "%0 = static_cast<float>(%1);"
  [(set_attr "type" "fpu")])

(define_insn "floatsidf2"
  [(set (match_operand:DF 0 "metal_register_operand" "=R")
	(float:DF (match_operand:SI 1 "metal_register_operand" "R")))]
  ""
  "%0 = static_cast<double>(%1);"
  [(set_attr "type" "fpu")])

(define_insn "floatdisf2"
  [(set (match_operand:SF 0 "metal_register_operand" "=R")
	(float:SF (match_operand:DI 1 "metal_register_operand" "R")))]
  ""
  "%0 = static_cast<float>(%1);"
  [(set_attr "type" "fpu")])

(define_insn "floatdidf2"
  [(set (match_operand:DF 0 "metal_register_operand" "=R")
	(float:DF (match_operand:DI 1 "metal_register_operand" "R")))]
  ""
  "%0 = static_cast<double>(%1);"
  [(set_attr "type" "fpu")])

(define_insn "fix_truncsfsi2"
  [(set (match_operand:SI 0 "metal_register_operand" "=R")
	(fix:SI (match_operand:SF 1 "metal_register_operand" "R")))]
  ""
  "%0 = static_cast<int>(%1);"
  [(set_attr "type" "fpu")])

(define_insn "fix_truncdfsi2"
  [(set (match_operand:SI 0 "metal_register_operand" "=R")
	(fix:SI (match_operand:DF 1 "metal_register_operand" "R")))]
  ""
  "%0 = static_cast<int>(%1);"
  [(set_attr "type" "fpu")])

(define_insn "fix_truncsfdi2"
  [(set (match_operand:DI 0 "metal_register_operand" "=R")
	(fix:DI (match_operand:SF 1 "metal_register_operand" "R")))]
  ""
  "%0 = static_cast<long>(%1);"
  [(set_attr "type" "fpu")])

(define_insn "fix_truncdfdi2"
  [(set (match_operand:DI 0 "metal_register_operand" "=R")
	(fix:DI (match_operand:DF 1 "metal_register_operand" "R")))]
  ""
  "%0 = static_cast<long>(%1);"
  [(set_attr "type" "fpu")])

;; Unsigned integer to float conversions

(define_insn "floatunssisf2"
  [(set (match_operand:SF 0 "metal_register_operand" "=R")
	(unsigned_float:SF (match_operand:SI 1 "metal_register_operand" "R")))]
  ""
  "%0 = static_cast<float>(%1);"
  [(set_attr "type" "fpu")])

(define_insn "floatunssidf2"
  [(set (match_operand:DF 0 "metal_register_operand" "=R")
	(unsigned_float:DF (match_operand:SI 1 "metal_register_operand" "R")))]
  ""
  "%0 = static_cast<double>(%1);"
  [(set_attr "type" "fpu")])

(define_insn "floatunsdisf2"
  [(set (match_operand:SF 0 "metal_register_operand" "=R")
	(unsigned_float:SF (match_operand:DI 1 "metal_register_operand" "R")))]
  ""
  "%0 = static_cast<float>(%1);"
  [(set_attr "type" "fpu")])

(define_insn "floatunsdidf2"
  [(set (match_operand:DF 0 "metal_register_operand" "=R")
	(unsigned_float:DF (match_operand:DI 1 "metal_register_operand" "R")))]
  ""
  "%0 = static_cast<double>(%1);"
  [(set_attr "type" "fpu")])

;; Float to unsigned integer conversions

(define_insn "fixuns_truncsfsi2"
  [(set (match_operand:SI 0 "metal_register_operand" "=R")
	(unsigned_fix:SI (match_operand:SF 1 "metal_register_operand" "R")))]
  ""
  "%0 = static_cast<uint>(%1);"
  [(set_attr "type" "fpu")])

(define_insn "fixuns_truncdfsi2"
  [(set (match_operand:SI 0 "metal_register_operand" "=R")
	(unsigned_fix:SI (match_operand:DF 1 "metal_register_operand" "R")))]
  ""
  "%0 = static_cast<uint>(%1);"
  [(set_attr "type" "fpu")])

(define_insn "fixuns_truncsfdi2"
  [(set (match_operand:DI 0 "metal_register_operand" "=R")
	(unsigned_fix:DI (match_operand:SF 1 "metal_register_operand" "R")))]
  ""
  "%0 = static_cast<ulong>(%1);"
  [(set_attr "type" "fpu")])

(define_insn "fixuns_truncdfdi2"
  [(set (match_operand:DI 0 "metal_register_operand" "=R")
	(unsigned_fix:DI (match_operand:DF 1 "metal_register_operand" "R")))]
  ""
  "%0 = static_cast<ulong>(%1);"
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
  "if (%1 %c0 %2) goto %l3;"
  [(set_attr "type" "branch")])
(define_expand "cbranchdi4"
  [(set (pc)
	(if_then_else (match_operator 0 "comparison_operator"
			[(match_operand:DI 1 "metal_register_operand")
			 (match_operand:DI 2 "metal_nonmemory_operand")])
		      (label_ref (match_operand 3 ""))
		      (pc)))]
  ""
  "")

(define_insn "*branch_di"
  [(set (pc)
	(if_then_else (match_operator 0 "comparison_operator"
			[(match_operand:DI 1 "metal_register_operand" "R")
			 (match_operand:DI 2 "metal_nonmemory_operand" "Ri")])
		      (label_ref (match_operand 3 "" ""))
		      (pc)))]
  ""
  "if (%1 %c0 %2) goto %l3;"
  [(set_attr "type" "branch")])

(define_expand "cbranchsf4"
  [(set (pc)
	(if_then_else (match_operator 0 "comparison_operator"
			[(match_operand:SF 1 "metal_register_operand")
			 (match_operand:SF 2 "metal_register_operand")])
		      (label_ref (match_operand 3 ""))
		      (pc)))]
  ""
  "")

(define_insn "*branch_sf"
  [(set (pc)
	(if_then_else (match_operator 0 "comparison_operator"
			[(match_operand:SF 1 "metal_register_operand" "R")
			 (match_operand:SF 2 "metal_register_operand" "R")])
		      (label_ref (match_operand 3 "" ""))
		      (pc)))]
  ""
  "if (%1 %c0 %2) goto %l3;"
  [(set_attr "type" "fpu")])

(define_expand "cbranchdf4"
  [(set (pc)
	(if_then_else (match_operator 0 "comparison_operator"
			[(match_operand:DF 1 "metal_register_operand")
			 (match_operand:DF 2 "metal_register_operand")])
		      (label_ref (match_operand 3 ""))
		      (pc)))]
  ""
  "")

(define_insn "*branch_df"
  [(set (pc)
	(if_then_else (match_operator 0 "comparison_operator"
			[(match_operand:DF 1 "metal_register_operand" "R")
			 (match_operand:DF 2 "metal_register_operand" "R")])
		      (label_ref (match_operand 3 "" ""))
		      (pc)))]
  ""
  "if (%1 %c0 %2) goto %l3;"
  [(set_attr "type" "fpu")])

(define_expand "cbranchhf4"
  [(set (pc)
	(if_then_else (match_operator 0 "comparison_operator"
			[(match_operand:HF 1 "metal_register_operand")
			 (match_operand:HF 2 "metal_register_operand")])
		      (label_ref (match_operand 3 ""))
		      (pc)))]
  ""
  "")

(define_insn "*branch_hf"
  [(set (pc)
	(if_then_else (match_operator 0 "comparison_operator"
			[(match_operand:HF 1 "metal_register_operand" "R")
			 (match_operand:HF 2 "metal_register_operand" "R")])
		      (label_ref (match_operand 3 "" ""))
		      (pc)))]
  ""
  "if (%1 %c0 %2) goto %l3;"
  [(set_attr "type" "fpu")])


;; Conditional store patterns (cstore)
;; These patterns set a register to 0 or 1 based on a comparison result

(define_expand "cstoresi4"
  [(set (match_operand:SI 0 "metal_register_operand")
	(match_operator:SI 1 "comparison_operator"
	  [(match_operand:SI 2 "metal_register_operand")
	   (match_operand:SI 3 "metal_nonmemory_operand")]))]
  ""
  "")

(define_insn "*cstoresi4_insn"
  [(set (match_operand:SI 0 "metal_register_operand" "=R")
	(match_operator:SI 1 "comparison_operator"
	  [(match_operand:SI 2 "metal_register_operand" "R")
	   (match_operand:SI 3 "metal_nonmemory_operand" "Ri")]))]
  ""
  "%0 = (%2 %c1 %3);"
  [(set_attr "type" "alu")])

(define_expand "cstoredi4"
  [(set (match_operand:SI 0 "metal_register_operand")
	(match_operator:SI 1 "comparison_operator"
	  [(match_operand:DI 2 "metal_register_operand")
	   (match_operand:DI 3 "metal_nonmemory_operand")]))]
  ""
  "")

(define_insn "*cstoredi4_insn"
  [(set (match_operand:SI 0 "metal_register_operand" "=R")
	(match_operator:SI 1 "comparison_operator"
	  [(match_operand:DI 2 "metal_register_operand" "R")
	   (match_operand:DI 3 "metal_nonmemory_operand" "Ri")]))]
  ""
  "%0 = (%2 %c1 %3);"
  [(set_attr "type" "alu")])

(define_expand "cstoresf4"
  [(set (match_operand:SI 0 "metal_register_operand")
	(match_operator:SI 1 "comparison_operator"
	  [(match_operand:SF 2 "metal_register_operand")
	   (match_operand:SF 3 "metal_register_operand")]))]
  ""
  "")

(define_insn "*cstoresf4_insn"
  [(set (match_operand:SI 0 "metal_register_operand" "=R")
	(match_operator:SI 1 "comparison_operator"
	  [(match_operand:SF 2 "metal_register_operand" "R")
	   (match_operand:SF 3 "metal_register_operand" "R")]))]
  ""
  "%0 = (%2 %c1 %3);"
  [(set_attr "type" "fpu")])

(define_expand "cstoredf4"
  [(set (match_operand:SI 0 "metal_register_operand")
	(match_operator:SI 1 "comparison_operator"
	  [(match_operand:DF 2 "metal_register_operand")
	   (match_operand:DF 3 "metal_register_operand")]))]
  ""
  "")

(define_insn "*cstoredf4_insn"
  [(set (match_operand:SI 0 "metal_register_operand" "=R")
	(match_operator:SI 1 "comparison_operator"
	  [(match_operand:DF 2 "metal_register_operand" "R")
	   (match_operand:DF 3 "metal_register_operand" "R")]))]
  ""
  "%0 = (%2 %c1 %3);"
  [(set_attr "type" "fpu")])

(define_expand "cstorehf4"
  [(set (match_operand:SI 0 "metal_register_operand")
	(match_operator:SI 1 "comparison_operator"
	  [(match_operand:HF 2 "metal_register_operand")
	   (match_operand:HF 3 "metal_register_operand")]))]
  ""
  "")

(define_insn "*cstorehf4_insn"
  [(set (match_operand:SI 0 "metal_register_operand" "=R")
	(match_operator:SI 1 "comparison_operator"
	  [(match_operand:HF 2 "metal_register_operand" "R")
	   (match_operand:HF 3 "metal_register_operand" "R")]))]
  ""
  "%0 = (%2 %c1 %3);"
  [(set_attr "type" "fpu")])

(define_expand "cbranchdi4"
  [(set (pc)
	(if_then_else (match_operator 0 "comparison_operator"
			[(match_operand:DI 1 "metal_register_operand")
			 (match_operand:DI 2 "metal_nonmemory_operand")])
		      (label_ref (match_operand 3 ""))
		      (pc)))]
  ""
  "")

(define_insn "*branch_di"
  [(set (pc)
	(if_then_else (match_operator 0 "comparison_operator"
			[(match_operand:DI 1 "metal_register_operand" "R")
			 (match_operand:DI 2 "metal_nonmemory_operand" "Ri")])
		      (label_ref (match_operand 3 "" ""))
		      (pc)))]
  ""
  "if (%1 %c0 %2) goto %l3;"
  [(set_attr "type" "branch")])

(define_expand "cbranchsf4"
  [(set (pc)
	(if_then_else (match_operator 0 "comparison_operator"
			[(match_operand:SF 1 "metal_register_operand")
			 (match_operand:SF 2 "metal_register_operand")])
		      (label_ref (match_operand 3 ""))
		      (pc)))]
  ""
  "")

(define_insn "*branch_sf"
  [(set (pc)
	(if_then_else (match_operator 0 "comparison_operator"
			[(match_operand:SF 1 "metal_register_operand" "R")
			 (match_operand:SF 2 "metal_register_operand" "R")])
		      (label_ref (match_operand 3 "" ""))
		      (pc)))]
  ""
  "if (%1 %c0 %2) goto %l3;"
  [(set_attr "type" "fpu")])

(define_expand "cbranchdf4"
  [(set (pc)
	(if_then_else (match_operator 0 "comparison_operator"
			[(match_operand:DF 1 "metal_register_operand")
			 (match_operand:DF 2 "metal_register_operand")])
		      (label_ref (match_operand 3 ""))
		      (pc)))]
  ""
  "")

(define_insn "*branch_df"
  [(set (pc)
	(if_then_else (match_operator 0 "comparison_operator"
			[(match_operand:DF 1 "metal_register_operand" "R")
			 (match_operand:DF 2 "metal_register_operand" "R")])
		      (label_ref (match_operand 3 "" ""))
		      (pc)))]
  ""
  "if (%1 %c0 %2) goto %l3;"
  [(set_attr "type" "fpu")])

(define_expand "cbranchhf4"
  [(set (pc)
	(if_then_else (match_operator 0 "comparison_operator"
			[(match_operand:HF 1 "metal_register_operand")
			 (match_operand:HF 2 "metal_register_operand")])
		      (label_ref (match_operand 3 ""))
		      (pc)))]
  ""
  "")

(define_insn "*branch_hf"
  [(set (pc)
	(if_then_else (match_operator 0 "comparison_operator"
			[(match_operand:HF 1 "metal_register_operand" "R")
			 (match_operand:HF 2 "metal_register_operand" "R")])
		      (label_ref (match_operand 3 "" ""))
		      (pc)))]
  ""
  "if (%1 %c0 %2) goto %l3;"
  [(set_attr "type" "fpu")])

;; Conditional select operations

;; Predicate to integer conversion
(define_insn "zero_extendbisi2"
  [(set (match_operand:SI 0 "metal_register_operand" "=R")
	(zero_extend:SI
	 (match_operand:BI 1 "metal_register_operand" "R")))]
  ""
  "%0 = %1 ? 1 : 0;"
  [(set_attr "type" "alu")])

(define_insn "zero_extendbidi2"
  [(set (match_operand:DI 0 "metal_register_operand" "=R")
	(zero_extend:DI
	 (match_operand:BI 1 "metal_register_operand" "R")))]
  ""
  "%0 = %1 ? 1 : 0;"
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
  "%0 = %1 ? %2 : %3;"
  [(set_attr "type" "alu")])

(define_insn "*sel_truedi"
  [(set (match_operand:DI 0 "metal_register_operand" "=R")
	(if_then_else:DI
	  (ne (match_operand:BI 1 "metal_register_operand" "R")
	      (const_int 0))
	  (match_operand:DI 2 "metal_nonmemory_operand" "Ri")
	  (match_operand:DI 3 "metal_nonmemory_operand" "Ri")))]
  ""
  "%0 = %1 ? %2 : %3;"
  [(set_attr "type" "alu")])

(define_insn "*sel_truesf"
  [(set (match_operand:SF 0 "metal_register_operand" "=R")
	(if_then_else:SF
	  (ne (match_operand:BI 1 "metal_register_operand" "R")
	      (const_int 0))
	  (match_operand:SF 2 "metal_register_operand" "R")
	  (match_operand:SF 3 "metal_register_operand" "R")))]
  ""
  "%0 = %1 ? %2 : %3;"
  [(set_attr "type" "fpu")])

(define_insn "*sel_truedf"
  [(set (match_operand:DF 0 "metal_register_operand" "=R")
	(if_then_else:DF
	  (ne (match_operand:BI 1 "metal_register_operand" "R")
	      (const_int 0))
	  (match_operand:DF 2 "metal_register_operand" "R")
	  (match_operand:DF 3 "metal_register_operand" "R")))]
  ""
  "%0 = %1 ? %2 : %3;"
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
  "%0 = %1 ? %3 : %2;"
  [(set_attr "type" "alu")])

(define_insn "*sel_falsedi"
  [(set (match_operand:DI 0 "metal_register_operand" "=R")
	(if_then_else:DI
	  (eq (match_operand:BI 1 "metal_register_operand" "R")
	      (const_int 0))
	  (match_operand:DI 2 "metal_nonmemory_operand" "Ri")
	  (match_operand:DI 3 "metal_nonmemory_operand" "Ri")))]
  ""
  "%0 = %1 ? %3 : %2;"
  [(set_attr "type" "alu")])

(define_insn "*sel_falsesf"
  [(set (match_operand:SF 0 "metal_register_operand" "=R")
	(if_then_else:SF
	  (eq (match_operand:BI 1 "metal_register_operand" "R")
	      (const_int 0))
	  (match_operand:SF 2 "metal_register_operand" "R")
	  (match_operand:SF 3 "metal_register_operand" "R")))]
  ""
  "%0 = %1 ? %3 : %2;"
  [(set_attr "type" "fpu")])

(define_insn "*sel_falsedf"
  [(set (match_operand:DF 0 "metal_register_operand" "=R")
	(if_then_else:DF
	  (eq (match_operand:BI 1 "metal_register_operand" "R")
	      (const_int 0))
	  (match_operand:DF 2 "metal_register_operand" "R")
	  (match_operand:DF 3 "metal_register_operand" "R")))]
  ""
  "%0 = %1 ? %3 : %2;"
  [(set_attr "type" "fpu")])

;; Jump

(define_insn "jump"
  [(set (pc)
	(label_ref (match_operand 0 "" "")))]
  ""
  "goto %l0;"
  [(set_attr "type" "branch")])

;; Indirect jump

(define_insn "indirect_jump"
  [(set (pc)
	(match_operand:DI 0 "metal_register_operand" "R"))]
  ""
  "goto *%0;"
  [(set_attr "type" "branch")])

;; Return

(define_insn "return"
  [(return)]
  ""
  "return;"
  [(set_attr "type" "branch")])

;; No-op

(define_insn "nop"
  [(const_int 0)]
  ""
  ";"
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

;; Stack management
;; MSL has implicit stack management, but we provide these patterns
;; for GCC's internal use (e.g., alloca, VLA support).

(define_expand "allocate_stack"
  [(set (match_operand:DI 0 "metal_register_operand")
        (minus:DI (reg:DI 1)
                  (match_operand:DI 1 "metal_register_operand")))]
  ""
{
  /* Adjust stack pointer downward by operands[1] bytes.
     Store new stack pointer in operands[0].  */
  rtx stack_ptr = gen_rtx_REG (DImode, STACK_POINTER_REGNUM);
  rtx size = operands[1];

  /* Align size to 16 bytes for Metal ABI.  */
  rtx aligned_size = expand_simple_binop (DImode, PLUS, size,
                                          GEN_INT (15), NULL_RTX,
                                          1, OPTAB_LIB_WIDEN);
  aligned_size = expand_simple_binop (DImode, AND, aligned_size,
                                      GEN_INT (-16), NULL_RTX,
                                      1, OPTAB_LIB_WIDEN);

  /* Subtract from stack pointer.  */
  rtx new_sp = expand_simple_binop (DImode, MINUS, stack_ptr,
                                    aligned_size, NULL_RTX,
                                    1, OPTAB_LIB_WIDEN);
  emit_move_insn (stack_ptr, new_sp);
  emit_move_insn (operands[0], stack_ptr);
  DONE;
})

(define_expand "save_stack_block"
  [(set (match_operand:DI 0 "metal_register_operand")
        (reg:DI 1))]
  ""
{
  /* Save current stack pointer to operands[0].  */
  emit_move_insn (operands[0], gen_rtx_REG (DImode, STACK_POINTER_REGNUM));
  DONE;
})

(define_expand "restore_stack_block"
  [(set (reg:DI 1)
        (match_operand:DI 1 "metal_register_operand"))]
  ""
{
  /* Restore stack pointer from operands[1].  */
  emit_move_insn (gen_rtx_REG (DImode, STACK_POINTER_REGNUM), operands[1]);
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

;; QI (8-bit) <-> HI (16-bit)

(define_insn "extendqihi2"
  [(set (match_operand:HI 0 "metal_register_operand" "=R")
	(sign_extend:HI (match_operand:QI 1 "metal_register_operand" "R")))]
  ""
  "%0 = static_cast<short>(%1);"
  [(set_attr "type" "alu")])

(define_insn "zero_extendqihi2"
  [(set (match_operand:HI 0 "metal_register_operand" "=R")
	(zero_extend:HI (match_operand:QI 1 "metal_register_operand" "R")))]
  ""
  "%0 = static_cast<ushort>(%1);"
  [(set_attr "type" "alu")])

(define_insn "trunchiqi2"
  [(set (match_operand:QI 0 "metal_register_operand" "=R")
	(truncate:QI (match_operand:HI 1 "metal_register_operand" "R")))]
  ""
  "%0 = static_cast<uchar>(%1);"
  [(set_attr "type" "alu")])

;; QI (8-bit) -> SI (32-bit)

(define_insn "extendqisi2"
  [(set (match_operand:SI 0 "metal_register_operand" "=R")
	(sign_extend:SI (match_operand:QI 1 "metal_register_operand" "R")))]
  ""
  "%0 = static_cast<int>(%1);"
  [(set_attr "type" "alu")])

(define_insn "zero_extendqisi2"
  [(set (match_operand:SI 0 "metal_register_operand" "=R")
	(zero_extend:SI (match_operand:QI 1 "metal_register_operand" "R")))]
  ""
  "%0 = static_cast<uint>(%1);"
  [(set_attr "type" "alu")])

;; HI (16-bit) -> SI (32-bit)

(define_insn "extendhisi2"
  [(set (match_operand:SI 0 "metal_register_operand" "=R")
	(sign_extend:SI (match_operand:HI 1 "metal_register_operand" "R")))]
  ""
  "%0 = static_cast<int>(%1);"
  [(set_attr "type" "alu")])

(define_insn "zero_extendhisi2"
  [(set (match_operand:SI 0 "metal_register_operand" "=R")
	(zero_extend:SI (match_operand:HI 1 "metal_register_operand" "R")))]
  ""
  "%0 = static_cast<uint>(%1);"
  [(set_attr "type" "alu")])

;; SI (32-bit) -> DI (64-bit)

(define_insn "zero_extendsidi2"
  [(set (match_operand:DI 0 "metal_register_operand" "=R")
	(zero_extend:DI (match_operand:SI 1 "metal_register_operand" "R")))]
  ""
  "%0 = static_cast<ulong>(%1);"
  [(set_attr "type" "alu")])

(define_insn "extendsidi2"
  [(set (match_operand:DI 0 "metal_register_operand" "=R")
	(sign_extend:DI (match_operand:SI 1 "metal_register_operand" "R")))]
  ""
  "%0 = static_cast<long>(%1);"
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
  "%0 = atomic_compare_exchange_weak_explicit((device atomic_int*)%1, &%2, %3, memory_order_relaxed, memory_order_relaxed);"
  [(set_attr "type" "alu")])

(define_insn "atomic_compare_and_swapdi"
  [(set (match_operand:DI 0 "metal_register_operand" "=R")
	(unspec_volatile:DI
	  [(match_operand:DI 1 "memory_operand" "m")
	   (match_operand:DI 2 "metal_register_operand" "R")
	   (match_operand:DI 3 "metal_register_operand" "R")]
	  UNSPEC_ATOMIC_CAS))]
  ""
  "%0 = atomic_compare_exchange_weak_explicit((device atomic_long*)%1, &%2, %3, memory_order_relaxed, memory_order_relaxed);"
  [(set_attr "type" "alu")])

(define_insn "atomic_exchangesi"
  [(set (match_operand:SI 0 "metal_register_operand" "=R")
	(unspec_volatile:SI
	  [(match_operand:SI 1 "memory_operand" "m")
	   (match_operand:SI 2 "metal_register_operand" "R")]
	  UNSPEC_ATOMIC_EXCH))]
  ""
  "%0 = atomic_exchange_explicit((device atomic_int*)%1, %2, memory_order_relaxed);"
  [(set_attr "type" "alu")])

(define_insn "atomic_exchangedi"
  [(set (match_operand:DI 0 "metal_register_operand" "=R")
	(unspec_volatile:DI
	  [(match_operand:DI 1 "memory_operand" "m")
	   (match_operand:DI 2 "metal_register_operand" "R")]
	  UNSPEC_ATOMIC_EXCH))]
  ""
  "%0 = atomic_exchange_explicit((device atomic_long*)%1, %2, memory_order_relaxed);"
  [(set_attr "type" "alu")])

(define_insn "atomic_fetch_addsi"
  [(set (match_operand:SI 0 "metal_register_operand" "=R")
	(unspec_volatile:SI
	  [(match_operand:SI 1 "memory_operand" "m")
	   (match_operand:SI 2 "metal_register_operand" "R")]
	  UNSPEC_ATOMIC_ADD))]
  ""
  "%0 = atomic_fetch_add_explicit((device atomic_int*)%1, %2, memory_order_relaxed);"
  [(set_attr "type" "alu")])

(define_insn "atomic_fetch_adddi"
  [(set (match_operand:DI 0 "metal_register_operand" "=R")
	(unspec_volatile:DI
	  [(match_operand:DI 1 "memory_operand" "m")
	   (match_operand:DI 2 "metal_register_operand" "R")]
	  UNSPEC_ATOMIC_ADD))]
  ""
  "%0 = atomic_fetch_add_explicit((device atomic_long*)%1, %2, memory_order_relaxed);"
  [(set_attr "type" "alu")])

(define_insn "atomic_fetch_andsi"
  [(set (match_operand:SI 0 "metal_register_operand" "=R")
	(unspec_volatile:SI
	  [(match_operand:SI 1 "memory_operand" "m")
	   (match_operand:SI 2 "metal_register_operand" "R")]
	  UNSPEC_ATOMIC_AND))]
  ""
  "%0 = atomic_fetch_and_explicit((device atomic_int*)%1, %2, memory_order_relaxed);"
  [(set_attr "type" "alu")])

(define_insn "atomic_fetch_anddi"
  [(set (match_operand:DI 0 "metal_register_operand" "=R")
	(unspec_volatile:DI
	  [(match_operand:DI 1 "memory_operand" "m")
	   (match_operand:DI 2 "metal_register_operand" "R")]
	  UNSPEC_ATOMIC_AND))]
  ""
  "%0 = atomic_fetch_and_explicit((device atomic_long*)%1, %2, memory_order_relaxed);"
  [(set_attr "type" "alu")])

(define_insn "atomic_fetch_orsi"
  [(set (match_operand:SI 0 "metal_register_operand" "=R")
	(unspec_volatile:SI
	  [(match_operand:SI 1 "memory_operand" "m")
	   (match_operand:SI 2 "metal_register_operand" "R")]
	  UNSPEC_ATOMIC_OR))]
  ""
  "%0 = atomic_fetch_or_explicit((device atomic_int*)%1, %2, memory_order_relaxed);"
  [(set_attr "type" "alu")])

(define_insn "atomic_fetch_ordi"
  [(set (match_operand:DI 0 "metal_register_operand" "=R")
	(unspec_volatile:DI
	  [(match_operand:DI 1 "memory_operand" "m")
	   (match_operand:DI 2 "metal_register_operand" "R")]
	  UNSPEC_ATOMIC_OR))]
  ""
  "%0 = atomic_fetch_or_explicit((device atomic_long*)%1, %2, memory_order_relaxed);"
  [(set_attr "type" "alu")])
