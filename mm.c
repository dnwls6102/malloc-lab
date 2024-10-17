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
    "team1",
    /* First member's full name */
    "최우진",
    /* First member's email address */
    "dnwls6102@naver.com",
    /* Second member's full name (leave blank if none) */
    "김동현",
    /* Second member's email address (leave blank if none) */
    "김수환"
};

#define WSIZE 4 //싱글 워드 크기 4
#define DSIZE 8 //더블 워드 크기 8
//정수 1을 12비트만큼 뒤로 이동시킨다
//12비트만큼 뒤로 이동시킨다 : 2^12만큼 값이 증가한다
//2^12 == 4096, 즉 4KB만큼 크기가 증가한다
#define CHUNKSIZE (1<<12) 

/* single word (4) or double word (8) alignment */
#define ALIGNMENT 8

/* rounds up to the nearest multiple of ALIGNMENT */
#define ALIGN(size) (((size) + (ALIGNMENT-1)) & ~0x7)


#define SIZE_T_SIZE (ALIGN(sizeof(size_t)))


#define MAX(x, y) ((x) > (y) ? (x) : (y))

//블록의 사이즈와 할당 여부를 체크하는 바이트를 합치는 매크로 함수 PACK
//| 연산 : 특정 비트를 특정 값으로 설정
// 0011과 0100을 연산하면 0111이라는 결과값이 나옴
#define PACK(size, alloc) ((size) | (alloc))

//주소 P의 데이터를 읽거나 쓰기
#define GET(p) (*(unsigned int *)(p))
#define PUT(p, val) (*(unsigned int *)(p) = (val))

//주소 p에 있는 헤더 또는 풋터의 size와 할당 비트를 리턴함
//비트 마스크를 사용한 연산
//& 0x1 : 하위 1비트의 값만 추출하겠다
//& 0x7 : 하위 3비트를 제외한 모든 값을 추출하겠다(할당 여부를 제외한 크기만 남김)
//0x7 => 00000111
#define GET_SIZE(p) (GET(p) & ~0x7)
#define GET_ALLOC(p) (GET(p) & 0x1)

//블록 헤더와 푸터를 가리키는 포인터를 반환하는 매크로 함수
//블록 포인터(bp)는 헤더의 종료 지점을 가리킴. 곧 데이터 저장 부분의 시작점
#define HDRP(bp) ((char *)(bp) - WSIZE)
#define FTRP(bp) ((char *)(bp) + GET_SIZE(HDRP(bp)) - DSIZE)

//다음 블록과 이전 블록의 블록 포인터를 반환하는 매크로 함수
//다음 블록의 블록 포인터는 현재 블록의 헤더를 참조하여 현재 블럭의 크기를 얻어낸 후
//블록 포인터에 현재 블럭의 크기만큼 더해서 다음 블록의 주소값을 얻어냄
//이전 블록의 블록 포인터는 이전 블록의 푸터를 참조하여 이전 블럭의 크기를 얻어낸 후
//블록 포인터에 이전 블럭의 크기만큼 빼서 이전 블록의 주소값을 얻어냄
#define NEXT_BLKP(bp) ((char *)(bp) + GET_SIZE(((char *)(bp) - WSIZE)))
#define PREV_BLKP(bp) ((char *)(bp) - GET_SIZE(((char *)(bp) - DSIZE)))

