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
    "6team",
    /* First member's full name */
    "Junryul Baek",
    /* First member's email address */
    "farvinjr104@gmail.com",
    /* Second member's full name (leave blank if none) */
    "",
    /* Second member's email address (leave blank if none) */
    ""};


// 매크로영역

// 워드, 더블, 청크사이즈 선언
#define WSIZE 4
#define DSIZE 8
#define CHUNKSIZE (1<<12) // 1을 왼쪽으로 12번 시프트 = 1*2^12 = 4KB / 시스템 기본 메모리 페이지 크기

// 헤더/푸터에 저장할 값 생성  (크키 | 할당여부)
#define PACK(size, alloc) ((size) | (alloc))

// 주소 p에서 4바이트(워드) 읽기/쓰기
// 64비트 컴퓨터에서도 4바이트를 기준으로 동작하도록 설계되어 있음
#define GET(p) (*(unsigned int *)(p))
#define PUT(p, val) (*(unsigned int *)(p) = (val))

// 헤더/푸터에서 크기와 할당 비트 추출
#define GET_SIZE(p) (GET(p) & ~0x7)
#define GET_ALLOC(p) (GET(p) & 0x1)

// 블록 포인터(bp)로 헤더와 푸터 주소 계산
#define HDRP(bp) ((char *)(bp) - WSIZE)
#define FTRP(bp) ((char *)(bp) + GET_SIZE(HDRP(bp)) - DSIZE)

// 블록 포인터(bp)로 이전/다음 블록 포인터 계산
#define NEXT_BLKP(bp) ((char *)(bp) + GET_SIZE(((char *)(bp) - WSIZE)))
#define PREV_BLKP(bp) ((char *)(bp) - GET_SIZE(((char *)(bp) - DSIZE)))

/* single word (4) or double word (8) alignment */
#define ALIGNMENT 8

// 적합한 블록이 없을 경우 확장에 사용하기 위한 매크로
#define MAX(x, y) ((x) > (y) ? (x) : (y))

// 글로벌 변수들
static char *heap_listp = 0;
static char *last_bp;

// 함수 미리 선언
static void *extend_heap(size_t words);
static void *coalesce(void *bp);
static void *find_fit(size_t asize);
static void place(void *bp, size_t asize);


/*
 * mm_init - initialize the malloc package.
 */
int mm_init(void)
{   
    // 1. 힙 뼈대를 위한 16바이트 공간 요청하고 성공여부 확인
    if ((heap_listp = mem_sbrk(4*WSIZE)) == (void *)-1)
    // (void *)-1 = 정수-1의 비트패턴을 강제변환한 값(최상단영역) = 메모리 할당 실패 표현
        return -1;


    // 2. 뼈대 구조 기록
    PUT(heap_listp, 0);                             // 패딩
    PUT(heap_listp + (1*WSIZE), PACK(DSIZE, 1));    // 프롤로그 헤더
    PUT(heap_listp + (2*WSIZE), PACK(DSIZE, 1));    // 프롤로그 푸터
    PUT(heap_listp + (3*WSIZE), PACK(0, 1));        // 에필로그 헤더
    heap_listp += (2*WSIZE);

    // find를 nextfit 방식으로 이용할 때만 활성화
    // last_bp = NEXT_BLKP(heap_listp);

    // 3. extend_heap을 호출해 첫 가용 블록 생성
    if (extend_heap(CHUNKSIZE/WSIZE) == NULL)
        return -1;
    return 0;
}

static void *extend_heap(size_t words) {
    char *bp;
    // 1. 요청 크기를 8바이트(더블워드) 정렬에 맞게 올림
    size_t size = (words % 2) ? (words+1) * WSIZE : words * WSIZE;

    // 2. sbrk로 커널에 메모리 요청
    if ((bp = mem_sbrk(size)) == (void *)-1)
      return NULL;

    // 3. 새로 받은 공간을 가용 블록으로 설정
    PUT(HDRP(bp), PACK(size, 0));           // 새 가용블록 헤더
    PUT(FTRP(bp), PACK(size, 0));           // 새 가용블록 푸터
    PUT(HDRP(NEXT_BLKP(bp)), PACK(0, 1));   // 에필로그 헤더

    bp = coalesce(bp);
    last_bp = bp;

    return bp;
}

// // 가용블록 검색 (first fit 방식) > 67점
// static void *find_fit(size_t asize) {
//     void *bp;

//     // 힙 순회
//     for (bp = heap_listp; GET_SIZE(HDRP(bp)) > 0; bp = NEXT_BLKP(bp)) {
//         // 가용상태이고 크기가 충분하다면,
//         if (!GET_ALLOC(HDRP(bp)) && (asize <= GET_SIZE(HDRP(bp)))) {
//             return bp;
//         }
//     }
//     return NULL;
//     // 못찾으면 NULL
// }

