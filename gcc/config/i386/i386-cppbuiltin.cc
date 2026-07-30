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

/* Shared between i386-c.cc (C/C++ frontend entry) and fortran/cpp.cc (via
   TARGET_GENERIC_CPU_CPP_BUILTINS).  Pragma handling and the C++20
   STDCPP_FLOAT16 undef remain in i386-c.cc because they use c-family
   globals.  */

#define IN_TARGET_CODE 1

#include "config.h"
#include "system.h"
#include "coretypes.h"
#include "tm.h"
#include "memmodel.h"
#include "tm_p.h"
#include "target.h"
#include "cpplib.h"
#include "config/i386/i386-cppbuiltin.h"

/* Parameterized copy of builtin_define_std from c-family/c-cppbuiltin.cc
   that takes PFILE and DEFINE_USER_NAMESPACE instead of reading parse_in
   and flag_iso directly.  Given "unix", defines __unix, __unix__, and (if
   DEFINE_USER_NAMESPACE) unix.  */

static void
ix86_cpp_builtin_define_std (cpp_reader *pfile, const char *macro,
			     bool define_user_namespace)
{
  size_t len = strlen (macro);
  char *buff = (char *) alloca (len + 5);
  char *p = buff + 2;
  char *q = p + len;

  /* prepend __ (or maybe just _) if in user's namespace.  */
  memcpy (p, macro, len + 1);
  if (!(*p == '_' && (p[1] == '_' || ISUPPER (p[1]))))
    {
      if (*p != '_')
	*--p = '_';
      if (p[1] != '_')
	*--p = '_';
    }
  cpp_define (pfile, p);

  /* If it was in user's namespace...  */
  if (p != buff + 2)
    {
      /* Define the macro with leading and following __.  */
      if (q[-1] != '_')
	*q++ = '_';
      if (q[-2] != '_')
	*q++ = '_';
      *q = '\0';
      cpp_define (pfile, p);

      /* Finally, define the original macro if permitted.  */
      if (define_user_namespace)
	cpp_define (pfile, macro);
    }
}

/* Emit the pragma-switchable x86 preprocessor macros: arch and tune
   identification, code model, and ISA feature flags.  Called with
   cpp_define at startup and twice per `#pragma GCC target' transition
   (first with cpp_undef to clear stale macros, then with cpp_define).  */

