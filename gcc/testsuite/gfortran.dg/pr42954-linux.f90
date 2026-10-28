! { dg-do preprocess { target *-*-linux* } }
! { dg-options "-cpp" }
!
! PR fortran/42954 - target macros missing in gfortran -cpp

#ifndef __linux__
# error __linux__ not defined
#endif

#ifndef __linux
# error __linux not defined
#endif

#ifndef __unix__
# error __unix__ not defined
#endif

#ifndef __unix
# error __unix not defined
#endif

#ifdef linux
# error bare linux should not be defined
#endif

#ifdef unix
# error bare unix should not be defined
#endif

#ifndef __ELF__
# error __ELF__ not defined
#endif
