/* Target Definitions for Apple Metal.
   Copyright (C) 2025 Free Software Foundation, Inc.

   This file is part of GCC.

   GCC is free software; you can redistribute it and/or modify it
   under the terms of the GNU General Public License as published
   by the Free Software Foundation; either version 3, or (at your
   option) any later version.

   GCC is distributed in the hope that it will be useful, but WITHOUT
   ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
   or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public
   License for more details.

   You should have received a copy of the GNU General Public License
   along with GCC; see the file COPYING3.  If not see
   <http://www.gnu.org/licenses/>.  */

#ifndef GCC_METAL_H
#define GCC_METAL_H

/* Run-time Target.  */

#define TARGET_CPU_CPP_BUILTINS() metal_cpu_cpp_builtins ()

/* Storage Layout.  */

#define BITS_BIG_ENDIAN 0
#define BYTES_BIG_ENDIAN 0
#define WORDS_BIG_ENDIAN 0

#define UNITS_PER_WORD 8

/* Alignments in bits.  */
#define PARM_BOUNDARY 32
#define STACK_BOUNDARY 128
#define FUNCTION_BOUNDARY 32
#define BIGGEST_ALIGNMENT 128
#define STRICT_ALIGNMENT 1

#define MAX_STACK_ALIGNMENT (1024 * 8)
#define MAX_OFILE_ALIGNMENT (32768 * 8)

/* Type Layout.  */

#define DEFAULT_SIGNED_CHAR 1

#define SHORT_TYPE_SIZE 16
#define INT_TYPE_SIZE 32
#define LONG_TYPE_SIZE 64
#define LONG_LONG_TYPE_SIZE 64
#define TARGET_SUPPORTS_WIDE_INT 1

#define MAX_FIXED_MODE_SIZE 128

#undef SIZE_TYPE
#define SIZE_TYPE "long unsigned int"
#undef PTRDIFF_TYPE
#define PTRDIFF_TYPE "long int"

#define POINTER_SIZE 64
#define Pmode DImode
#define STACK_SIZE_MODE Pmode

/* Registers.  Metal is a virtual target - define minimal registers.  */
#define FIRST_PSEUDO_REGISTER 16
#define FIXED_REGISTERS	    { 1, 1, 1, 1, 1, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0 }
#define CALL_USED_REGISTERS { 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1 }

/* Register Classes.  */
enum reg_class             {  NO_REGS,    ALL_REGS,	LIM_REG_CLASSES };
#define REG_CLASS_NAMES    { "NO_REGS",  "ALL_REGS" }
#define REG_CLASS_CONTENTS { { 0x0000 }, { 0xFFFF } }
#define N_REG_CLASSES (int) LIM_REG_CLASSES

#define GENERAL_REGS ALL_REGS
#define REGNO_REG_CLASS(R) ((void)(R), ALL_REGS)
#define BASE_REG_CLASS ALL_REGS
#define INDEX_REG_CLASS NO_REGS

#define REGNO_OK_FOR_BASE_P(X) true
#define REGNO_OK_FOR_INDEX_P(X) false

#define CLASS_MAX_NREGS(class, mode) \
  ((GET_MODE_SIZE (mode) + UNITS_PER_WORD - 1) / UNITS_PER_WORD)

#define PROMOTE_MODE(MODE, UNSIGNEDP, TYPE)		\
  if ((MODE) == QImode || (MODE) == HImode)		\
    {							\
      (MODE) = SImode;					\
      (void)(UNSIGNEDP);				\
      (void)(TYPE);					\
    }

/* Stack and Calling.  */

#define FRAME_GROWS_DOWNWARD 0
#define STACK_GROWS_DOWNWARD 1

#define METAL_RETURN_REGNUM 0
#define STACK_POINTER_REGNUM 1
#define FRAME_POINTER_REGNUM 2
#define HARD_FRAME_POINTER_REGNUM FRAME_POINTER_REGNUM
#define ARG_POINTER_REGNUM 3
#define STATIC_CHAIN_REGNUM 4

