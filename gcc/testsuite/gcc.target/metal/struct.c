/* { dg-do compile } */
/* { dg-options "-O2" } */

struct point {
    int x;
    int y;
};

struct point test_struct_return(int x, int y)
{
    struct point p;
    p.x = x;
    p.y = y;
    return p;
}

int test_struct_arg(struct point p)
{
    return p.x + p.y;
}

int test_struct_pointer(struct point *p)
{
    return p->x + p->y;
}

struct vector {
    float x;
    float y;
    float z;
};

float test_struct_float(struct vector v)
{
    return v.x + v.y + v.z;
}

struct nested {
    struct point p;
    int value;
};

int test_nested_struct(struct nested n)
{
    return n.p.x + n.p.y + n.value;
}

struct array_struct {
    int data[10];
    int size;
};

int test_struct_array(struct array_struct s)
{
    int sum = 0;
    for (int i = 0; i < s.size; i++) {
        sum += s.data[i];
    }
    return sum;
}