void
ix86_cpu_cpp_builtins_internal (cpp_reader *pfile,
				HOST_WIDE_INT isa_flag,
				HOST_WIDE_INT isa_flag2,
				enum processor_type arch,
				enum processor_type tune,
				enum fpmath_unit fpmath,
				void (*def_or_undef) (cpp_reader *,
						      const char *))
{
  /* For some of the k6/pentium variants there weren't separate ISA bits to
     identify which tune/arch flag was passed, so figure it out here.  */
  size_t arch_len = strlen (ix86_arch_string);
  size_t tune_len = strlen (ix86_tune_string);
  int last_arch_char = ix86_arch_string[arch_len - 1];
  int last_tune_char = ix86_tune_string[tune_len - 1];

  /* Built-ins based on -march=.  */
  switch (arch)
    {
    case PROCESSOR_I386:
      break;
    case PROCESSOR_I486:
      def_or_undef (pfile, "__i486");
      def_or_undef (pfile, "__i486__");
      break;
    case PROCESSOR_LAKEMONT:
      /* Intel MCU is based on Intel Pentium CPU.  */
    case PROCESSOR_PENTIUM:
      def_or_undef (pfile, "__i586");
      def_or_undef (pfile, "__i586__");
      def_or_undef (pfile, "__pentium");
      def_or_undef (pfile, "__pentium__");
      if (isa_flag & OPTION_MASK_ISA_MMX)
	def_or_undef (pfile, "__pentium_mmx__");
      break;
    case PROCESSOR_PENTIUMPRO:
      def_or_undef (pfile, "__i686");
      def_or_undef (pfile, "__i686__");
      def_or_undef (pfile, "__pentiumpro");
      def_or_undef (pfile, "__pentiumpro__");
      break;
    case PROCESSOR_GEODE:
      def_or_undef (pfile, "__geode");
      def_or_undef (pfile, "__geode__");
      break;
    case PROCESSOR_K6:
      def_or_undef (pfile, "__k6");
      def_or_undef (pfile, "__k6__");
      if (last_arch_char == '2')
	def_or_undef (pfile, "__k6_2__");
      else if (last_arch_char == '3')
	def_or_undef (pfile, "__k6_3__");
      else if (isa_flag & OPTION_MASK_ISA_3DNOW)
	def_or_undef (pfile, "__k6_3__");
      break;
    case PROCESSOR_ATHLON:
      def_or_undef (pfile, "__athlon");
      def_or_undef (pfile, "__athlon__");
      if (isa_flag & OPTION_MASK_ISA_SSE)
	def_or_undef (pfile, "__athlon_sse__");
      break;
    case PROCESSOR_K8:
      def_or_undef (pfile, "__k8");
      def_or_undef (pfile, "__k8__");
      break;
    case PROCESSOR_AMDFAM10:
      def_or_undef (pfile, "__amdfam10");
      def_or_undef (pfile, "__amdfam10__");
      break;
    case PROCESSOR_BDVER1:
      def_or_undef (pfile, "__bdver1");
      def_or_undef (pfile, "__bdver1__");
      break;
    case PROCESSOR_BDVER2:
      def_or_undef (pfile, "__bdver2");
      def_or_undef (pfile, "__bdver2__");
      break;
    case PROCESSOR_BDVER3:
      def_or_undef (pfile, "__bdver3");
      def_or_undef (pfile, "__bdver3__");
      break;
    case PROCESSOR_BDVER4:
      def_or_undef (pfile, "__bdver4");
      def_or_undef (pfile, "__bdver4__");
      break;
    case PROCESSOR_ZNVER1:
      def_or_undef (pfile, "__znver1");
      def_or_undef (pfile, "__znver1__");
      break;
    case PROCESSOR_ZNVER2:
      def_or_undef (pfile, "__znver2");
      def_or_undef (pfile, "__znver2__");
      break;
    case PROCESSOR_ZNVER3:
      def_or_undef (pfile, "__znver3");
      def_or_undef (pfile, "__znver3__");
      break;
    case PROCESSOR_ZNVER4:
      def_or_undef (pfile, "__znver4");
      def_or_undef (pfile, "__znver4__");
      break;
    case PROCESSOR_ZNVER5:
      def_or_undef (pfile, "__znver5");
      def_or_undef (pfile, "__znver5__");
      break;
    case PROCESSOR_ZNVER6:
      def_or_undef (pfile, "__znver6");
      def_or_undef (pfile, "__znver6__");
      break;
    case PROCESSOR_BTVER1:
      def_or_undef (pfile, "__btver1");
      def_or_undef (pfile, "__btver1__");
      break;
    case PROCESSOR_BTVER2:
      def_or_undef (pfile, "__btver2");
      def_or_undef (pfile, "__btver2__");
      break;
    case PROCESSOR_LUJIAZUI:
      def_or_undef (pfile, "__lujiazui");
      def_or_undef (pfile, "__lujiazui__");
      break;
    case PROCESSOR_YONGFENG:
      def_or_undef (pfile, "__yongfeng");
      def_or_undef (pfile, "__yongfeng__");
      break;
    case PROCESSOR_SHIJIDADAO:
      def_or_undef (pfile, "__shijidadao");
      def_or_undef (pfile, "__shijidadao__");
      break;
    case PROCESSOR_PENTIUM4:
      def_or_undef (pfile, "__pentium4");
      def_or_undef (pfile, "__pentium4__");
      break;
    case PROCESSOR_NOCONA:
      def_or_undef (pfile, "__nocona");
      def_or_undef (pfile, "__nocona__");
      break;
    case PROCESSOR_CORE2:
      def_or_undef (pfile, "__core2");
      def_or_undef (pfile, "__core2__");
      break;
    case PROCESSOR_NEHALEM:
      def_or_undef (pfile, "__corei7");
      def_or_undef (pfile, "__corei7__");
      def_or_undef (pfile, "__nehalem");
      def_or_undef (pfile, "__nehalem__");
      break;
    case PROCESSOR_SANDYBRIDGE:
      def_or_undef (pfile, "__corei7_avx");
      def_or_undef (pfile, "__corei7_avx__");
      def_or_undef (pfile, "__sandybridge");
      def_or_undef (pfile, "__sandybridge__");
      break;
    case PROCESSOR_HASWELL:
      def_or_undef (pfile, "__core_avx2");
      def_or_undef (pfile, "__core_avx2__");
      def_or_undef (pfile, "__haswell");
      def_or_undef (pfile, "__haswell__");
      break;
    case PROCESSOR_BONNELL:
      def_or_undef (pfile, "__atom");
      def_or_undef (pfile, "__atom__");
      def_or_undef (pfile, "__bonnell");
      def_or_undef (pfile, "__bonnell__");
      break;
    case PROCESSOR_SILVERMONT:
      def_or_undef (pfile, "__slm");
      def_or_undef (pfile, "__slm__");
      def_or_undef (pfile, "__silvermont");
      def_or_undef (pfile, "__silvermont__");
      break;
    case PROCESSOR_GOLDMONT:
      def_or_undef (pfile, "__goldmont");
      def_or_undef (pfile, "__goldmont__");
      break;
    case PROCESSOR_GOLDMONT_PLUS:
      def_or_undef (pfile, "__goldmont_plus");
      def_or_undef (pfile, "__goldmont_plus__");
      break;
    case PROCESSOR_TREMONT:
      def_or_undef (pfile, "__tremont");
      def_or_undef (pfile, "__tremont__");
      break;
    case PROCESSOR_SIERRAFOREST:
      def_or_undef (pfile, "__sierraforest");
      def_or_undef (pfile, "__sierraforest__");
      break;
    case PROCESSOR_GRANDRIDGE:
      def_or_undef (pfile, "__grandridge");
      def_or_undef (pfile, "__grandridge__");
      break;
    case PROCESSOR_CLEARWATERFOREST:
      def_or_undef (pfile, "__clearwaterforest");
      def_or_undef (pfile, "__clearwaterforest__");
      break;
      break;
    case PROCESSOR_SKYLAKE:
      def_or_undef (pfile, "__skylake");
      def_or_undef (pfile, "__skylake__");
      break;
    case PROCESSOR_SKYLAKE_AVX512:
      def_or_undef (pfile, "__skylake_avx512");
      def_or_undef (pfile, "__skylake_avx512__");
      break;
    case PROCESSOR_CANNONLAKE:
      def_or_undef (pfile, "__cannonlake");
      def_or_undef (pfile, "__cannonlake__");
      break;
    case PROCESSOR_ICELAKE_CLIENT:
      def_or_undef (pfile, "__icelake_client");
      def_or_undef (pfile, "__icelake_client__");
      break;
    case PROCESSOR_ICELAKE_SERVER:
      def_or_undef (pfile, "__icelake_server");
      def_or_undef (pfile, "__icelake_server__");
      break;
    case PROCESSOR_CASCADELAKE:
      def_or_undef (pfile, "__cascadelake");
      def_or_undef (pfile, "__cascadelake__");
      break;
    case PROCESSOR_TIGERLAKE:
      def_or_undef (pfile, "__tigerlake");
      def_or_undef (pfile, "__tigerlake__");
      break;
    case PROCESSOR_COOPERLAKE:
      def_or_undef (pfile, "__cooperlake");
      def_or_undef (pfile, "__cooperlake__");
      break;
    case PROCESSOR_SAPPHIRERAPIDS:
      def_or_undef (pfile, "__sapphirerapids");
      def_or_undef (pfile, "__sapphirerapids__");
      break;
    case PROCESSOR_GRANITERAPIDS:
      def_or_undef (pfile, "__graniterapids");
      def_or_undef (pfile, "__graniterapids__");
      break;
    case PROCESSOR_GRANITERAPIDS_D:
      def_or_undef (pfile, "__graniterapids_d");
      def_or_undef (pfile, "__graniterapids_d__");
      break;
    case PROCESSOR_ALDERLAKE:
      def_or_undef (pfile, "__alderlake");
      def_or_undef (pfile, "__alderlake__");
      break;
    case PROCESSOR_ROCKETLAKE:
      def_or_undef (pfile, "__rocketlake");
      def_or_undef (pfile, "__rocketlake__");
      break;
    case PROCESSOR_ARROWLAKE:
      def_or_undef (pfile, "__arrowlake");
      def_or_undef (pfile, "__arrowlake__");
      break;
    case PROCESSOR_ARROWLAKE_S:
      def_or_undef (pfile, "__arrowlake_s");
      def_or_undef (pfile, "__arrowlake_s__");
      break;
    case PROCESSOR_PANTHERLAKE:
      def_or_undef (pfile, "__pantherlake");
      def_or_undef (pfile, "__pantherlake__");
      break;
    case PROCESSOR_DIAMONDRAPIDS:
      def_or_undef (pfile, "__diamondrapids");
      def_or_undef (pfile, "__diamondrapids__");
      break;
    case PROCESSOR_NOVALAKE:
      def_or_undef (pfile, "__novalake");
      def_or_undef (pfile, "__novalake__");
      break;
    case PROCESSOR_C86_4G_M4:
      def_or_undef (pfile, "__c86_4g_m4");
      def_or_undef (pfile, "__c86_4g_m4__");
      break;
    case PROCESSOR_C86_4G_M6:
      def_or_undef (pfile, "__c86_4g_m6");
      def_or_undef (pfile, "__c86_4g_m6__");
      break;
    case PROCESSOR_C86_4G_M7:
      def_or_undef (pfile, "__c86_4g_m7");
      def_or_undef (pfile, "__c86_4g_m7__");
      break;
    case PROCESSOR_C86_4G_M8:
      def_or_undef (pfile, "__c86_4g_m8");
      def_or_undef (pfile, "__c86_4g_m8__");
      break;

    /* use PROCESSOR_max to not set/unset the arch macro.  */
    case PROCESSOR_max:
      break;
    case PROCESSOR_INTEL:
    case PROCESSOR_GENERIC:
      gcc_unreachable ();
    }

  /* Built-ins based on -mtune=.  */
  switch (tune)
    {
    case PROCESSOR_I386:
      def_or_undef (pfile, "__tune_i386__");
      break;
    case PROCESSOR_I486:
      def_or_undef (pfile, "__tune_i486__");
      break;
    case PROCESSOR_PENTIUM:
      def_or_undef (pfile, "__tune_i586__");
      def_or_undef (pfile, "__tune_pentium__");
      if (last_tune_char == 'x')
	def_or_undef (pfile, "__tune_pentium_mmx__");
      break;
    case PROCESSOR_PENTIUMPRO:
      def_or_undef (pfile, "__tune_i686__");
      def_or_undef (pfile, "__tune_pentiumpro__");
      switch (last_tune_char)
	{
	case '3':
	  def_or_undef (pfile, "__tune_pentium3__");
	  /* FALLTHRU */
	case '2':
	  def_or_undef (pfile, "__tune_pentium2__");
	  break;
	}
      break;
    case PROCESSOR_GEODE:
      def_or_undef (pfile, "__tune_geode__");
      break;
    case PROCESSOR_K6:
      def_or_undef (pfile, "__tune_k6__");
      if (last_tune_char == '2')
	def_or_undef (pfile, "__tune_k6_2__");
      else if (last_tune_char == '3')
	def_or_undef (pfile, "__tune_k6_3__");
      else if (isa_flag & OPTION_MASK_ISA_3DNOW)
	def_or_undef (pfile, "__tune_k6_3__");
      break;
    case PROCESSOR_ATHLON:
      def_or_undef (pfile, "__tune_athlon__");
      if (isa_flag & OPTION_MASK_ISA_SSE)
	def_or_undef (pfile, "__tune_athlon_sse__");
      break;
    case PROCESSOR_K8:
      def_or_undef (pfile, "__tune_k8__");
      break;
    case PROCESSOR_AMDFAM10:
      def_or_undef (pfile, "__tune_amdfam10__");
      break;
    case PROCESSOR_BDVER1:
      def_or_undef (pfile, "__tune_bdver1__");
      break;
    case PROCESSOR_BDVER2:
      def_or_undef (pfile, "__tune_bdver2__");
      break;
    case PROCESSOR_BDVER3:
      def_or_undef (pfile, "__tune_bdver3__");
      break;
    case PROCESSOR_BDVER4:
      def_or_undef (pfile, "__tune_bdver4__");
      break;
    case PROCESSOR_ZNVER1:
      def_or_undef (pfile, "__tune_znver1__");
      break;
    case PROCESSOR_ZNVER2:
      def_or_undef (pfile, "__tune_znver2__");
      break;
    case PROCESSOR_ZNVER3:
      def_or_undef (pfile, "__tune_znver3__");
      break;
    case PROCESSOR_ZNVER4:
      def_or_undef (pfile, "__tune_znver4__");
      break;
    case PROCESSOR_ZNVER5:
      def_or_undef (pfile, "__tune_znver5__");
      break;
    case PROCESSOR_ZNVER6:
      def_or_undef (pfile, "__tune_znver6__");
      break;
    case PROCESSOR_BTVER1:
      def_or_undef (pfile, "__tune_btver1__");
      break;
    case PROCESSOR_BTVER2:
      def_or_undef (pfile, "__tune_btver2__");
       break;
    case PROCESSOR_LUJIAZUI:
      def_or_undef (pfile, "__tune_lujiazui__");
       break;
    case PROCESSOR_YONGFENG:
      def_or_undef (pfile, "__tune_yongfeng__");
       break;
    case PROCESSOR_SHIJIDADAO:
      def_or_undef (pfile, "__tune_shijidadao__");
       break;
    case PROCESSOR_PENTIUM4:
      def_or_undef (pfile, "__tune_pentium4__");
      break;
    case PROCESSOR_NOCONA:
      def_or_undef (pfile, "__tune_nocona__");
      break;
    case PROCESSOR_CORE2:
      def_or_undef (pfile, "__tune_core2__");
      break;
    case PROCESSOR_NEHALEM:
      def_or_undef (pfile, "__tune_corei7__");
      def_or_undef (pfile, "__tune_nehalem__");
      break;
    case PROCESSOR_SANDYBRIDGE:
      def_or_undef (pfile, "__tune_corei7_avx__");
      def_or_undef (pfile, "__tune_sandybridge__");
      break;
    case PROCESSOR_HASWELL:
      def_or_undef (pfile, "__tune_core_avx2__");
      def_or_undef (pfile, "__tune_haswell__");
      break;
    case PROCESSOR_BONNELL:
      def_or_undef (pfile, "__tune_atom__");
      def_or_undef (pfile, "__tune_bonnell__");
      break;
    case PROCESSOR_SILVERMONT:
      def_or_undef (pfile, "__tune_slm__");
      def_or_undef (pfile, "__tune_silvermont__");
      break;
    case PROCESSOR_GOLDMONT:
      def_or_undef (pfile, "__tune_goldmont__");
      break;
    case PROCESSOR_GOLDMONT_PLUS:
      def_or_undef (pfile, "__tune_goldmont_plus__");
      break;
    case PROCESSOR_TREMONT:
      def_or_undef (pfile, "__tune_tremont__");
      break;
    case PROCESSOR_SIERRAFOREST:
      def_or_undef (pfile, "__tune_sierraforest__");
      break;
    case PROCESSOR_GRANDRIDGE:
      def_or_undef (pfile, "__tune_grandridge__");
      break;
    case PROCESSOR_CLEARWATERFOREST:
      def_or_undef (pfile, "__tune_clearwaterforest__");
      break;
    case PROCESSOR_SKYLAKE:
      def_or_undef (pfile, "__tune_skylake__");
      break;
    case PROCESSOR_SKYLAKE_AVX512:
      def_or_undef (pfile, "__tune_skylake_avx512__");
      break;
    case PROCESSOR_CANNONLAKE:
      def_or_undef (pfile, "__tune_cannonlake__");
      break;
    case PROCESSOR_ICELAKE_CLIENT:
      def_or_undef (pfile, "__tune_icelake_client__");
      break;
    case PROCESSOR_ICELAKE_SERVER:
      def_or_undef (pfile, "__tune_icelake_server__");
      break;
    case PROCESSOR_LAKEMONT:
      def_or_undef (pfile, "__tune_lakemont__");
      break;
    case PROCESSOR_CASCADELAKE:
      def_or_undef (pfile, "__tune_cascadelake__");
      break;
    case PROCESSOR_TIGERLAKE:
      def_or_undef (pfile, "__tune_tigerlake__");
      break;
    case PROCESSOR_COOPERLAKE:
      def_or_undef (pfile, "__tune_cooperlake__");
      break;
    case PROCESSOR_SAPPHIRERAPIDS:
      def_or_undef (pfile, "__tune_sapphirerapids__");
      break;
    case PROCESSOR_ALDERLAKE:
      def_or_undef (pfile, "__tune_alderlake__");
      break;
    case PROCESSOR_ROCKETLAKE:
      def_or_undef (pfile, "__tune_rocketlake__");
      break;
    case PROCESSOR_GRANITERAPIDS:
      def_or_undef (pfile, "__tune_graniterapids__");
      break;
    case PROCESSOR_GRANITERAPIDS_D:
      def_or_undef (pfile, "__tune_graniterapids_d__");
      break;
    case PROCESSOR_ARROWLAKE:
      def_or_undef (pfile, "__tune_arrowlake__");
      break;
    case PROCESSOR_ARROWLAKE_S:
      def_or_undef (pfile, "__tune_arrowlake_s__");
      break;
    case PROCESSOR_PANTHERLAKE:
      def_or_undef (pfile, "__tune_pantherlake__");
      break;
    case PROCESSOR_DIAMONDRAPIDS:
      def_or_undef (pfile, "__tune_diamondrapids__");
      break;
    case PROCESSOR_NOVALAKE:
      def_or_undef (pfile, "__tune_novalake__");
      break;
    case PROCESSOR_C86_4G_M4:
      def_or_undef (pfile, "__tune_c86_4g_m4__");
      break;
    case PROCESSOR_C86_4G_M6:
      def_or_undef (pfile, "__tune_c86_4g_m6__");
      break;
    case PROCESSOR_C86_4G_M7:
      def_or_undef (pfile, "__tune_c86_4g_m7__");
      break;
    case PROCESSOR_C86_4G_M8:
      def_or_undef (pfile, "__tune_c86_4g_m8__");
      break;
    case PROCESSOR_INTEL:
    case PROCESSOR_GENERIC:
      break;
    /* use PROCESSOR_max to not set/unset the tune macro.  */
    case PROCESSOR_max:
      break;
    }

  switch (ix86_cmodel)
    {
    case CM_SMALL:
    case CM_SMALL_PIC:
      def_or_undef (pfile, "__code_model_small__");
      break;
    case CM_MEDIUM:
    case CM_MEDIUM_PIC:
      def_or_undef (pfile, "__code_model_medium__");
      break;
    case CM_LARGE:
    case CM_LARGE_PIC:
      def_or_undef (pfile, "__code_model_large__");
      break;
    case CM_32:
      def_or_undef (pfile, "__code_model_32__");
      break;
    case CM_KERNEL:
      def_or_undef (pfile, "__code_model_kernel__");
      break;
    default:
      ;
    }

  if (isa_flag2 & OPTION_MASK_ISA2_WBNOINVD)
    def_or_undef (pfile, "__WBNOINVD__");
  if (isa_flag2 & OPTION_MASK_ISA2_AVX512VP2INTERSECT)
    def_or_undef (pfile, "__AVX512VP2INTERSECT__");
  if (isa_flag & OPTION_MASK_ISA_MMX)
    def_or_undef (pfile, "__MMX__");
  if (isa_flag & OPTION_MASK_ISA_3DNOW)
    def_or_undef (pfile, "__3dNOW__");
  if (isa_flag & OPTION_MASK_ISA_3DNOW_A)
    def_or_undef (pfile, "__3dNOW_A__");
  if (isa_flag & OPTION_MASK_ISA_SSE)
    def_or_undef (pfile, "__SSE__");
  if (isa_flag & OPTION_MASK_ISA_SSE2)
    def_or_undef (pfile, "__SSE2__");
  if (isa_flag & OPTION_MASK_ISA_SSE3)
    def_or_undef (pfile, "__SSE3__");
  if (isa_flag & OPTION_MASK_ISA_SSSE3)
    def_or_undef (pfile, "__SSSE3__");
  if (isa_flag & OPTION_MASK_ISA_SSE4_1)
    def_or_undef (pfile, "__SSE4_1__");
  if (isa_flag & OPTION_MASK_ISA_SSE4_2)
    def_or_undef (pfile, "__SSE4_2__");
  if (isa_flag & OPTION_MASK_ISA_AES)
    def_or_undef (pfile, "__AES__");
  if (isa_flag & OPTION_MASK_ISA_SHA)
    def_or_undef (pfile, "__SHA__");
  if (isa_flag & OPTION_MASK_ISA_PCLMUL)
    def_or_undef (pfile, "__PCLMUL__");
  if (isa_flag & OPTION_MASK_ISA_AVX)
    def_or_undef (pfile, "__AVX__");
  if (isa_flag & OPTION_MASK_ISA_AVX2)
    def_or_undef (pfile, "__AVX2__");
  if (isa_flag & OPTION_MASK_ISA_AVX512F)
    def_or_undef (pfile, "__AVX512F__");
  if (isa_flag & OPTION_MASK_ISA_AVX512CD)
    def_or_undef (pfile, "__AVX512CD__");
  if (isa_flag & OPTION_MASK_ISA_AVX512DQ)
    def_or_undef (pfile, "__AVX512DQ__");
  if (isa_flag & OPTION_MASK_ISA_AVX512BW)
    def_or_undef (pfile, "__AVX512BW__");
  if (isa_flag & OPTION_MASK_ISA_AVX512VL)
    {
      def_or_undef (pfile, "__AVX512VL__");
      def_or_undef (pfile, "__EVEX256__");
    }
  if (isa_flag & OPTION_MASK_ISA_AVX512VBMI)
    def_or_undef (pfile, "__AVX512VBMI__");
  if (isa_flag & OPTION_MASK_ISA_AVX512IFMA)
    def_or_undef (pfile, "__AVX512IFMA__");
  if (isa_flag & OPTION_MASK_ISA_AVX512VBMI2)
    def_or_undef (pfile, "__AVX512VBMI2__");
  if (isa_flag & OPTION_MASK_ISA_AVX512VNNI)
    def_or_undef (pfile, "__AVX512VNNI__");
  if (isa_flag2 & OPTION_MASK_ISA2_PCONFIG)
    def_or_undef (pfile, "__PCONFIG__");
  if (isa_flag2 & OPTION_MASK_ISA2_SGX)
    def_or_undef (pfile, "__SGX__");
  if (isa_flag & OPTION_MASK_ISA_AVX512BITALG)
    def_or_undef (pfile, "__AVX512BITALG__");
  if (isa_flag & OPTION_MASK_ISA_AVX512VPOPCNTDQ)
    def_or_undef (pfile, "__AVX512VPOPCNTDQ__");
  if (isa_flag & OPTION_MASK_ISA_FMA)
    def_or_undef (pfile, "__FMA__");
  if (isa_flag & OPTION_MASK_ISA_RTM)
    def_or_undef (pfile, "__RTM__");
  if (isa_flag & OPTION_MASK_ISA_SSE4A)
    def_or_undef (pfile, "__SSE4A__");
  if (isa_flag & OPTION_MASK_ISA_FMA4)
    def_or_undef (pfile, "__FMA4__");
  if (isa_flag & OPTION_MASK_ISA_XOP)
    def_or_undef (pfile, "__XOP__");
  if (isa_flag & OPTION_MASK_ISA_LWP)
    def_or_undef (pfile, "__LWP__");
  if (isa_flag & OPTION_MASK_ISA_ABM)
    def_or_undef (pfile, "__ABM__");
  if (isa_flag & OPTION_MASK_ISA_BMI)
    def_or_undef (pfile, "__BMI__");
  if (isa_flag & OPTION_MASK_ISA_BMI2)
    def_or_undef (pfile, "__BMI2__");
  if (isa_flag & OPTION_MASK_ISA_LZCNT)
    def_or_undef (pfile, "__LZCNT__");
  if (isa_flag & OPTION_MASK_ISA_TBM)
    def_or_undef (pfile, "__TBM__");
  if (isa_flag & OPTION_MASK_ISA_CRC32)
    def_or_undef (pfile, "__CRC32__");
  if (isa_flag & OPTION_MASK_ISA_POPCNT)
    def_or_undef (pfile, "__POPCNT__");
  if (isa_flag & OPTION_MASK_ISA_FSGSBASE)
    def_or_undef (pfile, "__FSGSBASE__");
  if (isa_flag & OPTION_MASK_ISA_RDRND)
    def_or_undef (pfile, "__RDRND__");
  if (isa_flag & OPTION_MASK_ISA_F16C)
    def_or_undef (pfile, "__F16C__");
  if (isa_flag & OPTION_MASK_ISA_RDSEED)
    def_or_undef (pfile, "__RDSEED__");
  if (isa_flag & OPTION_MASK_ISA_PRFCHW)
    def_or_undef (pfile, "__PRFCHW__");
  if (isa_flag & OPTION_MASK_ISA_ADX)
    def_or_undef (pfile, "__ADX__");
  if (isa_flag & OPTION_MASK_ISA_FXSR)
    def_or_undef (pfile, "__FXSR__");
  if (isa_flag & OPTION_MASK_ISA_XSAVE)
    def_or_undef (pfile, "__XSAVE__");
  if (isa_flag & OPTION_MASK_ISA_XSAVEOPT)
    def_or_undef (pfile, "__XSAVEOPT__");
  if ((fpmath & FPMATH_SSE) && (isa_flag & OPTION_MASK_ISA_SSE))
    def_or_undef (pfile, "__SSE_MATH__");
  if ((fpmath & FPMATH_SSE) && (isa_flag & OPTION_MASK_ISA_SSE2))
    def_or_undef (pfile, "__SSE2_MATH__");
  if (isa_flag & OPTION_MASK_ISA_CLFLUSHOPT)
    def_or_undef (pfile, "__CLFLUSHOPT__");
  if (isa_flag2 & OPTION_MASK_ISA2_CLZERO)
    def_or_undef (pfile, "__CLZERO__");
  if (isa_flag & OPTION_MASK_ISA_XSAVEC)
    def_or_undef (pfile, "__XSAVEC__");
  if (isa_flag & OPTION_MASK_ISA_XSAVES)
    def_or_undef (pfile, "__XSAVES__");
  if (isa_flag & OPTION_MASK_ISA_CLWB)
    def_or_undef (pfile, "__CLWB__");
  if (isa_flag2 & OPTION_MASK_ISA2_MWAITX)
    def_or_undef (pfile, "__MWAITX__");
  if (isa_flag & OPTION_MASK_ISA_PKU)
    def_or_undef (pfile, "__PKU__");
  if (isa_flag2 & OPTION_MASK_ISA2_RDPID)
    def_or_undef (pfile, "__RDPID__");
  if (isa_flag & OPTION_MASK_ISA_GFNI)
    def_or_undef (pfile, "__GFNI__");
  if ((isa_flag & OPTION_MASK_ISA_SHSTK))
    def_or_undef (pfile, "__SHSTK__");
  if (isa_flag2 & OPTION_MASK_ISA2_VAES)
    def_or_undef (pfile, "__VAES__");
  if (isa_flag & OPTION_MASK_ISA_VPCLMULQDQ)
    def_or_undef (pfile, "__VPCLMULQDQ__");
  if (isa_flag & OPTION_MASK_ISA_MOVDIRI)
    def_or_undef (pfile, "__MOVDIRI__");
  if (isa_flag2 & OPTION_MASK_ISA2_MOVDIR64B)
    def_or_undef (pfile, "__MOVDIR64B__");
  if (isa_flag2 & OPTION_MASK_ISA2_WAITPKG)
    def_or_undef (pfile, "__WAITPKG__");
  if (isa_flag2 & OPTION_MASK_ISA2_CLDEMOTE)
    def_or_undef (pfile, "__CLDEMOTE__");
  if (isa_flag2 & OPTION_MASK_ISA2_SERIALIZE)
    def_or_undef (pfile, "__SERIALIZE__");
  if (isa_flag2 & OPTION_MASK_ISA2_PTWRITE)
    def_or_undef (pfile, "__PTWRITE__");
  if (isa_flag2 & OPTION_MASK_ISA2_AVX512BF16)
    def_or_undef (pfile, "__AVX512BF16__");
  if (isa_flag2 & OPTION_MASK_ISA2_AVX512FP16)
    def_or_undef (pfile, "__AVX512FP16__");
  if (TARGET_MMX_WITH_SSE)
    def_or_undef (pfile, "__MMX_WITH_SSE__");
  if (isa_flag2 & OPTION_MASK_ISA2_ENQCMD)
    def_or_undef (pfile, "__ENQCMD__");
  if (isa_flag2 & OPTION_MASK_ISA2_TSXLDTRK)
    def_or_undef (pfile, "__TSXLDTRK__");
  if (isa_flag2 & OPTION_MASK_ISA2_AMX_TILE)
    def_or_undef (pfile, "__AMX_TILE__");
  if (isa_flag2 & OPTION_MASK_ISA2_AMX_INT8)
    def_or_undef (pfile, "__AMX_INT8__");
  if (isa_flag2 & OPTION_MASK_ISA2_AMX_BF16)
    def_or_undef (pfile, "__AMX_BF16__");
  if (isa_flag & OPTION_MASK_ISA_SAHF)
    def_or_undef (pfile, "__LAHF_SAHF__");
  if (isa_flag2 & OPTION_MASK_ISA2_MOVBE)
    def_or_undef (pfile, "__MOVBE__");
  if (isa_flag2 & OPTION_MASK_ISA2_UINTR)
    def_or_undef (pfile, "__UINTR__");
  if (isa_flag2 & OPTION_MASK_ISA2_HRESET)
    def_or_undef (pfile, "__HRESET__");
  if (isa_flag2 & OPTION_MASK_ISA2_KL)
    def_or_undef (pfile, "__KL__");
  if (isa_flag2 & OPTION_MASK_ISA2_WIDEKL)
    def_or_undef (pfile, "__WIDEKL__");
  if (isa_flag2 & OPTION_MASK_ISA2_AVXVNNI)
    def_or_undef (pfile, "__AVXVNNI__");
  if (isa_flag2 & OPTION_MASK_ISA2_AVXIFMA)
    def_or_undef (pfile, "__AVXIFMA__");
  if (isa_flag2 & OPTION_MASK_ISA2_AVXVNNIINT8)
    def_or_undef (pfile, "__AVXVNNIINT8__");
  if (isa_flag2 & OPTION_MASK_ISA2_AVXNECONVERT)
    def_or_undef (pfile, "__AVXNECONVERT__");
  if (isa_flag2 & OPTION_MASK_ISA2_CMPCCXADD)
    def_or_undef (pfile, "__CMPCCXADD__");
  if (isa_flag2 & OPTION_MASK_ISA2_AMX_FP16)
    def_or_undef (pfile, "__AMX_FP16__");
  if (isa_flag2 & OPTION_MASK_ISA2_PREFETCHI)
    def_or_undef (pfile, "__PREFETCHI__");
  if (isa_flag2 & OPTION_MASK_ISA2_RAOINT)
    def_or_undef (pfile, "__RAOINT__");
  if (isa_flag2 & OPTION_MASK_ISA2_AMX_COMPLEX)
    def_or_undef (pfile, "__AMX_COMPLEX__");
  if (isa_flag2 & OPTION_MASK_ISA2_AVXVNNIINT16)
    def_or_undef (pfile, "__AVXVNNIINT16__");
  if (isa_flag2 & OPTION_MASK_ISA2_SM3)
    def_or_undef (pfile, "__SM3__");
  if (isa_flag2 & OPTION_MASK_ISA2_SHA512)
    def_or_undef (pfile, "__SHA512__");
  if (isa_flag2 & OPTION_MASK_ISA2_SM4)
    def_or_undef (pfile, "__SM4__");
  if (isa_flag2 & OPTION_MASK_ISA2_USER_MSR)
    def_or_undef (pfile, "__USER_MSR__");
  if (isa_flag2 & OPTION_MASK_ISA2_AVX10_1)
    def_or_undef (pfile, "__AVX10_1__");
  if (isa_flag2 & OPTION_MASK_ISA2_APX_F)
    def_or_undef (pfile, "__APX_F__");
  if (ix86_apx_inline_asm_use_gpr32)
    def_or_undef (pfile, "__APX_INLINE_ASM_USE_GPR32__");
  if (isa_flag2 & OPTION_MASK_ISA2_AVX10_2)
    def_or_undef (pfile, "__AVX10_2__");
  if (isa_flag2 & OPTION_MASK_ISA2_AMX_AVX512)
    def_or_undef (pfile, "__AMX_AVX512__");
  if (isa_flag2 & OPTION_MASK_ISA2_AMX_FP8)
    def_or_undef (pfile, "__AMX_FP8__");
  if (isa_flag2 & OPTION_MASK_ISA2_MOVRS)
    def_or_undef (pfile, "__MOVRS__");
  if (isa_flag2 & OPTION_MASK_ISA2_AMX_MOVRS)
    def_or_undef (pfile, "__AMX_MOVRS__");
  if (isa_flag2 & OPTION_MASK_ISA2_AVX512BMM)
    def_or_undef (pfile, "__AVX512BMM__");
  if (isa_flag2 & OPTION_MASK_ISA2_AVX10V2AUX)
    def_or_undef (pfile, "__AVX10V2AUX__");
  if (isa_flag2 & OPTION_MASK_ISA2_ACEV1)
    def_or_undef (pfile, "__ACEV1__");
  if (TARGET_IAMCU)
    {
      def_or_undef (pfile, "__iamcu");
      def_or_undef (pfile, "__iamcu__");
    }
}

