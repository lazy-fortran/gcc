! { dg-do compile }
! { dg-options "-O2 -fopenacc -fdump-tree-omplower" }
!
! PR fortran/123252
! When OpenACC enter/exit data maps an allocatable component, scalar
! fields of the parent derived type are also mapped/unmapped.  This
! ensures scalars are available on device and mapping state stays
! consistent for remapping after reallocation.

program pr123252
  implicit none

  type :: container_t
    real(8), allocatable :: arr(:)
    logical :: flag
    integer :: n
  end type container_t

  type(container_t) :: c

  c%n = 8
  c%flag = .true.
  allocate(c%arr(c%n))

  !$acc enter data copyin(c%arr)
  !$acc exit data delete(c%arr)
end program pr123252

! Verify scalar fields are mapped on enter data.
! { dg-final { scan-tree-dump "map\\(to:c\\.flag" "omplower" } }
! { dg-final { scan-tree-dump "map\\(to:c\\.n" "omplower" } }

! Verify scalar fields are unmapped on exit data.
! { dg-final { scan-tree-dump "map\\(delete:c\\.flag" "omplower" } }
! { dg-final { scan-tree-dump "map\\(delete:c\\.n" "omplower" } }
