module chess_fft
    use iso_c_binding
    implicit none

contains

    pure subroutine fft1d8(re, im)
        real(c_double), intent(inout) :: re(8)
        real(c_double), intent(inout) :: im(8)

        real(c_double) :: tr(8)
        real(c_double) :: ti(8)
        real(c_double) :: wr
        real(c_double) :: wi
        real(c_double) :: ur
        real(c_double) :: ui
        real(c_double) :: vr
        real(c_double) :: vi
        real(c_double) :: angle
        integer :: step
        integer :: half
        integer :: start
        integer :: j
        integer, parameter :: rev(8) = [1, 5, 3, 7, 2, 6, 4, 8]

        do j = 1, 8
            tr(j) = re(rev(j))
            ti(j) = im(rev(j))
        end do

        re = tr
        im = ti

        step = 2
        do while (step <= 8)
            half = step / 2
            do start = 1, 8, step
                do j = 0, half - 1
                    angle = -2.0d0 * acos(-1.0d0) * dble(j) / dble(step)
                    wr = cos(angle)
                    wi = sin(angle)

                    ur = re(start + j)
                    ui = im(start + j)
                    vr = re(start + j + half)
                    vi = im(start + j + half)

                    tr(j + 1) = wr * vr - wi * vi
                    ti(j + 1) = wr * vi + wi * vr

                    re(start + j) = ur + tr(j + 1)
                    im(start + j) = ui + ti(j + 1)
                    re(start + j + half) = ur - tr(j + 1)
                    im(start + j + half) = ui - ti(j + 1)
                end do
            end do

            step = step * 2
        end do
    end subroutine fft1d8

    subroutine fft_batch_8x8(input, output_real, output_imag) &
        bind(C, name="fft_batch_8x8")

        real(c_float), intent(in) :: input(*)
        real(c_float), intent(out) :: output_real(*)
        real(c_float), intent(out) :: output_imag(*)

        real(c_double) :: row_re(8)
        real(c_double) :: row_im(8)
        real(c_double) :: col_re(8)
        real(c_double) :: col_im(8)
        real(c_double) :: plane_re(8, 8)
        real(c_double) :: plane_im(8, 8)
        integer :: channel
        integer :: row
        integer :: col
        integer :: base
        integer :: idx

        do channel = 0, 31
            base = channel * 64

            do row = 1, 8
                do col = 1, 8
                    idx = base + (row - 1) * 8 + col
                    plane_re(row, col) = dble(input(idx))
                    plane_im(row, col) = 0.0d0
                end do
            end do

            do row = 1, 8
                do col = 1, 8
                    row_re(col) = plane_re(row, col)
                    row_im(col) = plane_im(row, col)
                end do

                call fft1d8(row_re, row_im)

                do col = 1, 8
                    plane_re(row, col) = row_re(col)
                    plane_im(row, col) = row_im(col)
                end do
            end do

            do col = 1, 8
                do row = 1, 8
                    col_re(row) = plane_re(row, col)
                    col_im(row) = plane_im(row, col)
                end do

                call fft1d8(col_re, col_im)

                do row = 1, 8
                    idx = base + (row - 1) * 8 + col
                    output_real(idx) = real(col_re(row), c_float)
                    output_imag(idx) = real(col_im(row), c_float)
                end do
            end do
        end do
    end subroutine fft_batch_8x8

end module chess_fft
