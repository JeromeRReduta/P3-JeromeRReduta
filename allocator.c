/**
 * @file
 *
 * Explores memory management at the C runtime level.
 *
 * To use (one specific command):
 * LD_PRELOAD=$(pwd)/allocator.so command
 * ('command' will run with your allocator)
 *
 * To use (all following commands):
 * export LD_PRELOAD=$(pwd)/allocator.so
 * (Everything after this point will use your custom allocator -- be careful!)
 */

#include <pthread.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <sys/mman.h>
#include <unistd.h>
#include <sys/mman.h>
#include <fcntl.h>
#include <stdlib.h>
#include <limits.h>

// #include "linked_list_util.h" // TODO: find a way to split this into own file
#include "allocator.h"
#include "logger.h"

#define BLOCK_ALIGN 8

static struct mem_block *g_head = NULL; /*!< Start (head) of our linked list */
static struct mem_block *g_tail = NULL;

static unsigned long g_allocations = 0; /*!< Allocation counter */
static unsigned long g_regions = 0; /*!< Allocation counter */
static int counter = 0;
pthread_mutex_t alloc_mutex = PTHREAD_MUTEX_INITIALIZER; /*< Mutex for protecting the linked list */

void ll_log_block(struct mem_block *block);
void ll_log_list();
bool blocks_can_merge(struct mem_block *block, struct mem_block *neighbor);

size_t get_aligned_size(size_t total_size)
{
    return total_size % BLOCK_ALIGN == 0 ? total_size : total_size + (BLOCK_ALIGN - (total_size % BLOCK_ALIGN));
}

size_t get_region_size(size_t real_size) {

    int page_size = getpagesize();
    size_t num_pages = real_size / page_size;

    if (real_size % page_size != 0) {
        num_pages++;
    }

    size_t region_size = page_size * num_pages;
    LOG("Page size * num_pages; %d * %zu\n", page_size, num_pages);

    LOG("New region - size: %zu (%zu region(s))\n", region_size, num_pages);

    return region_size;
}

void ll_add(struct mem_block *prev_block, struct mem_block *new_block)
{
    LOGP("STARTING LL_ADD\n");

    struct mem_block *next_block = prev_block->next;
    ll_log_block(new_block);
    ll_log_block(prev_block);

    if (new_block == prev_block->next) {
        return;
    }

    if (prev_block != NULL && new_block != NULL) {
        prev_block->next = new_block;
        new_block->prev = prev_block;

    }

    if (new_block != NULL && next_block != NULL) {
        next_block->prev = new_block;
        new_block->next = next_block;
    }

    
    LOGP("DONE:\n");
    ll_log_block(new_block);
    LOG("'%s' <-----\n",
        new_block->prev->prev->name);

/*
    if (prev_block == NULL || new_block == NULL) {
        LOGP("ERROR - prev or new block is NULL - RETURNING\n");
        return;
    }

    // If new_block is already in the list, do nothing
    if (new_block == prev_block->next) {
        return;
    }
    struct mem_block* temp = prev_block->next;


    LOG("STATS:\n"
        "\tPREV BLOCK: %p <----- %p -----> %p (also temp)\n"
        "\tNEW_BLOCK: %p <----- %p -----> %p\n",
        prev_block->prev, prev_block, prev_block->next,
        new_block->prev, new_block, new_block->next);




    prev_block->next = new_block;
    new_block->prev = prev_block;

  
    
    if (temp != NULL) {
        new_block->next = temp;
        temp->prev = new_block;
    }
 */
    //LOG("FROM ll_add - NEW BLOCK AFTER (prev this and next): %p <----- %p -----> %p\n", new_block->prev, new_block, new_block->next);


}

void ll_delete(struct mem_block *block) {

    struct mem_block* prev = block->prev;
    struct mem_block* next = block->next;

    // For now, not considering case: "deleting head" - may have to add this later
    prev->next = next;

    // Case: next exists - link next w/ prev
    if (next != NULL) {
        next->prev = prev;    
    }

    // Case: block was g_tail - make g_tail = prev
    if (block == g_tail) {
        g_tail = prev;
    }
    
}

