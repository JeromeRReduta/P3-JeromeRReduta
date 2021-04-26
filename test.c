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
		ll_log_block(current);
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
	void *h = malloc(5000);

	ll_log_list();
	print_memory();

	return 0;
}

// CONFIRMED SUCCESS
void test_ll_delete_with_blocks() {

	struct mem_block a = {0};
	strcpy(a.name, "A");
	struct mem_block b = {0};
	strcpy(b.name, "B");
	struct mem_block c = {0};
	strcpy(c.name, "C");

	a.prev = NULL;


	a.next = &b;
	b.prev = &a;

	b.next = &c;
	c.prev = &b;

	c.next = NULL;

	LOGP("SANITY CHECK - LINKED LIST SHOULD BE: (NULL) --> A --> B --> C --> (NULL)\n");
	ll_print(&a);

	ll_delete(&b);

	LOGP("SHOULD BE: (NULL) --> A --> C --> (NULL)\n");
	ll_print(&a);

	// TODO: Make test for actual malloc blocks



}

// CONFIRMED SUCCESS
void test_ll_delete_with_malloc() {
	void *a = malloc_name(100, "A");
	void *b = malloc_name(200, "B");
	void *c = malloc_name(300, "C");
	void *d = malloc_name(4096, "END OF LIST");
	void *e = malloc_name(500, "E");

	LOGP("SANITY CHECK - LINKED LIST SHOULD BE: (NULL) --> A --> B --> C --> E --> END OF LIST (NULL)\n");
	ll_log_list();

	free(b);
	free(d);

	LOGP("CHECK - LINKED LIST SHOULD BE UNCHANGED\n");
	ll_log_list();

	LOGP("DELETING B HEAD AND D HEAD \n");

	ll_delete(get_header_from_data(b));
	ll_delete(get_header_from_data(d));

	LOGP("CHECK - LINKED LIST SHOULD BE: (NULL) --> A --> C --> E --> (NULL)\n");
	ll_log_list();

	LOGP("ATTEMPTING TO DELETE NULL - SHOULD RETURN ERROR MSG\n");
	ll_delete(NULL);

	LOGP("ATTEMPTING TO DELETE TAIL - SHOULD BE: (NULL) --> A --> C --> E --> ' ' --> (NULL) AND CHANGE TAIL\n");


	struct mem_block *final_block_in_list = get_header_from_data(e)->next->next;
	ll_delete(final_block_in_list);
	ll_log_list();
}

void test_ll_delete_sole_block() {
	void *a = malloc_name(100, "Only block");
	LOGP("SANITY CHECK - WHAT IS LIST?\n");
	ll_log_list();



	struct mem_block* a_head = get_header_from_data(a);
	struct mem_block* tail = get_data_from_header( a_head->next );

	free(a);

	LOGP("SANITY CHECK - ONLY BLOCK LEFT?\n");
	ll_log_list();



}

/* CONFIRMED SUCCESS IN FOLLOWING CASES:
	1. merging two valid blocks, with inputs in either order (true)
	2. trying to merge free and used block (false)
	3. trying to merge non-neighbors (error msg & false)
	4. trying to merge blocks in diff regions (false)
	5. trying to merge two used blocks (false)
*/
void test_blocks_can_merge_with_malloc() {

	void *a = malloc_name(100, "A");
	void *b = malloc_name(200, "B");
	void *c = malloc_name(300, "C");
	void *d = malloc_name(4096, "END OF LIST");
	void *e = malloc_name(500, "E");

	print_memory();


	struct mem_block *a_head = get_header_from_data(a);
	struct mem_block *b_head = get_header_from_data(b);
	struct mem_block *c_head = get_header_from_data(c);
	struct mem_block *d_head = get_header_from_data(d);
	struct mem_block *e_head = get_header_from_data(e);
	struct mem_block *end_of_region_0 = get_header_from_data(e)->next->next;


	LOG("FREEING A AND B - SHOULD BE FALSE - a and b are both used: '%s'\n",
		blocks_can_merge(a_head, b_head) ? "true" : "false");

	LOGP("FREEING B AND C AND TRYING TO MERGE - SHOULD BE TRUE \n");

	free(b);
	free(c);


	LOG("Can merge B and C? - SHOULD BE TRUE: '%s'\n", blocks_can_merge(b_head, c_head) ? "true" : "false");
	
	
	LOG("Can merge C and E? - SHOULD BE FALSE - one is used: '%s'\n", blocks_can_merge(c_head, e_head) ? "true": "false");

	free(e);

	LOG("Can merge C and E now? - SHOULD NOW BE TRUE: '%s'\n", blocks_can_merge(c_head, e_head) ? "true" : "false");
	LOG("What about putting in inputs backwards? - SHOULD BE TRUE: '%s'\n", blocks_can_merge(e_head, c_head) ? "true" : "false");

	LOG("Can merge B and E? - SHOULD BE FALSE - not neighbors: '%s'\n", blocks_can_merge(b_head, e_head) ? "true" : "false");


	free(d);
	

	
	LOG("Can merge end of region 0 and end of list (different regions) - SHOULD BE FALSE '%s'\n", blocks_can_merge(end_of_region_0, d_head) ? "true" : "false");

}

