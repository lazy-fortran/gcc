! { dg-do compile }
! { dg-options "-O2 -fopenacc -fdump-tree-omplower" }
!
! PR fortran/123252
! When OpenACC enter/exit data maps an allocatable component, scalar
! fields of the parent derived type are also mapped/unmapped.

program pr123252
  implicit none

  type :: payload_t
    logical :: flag
    integer :: n
    real(8), allocatable :: values(:)
    character(3) :: tag
  end type payload_t

  type :: wrapper_t
    type(payload_t) :: payload
  end type wrapper_t

  type(wrapper_t) :: copied
  type(payload_t) :: created

  allocate(copied%payload%values(8), created%values(5))

  !$acc enter data copyin(copied%payload%values)
  !$acc exit data delete(copied%payload%values)
  !$acc enter data create(created%values)
  !$acc exit data delete(created%values)
end program pr123252

! Copyin maps nested scalar fields, including characters, to the device.
! { dg-final { scan-tree-dump "map\\(to:copied\\.payload\\.flag" "omplower" } }
! { dg-final { scan-tree-dump "map\\(to:copied\\.payload\\.n" "omplower" } }
! { dg-final { scan-tree-dump "map\\(to:copied\\.payload\\.tag" "omplower" } }

! Create allocates scalar siblings without copying their host values.
! { dg-final { scan-tree-dump "map\\(alloc:created\\.flag" "omplower" } }
! { dg-final { scan-tree-dump "map\\(alloc:created\\.n" "omplower" } }

! Exit data uses the component's release semantics for its scalar siblings.
! { dg-final { scan-tree-dump "map\\(release:created\\.flag" "omplower" } }
! { dg-final { scan-tree-dump "map\\(release:created\\.n" "omplower" } }
