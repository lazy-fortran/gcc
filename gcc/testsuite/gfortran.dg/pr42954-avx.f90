! { dg-do preprocess { target { i?86-*-* x86_64-*-* } } }
! { dg-options "-cpp -mavx -mavx2 -mfma" }
!
! PR fortran/42954 - AVX/AVX2/FMA feature macros from Fortran

#ifndef __SSE__
# error __SSE__ not defined
#endif

#ifndef __AVX__
# error __AVX__ not defined
#endif

#ifndef __AVX2__
# error __AVX2__ not defined
#endif

#ifndef __FMA__
# error __FMA__ not defined
#endif
