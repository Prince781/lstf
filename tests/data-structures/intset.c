#include "data-structures/intset.h"
#include <stdio.h>
#include <stdlib.h>

int main(int argc, char *argv[])
{
    (void) argc;
    (void) argv;

    intset *set = intset_new(145);
    intset_set(set, 31);
    intset_set(set, 102);
    intset_set(set, 141);
    intset_set(set, 3);
    intset_unset(set, 3);

    printf("there are %u items in the intset\n", intset_count(set));
    free(set);
    return 0;
}
