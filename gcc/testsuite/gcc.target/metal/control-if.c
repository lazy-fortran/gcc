/* { dg-do compile } */
/* { dg-options "-O2" } */

int test_if_simple(int x)
{
    if (x > 0)
        return 1;
    return 0;
}

int test_if_else(int x)
{
    if (x > 0)
        return 1;
    else
        return -1;
}

int test_if_nested(int x, int y)
{
    if (x > 0) {
        if (y > 0)
            return 1;
        else
            return 2;
    } else {
        return 0;
    }
}

int test_if_chain(int x)
{
    if (x < 0)
        return -1;
    else if (x == 0)
        return 0;
    else
        return 1;
}
