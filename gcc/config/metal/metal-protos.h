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
extern void metal_declare_function_name (FILE *, const char *, tree);
extern void metal_declare_object_name (FILE *, const char *, const_tree);
extern void metal_output_aligned_decl (FILE *, const char *, const_tree,
					HOST_WIDE_INT, unsigned int);
extern void metal_function_end (FILE *);
extern bool metal_expand_cpymem (rtx *, bool);

#endif /* GCC_METAL_PROTOS_H */
