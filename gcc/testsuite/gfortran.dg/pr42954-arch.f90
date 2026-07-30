! { dg-do preprocess { target { i?86-*-* x86_64-*-* } } }
! { dg-options "-cpp -march=haswell -mtune=haswell" }
!
! PR fortran/42954 - arch/tune macros exercised from Fortran

#ifndef __haswell
# error __haswell not defined
#endif

#ifndef __haswell__
# error __haswell__ not defined
#endif

#ifndef __core_avx2
# error __core_avx2 not defined
#endif

#ifndef __core_avx2__
# error __core_avx2__ not defined
#endif

#ifndef __tune_haswell__
# error __tune_haswell__ not defined
#endif

#ifndef __tune_core_avx2__
# error __tune_core_avx2__ not defined
#endif
