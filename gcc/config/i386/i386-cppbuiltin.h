/* Shared x86 preprocessor builtins for the C-family and Fortran frontends.
   Copyright (C) 2026 Free Software Foundation, Inc.

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

#ifndef GCC_I386_CPPBUILTIN_H
#define GCC_I386_CPPBUILTIN_H

struct cpp_reader;

/* Emit all x86 target preprocessor macros that don't depend on the source
   language: architecture identification (__x86_64__, __i386__, ...), CPU
   arch/tune macros (__haswell__, __tune_haswell__, ...), code-model macros,
   HLE/CET/segment macros, and the full ISA feature block (__SSE__, __AVX__,
   __FMA__, ...).  DEFINE_USER_NAMESPACE selects whether user-namespace
   aliases are emitted alongside the reserved-namespace forms: the C frontend
   passes !flag_iso; the Fortran frontend passes false.  */
extern void ix86_cpu_cpp_builtins (cpp_reader *pfile,
				   bool define_user_namespace);

/* Pragma-switchable subset of the above (arch, tune, code-model, ISA).  Used
   at startup and during `#pragma GCC target` transitions: called once with
   cpp_undef to clear macros for disabled options, and once with cpp_define
   to set macros for newly enabled options.  */
extern void ix86_cpu_cpp_builtins_internal
  (cpp_reader *pfile,
   HOST_WIDE_INT isa_flag,
   HOST_WIDE_INT isa_flag2,
   enum processor_type arch,
   enum processor_type tune,
   enum fpmath_unit fpmath,
   void (*def_or_undef) (cpp_reader *, const char *));

#endif /* GCC_I386_CPPBUILTIN_H */
