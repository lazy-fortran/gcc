/* { dg-additional-options "-fdump-tree-gimple" } */

void
subr (int *a)
{
#pragma acc data deviceptr (a)
#pragma acc parallel
  a[0] += 1.0f;
}

/* { dg-final { scan-tree-dump-times "#pragma omp target oacc_parallel.*map\\(tofrom:a" 1 "gimple" } } */
