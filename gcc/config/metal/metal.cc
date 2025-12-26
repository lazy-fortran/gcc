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
#include "attribs.h"
#include "gimplify.h"
#include "omp-general.h"
#include "omp-low.h"
#include "omp-offload.h"
#include "gomp-constants.h"
#include "rtl-iter.h"

/* This file should be included last.  */
#include "target-def.h"

/* Codes for all the Metal builtins.  */
enum metal_builtins
{
  METAL_BUILTIN_THREAD_POSITION_IN_GRID,
  METAL_BUILTIN_THREADGROUP_POSITION_IN_GRID,
  METAL_BUILTIN_THREAD_POSITION_IN_THREADGROUP,
  METAL_BUILTIN_THREADS_PER_THREADGROUP,
  METAL_BUILTIN_THREADGROUP_BARRIER,
  METAL_BUILTIN_MAX
};

static GTY(()) tree metal_builtin_decls[METAL_BUILTIN_MAX];

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

/* Handle a "kernel" attribute; arguments as in
   struct attribute_spec.handler.  */

static tree
metal_handle_kernel_attribute (tree *node, tree name, tree ARG_UNUSED (args),
				int ARG_UNUSED (flags), bool *no_add_attrs)
{
  tree decl = *node;

  if (TREE_CODE (decl) != FUNCTION_DECL)
    {
      error ("%qE attribute only applies to functions", name);
      *no_add_attrs = true;
    }
  else if (!VOID_TYPE_P (TREE_TYPE (TREE_TYPE (decl))))
    {
      error ("%qE attribute requires a void return type", name);
      *no_add_attrs = true;
    }

  return NULL_TREE;
}

/* Table of valid machine attributes.  */
TARGET_GNU_ATTRIBUTES (metal_attribute_table,
{
  { "kernel", 0, 0, true, false, false, false,
    metal_handle_kernel_attribute, NULL }
});

/* Implement TARGET_INIT_BUILTINS.  */

