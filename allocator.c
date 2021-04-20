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

#include "allocator.h"
#include "logger.h"

#define BLOCK_ALIGN 8

static struct mem_block *g_head = NULL; /*!< Start (head) of our linked list */
static struct mem_block *g_tail = NULL;

static unsigned long g_allocations = 0; /*!< Allocation counter */
static unsigned long g_regions = 0; /*!< Allocation counter */

pthread_mutex_t alloc_mutex = PTHREAD_MUTEX_INITIALIZER; /*< Mutex for protecting the linked list */

size_t get_aligned_size(size_t total_size)
{
    return total_size % BLOCK_ALIGN == 0 ? total_size : total_size + (BLOCK_ALIGN - (total_size % BLOCK_ALIGN));
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
    // TODO: Maybe should align this, too
    size_t min_size = sizeof(struct mem_block) + BLOCK_ALIGN; // Should be 108

    if (size < min_size) {
        return NULL;
    }
    if (block == NULL || !block->free) {
        return NULL;
    }


    // TODO block splitting algorithm
    return NULL;
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
    // TODO block merging algorithm
    return NULL;
}

/**
 * Given a block size (header + data), locate a suitable location using the
 * first fit free space management algorithm.
 *
 * @param size size of the block (header + data)
 */
void *first_fit(size_t size)
{
    // TODO: first fit FSM implementation
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
    // TODO: worst fit FSM implementation
    return NULL;
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
    return NULL;
}

void *reuse(size_t size)
{
    // TODO: using free space management (FSM) algorithms, find a block of
    // memory that we can reuse. Return NULL if no suitable block is found.
    // 
    char *algo = getenv("ALLOCATOR_ALGORITHM");
    if (algo == NULL) {
        algo = "first_fit";
    }

    void* found = NULL;

    if (strcmp(algo, "first_fit") == 0) {
        found = first_fit(size);
    } else if (strcmp(algo, "best_fit") == 0) {
        found = best_fit(size);
    } else if (strcmp(algo, "worst_fit") == 0) {
        found = worst_fit(size);
    }

    if (found != NULL) {

        // TODO: Adjust linked list? Update data struct? maybe? Set free to false? Split block?
        // split_block(found, size);
    }
    return NULL;
}

void *malloc(size_t size)
{
    // TODO: allocate memory. You'll first check if you can reuse an existing
    // block. If not, map a new memory region.

    /* Lovingly ripped from lab code */

    // Have to make real_size divisible by 8, to comply w/ certain architectures
    size_t real_size = get_aligned_size(size + sizeof(struct mem_block));
    LOG("SIZES:\n"
        "\t->size = %zu\n"
        "\t->total size = %zu\n"
        "\t->aligned size = %zu\n",
        size, size + sizeof(struct mem_block), real_size);

    LOG("ALLOCATING BLOCK - SIZE %zu\n", real_size);



    struct mem_block* reused_block = reuse(real_size);

    if (reused_block != NULL) {
        return reused_block + 1;
    }

    int page_size = getpagesize();
    size_t num_pages = real_size / page_size;

    if (real_size % page_size != 0) {
        num_pages++;
    }

    size_t region_size = page_size * num_pages;
    LOG("Page size * num_pages; %d * %zu\n", page_size, num_pages);

    LOG("New region - size: %zu (%zu region(s))\n", region_size, num_pages);


    int prot_flags = PROT_READ | PROT_WRITE;
    int map_flags = MAP_PRIVATE | MAP_ANONYMOUS;

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

        new_block->prev = g_tail;
        g_tail->next = new_block;
        g_tail = new_block;

    }

    snprintf(new_block->name, 32, "Allocation %ld", g_allocations++);
    new_block->free = true;
    new_block->region_id = g_regions++;
    new_block->size = region_size;
    new_block->next = NULL;

    split_block(new_block, real_size);
    new_block->free = false;

    LOGP("RETURNING\n");


    return new_block + 1; // block is the memory header; block + 1 is the actual data
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

    while (current_block != NULL) {

        
        // If in new region, print out region string
        if (current_block ->region_id != current_region || current_block == g_head) {
            printf("[REGION %d] %p \n", current_block->region_id, current_block);
             
        }

        printf("    [BLOCK] %p-%p '%s' %zu [%s]\n",
                current_block,
                (char *)current_block + current_block->size, // do (char *) instead of (void *) b/c char * does 1 byte, whereas void * is undefined/platform-specific
                current_block->name,
                current_block->size,
                current_block->free ? "FREE" : "USED");   

        current_block = current_block->next;   
    }
}