void ll_log_block(struct mem_block *block) {

    if (block == NULL) {
        LOGP("NULL BLOCK\n");
        return;
    }
    LOG(" %p <----- %p -----> %p\n", block->prev, block, block->next);

    LOG(" '%s' <----- '%s' -----> '%s'\n",
        block->prev != NULL ? block->prev->name : "NULL BLOCK",
        block->name,
        block->next != NULL ? block->next->name: "NULL BLOCK");

    if (block->next != NULL && strcmp(block->next->name, "Test Allocation: 4") == 0) {
        LOGP("4TH ALLOCATION DETECTED\n");
        LOG("\t----->'%s'\n", block->next->next->name);
    }
    LOGP("\n");
}

void ll_log_list() {

    LOG("STARITNG FROM HEAD: %p '%s'------------------------------------------------------------------------------------------------->\n", g_head, g_head != NULL ? g_head->name : "NULL BLOCK");
    struct mem_block *current = g_head;

    while (current != NULL) {
        /*
        ll_log_block(current);
        */


        LOG("%p -----> %p\n", current, current->next);
        LOG("'%s' -----> '%s'\n\n", current->name, current->next != NULL ? current->next->name : "NULL BLOCK");
       current = current->next;
    }
}


/**
 * Given a free block, this function will split it into two pieces and update
 * the linked list.
 *
 * @param block the block to split
 * @param size new size of the first block after the split is complete,
 * including header sizes. The size of the second block will be the original
 * block's size minus this parameter.
 *
 * @return address of the resulting second block (the original address will be
 * unchanged) or NULL if the block cannot be split.
 */
struct mem_block *split_block(struct mem_block *block, size_t size)
{

    // Note: size is actual real size (including header)
    LOGP("SPLITTING BLOCK\n");

    size_t min_sz = sizeof(struct mem_block) + BLOCK_ALIGN;
    LOG("STATS:\n"
        "\t->block is null? %d\n"
        "\t->size: %zu\n"
        "\t->min size: %zu\n",
        block == NULL, size, min_sz);

    LOG("Must all be 0 (false):\n"
        "\t-> size < min_sz: %d\n"
        "\t->block == NULL: %d\n"
        "\t->!block->free: %d\n",
        size < min_sz, block == NULL, !block->free);

    if (size < min_sz || block == NULL || !block->free) {
        LOGP("INVALID - RETURNING NULL\n");
        return NULL;
    }

    if (block->size - size < min_sz) {
        LOG("NEW BLOCK WOULD BE SIZE: %d - too small returning null\n", block->size - size);
        return NULL;
    }

    LOGP("STARTING LEFTOVER_DATA_HEADER INIT\n");
    struct mem_block* leftover_data_header = (struct mem_block *) ( (char *)block + size  );


    /* REGION: Init leftover_data_header */

    LOGP("SETTING FREE TO TRUE\n");
    leftover_data_header->free = true;

    LOGP("CHANGING PREV VALUE\n");
    leftover_data_header->prev = NULL;

    LOGP("CHANGING NEXT VALUE\n");
    leftover_data_header->next = NULL;


    LOGP("CHANGING REGION ID\n");
    leftover_data_header->region_id = block->region_id;
    LOGP("BOOKMARK\n");
    LOG("block->free: %d leftover_data_header->free: %d\n", block->free, leftover_data_header->free);
    LOG("block->region_id: %lu leftover data header->region_id %lu\n", block->region_id, leftover_data_header->region_id);

    leftover_data_header->size = block->size - size;
    if (leftover_data_header->size < 0) {
        LOGP("WARNING - NEGATIVE SIZE\n");
    }

    LOGP("DONE INITALIZING DATA HEADER\n");

    /* REGION: Change size of block */
    block->size = size;

    /* REGION: Update linked list and tail */
    ll_add(block, leftover_data_header);
    LOGP("DONE W/ LL_ADD\n");
    ll_log_block(leftover_data_header);


    if (block == g_tail) {
        g_tail = leftover_data_header;
        g_tail->next = NULL;
    }


    LOGP("RETURNING LEFTOVER DATA HEADER\n");


    return leftover_data_header;

}

/**
 * Given a free block, this function attempts to merge it with neighboring
 * blocks --- both the previous and next neighbors --- and update the linked
 * list accordingly.
 *
 * @param block the block to merge
 *
 * @return address of the merged block or NULL if the block cannot be merged.
 */
struct mem_block *merge_block(struct mem_block *block)
{
    struct mem_block* header = block;

    if (blocks_can_merge(block, block->next)) {
        block->size += block->next->size;
        ll_delete(block->next);
    }

