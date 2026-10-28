! { dg-do preprocess { target { i?86-*-* x86_64-*-* } } }
! { dg-require-effective-target ia32 }
! { dg-options "-cpp -m32 -mno-80387 -mno-mmx -mno-sse" }
!
! PR fortran/42954 - _SOFT_FLOAT target macro from Fortran

#ifndef _SOFT_FLOAT
# error _SOFT_FLOAT not defined with -mno-80387
#endif

#ifdef __SSE__
# error __SSE__ unexpectedly defined with -mno-sse
#endif
