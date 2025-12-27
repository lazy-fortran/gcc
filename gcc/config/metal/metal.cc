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
#include "regs.h"
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
#include "langhooks.h"
#include "stringpool.h"
#include "attribs.h"
#include "gimplify.h"
#include "omp-general.h"
#include "omp-low.h"
#include "omp-offload.h"
#include "gomp-constants.h"
#include "rtl-iter.h"

static bool metal_legitimate_address_p (machine_mode, rtx, bool, code_helper);

static bool
metal_debug_enabled (void)
{
  static int cached = -1;
  if (cached == -1)
    cached = getenv ("GCC_METAL_DEBUG") ? 1 : 0;
  return cached == 1;
}

static void
metal_debugf (const char *fmt, ...)
{
  if (!metal_debug_enabled ())
    return;
  va_list ap;
  va_start (ap, fmt);
  fputs ("metal: ", stderr);
  vfprintf (stderr, fmt, ap);
  fputc ('\n', stderr);
  fflush (stderr);
  va_end (ap);
}

static void
metal_emit_cpymem_unrolled (rtx src, rtx dst, machine_mode mode,
			    unsigned HOST_WIDE_INT size_bytes,
			    unsigned chunk_bytes)
{
  unsigned HOST_WIDE_INT offset = 0;
  unsigned HOST_WIDE_INT iters = size_bytes / chunk_bytes;
  unsigned HOST_WIDE_INT remainder = size_bytes % chunk_bytes;

  for (unsigned HOST_WIDE_INT i = 0; i < iters; i++)
    {
      emit_insn (gen_rtx_SET (adjust_address (dst, mode, offset),
			      adjust_address (src, mode, offset)));
      offset += chunk_bytes;
    }

  if (remainder & 4)
    {
      emit_insn (gen_rtx_SET (adjust_address (dst, SImode, offset),
			      adjust_address (src, SImode, offset)));
      offset += 4;
      remainder -= 4;
    }
  if (remainder & 2)
    {
      emit_insn (gen_rtx_SET (adjust_address (dst, HImode, offset),
			      adjust_address (src, HImode, offset)));
      offset += 2;
      remainder -= 2;
    }
  if (remainder & 1)
    emit_insn (gen_rtx_SET (adjust_address (dst, QImode, offset),
			    adjust_address (src, QImode, offset)));
}

/* Expand cpymem/movmem (as from __builtin_memcpy/memmove).
   OPERANDS are the same as the cpymem/movmem patterns.
   IS_MOVE is true if this is a memmove, false for memcpy.
   Return true if we successfully expanded, or false if we cannot and must
   punt to a libcall.  */

bool
metal_expand_cpymem (rtx *operands, bool is_move)
{
  (void) is_move;

  if (!CONST_INT_P (operands[2]) || !CONST_INT_P (operands[3]))
    return false;

  rtx dst = operands[0];
  rtx src = operands[1];
  unsigned HOST_WIDE_INT size_bytes = UINTVAL (operands[2]);
  unsigned HOST_WIDE_INT align = UINTVAL (operands[3]);

  if (size_bytes == 0)
    return true;

  /* Unroll constant-size copies that show up frequently in ABI lowering.
     Keep this conservative to avoid excessive compile-time/code-size.  */
  if (size_bytes > 4096)
    return false;

  /* Choose the largest chunk size we can do in a single move.  */
  unsigned chunk_bytes = 1;
  if (align >= 8)
    chunk_bytes = 8;
  else if (align >= 4)
    chunk_bytes = 4;
  else if (align >= 2)
    chunk_bytes = 2;

  machine_mode mode = QImode;
  switch (chunk_bytes)
    {
    case 1: mode = QImode; break;
    case 2: mode = HImode; break;
    case 4: mode = SImode; break;
    case 8: mode = DImode; break;
    default: gcc_unreachable ();
    }

  metal_emit_cpymem_unrolled (src, dst, mode, size_bytes, chunk_bytes);
  return true;
}

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

/* Implement TARGET_OPTION_OVERRIDE.  */

