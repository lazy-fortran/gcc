/* Target definitions for AArch64 running on Darwin.

   This file is part of GCC.

   GCC is free software; you can redistribute it and/or modify it
   under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 3, or (at your option)
   any later version.

   GCC is distributed in the hope that it will be useful, but WITHOUT ANY
   WARRANTY; without even the implied warranty of MERCHANTABILITY or
   FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License
   for more details.

   You should have received a copy of the GNU General Public License
   along with GCC; see the file COPYING3.  If not see
   <http://www.gnu.org/licenses/>.  */

/* Enable Mach-O bits in generic AArch64 code.  */
#undef TARGET_MACHO
#define TARGET_MACHO 1

/* Identify Darwin/arm64 in shared Darwin code.  */
#undef DARWIN_ARM64
#define DARWIN_ARM64 1

/* Darwin/arm64 is always 64-bit.  */
#undef TARGET_64BIT
#define TARGET_64BIT 1

#undef PTRDIFF_TYPE
#define PTRDIFF_TYPE "long int"

/* Darwin/arm64 uses 64-bit long double.  */
#undef TARGET_LONG_DOUBLE_128
#define TARGET_LONG_DOUBLE_128 0

/* First available SYMBOL_REF flag bit for use by Darwin targets.  */
#define SYMBOL_FLAG_SUBT_DEP (SYMBOL_FLAG_MACH_DEP)

/* Non-PIE executables are forbidden by the Darwin/arm64 security model; drop
   these options rather than passing them to ld64.  */
#undef DARWIN_NOPIE_SPEC
#define DARWIN_NOPIE_SPEC \
" %<no-pie %<fno-pie %<fno-PIE "

/* The generic machopic path relies on target-specific UNSPECs/insns that are
   not yet implemented for AArch64 in this tree.  Use direct addressing for
   PIC and provide no-op fallbacks so that generic Darwin code can build.  */
#undef MACHOPIC_INDIRECT
#define MACHOPIC_INDIRECT 0

#ifndef UNSPEC_MACHOPIC_OFFSET
#define UNSPEC_MACHOPIC_OFFSET 0
#endif

#ifndef gen_macho_high
#define gen_macho_high(mode, reg, sym) (reg)
#endif

#ifndef gen_macho_low
#define gen_macho_low(mode, reg, hi, sym) (reg)
#endif

/* Mach-O does not use ELF-style .type/.set directives.  Provide no-op
   definitions where the generic AArch64 code expects them.  */
#ifndef ASM_OUTPUT_TYPE_DIRECTIVE
#define ASM_OUTPUT_TYPE_DIRECTIVE(STREAM, NAME, TYPE) \
  do { (void) (STREAM); (void) (NAME); (void) (TYPE); } while (0)
#endif

#ifndef ASM_OUTPUT_DEF
#define ASM_OUTPUT_DEF(FILE, ALIAS, NAME) \
  do { (void) (FILE); (void) (ALIAS); (void) (NAME); } while (0)
#endif

#ifndef ASM_DECLARE_FUNCTION_SIZE
#define ASM_DECLARE_FUNCTION_SIZE(FILE, NAME, DECL) \
  do { (void) (FILE); (void) (NAME); (void) (DECL); } while (0)
#endif

/* The AArch64 backend only defines unaligned integer output ops for a subset
   of object formats.  Darwin emits some unaligned multi-byte data in unwind
   tables; if the unaligned ops are unset, GCC falls back to byte-splitting,
   which is not possible for symbolic expressions and triggers an ICE.
   Use the same directives for aligned and unaligned cases.  */
#undef TARGET_ASM_UNALIGNED_HI_OP
#define TARGET_ASM_UNALIGNED_HI_OP TARGET_ASM_ALIGNED_HI_OP
#undef TARGET_ASM_UNALIGNED_SI_OP
#define TARGET_ASM_UNALIGNED_SI_OP TARGET_ASM_ALIGNED_SI_OP
#undef TARGET_ASM_UNALIGNED_DI_OP
#define TARGET_ASM_UNALIGNED_DI_OP TARGET_ASM_ALIGNED_DI_OP
