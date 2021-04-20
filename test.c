#include <stdlib.h>
#include "allocator.c"

void test_malloc() {
	void *a = malloc(1);
	void *b = malloc(42);
	void *c = malloc(4097);
	void *d = malloc(25);

}

void test_print_memory() {

	void *a = malloc(100);
	void *b = malloc(100);
	void *c = malloc(100);
	void *d = malloc(10);
	void *e = malloc(100);
	void *f = malloc(100);

	free(b);
	free(d);

	void *g = malloc(100);

	print_memory();
}

int main(void)
{


test_print_memory();

}