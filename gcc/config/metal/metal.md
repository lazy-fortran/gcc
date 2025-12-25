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