/*
* coalesce - combines current block with back and front blocks if empty
*/
static void *coalesce(void *bp)
{
    //앞 블록의 가용 여부를 저장하는 변수 prev_alloc
    size_t prev_alloc = GET_ALLOC(FTRP(PREV_BLKP(bp)));
    //다음 블록의 가용 여부를 저장하는 변수 next_alloc
    size_t next_alloc = GET_ALLOC(HDRP(NEXT_BLKP(bp)));
    //현재 블록의 크기를 저장하는 변수 size
    size_t size = GET_SIZE(HDRP(bp));

    //case 1 : 앞의 블록이 할당 상태이고 다음 블록이 할당 상태라면
    if (prev_alloc && next_alloc)
    {
        //그대로 반환
        return bp;
    }

    //case 2 : 앞의 블록은 할당 상태인데 다음 블록은 가용하다면
    else if (prev_alloc && !next_alloc)
    {
        //다음 블록의 크기를 size에 더해줌
        size += GET_SIZE(HDRP(NEXT_BLKP(bp)));
        //우선 현재 블록의 헤더에 병합 이후의 size를 기록
        PUT(HDRP(bp), PACK(size, 0));
        //FTRP의 구현 방식에 의해, bp의 size 정보를 푸터가 아닌 헤더에서 가져오게 되고
        //정상적으로 병합된 크기만큼 이동해 푸터를 초기화한다
        PUT(FTRP(bp), PACK(size, 0));
    }

    //case 3 : 앞의 블록은 가용하고 다음 블록은 할당 상태
    else if (!prev_alloc && next_alloc)
    {
        //앞 블록의 크기를 size에 더해줌
        size += GET_SIZE(HDRP(PREV_BLKP(bp)));
        //현재 블록의 푸터에 병합 이후의 size 크기를 기록
        PUT(FTRP(bp), PACK(size, 0));
        //이전 블록의 포인터(주소)를 얻어내고
        //그것을 HDRP에 넣어 이전 블럭의 헤더를 얻고
        //헤더에 업데이트된 size 크기를 기록
        PUT(HDRP(PREV_BLKP(bp)), PACK(size, 0));

        //(병합 이전의)앞 블록의 푸터를 할당 해제하지 않는 이유
        //어차피 순회할 때 앞 블록의 헤더를 만나면
        //size만큼 건너가기 때문에 할당 해제되지 않은 푸터를 만날 일이 없음

        //현재 블록의 포인터를 앞 블록으로 변경
        bp = PREV_BLKP(bp);
    }

    //case 4 : 둘 다 가용하면
    else
    {
        //size에 앞 블록의 크기와 뒤 블록 크기를 더하기
        size += GET_SIZE(HDRP(PREV_BLKP(bp))) +
            GET_SIZE(FTRP(NEXT_BLKP(bp)));
        //앞 블록의 헤더에 업데이트된 size를 기록
        PUT(HDRP(PREV_BLKP(bp)), PACK(size, 0));
        //뒤 블록의 푸터에 업데이트된 size를 기록
        PUT(FTRP(NEXT_BLKP(bp)), PACK(size, 0));
        //현재 블록의 포인터를 앞 블록으로 변경
        bp = PREV_BLKP(bp);
    }

    //포인터 반환
    return bp;
}

/*
* extend_heap - requests additional heap memory from OS
*/
static void *extend_heap(size_t words)
{
    char *bp;
    size_t size;

    //정렬 기준을 유지하기 위해 짝수개 만큼의 워드들을 할당함
    //만약 매개변수 words가 홀수면 1만큼 더해서(올림해서) 할당해주고
    //짝수라면 요청받은 만큼만 할당해준다
    size = (words % 2) ? (words + 1) * WSIZE : words * WSIZE;
    if ((long)(bp = mem_sbrk(size)) == -1) //bp에 추가된 힙 메모리 주소 넣기
        return NULL; //실패하면 NULL 반환

    //새로운 블록의 헤더와 푸터를 생성시켜주고
    //새로운 에필로그 블록을 생성시켜준다
    //기존 에필로그 블록에 대한 처리는?
    //이론상 bp에 PREV_BLKP를 구하고 bp와 병합시켜 준다면 될것 같긴 함
    PUT(HDRP(bp), PACK(size, 0));
    PUT(FTRP(bp), PACK(size, 0));
    PUT(HDRP(NEXT_BLKP(bp)), PACK(0, 1));
    
    //앞의 블록이 가용한 상태면 병합하기
    return coalesce(bp);
}


/* 
 * mm_init - initialize the malloc package.
 */

