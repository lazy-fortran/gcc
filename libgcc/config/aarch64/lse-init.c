/* Out-of-line LSE atomics for AArch64 architecture, Init.
   Copyright (C) 2019-2026 Free Software Foundation, Inc.
   Contributed by Linaro Ltd.

This file is part of GCC.

GCC is free software; you can redistribute it and/or modify it under
the terms of the GNU General Public License as published by the Free
Software Foundation; either version 3, or (at your option) any later
version.

GCC is distributed in the hope that it will be useful, but WITHOUT ANY
WARRANTY; without even the implied warranty of MERCHANTABILITY or
FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License
for more details.

Under Section 7 of GPL version 3, you are granted additional
permissions described in the GCC Runtime Library Exception, version
3.1, as published by the Free Software Foundation.

You should have received a copy of the GNU General Public License and
a copy of the GCC Runtime Library Exception along with this program;
see the files COPYING3 and COPYING.RUNTIME respectively.  If not, see
<http://www.gnu.org/licenses/>.  */

#include "auto-target.h"

#ifdef __NetBSD__
# include <stdio.h>
# include <sys/types.h>
# include <sys/sysctl.h>
# include <machine/armreg.h>
#endif

/* Define the symbol gating the LSE implementations.  */
_Bool __aarch64_have_lse_atomics
  __attribute__((visibility("hidden"), nocommon));

/* Gate availability of __getauxval on glibc.  All AArch64-supporting glibc
   versions support it.  */
#ifdef __gnu_linux__

# define AT_HWCAP	16
# define HWCAP_ATOMICS	(1 << 8)

unsigned long int __getauxval (unsigned long int);

/* Use a higher priority to ensure it runs before user constructors
   and library constructors with priority 100. */
static void __attribute__((constructor (90)))
init_have_lse_atomics (void)
{
  unsigned long hwcap = __getauxval (AT_HWCAP);
  __aarch64_have_lse_atomics = (hwcap & HWCAP_ATOMICS) != 0;
}

#elif defined(__NetBSD__)

/* Use a higher priority to ensure it runs before user constructors
   and library constructors with priority 100.  */
static void __attribute__((constructor (90)))
init_have_lse_atomics (void)
{
  unsigned int ncpu = 0;
  size_t len = sizeof (ncpu);

  if (sysctlbyname ("hw.ncpu", &ncpu, &len, 0, 0) != 0 || ncpu == 0)
    return;

  for (unsigned int cpu = 0; cpu < ncpu; ++cpu)
    {
      char name[sizeof ("machdep.cpu4294967295.cpu_id")];
      struct aarch64_sysctl_cpu_id id;

      snprintf (name, sizeof (name), "machdep.cpu%u.cpu_id", cpu);
      len = sizeof (id);
      if (sysctlbyname (name, &id, &len, 0, 0) != 0
	  || __SHIFTOUT (id.ac_aa64isar0, ID_AA64ISAR0_EL1_ATOMIC)
	     < ID_AA64ISAR0_EL1_ATOMIC_SWP)
	return;
    }

  __aarch64_have_lse_atomics = 1;
}

#endif
