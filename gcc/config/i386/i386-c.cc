/* Subroutines used for macro/preprocessor support on the ia-32.
   Copyright (C) 2008-2026 Free Software Foundation, Inc.

This file is part of GCC.

GCC is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; either version 3, or (at your option)
any later version.

GCC is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with GCC; see the file COPYING3.  If not see
<http://www.gnu.org/licenses/>.  */

#define IN_TARGET_CODE 1

#include "config.h"
#include "system.h"
#include "coretypes.h"
#include "target.h"
#include "c-family/c-common.h"
#include "memmodel.h"
#include "tm_p.h"
#include "c-family/c-pragma.h"
#include "config/i386/i386-cppbuiltin.h"

static bool ix86_pragma_target_parse (tree, tree);

/* Hook to validate the current #pragma GCC target and set the state, and
   update the macros based on what was changed.  If ARGS is NULL, then
   POP_TARGET is used to reset the options.  */

static bool
ix86_pragma_target_parse (tree args, tree pop_target)
{
  tree prev_tree
    = build_target_option_node (&global_options, &global_options_set);
  tree cur_tree;
  struct cl_target_option *prev_opt;
  struct cl_target_option *cur_opt;
  HOST_WIDE_INT prev_isa;
  HOST_WIDE_INT cur_isa;
  HOST_WIDE_INT diff_isa;
  HOST_WIDE_INT prev_isa2;
  HOST_WIDE_INT cur_isa2;
  HOST_WIDE_INT diff_isa2;
  enum processor_type prev_arch;
  enum processor_type prev_tune;
  enum processor_type cur_arch;
  enum processor_type cur_tune;

  if (! args)
    {
      cur_tree = (pop_target ? pop_target : target_option_default_node);
      cl_target_option_restore (&global_options, &global_options_set,
				TREE_TARGET_OPTION (cur_tree));
    }
  else
    {
      cur_tree = ix86_valid_target_attribute_tree (NULL_TREE, args,
						   &global_options,
						   &global_options_set, 0);
      if (!cur_tree || cur_tree == error_mark_node)
       {
         cl_target_option_restore (&global_options, &global_options_set,
                                   TREE_TARGET_OPTION (prev_tree));
         return false;
       }
    }

  target_option_current_node = cur_tree;
  ix86_reset_previous_fndecl ();

  /* Figure out the previous/current isa, arch, tune and the differences.  */
  prev_opt  = TREE_TARGET_OPTION (prev_tree);
  cur_opt   = TREE_TARGET_OPTION (cur_tree);
  prev_isa  = prev_opt->x_ix86_isa_flags;
  cur_isa   = cur_opt->x_ix86_isa_flags;
  diff_isa  = (prev_isa ^ cur_isa);
  prev_isa2  = prev_opt->x_ix86_isa_flags2;
  cur_isa2   = cur_opt->x_ix86_isa_flags2;
  diff_isa2  = (prev_isa2 ^ cur_isa2);
  prev_arch = (enum processor_type) prev_opt->arch;
  prev_tune = (enum processor_type) prev_opt->tune;
  cur_arch  = (enum processor_type) cur_opt->arch;
  cur_tune  = (enum processor_type) cur_opt->tune;

  /* If the same processor is used for both previous and current options, don't
     change the macros.  */
  if (cur_arch == prev_arch)
    cur_arch = prev_arch = PROCESSOR_max;

  if (cur_tune == prev_tune)
    cur_tune = prev_tune = PROCESSOR_max;

  /* Undef all of the macros for that are no longer current.  */
  cpp_force_token_locations (parse_in, BUILTINS_LOCATION);
  ix86_cpu_cpp_builtins_internal (parse_in,
				  prev_isa & diff_isa,
				  prev_isa2 & diff_isa2,
				  prev_arch,
				  prev_tune,
				  (enum fpmath_unit) prev_opt->x_ix86_fpmath,
				  cpp_undef);
  cpp_stop_forcing_token_locations (parse_in);

  /* For the definitions, ensure all newly defined macros are considered
     as used for -Wunused-macros.  There is no point warning about the
     compiler predefined macros.  */
  cpp_options *cpp_opts = cpp_get_options (parse_in);
  unsigned char saved_warn_unused_macros = cpp_opts->warn_unused_macros;
  cpp_opts->warn_unused_macros = 0;

  /* Define all of the macros for new options that were just turned on.  */
  cpp_force_token_locations (parse_in, BUILTINS_LOCATION);
  ix86_cpu_cpp_builtins_internal (parse_in,
				  cur_isa & diff_isa,
				  cur_isa2 & diff_isa2,
				  cur_arch,
				  cur_tune,
				  (enum fpmath_unit) cur_opt->x_ix86_fpmath,
				  cpp_define);
  cpp_stop_forcing_token_locations (parse_in);

  cpp_opts->warn_unused_macros = saved_warn_unused_macros;

  return true;
}

/* Function to tell the preprocessor about the defines for the current target.
   Delegates to the shared implementation in i386-cppbuiltin.cc and adds the
   C++-only handling of __STDCPP_FLOAT16_T__ / __STDCPP_BFLOAT16_T__ that
   cannot live there (it depends on cxx_dialect).  */

void
ix86_target_macros (void)
{
  ix86_cpu_cpp_builtins (parse_in, !flag_iso);

  /* HFmode/BFmode is supported without depending any isa
     in scalar_mode_supported_p and libgcc_floating_mode_supported_p,
     but according to psABI, they're really supported w/ SSE2 and above.
     Since libstdc++ uses __STDCPP_FLOAT16_T__ and __STDCPP_BFLOAT16_T__
     for backend support of the types, undef the macros to avoid
     build failure, see PR109504.  */
  if (!TARGET_SSE2 && c_dialect_cxx () && cxx_dialect > cxx20)
    {
      cpp_undef (parse_in, "__STDCPP_FLOAT16_T__");
      cpp_undef (parse_in, "__STDCPP_BFLOAT16_T__");
    }
}


/* Register target pragmas.  We need to add the hook for parsing #pragma GCC
   option here rather than in i386.cc since it will pull in various preprocessor
   functions, and those are not present in languages like fortran without a
   preprocessor.  */

void
ix86_register_pragmas (void)
{
  /* Update pragma hook to allow parsing #pragma GCC target.  */
  targetm.target_option.pragma_parse = ix86_pragma_target_parse;

  c_register_addr_space ("__seg_fs", ADDR_SPACE_SEG_FS);
  c_register_addr_space ("__seg_gs", ADDR_SPACE_SEG_GS);

#ifdef REGISTER_SUBTARGET_PRAGMAS
  REGISTER_SUBTARGET_PRAGMAS ();
#endif
}
