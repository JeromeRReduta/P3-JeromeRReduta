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

#include "allocator.h"
#include "logger.h"

#define BLOCK_ALIGN 8

static struct mem_block *g_head = NULL; /*!< Start (head) of our linked list */
static struct mem_block *g_tail = NULL;

static unsigned long g_regions = 0; /*!< Allocation counter */
pthread_mutex_t alloc_mutex = PTHREAD_MUTEX_INITIALIZER; /*< Mutex for protecting the linked list */

static int g_allocations = 0;

static int counter =0;


/* Func protoyptes */
void ll_log_block(struct mem_block *block);
void ll_log_list();
void ll_add(struct mem_block *prev_block, struct mem_block *new_block);
void ll_delete(struct mem_block *block);

bool blocks_can_merge(struct mem_block *block, struct mem_block *neighbor);

int try_to_expand_block_into(struct mem_block *head, struct mem_block *next, size_t size);

void *get_data_from_header(struct mem_block *header);
struct mem_block *get_header_from_data(void *data);

size_t get_diff(size_t first, size_t second);

void scribble_if_requested(struct mem_block *block, size_t real_size);

struct mem_block *map_new_region(size_t real_size);


/**
 * @brief      If able, expands a used block into next block. Else, does nothing
 *
 * @param      head  used block
 * @param      next  block right after head
 * @param      size  requested size
 * 
 * @return     0 if can't merge, 1 if it can and did
 */
int try_to_expand_block_into(struct mem_block *head, struct mem_block *next, size_t size)
{
    if (!blocks_can_merge(head, next) || head->size + next->size < size) {
        return 0;
    }

    head->size += next->size;
    ll_delete(next);
    return 1;
}

/**
 * @brief      Tells whether a given block is the only one in its region (i.e. its neighbors have different region ids)
 *
 * @param      block  block
 *
 * @return     True if block is the only one in its region. Else, false
 */
bool is_only_block_in_region(struct mem_block *block)
{
    struct mem_block* prev = block->prev;
    struct mem_block* next = block->next;

    bool no_prev_neighbors = prev == NULL || prev->region_id != block->region_id;
    bool no_next_neighbors = next == NULL || next->region_id != block->region_id;

    return no_prev_neighbors && no_next_neighbors;
}

/**
 * @brief      Given a header, returns the data block associated with it
 *
 * @param      header  header
 *
 * @return     data block associated with header
 */
void *get_data_from_header(struct mem_block *header)
{
    return header != NULL ? header + 1 : NULL;
}

/**
 * @brief      Given a block of data, returns the header associated with it
 *
 * @param      data  data block
 *
 * @return     header associated with data block
 */
struct mem_block *get_header_from_data(void *data)
{
    return data != NULL ? (struct mem_block *)data - 1 : NULL;
}

/**
 * @brief      Gets total_size, aligned to be divisble by 8, to work with certain architectures
 *
 * @param[in]  total_size  total size
 *
 * @return     total_size, increased until it is divisible by 8
 */
size_t get_aligned_size(size_t total_size)
{
    return total_size % BLOCK_ALIGN == 0 ? total_size :
        total_size + (BLOCK_ALIGN - (total_size % BLOCK_ALIGN));
}

/**
 * @brief      Gets size of a new region
 *
 * @param[in]  real_size  size of data block + size of header
 *
 * @return     real_size, increased until it is divisible by getpagesize() (as of time
 *              of this writing, 4096)
 */
size_t get_region_size(size_t real_size)
{
    int page_size = getpagesize();

    size_t num_pages = real_size % page_size != 0 ? real_size / page_size + 1 :
        real_size / page_size;

    return page_size * num_pages;
}

/**
 * @brief      Adds a block to a block that already exists in a linked list
 *
 * @param      prev_block  block we add new block to
 * @param      new_block   block we add to linked list
 */