    if (blocks_can_merge(block, block->prev)) {
        header = block->prev;
        block->prev->size += block->size;
        ll_delete(block);
    }

    return header;
}

/* Returns true if the following are true:
    1. neighbor exists
    2. neighbor and block have same region ids
    3. block and neighbor are both free (will instantly return false if block is not free)

    Else, returns false
    */
bool blocks_can_merge(struct mem_block *block, struct mem_block *neighbor) {

    // Neighbor check
    if (block->next != neighbor && block->prev != neighbor) {
        LOGP("ERROR - NOT NEIGHBORS - RETURNING NULL\n");
        return NULL;
    }

    if (!block->free) {
        LOG("ERROR - USED BLOCK: '%s' - returning immediately\n", !block->free ? "USED" : "FREE");
        return false;
    }

    return neighbor != NULL && block->region_id == neighbor->region_id && neighbor->free;

}

/**
 * Given a block size (header + data), locate a suitable location using the
 * first fit free space management algorithm.
 *
 * @param size size of the block (header + data)
 */
void *first_fit(size_t size)
{

    // Note: From Prof. Matthew: "If you get segfault here - probably something else is wrong"

    LOGP("STARTING FIRST_FIT\n");
    struct mem_block *current = g_head;

    while (current != NULL) {

        //ll_log_block(current);

/*
        LOG("%p <----- %p -----> %p\n", current->prev, current, current->next);
        LOG("'%s' <----- '%s' ['%s'] -----> '%s'\n",
            current->prev != NULL ? current->prev->name : "NULL BLOCK",
            current->name, current->free ? "FREE" : "USED",
            current->next != NULL ? current->next->name : "NULL BLOCK");
        LOG("SIZE %zu, CURRENT_SIZE %zu, SIZE < CURRENT->SIZE %d \n", size, current->size, size <= current->size);
        
  */      
        if (size <= current->size && current->free) {
            return current;
        }
        current = current->next;
    }

    return NULL;

}

/**
 * Given a block size (header + data), locate a suitable location using the
 * worst fit free space management algorithm. If there are ties (i.e., you find
 * multiple worst fit candidates with the same size), use the first candidate
 * found.
 *
 * @param size size of the block (header + data)
 */
void *worst_fit(size_t size)
{
    struct mem_block *current = g_head;
    struct mem_block *worst = NULL;

    size_t worst_delta = 0;

    while (current != NULL) {
        if (size <= current->size && current->free) {

            ssize_t diff = abs((ssize_t)size - (ssize_t)current->size);

            if (diff > worst_delta) {
                worst = current;
                worst_delta = diff;
            }



        } 
        current = current->next;
    }

    return worst;
}

/**
 * Given a block size (header + data), locate a suitable location using the
 * best fit free space management algorithm. If there are ties (i.e., you find
 * multiple best fit candidates with the same size), use the first candidate
 * found.
 *
 * @param size size of the block (header + data)
 */
void *best_fit(size_t size)
{
    // TODO: best fit FSM implementation
    struct mem_block *current = g_head;
    struct mem_block *best = NULL;

    ssize_t best_delta = INT_MAX;

    while (current != NULL) {
        if (size <= current->size && current->free) {

            ssize_t diff = abs((ssize_t)size - (ssize_t)current->size);

            if (diff < best_delta) {
                best = current;
                best_delta = diff;
            }



        } 
        current = current->next;
    }

    return best;
/*
    struct mem_block *current = g_head;
    struct mem_block *best = NULL;

    size_t best_size = INT_MAX;

    while (current != NULL) {


        //ssize_t diff = (ssize_t) nums get difference between current block size and best block size here


    }
    */
    return NULL;
}

