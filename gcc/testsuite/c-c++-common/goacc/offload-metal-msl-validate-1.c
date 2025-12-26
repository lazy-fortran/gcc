/* { dg-do link } */
/* { dg-require-effective-target offload_metal } */
/* { dg-require-effective-target metal_xcrun } */
/* { dg-additional-options "-foffload=metal -save-temps -v" } */
/* { dg-final { scan-output "Compiling MSL to AIR:" } } */
/* { dg-final { scan-output "Linking AIR to metallib:" } } */

int
main (void)
{
  int a[128];

  for (int i = 0; i < 128; i++)
    a[i] = i;

#pragma acc parallel loop copy(a)
  for (int i = 0; i < 128; i++)
    a[i] = a[i] + 1;

  return a[0] != 1;
}

