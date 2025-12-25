/* Prototypes for Apple Metal target functions.
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

#ifndef GCC_METAL_PROTOS_H
#define GCC_METAL_PROTOS_H

extern void metal_cpu_cpp_builtins (void);
extern void metal_print_operand (FILE *, rtx, int);
extern void metal_print_operand_address (FILE *, rtx);

/* MSL code generation functions.  */
extern const char *metal_output_mov_insn (rtx, rtx);
extern const char *metal_output_binop_insn (const char *, rtx *);
extern const char *metal_output_unop_insn (const char *, rtx *);
extern const char *metal_output_cmp_insn (const char *, rtx *);
extern const char *metal_output_load_insn (rtx, rtx);
extern const char *metal_output_store_insn (rtx, rtx);

#endif /* GCC_METAL_PROTOS_H */
