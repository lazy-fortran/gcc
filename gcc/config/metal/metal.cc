/* Target code for Apple Metal.
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

#define IN_TARGET_CODE 1

#include "config.h"
#include "system.h"
#include "coretypes.h"
#include "backend.h"
#include "target.h"
#include "rtl.h"
#include "tree.h"
#include "memmodel.h"
#include "tm_p.h"
#include "optabs.h"
#include "emit-rtl.h"
#include "recog.h"
#include "diagnostic.h"
#include "output.h"
#include "insn-attr.h"
#include "flags.h"
#include "explow.h"
#include "calls.h"
#include "varasm.h"
#include "expr.h"
#include "stor-layout.h"
#include "builtins.h"

/* This file should be included last.  */
#include "target-def.h"

/* Implement TARGET_CPU_CPP_BUILTINS.  */

void
metal_cpu_cpp_builtins (void)
{
  cpp_define (parse_in, "__metal__");
  cpp_define (parse_in, "__METAL__");
}

/* Implement TARGET_OPTION_OVERRIDE.  */

static void
metal_option_override (void)
{
  /* Metal generates MSL source, not object code.  */
  flag_no_common = 1;
}

/* Implement TARGET_INIT_BUILTINS.  */

static void
metal_init_builtins (void)
{
  /* Placeholder for Metal-specific builtins.  */
}

/* Implement TARGET_FUNCTION_ARG.  */

static rtx
metal_function_arg (cumulative_args_t, const function_arg_info &)
{
  /* All arguments passed in memory for now.  */
  return NULL_RTX;
}

/* Implement TARGET_FUNCTION_ARG_ADVANCE.  */

static void
metal_function_arg_advance (cumulative_args_t cum_v,
			    const function_arg_info &)
{
  CUMULATIVE_ARGS *cum = get_cumulative_args (cum_v);
  cum->count++;
}

/* Implement TARGET_FUNCTION_VALUE.  */

static rtx
metal_function_value (const_tree type, const_tree, bool)
{
  machine_mode mode = TYPE_MODE (type);
  return gen_rtx_REG (mode, METAL_RETURN_REGNUM);
}

/* Implement TARGET_LIBCALL_VALUE.  */

static rtx
metal_libcall_value (machine_mode mode, const_rtx)
{
  return gen_rtx_REG (mode, METAL_RETURN_REGNUM);
}

/* Implement TARGET_FUNCTION_VALUE_REGNO_P.  */

static bool
metal_function_value_regno_p (const unsigned int regno)
{
  return regno == METAL_RETURN_REGNUM;
}

/* Implement TARGET_STATIC_CHAIN.  */

static rtx
metal_static_chain (const_tree, bool)
{
  return gen_rtx_REG (Pmode, STATIC_CHAIN_REGNUM);
}

/* Implement TARGET_LEGITIMATE_ADDRESS_P.  */

static bool
metal_legitimate_address_p (machine_mode, rtx, bool, code_helper)
{
  /* For now, accept all addresses.  */
  return true;
}

/* Return MSL type string for machine mode.  */

static const char *
metal_msl_type_from_mode (machine_mode mode)
{
  switch (mode)
    {
    case E_QImode: return "char";
    case E_HImode: return "short";
    case E_SImode: return "int";
    case E_DImode: return "long";
    case E_HFmode: return "half";
    case E_SFmode: return "float";
    case E_DFmode: return "double";
    case E_V2SImode: return "int2";
    case E_V4SImode: return "int4";
    case E_V2SFmode: return "float2";
    case E_V4SFmode: return "float4";
    default:
      gcc_unreachable ();
    }
}

/* Implement PRINT_OPERAND.  */