void *reuse(size_t size)
{
    // TODO: using free space management (FSM) algorithms, find a block of
    // memory that we can reuse. Return NULL if no suitable block is found.
    // 
    LOG("REAL SIZE IS %zu\n", size);
    char *algo = getenv("ALLOCATOR_ALGORITHM");
    if (algo == NULL) {
        algo = "first_fit";
    }

    struct mem_block* found = NULL;

    LOGP("STARTING REUSE\n");

    if (strcmp(algo, "first_fit") == 0) {
        LOGP("SHOULD DO FIRST_FIT HERE\n");
        found = first_fit(size);
        
    } else if (strcmp(algo, "best_fit") == 0) {
        found = best_fit(size);
    } else if (strcmp(algo, "worst_fit") == 0) {
        found = worst_fit(size);
    }



    if (found != NULL) {

        // TODO: Adjust linked list? Update data struct? maybe? Set free to false? Split block?
        struct mem_block* new_head = split_block(found, size); // Note - only split if you actually can split block


        if (new_head != NULL) {
            new_head->region_id = found->region_id;
            new_head->free = true;
            LOG("NEW HEAD:\n"
                "\t%p<----- %p -----> %p\n"
                "\t'%s'<----- '%s' -----> '%s'\n",
                new_head->prev, new_head, new_head->next,
                new_head->prev->name, new_head->name, new_head->next->name);

            ll_add(found, new_head);
            
        }

        found->free = false;

        //LOG("FROM REUSE: %p <----- %p -----> %p\n", found->prev, found, found->next);
        return found;
    }
    return NULL;
}

void *malloc(size_t size)
{
    LOGP("___________________STARTING MALLOC________________________\n");
    LOG("ADDING ALLOCATION %d\n", g_allocations++);
    LOG("MALLOC - G_TAIL IS %p '%s' -----> '%s'\n", g_tail, g_tail != NULL ? g_tail->name : "NULL BLOCK");
    // TODO: allocate memory. You'll first check if you can reuse an existing
    // block. If not, map a new memory region.

    /* Lovingly ripped from lab code */

    // Have to make real_size divisible by 8, to comply w/ certain architectures

    /* REGION: Get real size */
    size_t real_size = get_aligned_size(size + sizeof(struct mem_block));

    LOG("SIZES:\n"
        "\t->size = %zu\n"
        "\t->total size = %zu\n"
        "\t->aligned size = %zu\n",
        size, size + sizeof(struct mem_block), real_size);




    /* REGION: REUSING A REGION */
    // Error - not actually getting anything from reuse()???
    struct mem_block* reused_block = reuse(real_size);
    //LOG("DONE REUSING - BLOCK IS NULL? '%s'\n", reused_block == NULL ? "NULL" : "EXISTS");

    if (reused_block != NULL) {

        char *scribble = getenv("ALLOCATOR_SCRIBBLE");
        LOG("SCRIBBLE IS: %c\n", scribble);

        if (scribble != NULL && strcmp(scribble, "1") == 0) {
            LOGP("SCRIBBLE IS 1 - setting data to 170\n");

            memset(reused_block + 1, 170, real_size - sizeof(struct mem_block));
        }

       // LOG("USING REUSED BLOCK IN REGION %lu INSTEAD\n", reused_block->region_id);
        return reused_block + 1;
    }




    /* REGION: ALLOCATING A NEW REGION? */
    size_t region_size = get_region_size(real_size);
    const int prot_flags = PROT_READ | PROT_WRITE;
    const int map_flags = MAP_PRIVATE | MAP_ANONYMOUS;

    struct mem_block *new_block
        = mmap(NULL, region_size, prot_flags, map_flags, -1, 0);

    if (new_block == MAP_FAILED) {
        perror("mmap");
    }

    if (g_head == NULL && g_tail == NULL) {
        g_head = new_block;
        g_tail = g_head;
        new_block->prev = NULL;
    }
    else {

        LOG("G_TAIL IS: %p '%s'\n", g_tail, g_tail != NULL ? g_tail->name : "NULL BLOCK");

        ll_add(g_tail, new_block);
        g_tail = new_block;

        LOG("NEW TAIL IS: '%s' <----- '%s'\n", g_tail->prev != NULL ? g_tail->prev->name : "NULL BLOCK", g_tail->name);
        
/*
        LOG("STATS:\n"
            "\t->G_TAIL: %p\n"
            "\t->G_TAIL->NEXT",
            g_tail = new_block);
        */
    }

    //snprintf(new_block->name, 32, "Allocation %ld", g_allocations++);
    new_block->region_id = g_regions++;
    new_block->free = true;
    new_block->size = region_size;
    new_block->next = NULL;
    
    //LOGP("PRINTING STUFF\n");

    struct mem_block *leftover_data_header = split_block(new_block, real_size);
    LOGP("MALLOC - LEAVING SPLIT_BLOCK\n");
    new_block->free = false;



    char *scribble = getenv("ALLOCATOR_SCRIBBLE");

    LOG("SCRIBBLE IS: %c\n", scribble);

    if (scribble != NULL && strcmp(scribble, "1") == 0) {
        LOGP("SCRIBBLE IS 1 - setting data to 170\n");

        memset(new_block + 1, 170, real_size - sizeof(struct mem_block));
    }

/*
    if (leftover_data_header->prev != NULL && leftover_data_header->prev->prev != NULL && leftover_data_header->prev->prev->prev != NULL && leftover_data_header->prev->prev->prev->prev != NULL) {
     LOG("'%s'<------ '%s' <----- '%s' <----- '%s' <----- '%s' \n", leftover_data_header->prev->prev->prev->prev->name, leftover_data_header->prev->prev->prev->name, leftover_data_header->prev->prev->name, leftover_data_header->prev->name, leftover_data_header->name);
    }
*/
    LOGP("RETURNING NEW_BLOCK + 1\n");
    return new_block + 1; // block is the memory header; block + 1 is the actual data
}