void ll_add(struct mem_block *prev_block, struct mem_block *new_block)
{


    sprintf(new_block->name, "New block %d", g_allocations + 1);
    ll_log_block(prev_block);
    ll_log_block(new_block);

    LOGP("BOOKMARK\n");
    struct mem_block *next_block = prev_block->next;

    if (new_block == prev_block->next) {
        LOGP("RETURNING\n");
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

    LOGP("DONE\n");
    ll_log_block(prev_block);





}

/**
 * @brief      Deletes a block from the linked list, updating g_head and g_tail as needed
 *
 * @param      block  block we will delete
 */
void ll_delete(struct mem_block *block) {

    if (block == NULL) {
        return;
    }

    struct mem_block* prev = block->prev;
    struct mem_block* next = block->next;

    // Case: prev exists - link prev w/ next
    if (prev != NULL) {
        prev->next = next;    
    }
    // Case: next exists - link next w/ prev
    if (next != NULL) {
        next->prev = prev;    
    }
    // Case: block was g_tail - make g_tail = prev
    if (block == g_tail) {
        g_tail = prev;
    }

    // Case: block was g_head - make g_head = next
    if (block == g_head) {
        g_head = next;
    }
}

/**
 * @brief      Logs info from a given block
 *
 * @param      block  memory block
 * 
 * @note       Warning: Useful but segfaults often - if there are segfaults in code, might want
 *              to comment out calls to this func first
 */
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

/**
 * @brief      Logs info from the whole linked list
 * 
 * @note       Not using ll_log_block to avoid segfaults
 */
void ll_log_list() {

    LOG("STARITNG FROM HEAD: %p '%s'------------------------------------------------------------------------------------------------->\n", g_head, g_head != NULL ? g_head->name : "NULL BLOCK");
    struct mem_block *current = g_head;

    while (current != NULL) {
        LOG("%p -----> %p\n", current, current->next);
        LOG("'%s' -----> '%s'\n\n", current->name, current->next != NULL ? current->next->name : "NULL BLOCK");
       current = current->next;
    }
}




/**
 * @brief      Given a free block, this function will split it into two pieces and update
 *              the linked list.
 *
 * @param      block  the block to split
 * @param[in]  size   new size of the first block after the split is complete,
 *                      including header sizes. The size of the second block will be the original
 *                      block's size minus this parameter.
 *
 * @return     address of the resulting second block (the original address will be
 *              unchanged) or NULL if the block cannot be split.
 *              
 * @note       This description is from Prof. Matthew. The only change is that it's been reformatted.
 *              Also, size includes header size
 */
struct mem_block *split_block(struct mem_block *block, size_t size)
{
    size_t min_sz = sizeof(struct mem_block) + BLOCK_ALIGN;

    if (size < min_sz || block == NULL || !block->free) {
        LOGP("INVALID - RETURNING NULL\n");
        return NULL;
    }
    if (block->size - size < min_sz) {
        LOG("NEW BLOCK WOULD BE SIZE: %zu - too small returning null\n", block->size - size);
        return NULL;
    }

    struct mem_block* leftover_data_header = (struct mem_block *) ( (char *)block + size  );

    leftover_data_header->free = true;
    leftover_data_header->prev = NULL;
    leftover_data_header->next = NULL;
    leftover_data_header->region_id = block->region_id;
    leftover_data_header->size = block->size - size;

    /* REGION: Change size of block */
    block->size = size;

    /* REGION: Update linked list and tail */
    ll_add(block, leftover_data_header);

    if (block == g_tail) {
        g_tail = leftover_data_header;
        g_tail->next = NULL;
    }

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

/**
 * @brief       Given a free block, this function attempts to merge it with neighboring
                blocks --- both the previous and next neighbors --- and update the linked
                list accordingly.
 *
 * @param      block  the block to merge
 *
 * @return     address of the merged block or NULL if the block cannot be merged.
 *
 * @note       Again Prof. Matthew's words, reformatted
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

/**
 * @brief      Returns true if the following are true:
 *              1. neighbor exists
 *              2. neighbor and block have same region ids
 *              3. neighbor and block are both free 
 *
 * @param      block     block
 * @param      neighbor  neighboring block (will check in function to make sure this is the case)
 *
 * @return     true if the above conditions are all true, else false
 */
bool blocks_can_merge(struct mem_block *block, struct mem_block *neighbor) {

    if (!block->free) {
       LOG("ERROR - BLOCK IS USED: '%s' - returning immediately\n", !block->free ? "USED" : "FREE");
       return false;
    }

    if (block->next != neighbor && block->prev != neighbor) {
        LOGP("ERROR - NOT NEIGHBORS - RETURNING FALSE\n");
        return false;
    }

    return neighbor != NULL && block->region_id == neighbor->region_id && neighbor->free;
}

/**
 * @brief      Returns absolute difference between first and second
 *
 * @param[in]  first   size value
 * @param[in]  second  other size value
 *
 * @return     absolute difference between first and second
 */
size_t get_diff(size_t first, size_t second)
{
    return abs( (ssize_t)first - (ssize_t)second );

}

/**
 * Given a block size (header + data), locate a suitable location using the
 * first fit free space management algorithm.
 *
 * @param size size of the block (header + data)
 */
/**
 * @brief      Given a block size (header + data), locate a suitable location using the
 *              first fit free space management algorithm.
 *
 * @param[in]  size  size of the block (header + data)
 *
 * @return     First matching block, or NULL if no match
 * 
 * @note       brief and params are Prof. Matthew's words. Same with best_fit and worst_fit. Also Prof. Matthew's words: "If you get segfault here - probably something else is wrong"
 */
void *first_fit(size_t size)
{
    LOGP("START FIRST_FIT------------------------------------------------------------------\n");
    struct mem_block *current = g_head;

    while (current != NULL) {
        // Case: found match - return header

        //ll_log_block(current);



        LOG("%p ['%s'] -----> %p ['%s']\n", current, current->free ? "FREE" : "USED", current->next, current->next != NULL && current->next->free ? "FREE" : "USED OR NULL");

        LOG("'%s' -----> '%s'\n", current->name, current->next != NULL ? current->next->name : "NULL BLOCK");

        if (current->prev != NULL && current->prev->prev != NULL && current->prev->prev->prev != NULL) {
           /*
            LOG("STUUUUUUUUUUUUF: %p <-- %p <-- %p <-- %p\n", current->prev->prev->prev, current->prev->prev, current->prev, current);
            LOG("STUUUUUUUUUUUUF: '%s' <-- '%s' <-- '%s' <-- '%s'\n", current->prev->prev->prev->name, current->prev->prev->name, current->prev->name, current->name);

            LOG("BUT FORWARD NOW: %p --> %p --> %p --> %p\n",
                current->prev->prev->prev, current->prev->prev->prev->next, current->prev->prev->prev->next->next, current->prev->prev->prev->next->next->next);

            LOG("BUT FORWARD NOW: '%s' --> '%s' --> '%s' --> '%s'\n________________________________________________________________________________________________________________________________________________________________\n",
                current->prev->prev->prev->name, current->prev->prev->prev->next->name, current->prev->prev->prev->next->next->name, current->prev->prev->prev->next->next->next->name);
        
            LOGP("PREV->PREV->PREV\n");
            ll_log_block(current->prev->prev->prev);

            LOGP("PREV->-PREV\n");
            ll_log_block(current->prev->prev);

            LOGP("PREV->PREV->PREV->NEXT\n");
            ll_log_block(current->prev->prev->prev->next);
*/
        }


   
        if (size <= current->size && current->free) {
            LOGP("DONE FIRST_FIT------------------------------------------------------------------\n");
            return current;
        }

        current = current->next;
    }
    // Case: no match - return NULL
    LOGP("DONE FIRST_FIT------------------------------------------------------------------\n");
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

/**
 * @brief       Given a block size (header + data), locate a suitable location using the
 *              worst fit free space management algorithm. If there are ties (i.e., you find
 *              multiple worst fit candidates with the same size), use the first candidate
 *              found.
 * @param[in]  size  size of the block (header + data)
 *
 * @return     Pointer to worst fitting block
 * 
 * @note       See first_fit note
 */
void *worst_fit(size_t size)
{
    struct mem_block *current = g_head;
    struct mem_block *worst = NULL;

    size_t worst_delta = 0;

    while (current != NULL) {

        // Case: diff > worst_delta - update worst and worst delta to current and diff
        if (size <= current->size && current->free) {
            ssize_t diff = get_diff(current->size, size);

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

 *
 * @param size size of the block (header + data)
 */
/**
 * @brief       Given a block size (header + data), locate a suitable location using the
 *              best fit free space management algorithm. If there are ties (i.e., you find
 *              multiple best fit candidates with the same size), use the first candidate
 *              found.
 *
 * @param[in]  size  size of the block (header + data)
 *
 * @return     best fitting header
 * 
 * @note       See first_fit note
 */
void *best_fit(size_t size)
{
    struct mem_block *current = g_head;
    struct mem_block *best = NULL;

    ssize_t best_delta = INT_MAX;

    while (current != NULL) {
        if (size <= current->size && current->free) {
            ssize_t diff = get_diff(current->size, size);

            if (diff < best_delta) {
                best = current;
                best_delta = diff;
            }
        } 

        current = current->next;
    }

    return best;
}

/**
 * @brief      Finds the best-suiting block and returns it, based on what FSM algorithm is being used.
 *
 * @param[in]  size  requested size (includes header)
 *
 * @return     best-suiting block based on FSM algorithm, or NULL if no suitable block
 * 
 * @note       Description based off Prof. Matthew's description of this function
 */
void *reuse(size_t size)
{
    char *algo = getenv("ALLOCATOR_ALGORITHM");
    if (algo == NULL) {
        algo = "first_fit";
    }

    struct mem_block* found = NULL;

    if (strcmp(algo, "first_fit") == 0){
        found = first_fit(size);
    }
    else if (strcmp(algo, "best_fit") == 0) {
        found = best_fit(size);
    }
    else if (strcmp(algo, "worst_fit") == 0) {
        found = worst_fit(size);
    }

    // Case: FSM algo found match - initalize leftover data (new_head) and return found
    if (found != NULL) {
        struct mem_block* new_head = split_block(found, size); // Note - only split if you actually can split block

        if (new_head != NULL) {
            new_head->region_id = found->region_id;
            new_head->free = true;
            //ll_add(found, new_head);
        }

        found->free = false;
        return found;
    }
    // Case: No match - return NULL
    return NULL;
}

/**
 * @brief      Allocates memory with initalizing. If ALLOCATOR_SCRIBBLE environment var is set to 1, will initalize every byte to
 *              0XAA to, as Apple puts it, "increase the chance that a program making assumptions about newly allocated memory fails" 
 *
 * @param[in]  size  size (not including header)
 *
 * @return     A block of data with its size = size
 */
void *malloc(size_t size)
{
    //pthread_mutex_lock(&alloc_mutex);
    /* Lovingly ripped from lab code */


    /* REGION: Get real size */
    size_t real_size = get_aligned_size(size + sizeof(struct mem_block));
   
    /* REGION: REUSING A REGION */
    // Error - not actually getting anything from reuse()???
    struct mem_block* reused_block = reuse(real_size);

    if (reused_block != NULL) {
        LOGP("ba\n");
        sprintf(reused_block->name, "Allocation %d", g_allocations++);
        scribble_if_requested(reused_block, real_size);
        //pthread_mutex_unlock(&alloc_mutex);

        return reused_block + 1;
    }

    struct mem_block *new_block = map_new_region(real_size);

    if (new_block == MAP_FAILED) {
        perror("mmap");
    }

    if (g_head == NULL && g_tail == NULL) {
        g_head = new_block;
        g_tail = g_head;
        new_block->prev = NULL;
    }
    else {
        ll_add(g_tail, new_block);
        g_tail = new_block;
        g_tail->next = NULL;
    }

    new_block->region_id = g_regions++;
    new_block->free = true;
    new_block->size = get_region_size(real_size);
    new_block->next = NULL;
    
    split_block(new_block, real_size);
    new_block->free = false;

    scribble_if_requested(new_block, real_size);
    sprintf(new_block->name, "Allocation %d", g_allocations++);
    //pthread_mutex_unlock(&alloc_mutex);
    return new_block + 1; // block is the memory header; block + 1 is the actual data
}

/**
 * @brief      Literally the same as malloc, but writes a custom name in the header
 *
 * @param[in]  size  requested size
 * @param      name  name to write in header
 *
 * @return     Block of data with its size = size
 */
void *malloc_name(size_t size, char *name)
{
    void* block = malloc(size);

    struct mem_block* head = get_header_from_data(block);
    strcpy(head->name, name);

    return block;
}

/**
 * @brief      If ALLOCATOR_SCRIBBLE enviornment var is set to 1, sets every byte in block to 0XAA to, as 
 *              Apple puts it in its own scribbler, "increase the chance that programs that make certain
 *              assumptions about uninitialized memory fail"
 *
 * @param      head header of block
 * @param[in]  real_size  real size, including header size
 */
void scribble_if_requested(struct mem_block *head, size_t real_size)
{
    char *scribble = getenv("ALLOCATOR_SCRIBBLE");

    if (scribble != NULL && strcmp(scribble, "1") == 0) {
        memset(get_data_from_header(head), 0XAA, real_size - sizeof(struct mem_block));
    }
}

/**
 * @brief      Maps a new region and returns the start as a header
 *
 * @param[in]  real_size  size, including header size
 *
 * @return     pointer to start/header of mapped region
 */
struct mem_block *map_new_region(size_t real_size)
{
    size_t region_size = get_region_size(real_size);
    const int prot_flags = PROT_READ | PROT_WRITE;
    const int map_flags = MAP_PRIVATE | MAP_ANONYMOUS;

    return (struct mem_block *) mmap(NULL, region_size, prot_flags, map_flags, -1, 0);
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

/**
 * @brief      Does the following things to a block of data:
 *              1. If it's NULL, does nothing
 *              2. Otherwise, sets ->free to true
 *                  2.1. Always attempts to merge neighboring free blocks together
 *                  2.2. If also the only block left in its region, unmaps it
 *
 * @param      ptr   ptr to block of data
 */
void free(void *ptr)
{
    //pthread_mutex_lock(&alloc_mutex);
    if (ptr == NULL) {
        pthread_mutex_unlock(&alloc_mutex);
        return;
    }

    struct mem_block *block = get_header_from_data(ptr);
    block->free = true;

    merge_block(block); // Attempt to merge block

    if (is_only_block_in_region(block)) { // This should also handle g_head or g_tail change
        ll_delete(block);
        munmap(block, block->size);
    }

    //pthread_mutex_unlock(&alloc_mutex);
}


/**
 * @brief      Mallocs, then writes 0's over all of data block
 * @param[in]  nmemb  number of type members being allocated (e.g. array.length() if allocating an array)
 * @param[in]  size   size of each member
 *
 * @return     pointer to block of data
 */
void *calloc(size_t nmemb, size_t size)
{
    void* block = malloc(nmemb * size);
    memset(block, 0, nmemb * size);

    return block;
}

/**
 * @brief      Does the following to a pointer to a data block, given a size:
 *              1. If ptr is NULL, runs malloc(size)
 *              2. If size == 0, runs free(ptr)
 *              3. Else:
 *                  3.1. If data block can expand into the next neighboring one (next neighbor must be free), does so
 *                  3.2. IF it can't, mallocs a new block, copies data from old block to new block, and frees old block
 *
 * @param      ptr   pointer to data block
 * @param[in]  size  requested size
 *
 * @return     pointer to processed
 */
void *realloc(void *ptr, size_t size)
{
    LOGP("REALLOC CALLED\n");
    // Case: ptr is NULL - do malloc(size)
    if (ptr == NULL) {
        return malloc(size);
    }
    // Case: size == 0 - do free(ptr) 
    else if (size == 0) {
        free(ptr);
        return NULL;
    }

    // Case: ptr != NULL, size != 0, reallocate data
    struct mem_block* head = get_header_from_data(ptr);
    struct mem_block* next = head->next;

    // Case: can expand block into head->next - do so and return header
    if (try_to_expand_block_into(head, next, size) == 1) {
        return get_data_from_header(head);
    }
    // Case: can't expand - copy over data from old block to new location, free old block, return new location
    else {
        void* new_data = malloc(size);
        struct mem_block* new_head = get_header_from_data(new_data);

        memcpy(new_head, head, head->size);
        free(ptr);

        return new_data;
    }
}

/**
 * @brief      Prints out memory
 */
void print_memory(void)
{

    puts("-- Current Memory State --");
    struct mem_block *current_block = g_head;

    unsigned long current_region = 0;

    while (current_block != NULL) {
        // Case: New region - print out region string
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