void
metal_print_operand (FILE *file, rtx x, int code)
{
  switch (code)
    {
    case 0:
      /* Default: print operand value.  */
      if (REG_P (x))
	fprintf (file, "r%d", REGNO (x));
      else if (CONST_INT_P (x))
	fprintf (file, HOST_WIDE_INT_PRINT_DEC, INTVAL (x));
      else if (CONST_DOUBLE_P (x))
	{
	  long val[2];
	  if (GET_MODE (x) == SFmode)
	    {
	      REAL_VALUE_TO_TARGET_SINGLE (*CONST_DOUBLE_REAL_VALUE (x),
					   val[0]);
	      fprintf (file, "0x%08lx", val[0]);
	    }
	  else if (GET_MODE (x) == DFmode)
	    {
	      REAL_VALUE_TO_TARGET_DOUBLE (*CONST_DOUBLE_REAL_VALUE (x), val);
	      fprintf (file, "0x%08lx%08lx", val[1], val[0]);
	    }
	}
      else
	output_addr_const (file, x);
      break;

    case 't':
      /* Print type suffix for mode.  */
      fprintf (file, "%s", metal_msl_type_from_mode (GET_MODE (x)));
      break;

    case 'T':
      /* Print type for explicit mode in operand.  */
      {
	machine_mode mode = GET_MODE (x);
	fprintf (file, "%s", metal_msl_type_from_mode (mode));
      }
      break;

    default:
      output_operand_lossage ("invalid %%xn code");
      break;
    }
}

/* Implement PRINT_OPERAND_ADDRESS.  */

void
metal_print_operand_address (FILE *file, rtx addr)
{
  if (REG_P (addr))
    fprintf (file, "r%d", REGNO (addr));
  else if (GET_CODE (addr) == PLUS)
    {
      rtx base = XEXP (addr, 0);
      rtx offset = XEXP (addr, 1);
      if (REG_P (base) && CONST_INT_P (offset))
	{
	  fprintf (file, "r%d", REGNO (base));
	  HOST_WIDE_INT off = INTVAL (offset);
	  if (off != 0)
	    fprintf (file, " + " HOST_WIDE_INT_PRINT_DEC, off);
	}
      else
	output_addr_const (file, addr);
    }
  else
    output_addr_const (file, addr);
}

/* Output MSL code for move instruction.  */

const char *
metal_output_mov_insn (rtx dst, rtx src)
{
  machine_mode mode = GET_MODE (dst);
  static char buf[128];

  /* Generate MSL variable declaration and assignment.  */
  snprintf (buf, sizeof (buf), "%s %%0 = %%1",
	    metal_msl_type_from_mode (mode));
  return buf;
}

/* Output MSL code for binary arithmetic operation.  */

const char *
metal_output_binop_insn (const char *msl_op, rtx *operands)
{
  machine_mode mode = GET_MODE (operands[0]);
  static char buf[128];

  snprintf (buf, sizeof (buf), "%s %%0 = %%1 %s %%2",
	    metal_msl_type_from_mode (mode), msl_op);
  return buf;
}

/* Output MSL code for unary operation.  */

const char *
metal_output_unop_insn (const char *msl_op, rtx *operands)
{
  machine_mode mode = GET_MODE (operands[0]);
  static char buf[128];

  snprintf (buf, sizeof (buf), "%s %%0 = %s%%1",
	    metal_msl_type_from_mode (mode), msl_op);
  return buf;
}

/* Output MSL code for comparison operation.  */

const char *
metal_output_cmp_insn (const char *msl_op, rtx *operands)
{
  static char buf[128];

  snprintf (buf, sizeof (buf), "bool %%0 = (%%1 %s %%2)", msl_op);
  return buf;
}

/* Output MSL code for load operation.  */

const char *
metal_output_load_insn (rtx dst, rtx src)
{
  machine_mode mode = GET_MODE (dst);
  static char buf[128];

  snprintf (buf, sizeof (buf), "%s %%0 = *(%s*)%%1",
	    metal_msl_type_from_mode (mode),
	    metal_msl_type_from_mode (mode));
  return buf;
}

/* Output MSL code for store operation.  */