int mm_init(void)
{
    //memlib.c의 mem_init()은 테스트 케이스를 위한 시뮬레이션 환경을 만들기 위한 init
    //동적 할당기 구현과는 전혀 관련이 없음

    //묵시적 가용 리스트의 첫 번째 워드는 미사용 패딩 워드
    //그 이후로 들어오는 블록은 특별한 프롤로그 블록
    //8바이트 할당 블록으로 헤더와 푸터로만 구성
    //그리고 힙은 특별한 에필로그 블록으로 끝남

    //에필로그 블록 : 헤더만으로 구성된 크기가 0으로 할당된 블록
    //프롤로그 블록과 에필로그 블록을 할당하는 이유 : 가장자리 조건을 없애기 위해
    //가장자리 조건? 메모리 블록을 할당하고 해제하는 과정에서 발생 가능한 특별한 상황들
    //프롤로그,에필로그 블록 없이 리스트의 맨 앞/끝에 있는 블록을 처리한다면
    //헤더, 풋터가 없거나 이전/다음 블록이 없는 상황이 발생해 seg fault가 생길 수 있음
    
    //힙 공간의 주소를 저장하는 heap_listp 포인터 변수 선언
    char * heap_listp;

    //텅 빈 heap 공간을 할당받기 : 실패할 경우 -1 반환
    //-1을 반환하는 이유 : mdriver.c의 592를 보면
    //mm_init()의 반환 값이 음수면 할당 실패로 간주하기 때문
    if ((heap_listp = mem_sbrk(4 * WSIZE)) == (void *)-1)
        return -1;

    //힙 공간의 첫 블럭은 Alignment Padding으로 할당
    //Alignment Padding을 할당하는 이유 : 8바이트 정렬이기 때문
    //힙 영역의 첫 주소는 프롤로그 블록의 중간 부분
    //그런데 Alignment Padding 없이 할당이 이루어지면
    //처음 주소가 8의 배수로 되는 것이 아닌 4의 배수가 되어버림
    //이를 방지하고자 넣는 것임
    PUT(heap_listp, 0); //이제 heap에 unsigned int 크기만큼(=4바이트)의 블록이 삽입됨

    //프롤로그 블록의 헤더를 삽입 : heap_listp에 4만큼 더한 후
    //블록 크기와 할당 여부 정보를 담고 있는 PACK(DSIZE, 1) 값을 부여
    PUT(heap_listp + (1 * WSIZE), PACK(DSIZE, 1));

    //프롤로그 블록의 푸터를 삽입 : 위 코드와 동일
    PUT(heap_listp + (2 * WSIZE), PACK(DSIZE, 1));

    //에필로그 블록의 헤더를 삽입 : 에필로그 블록의 크기는 0으로 설정
    //프롤로그와 다르게 크기를 0으로 설정하는 이유: 블록의 끝을 표시하기 위함
    //할당기가 크기가 0인데 할당이 외어있는 블록을 만나게 된다면
    //블록의 끝임을 쉽게 인지할 수 있음
    PUT(heap_listp + (3 * WSIZE), PACK(0, 1));

    //heap_listp는 이제 프롤로그 블록의 헤더가 끝나는 지점을 가리켜야 함
    //처음에 1워드짜리 Alignment Padding이 들어가고
    //그 다음에 프롤로그 블록의 헤더가 들어갔는데 이것도 1워드니까
    //WSIZE * 2 만큼을 더해준다
    heap_listp += (2 * WSIZE);

    //CHUNKSIZE(현재는 4KB, 이를 블럭화하면 1024개의 블럭)만큼의 힙 메모리 공간 확보함
    if (extend_heap(CHUNKSIZE/WSIZE) == NULL)
        return -1;

    return 0;
}

/* 
 * mm_malloc - Allocate a block by incrementing the brk pointer.
 *     Always allocate a block whose size is a multiple of the alignment.
 */
void *mm_malloc(size_t size)
{
    //조정한 블록 사이즈 = 최종적으로 할당해야 하는 크기
    size_t asize;
    //만약 fit한 블록이 없는 경우, 늘려야 하는 heap 메모리의 크기
    size_t extendsize;
    //블록 포인터
    char *bp;

    //요청받은 크기가 0이면
    if (size == 0)
        //NULL 포인터 반환
        return NULL;
    
    //요청받은 크기가 8 이하라면
    if (size <= DSIZE)
        //헤더 + 푸터 크기 8 + 정렬 기준 8 크기 = 16을 할당
        asize = 2 * DSIZE;
    else
        asize = DSIZE * ((size + (DSIZE) + (DSIZE - 1)) / DSIZE);

    if ((bp = find_fit(asize)) != NULL)
    {
        place(bp, asize);
        return bp;
    }

    extendsize = MAX(asize, CHUNKSIZE);
    if ((bp = extend_heap(extendsize/WSIZE)) == NULL)
        return NULL;
    place(bp, asize);
    return bp;
}

/*
 * mm_free - Freeing a block does nothing.
 */
void mm_free(void *ptr)
{
    //매개변수로 입력받은 포인터 블록의 크기를 측정한다
    //해당 블록의 헤더를 통해 크기를 알아내기
    size_t size = GET_SIZE(HDRP(ptr));

    //헤더 포인터에 할당 여부를 가용으로 바꾸기
    PUT(HDRP(ptr), PACK(size, 0));
    //푸터 포인터에 할당 여부를 가용으로 바꾸기
    PUT(FTRP(ptr), PACK(size, 0));
    //인접 가용 블록들을 통합하기
    coalesce(ptr);
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
