! { dg-do run }
! PR fortran/128397
!
! Automatic objects whose bounds depend on other automatic arrays must be
! set up in declaration order, also when a contained procedure references
! the dependent object first.

module m
  implicit none
contains
  subroutine s (k)
    integer, intent(in) :: k
    integer :: w(2*k)
    real :: x(size (w))
    real :: y(ubound (x, 1) + 1)
    character(len=size (y)) :: c
    x = 0
    y = 0
    if (size (w) /= 2*k) stop 1
    if (size (x) /= 2*k) stop 2
    if (size (y) /= 2*k + 1) stop 3
    if (len (c) /= 2*k + 1) stop 4
    call fill ()
    if (any (x /= 1.0)) stop 5
    if (any (y /= 2.0)) stop 6
    if (c /= repeat ('z', 2*k + 1)) stop 7
  contains
    subroutine fill ()
      c = repeat ('z', len (c))
      y = 2.0
      x = 1.0
    end subroutine
  end subroutine
end module

  use m
  call s (4)
  call s (1)
end
