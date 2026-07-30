! { dg-do preprocess { target { i?86-*-* x86_64-*-* } } }
! { dg-options "-cpp" }
!
! PR fortran/42954 - target macros missing in gfortran -cpp

#if !defined(__i386__) && !defined(__i386) \
    && !defined(__x86_64__) && !defined(__x86_64) \
    && !defined(__amd64__) && !defined(__amd64)
# error x86 target macros not defined
#endif