// 가용블록 검색 (next fit 방식) > 83점
static void *find_fit(size_t asize) {
    void *bp;

    if (last_bp == NULL || GET_SIZE(HDRP(last_bp)) == 0) 
        last_bp = NEXT_BLKP(heap_listp);

    bp = last_bp;

    do {
        size_t bsize = GET_SIZE(HDRP(bp));

        if (bsize == 0) {
            bp = NEXT_BLKP(heap_listp);
            continue;
        }

        if (!GET_ALLOC(HDRP(bp)) && asize <= bsize) {
            last_bp = bp;
            return bp;
        }
        
        bp = NEXT_BLKP(bp);

    } while (bp != last_bp);
    
    return NULL;
}


// 가용블록 검색 bestfit 방식 > 67점

// 저장해둘 포인터 하나 선언
// 순회하면서 가용 & 사이즈충분이면 저장해둔 블록이랑 비교해서 더 작은걸로 업뎃

// static void *find_fit(size_t asize) {
//     void* bp;
//     void* min_bp = NULL;

//     for (bp = heap_listp; GET_SIZE(HDRP(bp)) > 0; bp = NEXT_BLKP(bp)) {
//         if (!GET_ALLOC(HDRP(bp)) && (asize <= GET_SIZE(HDRP(bp)))) {
//             if (min_bp == NULL || (GET_SIZE(HDRP(bp)) < GET_SIZE(HDRP(min_bp)))) {
//                 min_bp = bp;
//             }
//         }
//     }
//     return min_bp;
// }

