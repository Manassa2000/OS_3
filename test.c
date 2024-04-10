#include <stdio.h>
#include <stdlib.h>

int main() {
	int *p = (int*)malloc(3 * sizeof(int));
	p[0] = 0;
	p[1] = 1;
	p[2] = 2;
	printf("%p\n", &p);
	return 0;
}
