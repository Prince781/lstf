#include "data-structures/array.h"
#include <stdio.h>

#define N 1000
#define triangle_sum(n) (n) * (n + 1) / 2

int main(int argc, char *argv[])
{
    (void) argc;
    (void) argv;

    array(int) *ints = array_new();

    for (size_t i = 0; i < N; i++)
        array_add(ints, i);

    for (size_t i = 0; ints->length > N / 2; i++)
        array_remove(ints, i);

    int sum = 0;
    for (size_t i = 0; i < ints->length; i++) {
        sum += ints->elements[i];
    }
    printf("ints = [");
    for (size_t i = 0; i < ints->length; i++)
        printf("%d%s", ints->elements[i], i < ints->length - 1u ? ", " : "");
    printf("]\n");

    printf("sum = %d\n", sum);

    array_destroy(ints);
    return sum == triangle_sum(N) - 2*triangle_sum(N/2) ? 0 : 1;
}
