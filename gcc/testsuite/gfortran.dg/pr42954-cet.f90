! { dg-do preprocess { target { i?86-*-* x86_64-*-* } } }
! { dg-options "-cpp -fcf-protection=full" }
!
! PR fortran/42954 - Control-flow protection macro from Fortran

#ifndef __CET__
# error __CET__ not defined
#endif

#if __CET__ != 3
# error __CET__ has wrong value (expected 3 for branch+return)
#endif
