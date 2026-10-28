/* VMS support needed only by Fortran frontends.
   Copyright (C) 2012-2026 Free Software Foundation, Inc.

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
#include "tm.h"
#include "memmodel.h"
#include "tm_p.h"

void
vms_c_register_includes (const char *sysroot ATTRIBUTE_UNUSED,
                         const char *iprefix ATTRIBUTE_UNUSED,
			 int stdinc ATTRIBUTE_UNUSED)
{
  /* No-op for fortran.  */
}

/* TARGET_OS_CPP_BUILTINS in vms.h uses these to define __CRTL_VER and
   __VMS_VER.  The Fortran preprocessor expands that hook too, so provide
   the same defaults as vms-c.cc.  */

int
vms_c_get_crtl_ver (void)
{
  return VMS_DEFAULT_CRTL_VER;
}

int
vms_c_get_vms_ver (void)
{
  return VMS_DEFAULT_VMS_VER;
}
