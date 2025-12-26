/* Subroutines for the C front end on the Apple Metal architecture.
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
#include "target.h"
#include "c-family/c-common.h"
#include "tm_p.h"
#include "c-family/c-pragma.h"

/* Function to tell the preprocessor about the defines for the target.  */
void
metal_cpu_cpp_builtins (void)
{
  cpp_assert (parse_in, "machine=metal");
  cpp_assert (parse_in, "cpu=metal");
  cpp_define (parse_in, "__metal__");
  cpp_define (parse_in, "__METAL__");
}