// place() 블록 분할 및 배치함수
static void place(void *bp, size_t asize) {
    size_t csize = GET_SIZE(HDRP(bp));

    // 남는 공간이 최소 블록 크기(16바이트) 이상이면 분할
    if ((csize - asize) >= (2*DSIZE)) {
        PUT(HDRP(bp), PACK(asize, 1)); // 앞부분 할당
        PUT(FTRP(bp), PACK(asize, 1));
        bp = NEXT_BLKP(bp);
        PUT(HDRP(bp), PACK(csize-asize, 0)); // 뒷부분 새 가용 블록
        PUT(FTRP(bp), PACK(csize-asize, 0));
    } else { // 남는 공간이 적으면 그냥 전체할당
        PUT(HDRP(bp), PACK(csize, 1));
        PUT(FTRP(bp), PACK(csize, 1));
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
    char* bp;

    // 의미없는 요청 무시
    if (size == 0) return NULL;

    // 오버헤드와 정렬 요구사항 포함 블록 크기 조정
    if (size <= DSIZE)
        asize = 2*DSIZE;
    else
        asize = DSIZE * ((size + (DSIZE) + (DSIZE-1)) / DSIZE);
    
    // 가용 리스트에서 적합한 블록 검색
    if ((bp = find_fit(asize)) != NULL) {
        place(bp, asize);
        return bp;
    }

    // 적합한 블록이 없을 경우: 힙 확장하고 블록 배치
    extendsize = MAX(asize, CHUNKSIZE);
    if ((bp = extend_heap(extendsize/WSIZE)) == NULL)
        return NULL;
    place(bp, asize);
    return bp;
}

/*
 * mm_free - Freeing a block does nothing.
 */
void mm_free(void *bp) {
    size_t size = GET_SIZE(HDRP(bp));
    //헤더와 푸터를 가용 상태로 변경
    PUT(HDRP(bp), PACK(size, 0));
    PUT(FTRP(bp), PACK(size, 0));
    // 주변 블록 확인 및 연결시도
    bp = coalesce(bp);
    last_bp = bp;
}

static void *coalesce(void *bp) {
    // 이전/다음 블록의 할당 상태를 가져옴
    size_t prev_alloc = GET_ALLOC(FTRP(PREV_BLKP(bp)));
    size_t next_alloc = GET_ALLOC(HDRP(NEXT_BLKP(bp)));
    size_t size = GET_SIZE(HDRP(bp));

    // case1: 양쪽 다 할당 상태
    if (prev_alloc && next_alloc) { return bp;}
    // case2: 다음 블록 가용
    else if (prev_alloc && !next_alloc) {
        size += GET_SIZE(HDRP(NEXT_BLKP(bp)));
        PUT(HDRP(bp), PACK(size, 0));
        PUT(FTRP(bp), PACK(size, 0));
    }
    // case3: 이전 블록 가용
    else if (!prev_alloc && next_alloc) {
        size += GET_SIZE(HDRP(PREV_BLKP(bp)));
        PUT(FTRP(bp), PACK(size, 0));
        PUT(HDRP(PREV_BLKP(bp)), PACK(size, 0));
        bp = PREV_BLKP(bp);
    }
    // case4: 둘다 가용
    else {
        size += GET_SIZE(HDRP(PREV_BLKP(bp))) + GET_SIZE(FTRP(NEXT_BLKP(bp)));
        PUT(HDRP(PREV_BLKP(bp)), PACK(size, 0));
        PUT(FTRP(NEXT_BLKP(bp)), PACK(size, 0));
        bp = PREV_BLKP(bp);
    }

    last_bp = bp; // 병합결과를 next-fit 시작점으로
    return bp;
}

/*
 * mm_realloc - Implemented simply in terms of mm_malloc and mm_free
 */ // 기존의 realloc
// void *mm_realloc(void *ptr, size_t size)
// {
//     void *oldptr = ptr;
//     void *newptr;
//     size_t copySize;

//     newptr = mm_malloc(size);
//     if (newptr == NULL)
//         return NULL;
//     copySize = GET_SIZE(HDRP(oldptr)) - DSIZE;

//     if (size < copySize)
//         copySize = size;

//     memcpy(newptr, oldptr, copySize);
//     mm_free(oldptr);
//     return newptr;
// }


void *mm_realloc(void *ptr, size_t size)
{
    void *oldptr = ptr;
    void *newptr;
    size_t copySize;
    size_t wantsize;
    size_t oldsize;

    // NULL이 들어오면 그냥 malloc(size) 반환
    if (ptr == NULL) {
        return mm_malloc(size);
    }
    // size가 0이면 free 반환
    if (size == 0) {
        mm_free(ptr);
        return NULL;
    }

    if (size <= DSIZE) {
        wantsize = 2*DSIZE;
    } else {
        wantsize = DSIZE * ((size + (DSIZE) + (DSIZE-1)) / DSIZE);
    }

    // 4가지 케이스
    // case 1. 요청 크기가 기존 크기보다 작을 경우

    oldsize = GET_SIZE(HDRP(ptr));

    // if (wantsize <= oldsize) {
    //     if ((oldsize - wantsize) >= 4*WSIZE) {
    //         PUT(HDRP(ptr), PACK(wantsize, 1));
    //         PUT(FTRP(ptr), PACK(wantsize, 1));

    //         void* remainder_ptr = NEXT_BLKP(ptr);
    //         PUT(HDRP(remainder_ptr), PACK(oldsize - wantsize, 0));
    //         PUT(FTRP(remainder_ptr), PACK(oldsize - wantsize, 0));

    //         coalesce(remainder_ptr);

    //         return ptr;
    //     } else {
    //         newptr =  ptr;
    //         return newptr;
    //     }
    // }

    if (wantsize <= oldsize) {
        return ptr;
    }


    // case 2. 인접공간 활용이 가능할 경우
    // 앞공간 vs 뒷공간 구분해야함
    // next가 가용일때
    else if (GET_ALLOC(HDRP(NEXT_BLKP(ptr))) == 0 && (GET_SIZE(HDRP(NEXT_BLKP(ptr))) + oldsize) >= wantsize) {
        oldsize += GET_SIZE(HDRP(NEXT_BLKP(ptr)));
        PUT(HDRP(ptr), PACK(oldsize, 1));
        PUT(FTRP(ptr), PACK(oldsize, 1));
        last_bp = ptr;
        return ptr; 
    } 
    // prev가 가용일때
    else if (GET_ALLOC(HDRP(PREV_BLKP(ptr))) == 0 && (GET_SIZE(HDRP(PREV_BLKP(ptr))) + oldsize) >= wantsize) {
        size_t oldpayloadsize = oldsize - DSIZE;
        oldsize += GET_SIZE(HDRP(PREV_BLKP(ptr)));
        newptr = PREV_BLKP(ptr);
        PUT(HDRP(newptr), PACK(oldsize, 1));
        PUT(FTRP(newptr), PACK(oldsize, 1));
                
        memmove(newptr, oldptr, oldpayloadsize);
        last_bp = newptr;
        return newptr;
    }

    // case 3. 힙 영역 확장의 경우

    else if (GET_SIZE(NEXT_BLKP(ptr)) == 0 && GET_ALLOC(NEXT_BLKP(ptr)) == 1) {
        if (mem_sbrk(wantsize - oldsize) != (void *)-1) {
            PUT(HDRP(ptr), PACK(wantsize, 1));           // 새 가용블록 헤더
            PUT(FTRP(ptr), PACK(wantsize, 1));           // 새 가용블록 푸터

            void *new_epilogue = NEXT_BLKP(ptr);
            PUT(HDRP(new_epilogue), PACK(0, 1));
            return ptr;
        } else return NULL;
        
    }

    // case 4. 다 안돼서 새로 할당해야 하는 경우

    else {
        newptr = mm_malloc(size);
        if (newptr == NULL) return NULL;
        copySize = GET_SIZE(HDRP(oldptr)) - DSIZE;
        size_t oldpayloadsize = oldsize - DSIZE;
        copySize = oldpayloadsize < size ? oldpayloadsize : size;
        memcpy(newptr, oldptr, copySize);
        mm_free(ptr);

        return newptr;
    }
}