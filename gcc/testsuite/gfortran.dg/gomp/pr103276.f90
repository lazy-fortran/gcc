! { dg-do compile }
! { dg-additional-options "-fdump-tree-original" }
!
! PR fortran/103276
! OpenMP ENTER/EXIT DATA on an ordinary scalar dummy argument must not map
! the reference slot, which lives on the current stack frame.

subroutine map_scalar (value, ptr)
  integer, intent(inout) :: value
  integer, pointer, intent(inout) :: ptr

  !$omp target enter data map(to: value, ptr)
  !$omp target exit data map(from: value, ptr)
end subroutine

! The actual pointer still needs pointer mappings.
! { dg-final { scan-tree-dump-not "value \\\[pointer assign" "original" } }
! { dg-final { scan-tree-dump-times "ptr \\\[pointer assign" 2 "original" } }
