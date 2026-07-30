! { dg-do preprocess { target { i?86-*-* x86_64-*-* } } }
! { dg-options "-cpp -mfma4 -mno-fma" }
!
! PR fortran/42954 - FMA4 feature macro from Fortran

#ifndef __FMA4__
# error __FMA4__ not defined
#endif

#ifdef __FMA__
# error __FMA__ unexpectedly defined with -mno-fma
#endif
