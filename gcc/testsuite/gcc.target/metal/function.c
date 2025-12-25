/* { dg-do compile } */
/* { dg-options "-O2" } */

int helper_add(int a, int b)
{
    return a + b;
}

int test_function_call(int x, int y)
{
    return helper_add(x, y);
}

int recursive_factorial(int n)
{
    if (n <= 1)
        return 1;
    return n * recursive_factorial(n - 1);
}

static inline int inline_add(int a, int b)
{
    return a + b;
}

int test_inline_function(int x, int y)
{
    return inline_add(x, y);
}

int multi_arg_function(int a, int b, int c, int d, int e)
{
    return a + b + c + d + e;
}

void void_function(int *result, int x, int y)
{
    *result = x + y;
}

int test_void_function(int x, int y)
{
    int result;
    void_function(&result, x, y);
    return result;
}

typedef int (*func_ptr)(int, int);

int test_function_pointer(func_ptr f, int x, int y)
{
    return f(x, y);
}
