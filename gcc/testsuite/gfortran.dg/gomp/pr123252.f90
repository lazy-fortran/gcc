! { dg-do compile }
! { dg-options "-fopenmp -fdump-tree-omplower" }
!
! PR fortran/123252
! Scalar siblings of mapped derived-type array components must follow the
! component's enter/exit data mapping semantics for OpenMP as well.

program pr123252
  implicit none

  type :: record_t
    integer :: count
    logical :: enabled
    real, allocatable :: samples(:)
  end type record_t

  type(record_t) :: copied
  type(record_t) :: created

  allocate(copied%samples(6), created%samples(4))

  !$omp target enter data map(to:copied%samples)
  !$omp target exit data map(delete:copied%samples)
  !$omp target enter data map(alloc:created%samples)
  !$omp target exit data map(release:created%samples)
end program pr123252

! { dg-final { scan-tree-dump "map\\(to:copied\\.count" "omplower" } }
! { dg-final { scan-tree-dump "map\\(to:copied\\.enabled" "omplower" } }
! { dg-final { scan-tree-dump "map\\(alloc:created\\.count" "omplower" } }
! { dg-final { scan-tree-dump "map\\(alloc:created\\.enabled" "omplower" } }
! { dg-final { scan-tree-dump "map\\(delete:copied\\.count" "omplower" } }
! { dg-final { scan-tree-dump "map\\(release:created\\.count" "omplower" } }
