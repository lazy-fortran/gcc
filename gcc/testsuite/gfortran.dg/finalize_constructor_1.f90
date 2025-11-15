! { dg-do run }
! { dg-output " constructor: *1(\n|\r\n|\r)" }
! { dg-output " finalizer: *2(\n|\r\n|\r)" }
! PR fortran/121472
! Test that derived types with finalizers and constructor interfaces compile
! without ICE and produce ISO F2018 compliant finalization behavior.
!
! ISO/IEC 1539-1:2018 Section 7.5.6.3 paragraph 3:
! "Finalization occurs ... (3) when an intrinsic assignment statement is
! executed and ... the variable is of a finalizable type, the variable is
! finalized after evaluation of expr and before the definition of the variable."
!
! For the statement: obj = container_t()
!
! Expected finalization count: 2
!   (1) Function result from container_t() after assignment per 7.5.6.3(3)
!   (2) Variable obj at end of scope per 7.5.6.3(1)
!
! Expected constructor count: 1
!   (1) Call to container_constructor via container_t()
!
! This used to trigger an internal compiler error in gimplify_expr because
! finalization was attempted on unevaluated CALL_EXPR nodes for types with
! non-allocatable components but transitive alloc_comp marking.
!
! Contributed by Andrew Benson

module pr121472_m
  implicit none

  integer :: constructor_count = 0
  integer :: finalizer_count = 0

  type :: base_t
  end type base_t

  type :: container_t
    type(base_t) :: component
  contains
    final :: container_finalize
  end type container_t

  interface container_t
    module procedure container_constructor
  end interface container_t

contains

  subroutine container_finalize(this)
    type(container_t), intent(inout) :: this
    finalizer_count = finalizer_count + 1
  end subroutine container_finalize

  function container_constructor() result(res)
    type(container_t) :: res
    constructor_count = constructor_count + 1
  end function container_constructor

end module pr121472_m

program test_finalize_constructor
  use pr121472_m
  implicit none

  type(container_t) :: obj

  ! This assignment from constructor used to cause ICE
  obj = container_t()

  print *, 'constructor:', constructor_count
  print *, 'finalizer:', finalizer_count

end program test_finalize_constructor