const char *
metal_output_store_insn (rtx dst, rtx src)
{
  machine_mode mode = GET_MODE (src);
  static char buf[128];

  snprintf (buf, sizeof (buf), "*(%s*)%%0 = %%1",
	    metal_msl_type_from_mode (mode));
  return buf;
}

/* Implement TARGET_ASM_FILE_START.  */

static void
metal_file_start (void)
{
  fputs ("// Metal Shading Language output\n", asm_out_file);
  fputs ("// Generated by GCC\n\n", asm_out_file);
  fputs ("#include <metal_stdlib>\n", asm_out_file);
  fputs ("using namespace metal;\n\n", asm_out_file);
}

/* Implement TARGET_ASM_FILE_END.  */

static void
metal_file_end (void)
{
  fputs ("\n// End of Metal output\n", asm_out_file);
}

/* Implement TARGET_PASS_BY_REFERENCE.  */

static bool
metal_pass_by_reference (cumulative_args_t, const function_arg_info &arg)
{
  /* Large types passed by reference.  */
  return (arg.type
	  && (TREE_CODE (TYPE_SIZE (arg.type)) != INTEGER_CST
	      || TREE_INT_CST_LOW (TYPE_SIZE (arg.type)) > 128));
}

/* Implement TARGET_RETURN_IN_MEMORY.  */

static bool
metal_return_in_memory (const_tree, const_tree)
{
  /* All values returned in registers for now.  */
  return false;
}

/* Implement TARGET_MODES_TIEABLE_P.  */

static bool
metal_modes_tieable_p (machine_mode, machine_mode)
{
  /* All modes are tieable.  */
  return true;
}

/* Implement TARGET_LEGITIMATE_CONSTANT_P.  */

static bool
metal_legitimate_constant_p (machine_mode, rtx)
{
  /* All constants are legitimate.  */
  return true;
}

/* Initialize the GCC target structure.  */

#undef TARGET_OPTION_OVERRIDE
#define TARGET_OPTION_OVERRIDE metal_option_override

#undef TARGET_INIT_BUILTINS
#define TARGET_INIT_BUILTINS metal_init_builtins

#undef TARGET_FUNCTION_ARG
#define TARGET_FUNCTION_ARG metal_function_arg

#undef TARGET_FUNCTION_ARG_ADVANCE
#define TARGET_FUNCTION_ARG_ADVANCE metal_function_arg_advance

#undef TARGET_FUNCTION_VALUE
#define TARGET_FUNCTION_VALUE metal_function_value

#undef TARGET_LIBCALL_VALUE
#define TARGET_LIBCALL_VALUE metal_libcall_value

#undef TARGET_FUNCTION_VALUE_REGNO_P
#define TARGET_FUNCTION_VALUE_REGNO_P metal_function_value_regno_p

#undef TARGET_STATIC_CHAIN
#define TARGET_STATIC_CHAIN metal_static_chain

#undef TARGET_LEGITIMATE_ADDRESS_P
#define TARGET_LEGITIMATE_ADDRESS_P metal_legitimate_address_p

#undef TARGET_ASM_FILE_START
#define TARGET_ASM_FILE_START metal_file_start

#undef TARGET_ASM_FILE_END
#define TARGET_ASM_FILE_END metal_file_end

#undef TARGET_PASS_BY_REFERENCE
#define TARGET_PASS_BY_REFERENCE metal_pass_by_reference

#undef TARGET_RETURN_IN_MEMORY
#define TARGET_RETURN_IN_MEMORY metal_return_in_memory

#undef TARGET_MODES_TIEABLE_P
#define TARGET_MODES_TIEABLE_P metal_modes_tieable_p

#undef TARGET_LEGITIMATE_CONSTANT_P
#define TARGET_LEGITIMATE_CONSTANT_P metal_legitimate_constant_p

struct gcc_target targetm = TARGET_INITIALIZER;
