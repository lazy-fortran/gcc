! { dg-do run }
! { dg-skip-if "Metal not available" { ! openacc_metal_selected } }
! { dg-additional-options "-fopenacc" }

! Test Metal memory management and reference counting

program metal_memory
    use openacc
    use iso_fortran_env, only: dp => real64
    implicit none

    integer, parameter :: n = 1024
    real(dp) :: a(n), b(n)
    real(dp), allocatable :: c(:), d(:)
    integer :: i, errors

    ! Test 1: Multiple enter/exit cycles
    do i = 1, n
        a(i) = real(i, dp)
        b(i) = 0.0_dp
    end do

    do i = 1, 5
        !$acc enter data copyin(a)
        !$acc parallel loop present(a)
        do concurrent (i = 1:n)
            b(i) = a(i) * real(i, dp)
        end do
        !$acc end parallel loop
        !$acc exit data delete(a)
    end do

    ! Test 2: Allocatable arrays
    allocate(c(n), d(n))

    do i = 1, n
        c(i) = real(i * 2, dp)
        d(i) = 0.0_dp
    end do

    !$acc enter data copyin(c) create(d)

    if (.not. acc_is_present(c)) then
        print *, "Error: allocatable c not present"
        stop 1
    end if

    if (.not. acc_is_present(d)) then
        print *, "Error: allocatable d not present"
        stop 2
    end if

    !$acc parallel loop present(c, d)
    do i = 1, n
        d(i) = c(i) / 2.0_dp
    end do
    !$acc end parallel loop

    !$acc exit data copyout(d) delete(c)

    errors = 0
    do i = 1, n
        if (abs(d(i) - real(i, dp)) > 1.0e-12_dp) then
            errors = errors + 1
        end if
    end do

    if (errors > 0) then
        print *, "Test 2 (allocatable arrays) failed:", errors, "errors"
        stop 3
    end if

    deallocate(c, d)

    ! Test 3: Reference counting with nested data regions
    a(:) = real(42, dp)

    !$acc data copy(a)
        !$acc data present(a)
            !$acc parallel loop
            do i = 1, n
                a(i) = a(i) + 1.0_dp
            end do
            !$acc end parallel loop
        !$acc end data
    !$acc end data

    errors = 0
    do i = 1, n
        if (abs(a(i) - 43.0_dp) > 1.0e-12_dp) then
            errors = errors + 1
        end if
    end do

    if (errors > 0) then
        print *, "Test 3 (nested data regions) failed:", errors, "errors"
        stop 4
    end if

    ! Test 4: Copy vs copyin vs copyout
    a(:) = 1.0_dp
    b(:) = 2.0_dp

    !$acc data copy(a) copyin(b)
        !$acc parallel loop
        do i = 1, n
            a(i) = a(i) + b(i)
        end do
        !$acc end parallel loop
    !$acc end data

    errors = 0
    do i = 1, n
        if (abs(a(i) - 3.0_dp) > 1.0e-12_dp) then
            errors = errors + 1
        end if
    end do

    if (errors > 0) then
        print *, "Test 4 (copy directives) failed:", errors, "errors"
        stop 5
    end if

    ! Test 5: Present clause validation
    a(:) = 5.0_dp

    !$acc enter data copyin(a)

    !$acc parallel loop present(a)
    do i = 1, n
        a(i) = a(i) * 2.0_dp
    end do
    !$acc end parallel loop

    !$acc exit data copyout(a)

    errors = 0
    do i = 1, n
        if (abs(a(i) - 10.0_dp) > 1.0e-12_dp) then
            errors = errors + 1
        end if
    end do

    if (errors > 0) then
        print *, "Test 5 (present clause) failed:", errors, "errors"
        stop 6
    end if

end program metal_memory
