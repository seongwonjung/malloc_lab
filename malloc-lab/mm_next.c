/*
 * mm-naive.c - The fastest, least memory-efficient malloc package.
 *
 * In this naive approach, a block is allocated by simply incrementing
 * the brk pointer.  A block is pure payload. There are no headers or
 * footers.  Blocks are never coalesced or reused. Realloc is
 * implemented directly using mm_malloc and mm_free.
 *
 * NOTE TO STUDENTS: Replace this header comment with your own header
 * comment that gives a high level description of your solution.
 */
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <unistd.h>
#include <string.h>

#include "mm.h"
#include "memlib.h"

/*********************************************************
 * NOTE TO STUDENTS: Before you do anything else, please
 * provide your team information in the following struct.
 ********************************************************/
team_t team = {
    /* Team name */
    "ateam",
    /* First member's full name */
    "Harry Bovik",
    /* First member's email address */
    "bovik@cs.cmu.edu",
    /* Second member's full name (leave blank if none) */
    "",
    /* Second member's email address (leave blank if none) */
    ""};

/* single word (4) or double word (8) alignment */
#define ALIGNMENT 8

/* rounds up to the nearest multiple of ALIGNMENT */
#define ALIGN(size) (((size) + (ALIGNMENT - 1)) & ~0x7) 

#define SIZE_T_SIZE (ALIGN(sizeof(size_t)))

#define WSIZE 4
#define DSIZE 8
#define CHUNKSIZE (1<<12)

#define MAX(x, y) (x > y) ? (x) : (y)

#define PACK(size, alloc) ((size) | (alloc))
#define GET(p) (*(unsigned int *) (p))
#define PUT(p, val)  (*(unsigned int *) (p) = (val))

#define GET_SIZE(p) (GET(p) & ~0x7)
#define GET_ALLOC(p) (GET(p) & 0x1)

#define HDRP(bp) ((char*)(bp) - WSIZE)
#define FTRP(bp) ((char*)(bp) + GET_SIZE(HDRP(bp)) - DSIZE)

#define NEXT_BLKP(bp) ((char *)(bp) + GET_SIZE(((char *)(bp) - WSIZE)))
#define PREV_BLKP(bp) ((char *)(bp) - GET_SIZE(((char *)(bp) - DSIZE)))

static char* heap_listp;
static char* next_ptr;

static void *coalesce(void *bp);
static void *extend_heap(size_t words);
char* find_fit(size_t asize);
int mm_init(void);
void place(char* bp, size_t asize);
void *mm_malloc(size_t size);
void *mm_realloc(void *ptr, size_t size);
/*
 * mm_init - initialize the malloc package.
 */

int mm_init(void)
{
    // 4word 할당 -> 실패 시 -1 return
    if((heap_listp = mem_sbrk(4*WSIZE)) == (void*)-1)
        return -1;
    PUT(heap_listp, 0);
    PUT(heap_listp + (1*WSIZE), PACK(DSIZE, 1));
    PUT(heap_listp + (2*WSIZE), PACK(DSIZE, 1));
    PUT(heap_listp + (3*WSIZE), PACK(0, 1));
    heap_listp += (2*WSIZE);
    next_ptr = NEXT_BLKP(heap_listp);
    if(extend_heap(CHUNKSIZE/WSIZE) == NULL)
        return -1;
    return 0;
}

static void *extend_heap(size_t words)
{
    char *bp;
    size_t size;

    /* Allocate an even number of words to maintain aligment */
    size = (words%2) ? (words+1)*WSIZE : words*WSIZE;
    if((long)(bp = mem_sbrk(size)) == -1)
        return NULL;

    PUT(HDRP(bp), PACK(size, 0));
    PUT(FTRP(bp), PACK(size, 0));
    PUT(HDRP(NEXT_BLKP(bp)), PACK(0, 1));
    
    return coalesce(bp);
}

char* find_fit(size_t asize){
    // next_fit 으로 
    // next_fit = 이전에 할당했던 위치의 다음 위치부터 찾기
    // 이전에 할당했던 위치를 저장해 놔야 함 전역 변수 하나 쓰자
    // prev_allocated 라는 전역 변수 하나 만들고 mm_init 할 때 heap_listp 가리키게 해 놓음
    // 탐색의 시작 위치는 next_ptr
    char *bp = next_ptr;
    // next_ptr ~ 끝까지
    while(GET_SIZE(HDRP(bp)) > 0){
        if(GET_SIZE(HDRP(bp)) >= asize && !GET_ALLOC(HDRP(bp))){
            next_ptr = bp;
            return bp;
        }
        bp = NEXT_BLKP(bp);
    }
    // 시작부터 ~ next_ptr 까지
    bp = NEXT_BLKP(heap_listp);
    while(GET_SIZE(HDRP(bp)) > 0 && bp != next_ptr){
        if(GET_SIZE(HDRP(bp)) >= asize && !GET_ALLOC(HDRP(bp))){
            next_ptr = bp;
            return bp;
        }
        bp = NEXT_BLKP(bp);
    }
    return NULL;
}