static void
metal_init_builtins (void)
{
#define DEF(ID, NAME, T)						\
  (metal_builtin_decls[METAL_BUILTIN_ ## ID]				\
   = add_builtin_function ("__metal_" NAME,				\
			   build_function_type_list T,			\
			   METAL_BUILTIN_ ## ID, BUILT_IN_MD, NULL, NULL))
#define UINT unsigned_type_node
#define VOID void_type_node

  DEF (THREAD_POSITION_IN_GRID, "thread_position_in_grid",
       (UINT, UINT, NULL_TREE));
  DEF (THREADGROUP_POSITION_IN_GRID, "threadgroup_position_in_grid",
       (UINT, UINT, NULL_TREE));
  DEF (THREAD_POSITION_IN_THREADGROUP, "thread_position_in_threadgroup",
       (UINT, UINT, NULL_TREE));
  DEF (THREADS_PER_THREADGROUP, "threads_per_threadgroup",
       (UINT, UINT, NULL_TREE));
  DEF (THREADGROUP_BARRIER, "threadgroup_barrier", (VOID, VOID, NULL_TREE));

#undef DEF
#undef UINT
#undef VOID
}

/* Implement TARGET_BUILTIN_DECL.  */

static tree
metal_builtin_decl (unsigned code, bool ARG_UNUSED (initialize_p))
{
  if (code >= METAL_BUILTIN_MAX)
    return error_mark_node;

  return metal_builtin_decls[code];
}

/* Implement TARGET_EXPAND_BUILTIN.  */

static rtx
metal_expand_builtin (tree exp, rtx target, rtx ARG_UNUSED (subtarget),
		      machine_mode mode, int ignore)
{
  tree fndecl = TREE_OPERAND (CALL_EXPR_FN (exp), 0);
  unsigned int fcode = DECL_MD_FUNCTION_CODE (fndecl);

  if (ignore)
    return NULL_RTX;

  switch (fcode)
    {
    case METAL_BUILTIN_THREAD_POSITION_IN_GRID:
    case METAL_BUILTIN_THREADGROUP_POSITION_IN_GRID:
    case METAL_BUILTIN_THREAD_POSITION_IN_THREADGROUP:
    case METAL_BUILTIN_THREADS_PER_THREADGROUP:
      if (!target || !register_operand (target, mode))
	target = gen_reg_rtx (mode);
      return target;
    case METAL_BUILTIN_THREADGROUP_BARRIER:
      /* Void builtin - no return value, just emit barrier.  */
      return const0_rtx;
    default:
      gcc_unreachable ();
    }

  return NULL_RTX;
}

/* Implement TARGET_ADDR_SPACE_POINTER_MODE.  */

static scalar_int_mode
metal_addr_space_pointer_mode (addr_space_t addrspace)
{
  switch (addrspace)
    {
    case ADDR_SPACE_GENERIC:
    case ADDR_SPACE_DEVICE:
    case ADDR_SPACE_THREADGROUP:
    case ADDR_SPACE_CONSTANT:
      return ptr_mode;
    default:
      gcc_unreachable ();
    }
}

/* Implement TARGET_ADDR_SPACE_ADDRESS_MODE.  */

static scalar_int_mode
metal_addr_space_address_mode (addr_space_t addrspace)
{
  return metal_addr_space_pointer_mode (addrspace);
}

/* Implement TARGET_ADDR_SPACE_VALID_POINTER_MODE.  */

static bool
metal_addr_space_valid_pointer_mode (scalar_int_mode mode,
				     addr_space_t ARG_UNUSED (as))
{
  return mode == ptr_mode;
}

/* Implement TARGET_ADDR_SPACE_LEGITIMATE_ADDRESS_P.  */

static bool
metal_addr_space_legitimate_address_p (machine_mode mode, rtx x,
				       bool strict, addr_space_t as,
				       code_helper ch)
{
  return metal_legitimate_address_p (mode, x, strict, ch);
}

/* Implement TARGET_ADDR_SPACE_SUBSET_P.  */

static bool
metal_addr_space_subset_p (addr_space_t subset, addr_space_t superset)
{
  return subset == superset || superset == ADDR_SPACE_GENERIC;
}

/* Implement TARGET_ADDR_SPACE_CONVERT.  */

static rtx
metal_addr_space_convert (rtx op, tree from_type, tree to_type)
{
  if (!from_type || !to_type)
    return op;

  tree from_tree_type = TREE_TYPE (from_type);
  tree to_tree_type = TREE_TYPE (to_type);

  if (!from_tree_type || !to_tree_type)
    return op;

  addr_space_t from_as = TYPE_ADDR_SPACE (from_tree_type);
  addr_space_t to_as = TYPE_ADDR_SPACE (to_tree_type);

  if (from_as == to_as)
    return op;

  return convert_memory_address (TYPE_MODE (to_type), op);
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
metal_legitimate_address_p (machine_mode mode, rtx x, bool strict,
			    code_helper)
{
  /* Basic address validation for Metal address spaces.  */
  if (REG_P (x))
    return true;

  if (CONST_INT_P (x))
    return true;

  if (GET_CODE (x) == PLUS)
    {
      rtx op0 = XEXP (x, 0);
      rtx op1 = XEXP (x, 1);

      if (REG_P (op0) && CONST_INT_P (op1))
	return true;

      if (REG_P (op1) && CONST_INT_P (op0))
	return true;
    }

  if (GET_CODE (x) == SYMBOL_REF || GET_CODE (x) == LABEL_REF)
    return true;

  return false;
}

/* Return the MSL type name for a given machine mode and signedness.
   This is used for generating proper type casts in MSL output.

   Note: Unlike NVPTX which uses a 'promote' parameter, MSL requires
   signedness distinction (int vs uint) for integer types, hence the
   unsigned_p parameter.  */

static const char *
metal_msl_type_from_mode (machine_mode mode, bool unsigned_p)
{
  switch (mode)
    {
    case E_BImode:
      return "bool";
    case E_QImode:
      return unsigned_p ? "uchar" : "char";
    case E_HImode:
      return unsigned_p ? "ushort" : "short";
    case E_SImode:
      return unsigned_p ? "uint" : "int";
    case E_DImode:
      return unsigned_p ? "ulong" : "long";
    case E_HFmode:
      return "half";
    case E_SFmode:
      return "float";
    /* E_DFmode: Metal Shading Language does not support double-precision
       floating-point types per Apple Metal Shading Language Spec v3.2.
       If we reach this case, it indicates a configuration error or
       unsupported operation.  */
    default:
      gcc_unreachable ();
    }
}

/* Return the Metal address space qualifier string for a given address space.  */

static const char *
metal_addrspace_string (addr_space_t as)
{
  switch (as)
    {
    case ADDR_SPACE_GENERIC:
      return "";
    case ADDR_SPACE_DEVICE:
      return "device ";
    case ADDR_SPACE_THREADGROUP:
      return "threadgroup ";
    case ADDR_SPACE_CONSTANT:
      return "constant ";
    default:
      gcc_unreachable ();
    }
}

/* Extract address space from a memory RTX.  */

static addr_space_t
metal_mem_addr_space (rtx mem)
{
  gcc_assert (MEM_P (mem));

  const_rtx addr = XEXP (mem, 0);
  subrtx_iterator::array_type array;
  FOR_EACH_SUBRTX (iter, array, addr, ALL)
    if (SYMBOL_REF_P (*iter))
      {
	tree decl = SYMBOL_REF_DECL (*iter);
	if (decl && TREE_CODE (decl) == VAR_DECL)
	  return TYPE_ADDR_SPACE (TREE_TYPE (decl));
      }

  return ADDR_SPACE_GENERIC;
}

/* Implement PRINT_OPERAND.

   Supported codes:
   'A' - Print address space qualifier for memory operand.  */

void
metal_print_operand (FILE *file, rtx x, int code)
{
  switch (code)
    {
    case 'A':
      if (MEM_P (x))
	{
	  addr_space_t as = metal_mem_addr_space (x);
	  fputs (metal_addrspace_string (as), file);
	}
      return;

    case 0:
      if (REG_P (x))
	fprintf (file, "r%d", REGNO (x));
      else if (CONST_INT_P (x))
	fprintf (file, HOST_WIDE_INT_PRINT_DEC, INTVAL (x));
      else
	output_addr_const (file, x);
      return;

    default:
      output_operand_lossage ("invalid %%code for print_operand");
      return;
    }
}

/* Implement PRINT_OPERAND_ADDRESS.  */

void
metal_print_operand_address (FILE *file, rtx addr)
{
  output_addr_const (file, addr);
}

/* Check if a function should be emitted as a kernel.  */

static bool
metal_write_as_kernel (tree attrs)
{
  return lookup_attribute ("kernel", attrs) != NULL_TREE;
}

/* Implement ASM_DECLARE_FUNCTION_NAME.  Emit [[kernel]] attribute
   for functions marked with the kernel attribute, followed by function
   signature and opening brace.  */

void
metal_declare_function_name (FILE *file, const char *name, tree decl)
{
  if (metal_write_as_kernel (DECL_ATTRIBUTES (decl)))
    fprintf (file, "[[kernel]] ");
  fprintf (file, "void %s(", name);

  /* Emit parameter list.  For now, emit empty parameter list.
     Full parameter handling will be implemented in future work.  */
  fprintf (file, ")");

  /* Emit opening brace for function body.  */
  fprintf (file, "\n{\n");
}

/* Terminate a function by writing a closing brace to FILE.
   This is called via ASM_DECLARE_FUNCTION_SIZE.  */

void
metal_function_end (FILE *file)
{
  fprintf (file, "}\n");
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

/* Record a symbol for mkoffload to enter into the mapping table.  */

static void
metal_record_offload_symbol (tree decl)
{
  switch (TREE_CODE (decl))
    {
    case VAR_DECL:
      fprintf (asm_out_file, "//VAR_MAP \"%s\"\n",
	       IDENTIFIER_POINTER (DECL_ASSEMBLER_NAME (decl)));
      break;

    case FUNCTION_DECL:
      {
	tree attr = oacc_get_fn_attrib (decl);
	/* OpenMP offloading does not set this attribute.  */
	tree dims = attr ? TREE_VALUE (attr) : NULL_TREE;

	fprintf (asm_out_file, "//");
	if (lookup_attribute ("omp declare target indirect",
			      DECL_ATTRIBUTES (decl)))
	  fprintf (asm_out_file, "IND_FUNC_MAP");
	else
	  fprintf (asm_out_file, "FUNC_MAP");
	fprintf (asm_out_file, " \"%s\"",
		 IDENTIFIER_POINTER (DECL_ASSEMBLER_NAME (decl)));

	for (; dims; dims = TREE_CHAIN (dims))
	  {
	    int size = TREE_INT_CST_LOW (TREE_VALUE (dims));

	    gcc_assert (!TREE_PURPOSE (dims));
	    fprintf (asm_out_file, ", %#x", size);
	  }

	fprintf (asm_out_file, "\n");
      }
      break;

    default:
      gcc_unreachable ();
    }
}

/* Initialize the GCC target structure.  */

#undef TARGET_OPTION_OVERRIDE
#define TARGET_OPTION_OVERRIDE metal_option_override

#undef TARGET_INIT_BUILTINS
#define TARGET_INIT_BUILTINS metal_init_builtins

#undef TARGET_BUILTIN_DECL
#define TARGET_BUILTIN_DECL metal_builtin_decl

#undef TARGET_EXPAND_BUILTIN
#define TARGET_EXPAND_BUILTIN metal_expand_builtin

#undef TARGET_ADDR_SPACE_POINTER_MODE
#define TARGET_ADDR_SPACE_POINTER_MODE metal_addr_space_pointer_mode

#undef TARGET_ADDR_SPACE_ADDRESS_MODE
#define TARGET_ADDR_SPACE_ADDRESS_MODE metal_addr_space_address_mode

#undef TARGET_ADDR_SPACE_VALID_POINTER_MODE
#define TARGET_ADDR_SPACE_VALID_POINTER_MODE \
  metal_addr_space_valid_pointer_mode

#undef TARGET_ADDR_SPACE_LEGITIMATE_ADDRESS_P
#define TARGET_ADDR_SPACE_LEGITIMATE_ADDRESS_P \
  metal_addr_space_legitimate_address_p

#undef TARGET_ADDR_SPACE_SUBSET_P
#define TARGET_ADDR_SPACE_SUBSET_P metal_addr_space_subset_p

#undef TARGET_ADDR_SPACE_CONVERT
#define TARGET_ADDR_SPACE_CONVERT metal_addr_space_convert

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

#undef TARGET_RECORD_OFFLOAD_SYMBOL
#define TARGET_RECORD_OFFLOAD_SYMBOL metal_record_offload_symbol

struct gcc_target targetm = TARGET_INITIALIZER;
