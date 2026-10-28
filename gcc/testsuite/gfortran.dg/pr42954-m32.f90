! { dg-do preprocess { target { i?86-*-* x86_64-*-* } } }
! { dg-require-effective-target ia32 }
! { dg-options "-cpp -m32" }
!
! PR fortran/42954 - 32-bit x86 target macros from Fortran

#ifndef __i386__
# error __i386__ not defined with -m32
#endif

#ifndef __i386
# error __i386 not defined with -m32
#endif

#ifdef i386
# error bare i386 should not be defined with -m32
#endif

#ifndef _ILP32
# error _ILP32 not defined with -m32
#endif

#ifndef __ILP32__
# error __ILP32__ not defined with -m32
#endif

#ifdef __x86_64__
# error __x86_64__ unexpectedly defined with -m32
#endif

#ifdef __amd64__
# error __amd64__ unexpectedly defined with -m32
#endif
