/* { dg-do compile } */
/* { dg-options "-O2" } */

int test_add_int(int a, int b)
{
    return a + b;
}

unsigned int test_add_uint(unsigned int a, unsigned int b)
{
    return a + b;
}

long test_add_long(long a, long b)
{
    return a + b;
}

float test_add_float(float a, float b)
{
    return a + b;
}

double test_add_double(double a, double b)
{
    return a + b;
}
