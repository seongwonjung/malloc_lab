#include "config.h"
#include <errno.h>
#include <stdio.h>
/* Private global variables*/
static char *mem_heap;      /* Points to first */
static char *mem_brk;       /* Points to last btyefo geap plus 1 */
static char *mem_max_addr;  /* Max legal heap addr plus 1 */

// mem_init - Initialize the memory system model
void mem_init(void){
    mem_heap = (char *)Malloc(MAX_HEAP);
    mem_brk = (char *)mem_heap;
    mem_max_addr = (char *)(mem_heap + MAX_HEAP);
}

/*
mem_sbrk = Simple model of the sbrk function. Extends the heap
by incr bytes and returns the start address of the new area. In
this model, the heap cannot be shrunk
*/
void *mem_sbrk(int incr){
    // 이전 포인터를 old_brk에 담음
    char *old_brk = mem_brk;
    
    if((incr < 0) || ((mem_brk + incr) > mem_max_addr)){
        // errno => #include <errno.h>
        errno = ENOMEM; // out of memory
        fprintf(stderr, "ERROR: mem_sbrk failed. Run out of memory... \n");
        return (void *)old_brk;
    }
    mem_brk += incr;
    return (void *)old_brk;
}


/* Basic constants and macros */
#define WSIZE   4   // Word and header/footer size (bytes)
#define DSIZE   8   // Double word size (bytes)
#define CHUNKSIZE   (1<<12) // Extend heap by this amount (bytes)

#define MAX(x, y) ((x) > (y) ? (x) : (y))

// Pack a size and allocated bit into a word
#define PACK(size, alloc)   ((size) | (alloc))

// Read and write a word at address p
#define GET(p)  (*(unsigned *)(p))
#define PUT(p, val)  (*((unsigned *)(p)) = (val))

// Read the size and allocated fields from address p
#define GET_SIZE(p)     (GET(p) & (~0x7))
#define GET_ALLOC(p)    (GET(p) & (0x1))

// Given block ptr bp, compute address of its header and footer
#define HDRP(bp)    ((char *)(bp) - WSIZE)
#define FTRP(bp)    ((char *)(bp) + GET_SIZE(HDRP(bp)) - DSIZE)

// Given block ptr bp, compute address of next and previous blocks
#define NEXT_BLKP(bp)   ((char *)(bp) + GET_SIZE(((char *)(bp) - WSIZE)))
#define PREV_BLKP(bp)   ((char *)(bp) - GET_SIZE(((char *)(bp) - DSIZE)))
static char* heap_listp;
int mm_init(void){
    /* Create the initial empty heap*/
    /* */
    if ((heap_listp = mem_sbrk(4*WSIZE)) == (void *)-1)
        return -1;
    PUT(heap_listp, 0);
    PUT(heap_listp + (1*WSIZE), PACK(DSIZE, 1));
    PUT(heap_listp + (2*WSIZE), PACK(DSIZE, 1));
    PUT(heap_listp + (3*WSIZE), PACK(0,1));
    heap_listp += (2*WSIZE);

    if (extend_heap(CHUNKSIZE/WSIZE) == NULL)
        return -1;
    return 0;
}

static void *extend_heap(size_t words){
    char *bp;
    size_t size;

    size = (words % 2) ? (words+1) * WSIZE : words * WSIZE;
    if((long)(bp = mem_sbrk(size)) == -1)
        return NULL;
    
}