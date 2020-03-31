#include<stdio.h>
#include<malloc.h>
#include "def.h"

int * def_test(int **p)
{
	int *test;
	test = calloc (1, sizeof (int));
	printf("def_test----------->p=%p\n", test);
	*p = test;
	return test;
}