/* Emit the full set of x86 preprocessor macros: architecture identity,
   float-size/soft-float tags, HLE/CET/segment macros,
   __GCC_ASM_FLAG_OUTPUTS__, and (via ix86_cpu_cpp_builtins_internal) the
   arch/tune/code-model/ISA-feature macros.

   DEFINE_USER_NAMESPACE enables unqualified aliases like bare "i386"
   alongside "__i386__".  C/C++ pass !flag_iso; Fortran passes false.  */

void
ix86_cpu_cpp_builtins (cpp_reader *pfile, bool define_user_namespace)
{
  /* 32/64-bit won't change with target specific options, so do the assert
     and builtin_define_std calls here.  */
  if (TARGET_64BIT)
    {
      cpp_assert (pfile, "cpu=x86_64");
      cpp_assert (pfile, "machine=x86_64");
      cpp_define (pfile, "__amd64");
      cpp_define (pfile, "__amd64__");
      cpp_define (pfile, "__x86_64");
      cpp_define (pfile, "__x86_64__");
      if (TARGET_X32)
	{
	  cpp_define (pfile, "_ILP32");
	  cpp_define (pfile, "__ILP32__");
	}
    }
  else
    {
      cpp_assert (pfile, "cpu=i386");
      cpp_assert (pfile, "machine=i386");
      ix86_cpp_builtin_define_std (pfile, "i386", define_user_namespace);
      cpp_define (pfile, "_ILP32");
      cpp_define (pfile, "__ILP32__");
    }

  if (!TARGET_80387)
    cpp_define (pfile, "_SOFT_FLOAT");

  if (TARGET_LONG_DOUBLE_64)
    cpp_define (pfile, "__LONG_DOUBLE_64__");

  if (TARGET_LONG_DOUBLE_128)
    cpp_define (pfile, "__LONG_DOUBLE_128__");

  cpp_define_formatted (pfile, "__SIZEOF_FLOAT80__=%d",
			GET_MODE_SIZE (XFmode));

  cpp_define (pfile, "__SIZEOF_FLOAT128__=16");

  cpp_define_formatted (pfile, "__ATOMIC_HLE_ACQUIRE=%d", IX86_HLE_ACQUIRE);
  cpp_define_formatted (pfile, "__ATOMIC_HLE_RELEASE=%d", IX86_HLE_RELEASE);

  cpp_define (pfile, "__GCC_ASM_FLAG_OUTPUTS__");

  ix86_cpu_cpp_builtins_internal (pfile,
				  ix86_isa_flags,
				  ix86_isa_flags2,
				  ix86_arch,
				  ix86_tune,
				  ix86_fpmath,
				  cpp_define);

  cpp_define (pfile, "__SEG_FS");
  cpp_define (pfile, "__SEG_GS");

  if (flag_cf_protection != CF_NONE)
    cpp_define_formatted (pfile, "__CET__=%d", flag_cf_protection & ~CF_SET);
}
