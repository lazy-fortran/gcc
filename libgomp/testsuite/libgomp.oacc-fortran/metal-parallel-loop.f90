! { dg-do run }
! { dg-skip-if "Metal not available" { ! openacc_metal_selected } }
! { dg-additional-options "-fopenacc" }

! Test Metal parallel loop execution with various patterns

program metal_parallel_loop
    use iso_fortran_env, only: dp => real64
    implicit none

    integer, parameter :: n = 2048
    real(dp) :: a(n), b(n), c(n)
    integer :: i, errors

    ! Initialize arrays
    do i = 1, n
        a(i) = real(i, dp)
        b(i) = real(i * 2, dp)
        c(i) = 0.0_dp
    end do

    ! Test 1: Simple parallel loop
    !$acc parallel loop
    do i = 1, n
        c(i) = a(i) + b(i)
    end do
    !$acc end parallel loop

    errors = 0
    do i = 1, n
        if (abs(c(i) - real(i * 3, dp)) > 1.0e-12_dp) then
            errors = errors + 1
        end if
    end do

    if (errors > 0) then
        print *, "Test 1 failed:", errors, "errors"
        stop 1
    end if

    ! Test 2: Parallel loop with explicit data clauses
    c(:) = 0.0_dp

    !$acc parallel loop copyin(a, b) copyout(c)
    do i = 1, n
        c(i) = a(i) * b(i)
    end do
    !$acc end parallel loop

    errors = 0
    do i = 1, n
        if (abs(c(i) - real(i * i * 2, dp)) > 1.0e-10_dp) then
            errors = errors + 1
        end if
    end do

    if (errors > 0) then
        print *, "Test 2 failed:", errors, "errors"
        stop 2
    end if

    ! Test 3: Parallel loop with gang, worker, vector
    c(:) = 0.0_dp

    !$acc parallel loop gang worker vector
    do i = 1, n
        c(i) = sqrt(a(i)) + sqrt(b(i))
    end do
    !$acc end parallel loop

    errors = 0
    do i = 1, n
        if (abs(c(i) - (sqrt(real(i, dp)) + sqrt(real(i * 2, dp)))) > &
            1.0e-10_dp) then
            errors = errors + 1
        end if
    end do

    if (errors > 0) then
        print *, "Test 3 failed:", errors, "errors"
        stop 3
    end if

    ! Test 4: In-place modification
    !$acc parallel loop copy(a)
    do i = 1, n
        a(i) = a(i) + 100.0_dp
    end do
    !$acc end parallel loop

    errors = 0
    do i = 1, n
        if (abs(a(i) - (real(i, dp) + 100.0_dp)) > 1.0e-12_dp) then
            errors = errors + 1
        end if
    end do

    if (errors > 0) then
        print *, "Test 4 failed:", errors, "errors"
        stop 4
    end if

end program metal_parallel_loop
