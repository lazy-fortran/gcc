! { dg-do compile }
! { dg-options "-frange-check -funsigned" }

print *, 70000 * 70000 ! { dg-error "Arithmetic overflow" }
end