/* Register elimination.  */
#define ELIMINABLE_REGS							\
  { { ARG_POINTER_REGNUM, STACK_POINTER_REGNUM },			\
    { ARG_POINTER_REGNUM, FRAME_POINTER_REGNUM },			\
    { FRAME_POINTER_REGNUM, STACK_POINTER_REGNUM } }

#define INITIAL_ELIMINATION_OFFSET(FROM, TO, OFFSET) \
  ((OFFSET) = 0)

#define REGISTER_NAMES							\
  {									\
    "%value", "%stack", "%frame", "%args",				\
    "%chain", "%hr5", "%hr6", "%hr7",					\
    "%hr8", "%hr9", "%hr10", "%hr11", "%hr12", "%hr13", "%hr14", "%hr15" \
  }

#define FIRST_PARM_OFFSET(FNDECL) ((void)(FNDECL), 0)
#define PUSH_ARGS_REVERSED 1
#define ACCUMULATE_OUTGOING_ARGS 1

#define FRAME_POINTER_CFA_OFFSET(FNDECL) ((void)(FNDECL), 0)

#ifdef HOST_WIDE_INT
struct metal_args {
  tree fntype;
  int count;
};
#endif

#define CUMULATIVE_ARGS struct metal_args

#define INIT_CUMULATIVE_ARGS(CUM, FNTYPE, LIBNAME, FNDECL, N_NAMED_ARGS) \
  ((CUM).fntype = (FNTYPE), (CUM).count = 0, (void)0)

#define FUNCTION_ARG_REGNO_P(r) 0

#define DEFAULT_PCC_STRUCT_RETURN 0

#define FUNCTION_PROFILER(file, labelno) \
  fatal_error (input_location, \
	       "profiling is not yet implemented for Metal target")

#define TRAMPOLINE_SIZE 32
#define TRAMPOLINE_ALIGNMENT 256

/* Addressing Modes.  */

#define MAX_REGS_PER_ADDRESS 1
#define LEGITIMATE_PIC_OPERAND_P(X) 1

/* Assembler Format.  */

#define ASM_COMMENT_START "//"
#define ASM_APP_ON "/* APP */\n"
#define ASM_APP_OFF "/* END APP */\n"

#define ASM_OUTPUT_ALIGN(FILE, POWER) \
  fprintf (FILE, "\t// .align %d\n", 1 << (POWER))

#define PRINT_OPERAND(FILE, X, CODE) metal_print_operand (FILE, X, CODE)
#define PRINT_OPERAND_ADDRESS(FILE, ADDR) \
  metal_print_operand_address (FILE, ADDR)

#define ASM_OUTPUT_LABELREF(FILE, NAME) \
  fputs (NAME, FILE)

#define GLOBAL_ASM_OP "// .global "

#define DWARF2_ASM_LINE_DEBUG_INFO 0
#define DWARF2_DEBUGGING_INFO 0
#define PREFERRED_DEBUGGING_TYPE NO_DEBUG

/* Misc.  */

#define NO_DOT_IN_LABEL
#define ASM_OUTPUT_DEF(FILE, ALIAS, NAME) do { } while (0)

#define CASE_VECTOR_MODE SImode
#define FUNCTION_MODE QImode
#define MOVE_MAX 8
#define TRULY_NOOP_TRUNCATION(op, ip) 1
#define CLZ_DEFINED_VALUE_AT_ZERO(MODE, VALUE) ((VALUE) = 0, 0)
#define CTZ_DEFINED_VALUE_AT_ZERO(MODE, VALUE) ((VALUE) = 0, 0)

#define HAS_INIT_SECTION 1

#define SUPPORTS_WEAK 0

/* Metal generates MSL source code, not object code.  */
#define OBJECT_FORMAT_OTHER

#endif /* GCC_METAL_H */
