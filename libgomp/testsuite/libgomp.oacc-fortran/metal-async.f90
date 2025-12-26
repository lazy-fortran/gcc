! { dg-do run }
! { dg-skip-if "Metal not available" { ! openacc_metal_accel_selected } }
! { dg-additional-options "-fopenacc" }

! Test Metal async operations and queue management

program metal_async
    use openacc
    use iso_fortran_env, only: dp => real64
    implicit none

    integer, parameter :: n = 2048
    real(dp) :: a(n), b(n), c(n), d(n), e(n)
    integer :: i

    ! Initialize arrays
    do i = 1, n
        a(i) = real(i, dp)
        b(i) = 0.0_dp
        c(i) = 0.0_dp
        d(i) = 0.0_dp
        e(i) = 0.0_dp
    end do

    call t1 (a, b)
    call t2 (a, b, c)
    call t3 (a, b, c, d)
    call t4 (a, b, c, d, e)
    call t5 (a, b)
    call t6 (b)

contains

    subroutine t1 (a, b)
        real(dp), intent(in) :: a(:)
        real(dp), intent(inout) :: b(:)
        integer :: i, errors

        !$acc parallel loop async
        do i = 1, size(a)
            b(i) = a(i) * 2.0_dp
        end do
        !$acc end parallel loop

        !$acc wait

        errors = 0
        do i = 1, size(a)
            if (abs(b(i) - real(i * 2, dp)) > 1.0e-12_dp) then
                errors = errors + 1
            end if
        end do

        if (errors > 0) then
            print *, "Test 1 (simple async) failed:", errors, "errors"
            stop 1
        end if
    end subroutine t1

    subroutine t2 (a, b, c)
        real(dp), intent(in) :: a(:)
        real(dp), intent(inout) :: b(:), c(:)
        integer :: i, errors

        b(:) = 0.0_dp
        c(:) = 0.0_dp

        !$acc parallel loop async(1)
        do i = 1, size(a)
            b(i) = a(i) + 10.0_dp
        end do
        !$acc end parallel loop

        !$acc parallel loop async(2)
        do i = 1, size(a)
            c(i) = a(i) + 20.0_dp
        end do
        !$acc end parallel loop

        !$acc wait(1)
        !$acc wait(2)

        errors = 0
        do i = 1, size(a)
            if (abs(b(i) - (real(i, dp) + 10.0_dp)) > 1.0e-12_dp) then
                errors = errors + 1
            end if
            if (abs(c(i) - (real(i, dp) + 20.0_dp)) > 1.0e-12_dp) then
                errors = errors + 1
            end if
        end do

        if (errors > 0) then
            print *, "Test 2 (async queues) failed:", errors, "errors"
            stop 2
        end if
    end subroutine t2

    subroutine t3 (a, b, c, d)
        real(dp), intent(in) :: a(:)
        real(dp), intent(inout) :: b(:), c(:), d(:)
        integer :: i, errors

        b(:) = 0.0_dp
        c(:) = 0.0_dp
        d(:) = 0.0_dp

        !$acc parallel loop async(1)
        do i = 1, size(a)
            b(i) = a(i) * 2.0_dp
        end do
        !$acc end parallel loop

        !$acc parallel loop async(1)
        do i = 1, size(a)
            c(i) = b(i) + 5.0_dp
        end do
        !$acc end parallel loop

        !$acc parallel loop async(1)
        do i = 1, size(a)
            d(i) = c(i) * 3.0_dp
        end do
        !$acc end parallel loop

        !$acc wait(1)

        errors = 0
        do i = 1, size(a)
            if (abs(d(i) - ((real(i, dp) * 2.0_dp + 5.0_dp) * 3.0_dp)) > &
                1.0e-10_dp) then
                errors = errors + 1
            end if
        end do

        if (errors > 0) then
            print *, "Test 3 (dependency chain) failed:", errors, "errors"
            stop 3
        end if
    end subroutine t3

    subroutine t4 (a, b, c, d, e)
        real(dp), intent(in) :: a(:)
        real(dp), intent(inout) :: b(:), c(:), d(:), e(:)
        integer :: i, errors

        b(:) = 0.0_dp
        c(:) = 0.0_dp
        d(:) = 0.0_dp
        e(:) = 0.0_dp

        !$acc parallel loop async(1)
        do i = 1, size(a)
            b(i) = a(i) * 3.0_dp
        end do
        !$acc end parallel loop

        !$acc parallel loop async(2)
        do i = 1, size(a)
            c(i) = a(i) * 4.0_dp
        end do
        !$acc end parallel loop

        !$acc parallel loop async(3)
        do i = 1, size(a)
            d(i) = a(i) * 5.0_dp
        end do
        !$acc end parallel loop

        !$acc parallel loop wait(1) wait(2) wait(3) async(4)
        do i = 1, size(a)
            e(i) = b(i) + c(i) + d(i)
        end do
        !$acc end parallel loop

        !$acc wait(4)

        errors = 0
        do i = 1, size(a)
            if (abs(e(i) - (real(i, dp) * 12.0_dp)) > 1.0e-10_dp) then
                errors = errors + 1
            end if
        end do

        if (errors > 0) then
            print *, "Test 4 (independent queues) failed:", errors, "errors"
            stop 4
        end if
    end subroutine t4

    subroutine t5 (a, b)
        real(dp), intent(inout) :: a(:), b(:)
        integer :: i, errors

        b(:) = 0.0_dp

        !$acc data copy(a, b)

        !$acc parallel loop async(1)
        do i = 1, size(a)
            b(i) = a(i) * 7.0_dp
        end do
        !$acc end parallel loop

        !$acc wait(1)

        !$acc end data

        errors = 0
        do i = 1, size(a)
            if (abs(b(i) - (real(i, dp) * 7.0_dp)) > 1.0e-12_dp) then
                errors = errors + 1
            end if
        end do

        if (errors > 0) then
            print *, "Test 5 (async with data) failed:", errors, "errors"
            stop 5
        end if
    end subroutine t5

    subroutine t6 (b)
        real(dp), intent(inout) :: b(:)
        integer :: i, errors

        b(:) = 0.0_dp

        !$acc data create(b)
            do i = 1, size(b)
                b(i) = real(i + 100, dp)
            end do

            !$acc update device(b) async(1)

            !$acc parallel loop async(1) present(b)
            do i = 1, size(b)
                b(i) = b(i) + 3.0_dp
            end do
            !$acc end parallel loop

            !$acc update self(b) async(1)
            !$acc wait(1)
        !$acc end data

        errors = 0
        do i = 1, size(b)
            if (abs(b(i) - real(i + 103, dp)) > 1.0e-12_dp) then
                errors = errors + 1
            end if
        end do

        if (errors > 0) then
            print *, "Test 6 (async update) failed:", errors, "errors"
            stop 6
        end if
    end subroutine t6

end program metal_async