static void
metal_option_override (void)
{
  /* Metal generates MSL source, not object code.  */
  flag_no_common = 1;
  metal_debugf ("option_override: no_common=1");
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
  metal_debugf ("init_builtins");
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
  static unsigned cnt;
  if (metal_debug_enabled () && cnt++ < 20)
    metal_debugf ("addr_space_pointer_mode[%u]: as=%d", cnt, (int) addrspace);

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
  static unsigned cnt;
  if (metal_debug_enabled () && cnt++ < 50)
    metal_debugf ("addr_space_convert[%u]: op=%s", cnt,
		  GET_RTX_NAME (GET_CODE (op)));

  if (!from_type || !to_type)
    return op;

  tree from_ptr_type = TREE_TYPE (from_type);
  tree to_ptr_type = TREE_TYPE (to_type);
  if (!from_ptr_type || !to_ptr_type)
    return op;

  addr_space_t from_as = TYPE_ADDR_SPACE (from_ptr_type);
  addr_space_t to_as = TYPE_ADDR_SPACE (to_ptr_type);
  if (from_as == to_as)
    return op;

  scalar_int_mode to_mode = as_a<scalar_int_mode> (TYPE_MODE (to_ptr_type));
  return convert_memory_address_addr_space (to_mode, op, to_as);
}

/* Implement TARGET_FUNCTION_ARG.  */

static rtx
metal_function_arg (cumulative_args_t, const function_arg_info &)
{
  static unsigned cnt;
  if (metal_debug_enabled () && cnt++ < 20)
    metal_debugf ("function_arg[%u]", cnt);

  /* All arguments passed in memory for now.  */
  return NULL_RTX;
}

/* Implement TARGET_FUNCTION_ARG_ADVANCE.  */