void *malloc_name(size_t size, char *name)
{

    LOG("________________ADDING ALLOCATION: '%s'________________________\n", name);

    void* block = malloc(size);

    struct mem_block* head = (struct mem_block *)( (char*)block - 100 );
    strcpy(head->name, name);

    // ll_log_block(head);


    return block;
}

/* Copying extremely helpful diagram from class:
 * 
 * aloocate(50) with header:
 * 
 * [HHHHHHHHHH][XXXXX]
 * |            |
 * |            |
 * |            user wants this pointer - malloc returns ptr to here
 * |
 * mmap returns ptr here
 * 
 */

void free(void *ptr)
{
    if (ptr == NULL) {
        /* Freeing a NULL pointer does nothing */
        return;
    }

    LOG("FREEING %p\n", ptr);
    struct mem_block *block = (struct mem_block *) ptr - 1;
    block->free = true;

    /* Commented all code out - so that free() code doesn't mess up any behavior

    TODO: Work on this after merging, splitting, calloc, malloc, etc. all work
    struct mem_block* block_stats = (struct mem_block *)ptr - 1;

    LOG("Freeing ptr - Address: %p\t Size: %zu\n", ptr, block_stats->size);


    LOGP("UNMAPPING\n");
    munmap(block_stats, block_stats->size);
    */




    // TODO: free memory. If the containing region is empty (i.e., there are no
    // more blocks in use), then it should be unmapped.
}


// Malloc then write 0s over all of memory
void *calloc(size_t nmemb, size_t size)
{
    void* block = malloc(nmemb * size);

    LOGP("CLEARING MEMORY\n");

    memset(block, 0, nmemb * size);

    return block;
}

void *realloc(void *ptr, size_t size)
{
    if (ptr == NULL) {
        /* If the pointer is NULL, then we simply malloc a new block */
        LOGP("Null pointer - just gonna do malloc\n");
        return malloc(size);
    }

    if (size == 0) {
        /* Realloc to 0 is often the same as freeing the memory block... But the
         * C standard doesn't require this. We will free the block and return
         * NULL here. */

        LOGP("size == 0 - freeing\n");
        free(ptr);
        return NULL;
    }

    LOGP("ACTUALLY DOING REALLOC\n");
    // TODO: reallocation logic

    return NULL;
}

/**
 * print_memory
 *
 * Prints out the current memory state, including both the regions and blocks.
 * Entries are printed in order, so there is an implied link from the topmost
 * entry to the next, and so on.
 */
void print_memory(void)
{

    puts("-- Current Memory State --");
    struct mem_block *current_block = g_head;
    // struct mem_block *current_region = NULL; ???
    

    unsigned long current_region = 0;
    // TODO implement memory printout

    int counter = 0;
    while (current_block != NULL) {

        
        // If in new region, print out region string
        if (current_block ->region_id != current_region || current_block == g_head) {
            printf("[REGION %lu] %p \n", current_block->region_id, current_block);
            current_region = current_block->region_id;
             
        }

        printf("    [BLOCK] %p-%p in region %lu '%s' %zu [%s] -> %p\n",
                current_block,
                (struct mem_block *) ( (char *)current_block + current_block->size ), // do (char *) instead of (void *) b/c char * does 1 byte, whereas void * is undefined/platform-specific
                current_block->region_id,
                current_block->name != NULL ? current_block->name : "(NULL)",
                current_block->size,
                current_block->free ? "FREE" : "USED",
                current_block->next);   

        current_block = current_block->next;   
    }
}