void place(char* bp, size_t asize){
    size_t f_size = GET_SIZE(HDRP(bp));
    // 남는 공간이 최소 블럭 크기(16byte) 보다 클 때 -> 분할
    if(f_size - asize >= 2*DSIZE){
        // 앞 쪽 부분을 쓰고, 뒤쪽 부분은 가용 상태로
        PUT(HDRP(bp), PACK(asize, 1));
        PUT(FTRP(bp), PACK(asize, 1));
        PUT(HDRP(NEXT_BLKP(bp)), PACK((f_size - asize), 0));
        PUT(FTRP(NEXT_BLKP(bp)), PACK((f_size - asize), 0));
    }else{
        PUT(HDRP(bp), PACK(f_size, 1));
        PUT(FTRP(bp), PACK(f_size, 1));
    }
}

/*
 * mm_malloc - Allocate a block by incrementing the brk pointer.
 *     Always allocate a block whose size is a multiple of the alignment.
 */
void *mm_malloc(size_t size)
{
    size_t asize;
    size_t extendsize;
    char *bp;

    if (size == 0)
        return NULL;
    
    if (size <= DSIZE)
        asize = 2*DSIZE;
    else{
        asize = DSIZE * ((size + (DSIZE) + (DSIZE - 1))/DSIZE);
    }

    if ((bp = find_fit(asize)) != NULL){
        place(bp, asize);
        return bp;
    }

    extendsize = MAX(asize, CHUNKSIZE);
    if((bp = extend_heap(extendsize/WSIZE)) == NULL)
        return NULL;
    place(bp, asize);
    return bp;

    // int newsize = ALIGN(size + SIZE_T_SIZE);
    // void *p = mem_sbrk(newsize);
    // if (p == (void *)-1)
    //     return NULL;
    // else
    // {
    //     *(size_t *)p = size;
    //     return (void *)((char *)p + SIZE_T_SIZE);
    // }
}

/*
 * mm_free - Freeing a block does nothing.
 */
void mm_free(void *bp)
{
    size_t size = GET_SIZE(HDRP(bp));
    
    PUT(HDRP(bp), PACK(size, 0));
    PUT(FTRP(bp), PACK(size, 0));
    coalesce(bp);
}

static void *coalesce(void *bp)
{
    size_t prev_alloc = GET_ALLOC(FTRP(PREV_BLKP(bp)));
    size_t next_alloc = GET_ALLOC(HDRP(NEXT_BLKP(bp)));
    size_t size = GET_SIZE(HDRP(bp));
    
    // case 1
    if(prev_alloc && next_alloc){
        return bp;
    }
    // case 2
    else if(prev_alloc && !next_alloc){
        if (next_ptr == NEXT_BLKP(bp)){
            next_ptr = bp;
        }
        size += GET_SIZE(HDRP(NEXT_BLKP(bp)));
        PUT(HDRP(bp), PACK(size, 0));
        PUT(FTRP(bp), PACK(size, 0));
    }
    // case 3
    else if(!prev_alloc && next_alloc){
        if (next_ptr == bp){
            next_ptr = PREV_BLKP(bp);
        }
        size += GET_SIZE(HDRP(PREV_BLKP(bp)));
        PUT(FTRP(bp), PACK(size, 0));
        PUT(HDRP(PREV_BLKP(bp)), PACK(size, 0));
        bp = PREV_BLKP(bp);
    }
    // case 4
    else{
        if(next_ptr == bp || next_ptr == NEXT_BLKP(bp)){
            next_ptr = PREV_BLKP(bp);
        }
        size += GET_SIZE(HDRP(NEXT_BLKP(bp))) + GET_SIZE(HDRP(PREV_BLKP(bp)));
        PUT(HDRP(PREV_BLKP(bp)), PACK(size, 0));
        PUT(FTRP(NEXT_BLKP(bp)), PACK(size, 0));
        bp = PREV_BLKP(bp);
    }
    return bp;
}

/*
 * mm_realloc - Implemented simply in terms of mm_malloc and mm_free
 */
void *mm_realloc(void *ptr, size_t size)
{
    void *oldptr = ptr;
    void *newptr;
    size_t copySize;

    newptr = mm_malloc(size);
    if (newptr == NULL)
        return NULL;
    copySize = *(size_t *)((char *)oldptr - SIZE_T_SIZE);
    if (size < copySize)
        copySize = size;
    memcpy(newptr, oldptr, copySize);
    mm_free(oldptr);
    return newptr;
}