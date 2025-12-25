/* { dg-do compile } */
/* { dg-options "-O2" } */

int test_pointer_deref(int *p)
{
    return *p;
}

void test_pointer_assign(int *p, int value)
{
    *p = value;
}

int test_pointer_array(int *arr, int idx)
{
    return arr[idx];
}

int test_pointer_offset(int *p, int offset)
{
    return *(p + offset);
}

void test_pointer_inc(int *p)
{
    (*p)++;
}

int test_pointer_diff(int *p1, int *p2)
{
    return p1 - p2;
}

void test_pointer_null(int *p)
{
    if (p != 0)
        *p = 0;
}
