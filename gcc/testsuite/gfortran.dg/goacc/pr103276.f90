! { dg-do compile }
! { dg-additional-options "-fdump-tree-original" }
!
! PR fortran/103276
! OpenACC ENTER/EXIT DATA on a derived-type dummy argument must not map
! the reference slot, which lives on the current stack frame.

module m
  implicit none

  type :: t
    integer :: v
  end type

contains

  subroutine map_record (record, ptr)
    type(t), intent(inout) :: record
    type(t), pointer, intent(inout) :: ptr
    !$acc enter data copyin(record, ptr)
    !$acc exit data copyout(record, ptr)
  end subroutine

end module

! The actual pointer still needs pointer mappings.
! { dg-final { scan-tree-dump-not "record \\\[pointer assign" "original" } }
! { dg-final { scan-tree-dump-times "ptr \\\[pointer assign" 4 "original" } }
