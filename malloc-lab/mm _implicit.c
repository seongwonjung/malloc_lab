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
    "SeongWonJung",
    /* First member's email address */
    "jsjsw0918@gmail.com",
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

static char* heap_listp = NULL;
static char* next_ptr = NULL;

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

    // 8의 배수로 맞춰주기 위함 -> words를 짝수로 맞춰줌
    size = (words%2) ? (words+1)*WSIZE : words*WSIZE;
    if((long)(bp = mem_sbrk(size)) == -1)
        return NULL;

    PUT(HDRP(bp), PACK(size, 0));
    PUT(FTRP(bp), PACK(size, 0));
    PUT(HDRP(NEXT_BLKP(bp)), PACK(0, 1));
    
    return coalesce(bp);
}

//next_fit 방식 
char* find_fit(size_t asize){
    // next_fit = 이전에 할당했던 위치의 다음 위치부터 찾기
    // 이전에 할당했던 위치를 저장해 놔야 함 전역 변수 하나 쓰자
    // next_ptr 라는 전역 변수 하나 만들고 mm_init 할 때 heap_listp 가리키게 초기화
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

// // first-fit
// char* find_fit(size_t asize){
//     // 탐색은 heap_listp = prologue이므로 NEXT_BLKP(heap_listp)부터 시작
//     char *bp = NEXT_BLKP(heap_listp);
//     char *hdrp = HDRP(bp);
//     // GET_SIZE(bp) 0 일 때 == 에필로그 일 때
//     while(GET_SIZE(hdrp) != 0){
//         hdrp = HDRP(bp);
//         // 가용 상태이고, 담을 수 있는 크기 일 때
//         if(GET_ALLOC(hdrp) == 0 && GET_SIZE(hdrp) >= asize){
//             return bp;
//         }
//         bp = NEXT_BLKP(bp);
//     }
//     return NULL;
// }

// best-fit
// char* find_fit(size_t asize){
//     /*
//     아이디어:
//     NEXT_BLKP(heap_listp) 부터 끝까지 순회 하면서 asize에 딱 맞는 블럭을 찾으면 된다.
//     넣을 수 있는 공간이 나왔을 때, best_fit을 갱신해주자
//     */
//     char* ptr = NEXT_BLKP(heap_listp);
//     char* best_ptr = NULL;
//     // size_t best_fit = CHUNKSIZE;
//     size_t best_fit = (size_t)-1;
//     while(GET_SIZE(HDRP(ptr)) != 0){
//         if(!GET_ALLOC(HDRP(ptr))){
//             // 사이즈가 딱 맞으면 바로 return
//             size_t cur_size = GET_SIZE(HDRP(ptr));
//             if (cur_size == asize){
//                 return ptr;
//             }else if(best_fit > cur_size && cur_size > asize){
//                 best_fit = cur_size;
//                 best_ptr = ptr;
//             }
//         }
//         ptr = NEXT_BLKP(ptr);
//     }
//     return best_ptr;
// }


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
    // 할당 할 수 있는 크기의 블럭이 있을 때
    if ((bp = find_fit(asize)) != NULL){
        place(bp, asize);
        return bp;
    }

    // 할당 할 수 있는 크기의 블럭이 없을 때
    extendsize = MAX(asize, CHUNKSIZE);
    if((bp = extend_heap(extendsize/WSIZE)) == NULL)
        return NULL;
    place(bp, asize);
    return bp;
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
    // ptr == NULL -> mm_malloc, size == 0 -> mm_free
    if (ptr == NULL) {
        return mm_malloc(size);
    }
    if (size == 0) {
        mm_free(ptr);
        return NULL;
    }
    
    size_t old_size = GET_SIZE(HDRP(ptr));
    size_t asize;

    void *prev_ptr = PREV_BLKP(ptr);
    void *next_ptr_val = NEXT_BLKP(ptr);
    int prev_aloc = GET_ALLOC(HDRP(prev_ptr));
    int next_aloc = GET_ALLOC(HDRP(next_ptr_val));
    size_t prev_size = GET_SIZE(HDRP(prev_ptr));
    size_t next_size = GET_SIZE(HDRP(next_ptr_val));
    size_t total_size;

    if (size <= DSIZE) {
        asize = 2 * DSIZE;
    } else {
        // 8 배수 올림
        asize = DSIZE * ((size + DSIZE + (DSIZE - 1)) / DSIZE);
    }

    // 복사할 데이터 크기 계산 -> min(이전 페이로드 크기, 새로 요청된 payload 크기)
    size_t copySize = old_size - DSIZE;
    if (size < copySize) {
        copySize = size;
    }

    // case 1: 요청한 크기가 기존 블록 크기와 같다면 바로 리턴
    if (asize == old_size) {
        return ptr;
    }

    // case 2: 축소하는 경우
    if (asize < old_size) {
        // 제자리에서 축소

        place(ptr, asize);
        return ptr;
    }
    
    // 확장하는 경우
    // Case 3.1: 다음 블록이 가용하고 공간이 충분한 경우
    if (!next_aloc && (asize <= old_size + next_size)) {
        total_size = old_size + next_size;
        // 헤더만 전체 크기로 미리 설정하고 place 호출
        PUT(HDRP(ptr), PACK(total_size, 0));
        place(ptr, asize);
        return ptr;
    }

    // Case 3.2: 다음 블록이 힙의 끝(에필로그)인 경우
    else if (next_size == 0) {
        // 필요한 추가 공간 계산
        size_t needsize = asize - old_size;
        // mem_sbrk로 필요한 만큼만 할당
        if (mem_sbrk(needsize) == (void *)-1) {
            return NULL;
        }
        // 현재 블럭의 헤더, 푸터 설정
        PUT(HDRP(ptr), PACK(asize, 1));
        PUT(FTRP(ptr), PACK(asize, 1));
        // 새 에필로그 설정
        PUT(HDRP(NEXT_BLKP(ptr)), PACK(0, 1));
        return ptr;
    }

    // Case 3.3: 앞, 뒤 블록 모두 가용하고 공간이 충분한 경우
    else if (!prev_aloc && !next_aloc && (asize <= old_size + prev_size + next_size)) {
        total_size = old_size + prev_size + next_size;
        memmove(prev_ptr, ptr, copySize);
        // 헤더만 전체 크기로 미리 설정하고 place 호출
        PUT(HDRP(prev_ptr), PACK(total_size, 0));
        place(prev_ptr, asize);

        next_ptr = prev_ptr;
        return prev_ptr;
    }
    
    // Case 3.4: 앞 블록만 가용하고 공간이 충분한 경우
    else if (!prev_aloc && (asize <= old_size + prev_size)) {
        total_size = old_size + prev_size;
        memmove(prev_ptr, ptr, copySize);
        // 헤더만 전체 크기로 미리 설정하고 place 호출
        PUT(HDRP(prev_ptr), PACK(total_size, 0));
        place(prev_ptr, asize);
        next_ptr = prev_ptr;
        return prev_ptr;
    }

    // Case 4: 위 모든 방법으로 확장 불가, 새로 할당
    else {
        void *new_ptr = mm_malloc(size);
        if (new_ptr == NULL) {
            return NULL;
        }
        memcpy(new_ptr, ptr, copySize);
        mm_free(ptr);
        return new_ptr;
    }
}