#include <stdlib.h>
#include "allocator.c"

void test_malloc();
void test_print_memory();
void test_ll_add();
void test_split();

void ll_print(struct mem_block *head);

void test_malloc() {
	void *a = malloc(1);
	struct mem_block *a_head = (char *)a - 100;
	strcpy(a_head->name, "A");
	
	void *b = malloc(42);
	void *d = malloc(10);
	/*
	void *c = malloc(4097);
	*/
}

void test_malloc_name() {
	//void *a = malloc_name(1, "A");
	//void *b = malloc_name(10, "B");

	void *a = malloc_name(1, "Abba here we go again");
	void *b = malloc_name(2, "Bubba bia");
	void*c = malloc_name(4096, "1 full block");
	//void *c = malloc_name(3000, "C");
	/*
	void *c = malloc_name(3, "Allo");
	void *d = malloc_name(4, "D");
	void *e = malloc_name(5, "E");
	void *f = malloc_name(4097, "F");
*/



	struct mem_block* a_head = (char*)a - 100;
	LOG("'%s'\n", a_head->name);

	struct mem_block* b_head = (char*)b - 100;
	LOG("'%s'\n", b_head->name);

	print_memory();
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

// TODO: test_ll_add - make linked_list of blocks w/ naems = "a", "b", "c", etc. and then test out ll_add

// Confirmed success
void test_ll_add() {
	LOGP("STARTING ADD\n");
	struct mem_block a = {0};
	strcpy(a.name, "A");
	
	struct mem_block b = {0};
	strcpy(b.name, "B");
	
	struct mem_block c = {0};
	strcpy(c.name, "C");

	a.next = &b;
	b.prev = &a;

	b.next = &c;
	c.prev = &b;
	
	LOGP("BEFORE ADD: SHOULD BE A -> B -> C -> NULL\n");
	ll_print(&a);


	LOGP("ADDING A TO NULL: SHOULD DO NOTHING\n");
	ll_add(NULL, &a);
	ll_print(&a);

	LOGP("ADDING NULL TO A: SHOULD ALSO DO NOTHING\n");
	ll_add(&a, NULL);
	ll_print(&a);





	LOGP("ADDING D TO A: SHOULD BE A -> D -> B -> C -> NULL\n");
	struct mem_block d = {0};
	strcpy(d.name, "D");
	ll_add(&a, &d);


	ll_print(&a);

	LOGP("ADDING E TO END: SHOULD BE A -> D -> B -> C -> E -> NULL\n");
	struct mem_block e = {0};
	strcpy(e.name, "E");
	ll_add(&c, &e);

	
	ll_print(&a);

	struct mem_block f = {0};
	strcpy(f.name, "F");
	ll_add(&e, &f);
	ll_print(&a);

}



void ll_print(struct mem_block *head) {

	struct mem_block* current = head;

	while (current != NULL) {
		printf("'%s' -> ", current->name);
		current = current->next;
	}
	printf("'%s'\n", current);

}

void test_split() {

	void* a = malloc_name(100, "A");
	void* b = malloc_name(100, "B");
	void* c = malloc_name(100, "C");
	

	print_memory();

/*
	LOGP("SHOULD BE A -> B -> C -> LEFTOVER DATA\n");
	print_memory();
	free(b);

	LOGP("SHOULD BE A -> B (free) -> C -> LEFTOVER DATA\n");
	print_memory();

	void* d = malloc_name(50, "D");

	LOGP("SHOULD BE A -> D -> LEFTOVER -> C -> LEFTOVER)\n");
	print_memory();
	*/
}


void test_allocations_1() {

	void *a = malloc_name(100, "Test Allocation: 0");
	void *b = malloc_name(100, "Test Allocation: 1"); /* Will be deleted */
	void *c = malloc_name(100, "Test Allocation: 2");
	void *d = malloc_name(10,  "Test Allocation: 3");  /* Will be deleted */
	void *e = malloc_name(100, "Test Allocation: 4");
	void *f = malloc_name(100, "Test Allocation: 5");

	free(b);
	free(d);

	/* This will split:
	 * - b with first fit
	 * - d with best fit
	 * - f with worst fit
	 */
	void *g = malloc_name(10, "Test Allocation: 6");

	print_memory();

	return 0;
}
int main(void)
{

	//test_ll_add();
	//test_print_memory();
	//test_malloc_name();
	//test_split();

	test_allocations_1();
}