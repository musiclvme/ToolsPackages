#include <stdio.h>
#include <malloc.h>
#include "def.h"



void main(void)
{
	int *ret1;
	int *pointer1 = NULL;

	ret1 = def_test(&pointer1);
	printf("%p , ret1=%p \n", pointer1, ret1);

	if (pointer1 != NULL)
		free(pointer1);

}
