! { dg-do run }
! { dg-options "-pthread" }

! Concurrency coverage for the shared-memory coarray read guard
! (PR fortran/125113).  The caf_shmem runtime installs a process-wide
! SIGSEGV disposition around each coindexed read accessor, serialized by a
! process-wide mutex.  Spawn several worker threads of one image that
! simultaneously perform coindexed reads; this verifies that concurrent
! guarded accessors do not clobber one another's guard state or the shared
! signal disposition.

module caf_shmem_threads
  use iso_c_binding
  implicit none
  integer, codimension[*] :: x
  integer :: bad(4)
contains
  ! Worker thread: repeatedly read image 2's up-to-date value through the
  ! guarded accessor and count any incorrect results.
  function read_remote (arg) bind (C) result (res)
    type(c_ptr), value :: arg
    type(c_ptr) :: res
    integer, pointer :: idx
    integer :: i, v
    res = c_null_ptr
    call c_f_pointer (arg, idx)
    bad (idx) = 0
    do i = 1, 2000
      v = x[2]
      if (v /= 2) bad (idx) = bad (idx) + 1
    end do
  end function read_remote
end module caf_shmem_threads

program ptr_comp_8
  use iso_c_binding
  use caf_shmem_threads
  implicit none
  integer, parameter :: nthreads = 4
  type(c_ptr) :: handles(nthreads)
  type(c_ptr), target :: tids(nthreads)
  integer, target :: idxs(nthreads)
  integer(c_int) :: rc
  integer :: j

  interface
    function pthread_create (thread, attr, start, arg) bind (C, name="pthread_create")
      use iso_c_binding
      type(c_ptr), value :: thread
      type(c_ptr), value :: attr
      type(c_funptr), value :: start
      type(c_ptr), value :: arg
      integer(c_int) :: pthread_create
    end function pthread_create
    function pthread_join (thread, retval) bind (C, name="pthread_join")
      use iso_c_binding
      type(c_ptr), value :: thread
      type(c_ptr), value :: retval
      integer(c_int) :: pthread_join
    end function pthread_join
  end interface

  x = this_image ()
  do j = 1, nthreads
    idxs(j) = j
  end do
  sync all
  ! This is only interesting with at least two images (the caf_shmem
  ! runtime).  Single-image modes simply skip the threaded access.
  if (this_image () == 1 .and. num_images () >= 2) then
    do j = 1, nthreads
      rc = pthread_create (c_loc (tids(j)), c_null_ptr, c_funloc (read_remote), &
                           c_loc (idxs(j)))
      if (rc /= 0) error stop
    end do
    do j = 1, nthreads
      rc = pthread_join (tids(j), c_null_ptr)
      if (rc /= 0) error stop
    end do
    if (any (bad /= 0)) error stop
  end if
  sync all
end program ptr_comp_8