/* Cases to test:
	1. Head and next are used - should do nothing, return head
	2. Head and next are freed - should merge, return head
	3. Head is free, next (c) is used - should do nothing, return head
	4. End of region 0 is free, thing after is free, but not in same region - should do nothing, return end of region 0
	5. Merge tail and second to last - should merge and return second to last - should also change tail 
	6. Free everything in region 0 - should all merge to head and return head
	*/

void test_merge_block() {

	void *a = malloc_name(100, "A");
	void *b = malloc_name(200, "B");
	void *c = malloc_name(300, "C");
	void *d = malloc_name(4096, "END OF LIST");
	void *e = malloc_name(500, "E");

	LOGP("SANITY CHECK - SHOULD BE (NULL) -> A -> B -> C -> E -> END OF LIST -> (NULL)\n");

	ll_log_list();
	print_memory();


	struct mem_block *a_head = get_header_from_data(a);
	struct mem_block *b_head = get_header_from_data(b);
	struct mem_block *c_head = get_header_from_data(c);
	struct mem_block *d_head = get_header_from_data(d);
	struct mem_block *e_head = get_header_from_data(e);
	struct mem_block *end_of_region_0 = get_header_from_data(d)->prev;

	// Success
	LOGP("Test 1. TRY TO MERGE HEAD - EVERYTHING IS USED - SHOULD DO NOTHING AND RETURN HEAD\n");

	struct mem_block* should_be_a_head = merge_block(a_head);

	LOG("Head SHOULD BE A: %p '%s'\n", should_be_a_head, should_be_a_head->name);
	ll_log_list();

	// Success
	LOGP("Test 2. TRY TO MERGE HEAD - HEAD AND NEXT ARE FREE - SHOULD MERGE HEAD AND NEXT AND RETURN HEAD\n");

	free(a);
	free(b);

	struct mem_block* merged_a_head = merge_block(a_head);
	LOG("SHOULD BE MERGED A HEAD: %p '%s'\n", merged_a_head, merged_a_head->name);
	ll_log_list();


	// Success
	LOGP("Test 3. TRY TO MERGE A AND C HEADS - SHOULD DO NOTHING B/C C USED\n");
	struct mem_block* merged = merge_block(a_head);
	LOG("SHOULD BE SAME AS A HEAD: %p '%s'\n", merged, merged->name);
	ll_log_list();


	// Success
	LOGP("Test 4. End of region 0 is free, thing after is free, but not in same region"
		" should not merge, should return end of region 0\n");

	LOG("SANITY CHECK - REGION IDS ARE DIFF: '%s'\n", end_of_region_0->region_id != end_of_region_0->next->region_id ? "true" : "false");
	free(get_data_from_header(end_of_region_0));
	free(get_data_from_header(end_of_region_0->next));
	LOGP("DOING STUFF NOW\n");
	merged = merge_block(end_of_region_0);
	LOG("SHOULD BE SAME AS END OF REGION 0: %p '%s'\n", merged, merged->name);
	ll_log_list();

	// Success
	LOGP("Test 5. tail is freed, tail->prev is freed, merge(tail) - should merge tail and tail->prev,"
		" should change tail value, and should return tail->prev");

	struct mem_block* tail = d_head->next;

	LOG("SANITY CHECK: TAIL->NEXT SHOULD BE NULL: %p\n", tail->next);

	free( get_data_from_header(tail) );
	free( get_data_from_header(tail->prev) );

	struct mem_block* prev = tail->prev;
	merged = merge_block(tail);

	LOG("MERGED AND PREV SHOULD HAVE SAME ADDRESS: '%s'\n", merged == prev ? "true" : "false");
	ll_log_list();
	print_memory();


	// Success
	LOGP("6. FREE EVERYTHING IN REGION 0 - then merge w/ a until can't anymore - list should just be (NULL) --> A --> END OF LIST --> (NULL)\n");

	struct mem_block* extra_block = e_head->next;
	free(c);
	free(e);
	free( get_data_from_header(extra_block) );

	LOGP("SANITY CHECK - EVERYTHING IN REGION 0 SHOULD BE FREE\n");
	print_memory();

	
	for (int i = 0; i < 100; i++) { // should make sure that excess calls to merge() do nothing
		merged = merge_block(a_head);

	}

	LOGP("LIST SHOULD BE A --> END OF LIST --> (NULL)\n");

	//print_memory();

	LOGP("GOOD\n");






}

// TODO: note: all the above functions were made when free() only set the header to free
// (no unmapping yet) - these functions won't work unless unmapping is turned off
int main(void)
{

	//test_ll_add();
	//test_print_memory();
	//test_malloc_name();
	//test_split();

	//test_allocations_1();

	//test_ll_delete_with_blocks();
	//test_ll_delete_with_malloc();
	
	//test_blocks_can_merge_with_malloc();

	//test_merge_block();
	//test_ll_delete_sole_block();
}