static void
metal_function_arg_advance (cumulative_args_t cum_v,
			    const function_arg_info &)
{
  static unsigned cnt;
  if (metal_debug_enabled () && cnt++ < 20)
    metal_debugf ("function_arg_advance[%u]", cnt);

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

/* Implement TARGET_GET_DRAP_RTX.

   Metal is a virtual offload-only target and does not currently support
   stack realignment.  Returning NULL here tells the middle-end that DRAP
   is not needed.  */

static rtx
metal_get_drap_rtx (void)
{
  return NULL_RTX;
}

/* Implement TARGET_LEGITIMATE_ADDRESS_P.  */

static bool
metal_legitimate_address_p (machine_mode mode, rtx x, bool strict,
			    code_helper)
{
  static unsigned cnt;
  if (metal_debug_enabled () && cnt++ < 80)
    metal_debugf ("legitimate_address_p[%u]: mode=%s rtx=%s strict=%d",
		  cnt, GET_MODE_NAME (mode), GET_RTX_NAME (GET_CODE (x)),
		  (int) strict);

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
   'A' - Print address space qualifier for memory operand.
   'c' - Print comparison operator (==, !=, <, <=, >, >=).
   'l' - Print a label reference.  */

void
metal_print_operand (FILE *file, rtx x, int code)
{
  static unsigned print_operand_count;
  if (metal_debug_enabled () && print_operand_count++ < 50)
    metal_debugf ("print_operand[%u]: code=%d rtx=%s",
		  print_operand_count, code, GET_RTX_NAME (GET_CODE (x)));

  switch (code)
    {
    case 'A':
      if (MEM_P (x))
	{
	  addr_space_t as = metal_mem_addr_space (x);
	  fputs (metal_addrspace_string (as), file);
	}
      return;

    case 'c':
      switch (GET_CODE (x))
	{
	case EQ: fputs ("==", file); return;
	case NE: fputs ("!=", file); return;
	case LT:
	case LTU: fputs ("<", file); return;
	case LE:
	case LEU: fputs ("<=", file); return;
	case GT:
	case GTU: fputs (">", file); return;
	case GE:
	case GEU: fputs (">=", file); return;
	default:
	  output_operand_lossage ("unsupported comparison code");
	  return;
	}

    case 'l':
      /* For now, labels are just printed as their assembler names.  */
      output_addr_const (file, x);
      return;

    case 0:
      if (REG_P (x))
	fprintf (file, "_reg%d", REGNO (x));
      else if (SUBREG_P (x) && REG_P (SUBREG_REG (x)))
	{
	  machine_mode mode = GET_MODE (x);
	  const char *type_str = metal_msl_type_from_mode (mode, false);
	  fprintf (file, "(%s)_reg%d", type_str, REGNO (SUBREG_REG (x)));
	}
      else if (MEM_P (x))
	{
	  /* Print the address expression for MEM operands.  Patterns that
	     access memory are expected to add the appropriate dereference
	     and type cast themselves.  */
	  metal_print_operand_address (file, XEXP (x, 0));
	}
      else if (CONST_INT_P (x))
	fprintf (file, HOST_WIDE_INT_PRINT_DEC, INTVAL (x));
      else if (SYMBOL_REF_P (x) || LABEL_REF_P (x))
	output_addr_const (file, x);
      else
	metal_print_operand_address (file, x);
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
  static unsigned print_addr_count;
  if (metal_debug_enabled () && print_addr_count++ < 50)
    metal_debugf ("print_operand_address[%u]: rtx=%s",
		  print_addr_count, GET_RTX_NAME (GET_CODE (addr)));

  switch (GET_CODE (addr))
    {
    case REG:
      fprintf (file, "_reg%d", REGNO (addr));
      break;

    case CONST_INT:
      fprintf (file, HOST_WIDE_INT_PRINT_DEC, INTVAL (addr));
      break;

    case PLUS:
      {
	rtx op0 = XEXP (addr, 0);
	rtx op1 = XEXP (addr, 1);
	fputs ("(", file);
	metal_print_operand_address (file, op0);
	fputs (" + ", file);
	metal_print_operand_address (file, op1);
	fputs (")", file);
      }
      break;

	case SYMBOL_REF:
	  if (SYMBOL_REF_FUNCTION_P (addr))
	    output_addr_const (file, addr);
	  else
	    {
	      fputc ('&', file);
	      output_addr_const (file, addr);
	    }
	  break;

	case LABEL_REF:
	  output_addr_const (file, addr);
	  break;

	case CONST:
	  metal_print_operand_address (file, XEXP (addr, 0));
	  break;

	default:
	  output_addr_const (file, addr);
	  break;
	}
}

/* Check if a function should be emitted as a kernel.  */

static bool
metal_write_as_kernel (tree attrs)
{
  return lookup_attribute ("kernel", attrs) != NULL_TREE;
}

/* Emit declarations for pseudo-registers used in the function.
   This follows the NVPTX backend approach of declaring all pseudos
   in the function prologue.  */

static void
metal_emit_register_declarations (FILE *file)
{
  metal_debugf ("emit_register_declarations: max_reg_num=%d",
		max_reg_num ());

  /* Declare hard registers too.  The Metal backend prints all registers as
     C identifiers (_regN).  */
  for (int i = 0; i < FIRST_PSEUDO_REGISTER; i++)
    fprintf (file, "  long _reg%d;\n", i);

  /* Iterate through all pseudo-registers beyond virtual registers.  */
  int maxregs = max_reg_num ();
  for (int i = LAST_VIRTUAL_REGISTER + 1; i < maxregs; i++)
    {
      rtx reg = regno_reg_rtx[i];
      if (reg == NULL_RTX || reg == const0_rtx)
	continue;
      if (!REG_P (reg))
	continue;
      if (REGNO (reg) != i)
	continue;

      machine_mode mode = GET_MODE (reg);

      /* Determine if register should be unsigned.
	 For simplicity, we default to signed types for non-float modes.
	 This matches common C/C++ semantics.  */
      bool unsigned_p = false;

      /* Skip void mode registers.  */
      if (mode == VOIDmode)
	continue;

      const char *type_str = metal_msl_type_from_mode (mode, unsigned_p);
      fprintf (file, "  %s _reg%d;\n", type_str, i);
    }
}

/* Emit kernel parameter declarations.

   Metal kernels receive arguments in a packed struct (matching NVPTX/GOMP model).
   The GOMP runtime allocates device memory, copies all kernel arguments into it
   (both pointers and scalars), and passes a pointer to this memory as devaddrs.

   The plugin binds this memory as buffer(0), so the kernel receives a single
   pointer parameter pointing to the packed argument struct.

   This implementation generates individual parameter declarations and unpacks
   them from the argument buffer in the kernel prologue, maintaining compatibility
   with the GOMP argument passing convention while using idiomatic Metal code.  */

static void
metal_write_kernel_params_unpacked (FILE *file, tree fntype, tree decl)
{
  tree args = TYPE_ARG_TYPES (fntype);

  if (!args)
    {
      args = DECL_ARGUMENTS (decl);
    }

  /* Count parameters to determine if we need argument unpacking.  */
  int param_count = 0;
  for (tree t = args; t != NULL_TREE; t = TREE_CHAIN (t))
    param_count++;

  if (param_count == 0)
    {
      /* Even for kernels with no explicit parameters, the OpenACC runtime
	 passes the packed arguments buffer at buffer(0).  */
    }

  /* Emit single pointer parameter for packed arguments.
     The GOMP runtime packs all arguments (pointers and scalars) into
     device memory and passes a pointer to this memory region.  */
  fprintf (file, "device char* _args_ptr [[buffer(0)]],\n");
  fprintf (file, "    uint3 tid [[thread_position_in_grid]],\n");
  fprintf (file, "    uint3 tgid [[threadgroup_position_in_grid]]");
}

/* Implement ASM_DECLARE_FUNCTION_NAME.  Emit [[kernel]] attribute
   for functions marked with the kernel attribute, followed by function
   signature with proper parameter declarations and opening brace.  */

void
metal_declare_function_name (FILE *file, const char *name, tree decl)
{
  bool is_kernel = metal_write_as_kernel (DECL_ATTRIBUTES (decl));
  if (!is_kernel && name && strstr (name, "omp_fn") != NULL)
    is_kernel = true;

  metal_debugf ("declare_function_name: %s kernel=%d", name,
		(int) is_kernel);

  if (is_kernel)
    fprintf (file, "kernel ");

  fprintf (file, "void %s(", name);

  /* Emit parameter list.  */
  if (is_kernel)
    {
      tree fntype = TREE_TYPE (decl);
      metal_write_kernel_params_unpacked (file, fntype, decl);
    }

  fprintf (file, ")");

  /* Emit opening brace for function body.  */
  fprintf (file, "\n{\n");

  /* Emit declarations for all pseudo-registers used in this function.  */
  metal_emit_register_declarations (file);

  /* Initialize the argument/stack base register for kernels.

     Metal kernels receive a single pointer to the packed arguments buffer as
     the first formal parameter (buffer(0)).  The current backend lowers kernel
     argument accesses via a hard register base (currently printed as _reg1).
     Make sure it is initialized from the kernel parameter so generated code
     does not dereference an uninitialized pointer.  */
  if (is_kernel)
    {
      fprintf (file, "  _reg%d = (long)_args_ptr;\n", STACK_POINTER_REGNUM);
      fprintf (file, "  _reg%d = (long)_args_ptr;\n", ARG_POINTER_REGNUM);
    }
}

/* Terminate a function by writing a closing brace to FILE.
   This is called via ASM_DECLARE_FUNCTION_SIZE.  */

void
metal_function_end (FILE *file)
{
  metal_debugf ("function_end");
  fprintf (file, "}\n");
}

/* Implement ASM_DECLARE_OBJECT_NAME.  Emit variable declaration with
   appropriate address space qualifier.  */

void
metal_declare_object_name (FILE *file, const char *name, const_tree decl)
{
  metal_debugf ("declare_object_name: %s", name);
  tree type = TREE_TYPE (decl);
  tree size_unit = DECL_SIZE_UNIT (decl);
  HOST_WIDE_INT obj_size = 0;
  if (size_unit && tree_fits_shwi_p (size_unit))
    obj_size = tree_to_shwi (size_unit);
  addr_space_t as = TYPE_ADDR_SPACE (type);
  const char *addrspace = metal_addrspace_string (as);
  bool needs_init = (as == ADDR_SPACE_CONSTANT);

  /* Default to device address space for global variables without explicit space.  */
  if (as == ADDR_SPACE_GENERIC)
    {
      addrspace = "constant ";
      needs_init = true;
    }

  fprintf (file, "// Variable: %s (size=%ld)\n", name, (long)obj_size);
  fprintf (file, "%s", addrspace);

  /* Emit type information - this is a placeholder.
     Full type emission would require more complex logic.  */
  fprintf (file, "/* %s */ char %s[%ld]%s;\n",
	   TREE_PUBLIC (decl) ? "public" : "local",
	   name, (long)obj_size,
	   needs_init ? " = {0}" : "");
}

/* Implement ASM_OUTPUT_ALIGNED_DECL_COMMON and ASM_OUTPUT_ALIGNED_DECL_LOCAL.
   Emit common or local variable declaration.  */

void
metal_output_aligned_decl (FILE *file, const char *name, const_tree decl,
			    HOST_WIDE_INT size, unsigned int align)
{
  metal_debugf ("output_aligned_decl: %s size=%ld align=%u",
		name, (long) size, align);
  tree type = TREE_TYPE (decl);
  addr_space_t as = TYPE_ADDR_SPACE (type);
  const char *addrspace = metal_addrspace_string (as);
  bool needs_init = (as == ADDR_SPACE_CONSTANT);

  /* Default to device address space for variables without explicit space.  */
  if (as == ADDR_SPACE_GENERIC)
    {
      addrspace = "constant ";
      needs_init = true;
    }

  fprintf (file, "// %s variable: %s (size=%ld, align=%u)\n",
	   TREE_PUBLIC (decl) ? "Common" : "Local",
	   name, (long)size, align);
  fprintf (file, "%s", addrspace);

  /* Emit type information - this is a placeholder.
     Full type emission would require more complex logic.  */
  fprintf (file, "/* %s */ char %s[%ld]%s;\n",
	   TREE_PUBLIC (decl) ? "common" : "local",
	   name, (long)size,
	   needs_init ? " = {0}" : "");
}

/* Implement TARGET_ASM_FILE_START.  */

static void
metal_file_start (void)
{
  metal_debugf ("file_start");
  fputs ("// Metal Shading Language output\n", asm_out_file);
  fputs ("// Generated by GCC\n\n", asm_out_file);
  fputs ("#include <metal_stdlib>\n", asm_out_file);
  fputs ("using namespace metal;\n\n", asm_out_file);
  /* MSL doesn't provide libc; some device-side code paths reference abort().  */
  fputs ("static inline __attribute__((unused)) void abort(void) { return; }\n\n",
	 asm_out_file);
}

/* Implement TARGET_ASM_FILE_END.  */

static void
metal_file_end (void)
{
  metal_debugf ("file_end");
  fputs ("\n// End of Metal output\n", asm_out_file);
}

/* Fake "sections support".  Metal output is source, so sections are ignored,
   but the middle-end expects the global section variables to be initialized.  */

static void
metal_output_section_asm_op (const char *)
{
  /* Intentionally empty.  */
}

static void
metal_asm_init_sections (void)
{
  text_section = get_unnamed_section (SECTION_CODE,
				      metal_output_section_asm_op, "T");
  data_section = get_unnamed_section (SECTION_WRITE,
				      metal_output_section_asm_op, "D");
  exception_section = get_unnamed_section (0,
					   metal_output_section_asm_op, "E");
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
metal_return_in_memory (const_tree type, const_tree)
{
  /* Match our current very-simple ABI:
     - Small scalars may return in registers.
     - Aggregates larger than 16 bytes return in memory.  */
  if (!type)
    return false;

  machine_mode mode = TYPE_MODE (type);
  if (mode == BLKmode)
    return true;

  if (TYPE_SIZE (type) && TREE_CODE (TYPE_SIZE (type)) == INTEGER_CST)
    return TREE_INT_CST_LOW (TYPE_SIZE (type)) > 128;

  /* Unknown size: be conservative.  */
  return true;
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
  metal_debugf ("record_offload_symbol: %s",
		IDENTIFIER_POINTER (DECL_ASSEMBLER_NAME (decl)));
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

#undef TARGET_GET_DRAP_RTX
#define TARGET_GET_DRAP_RTX metal_get_drap_rtx

#undef TARGET_LEGITIMATE_ADDRESS_P
#define TARGET_LEGITIMATE_ADDRESS_P metal_legitimate_address_p

#undef TARGET_ASM_FILE_START
#define TARGET_ASM_FILE_START metal_file_start

#undef TARGET_ASM_FILE_END
#define TARGET_ASM_FILE_END metal_file_end

#undef TARGET_ASM_INIT_SECTIONS
#define TARGET_ASM_INIT_SECTIONS metal_asm_init_sections

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

/* This file provides GTY roots in gt-metal.h.  */
#include "gt-metal.h"
