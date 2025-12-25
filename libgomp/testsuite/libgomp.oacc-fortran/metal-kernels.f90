! { dg-do run }
! { dg-skip-if "Metal not available" { ! openacc_metal_selected } }
! { dg-additional-options "-fopenacc" }

! Test Metal kernels construct with automatic parallelization

program metal_kernels
    use iso_fortran_env, only: dp => real64
    implicit none

    integer, parameter :: n = 1024
    integer, parameter :: m = 512
    real(dp) :: a(n), b(n), c(n)
    real(dp) :: matrix(m, m), result(m, m)
    integer :: i, j, errors

    ! Test 1: Simple kernels loop
    do i = 1, n
        a(i) = real(i, dp)
        b(i) = real(n - i + 1, dp)
        c(i) = 0.0_dp
    end do

    !$acc kernels
    do i = 1, n
        c(i) = a(i) + b(i)
    end do
    !$acc end kernels

    errors = 0
    do i = 1, n
        if (abs(c(i) - real(n + 1, dp)) > 1.0e-12_dp) then
            errors = errors + 1
        end if
    end do

    if (errors > 0) then
        print *, "Test 1 (kernels loop) failed:", errors, "errors"
        stop 1
    end if

    ! Test 2: Kernels with nested loops
    do i = 1, m
        do j = 1, m
            matrix(i, j) = real(i + j, dp)
            result(i, j) = 0.0_dp
        end do
    end do

    !$acc kernels
    do i = 1, m
        do j = 1, m
            result(i, j) = matrix(i, j) * 2.0_dp
        end do
    end do
    !$acc end kernels

    errors = 0
    do i = 1, m
        do j = 1, m
            if (abs(result(i, j) - real((i + j) * 2, dp)) > 1.0e-12_dp) then
                errors = errors + 1
            end if
        end do
    end do

    if (errors > 0) then
        print *, "Test 2 (nested loops) failed:", errors, "errors"
        stop 2
    end if

    ! Test 3: Kernels with data clauses
    c(:) = 0.0_dp

    !$acc kernels copyin(a, b) copyout(c)
    do i = 1, n
        c(i) = a(i) * b(i)
    end do
    !$acc end kernels

    errors = 0
    do i = 1, n
        if (abs(c(i) - (real(i, dp) * real(n - i + 1, dp))) > &
            1.0e-10_dp) then
            errors = errors + 1
        end if
    end do

    if (errors > 0) then
        print *, "Test 3 (data clauses) failed:", errors, "errors"
        stop 3
    end if

    ! Test 4: Multiple independent loops in kernels region
    a(:) = 1.0_dp
    b(:) = 2.0_dp
    c(:) = 3.0_dp

    !$acc kernels
    do i = 1, n
        a(i) = a(i) + 1.0_dp
    end do

    do i = 1, n
        b(i) = b(i) * 2.0_dp
    end do

    do i = 1, n
        c(i) = a(i) + b(i)
    end do
    !$acc end kernels

    errors = 0
    do i = 1, n
        if (abs(a(i) - 2.0_dp) > 1.0e-12_dp) errors = errors + 1
        if (abs(b(i) - 4.0_dp) > 1.0e-12_dp) errors = errors + 1
        if (abs(c(i) - 6.0_dp) > 1.0e-12_dp) errors = errors + 1
    end do

    if (errors > 0) then
        print *, "Test 4 (multiple loops) failed:", errors, "errors"
        stop 4
    end if

end program metal_kernels
