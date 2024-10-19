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

//힙 공간의 주소를 저장하는 heap_listp 포인터 변수 선언
char * heap_listp;
//next fit 구현을 위한 bp_for_next_fit 포인터 변수 선언
char * bp_for_next_fit;

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

    //병합 이후 전역 포인터 업데이트하기
    bp_for_next_fit = bp;

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
    //next fit 구현을 위한 포인터 변수 bp_for_next_fit도 heap 메모리의 첫 번째 주소로 초기화
    bp_for_next_fit = heap_listp;

    //CHUNKSIZE(현재는 4KB, 이를 블럭화하면 1024개의 블럭)만큼의 힙 메모리 공간 확보함
    if (extend_heap(CHUNKSIZE/WSIZE) == NULL)
        return -1;

    return 0;
}

/*
* find_fit - Find appropriate memory block on heap that can save a data of a size of (asize) 
*/

static void *find_fit(size_t asize)
{
    //First Fit 방식
    // void *bp;

    // //heap의 첫 번째 블럭부터 에필로그 블럭 전까지 
    // for (bp = heap_listp; GET_SIZE(HDRP(bp)) > 0; bp = NEXT_BLKP(bp))
    // {
    //     //헤더를 살펴봤는데 할당이 되어있지 않고, asize보다 크기가 큰 블럭을 발견하면
    //     if (!GET_ALLOC(HDRP(bp)) && (asize <= GET_SIZE(HDRP(bp))))
    //     {
    //         //printf("Found appropriate memory address : %p\n", bp);
    //         //해당 블럭의 포인터 반환
    //         return bp;
    //     }
    // }

    // //할당 가능한 블럭이 없다면
    // return NULL;

    //Next Fit 방식
    //이전 검색이 종료된 지점에서 검색을 시작
    //이전 검색이 종료된 지점 : 힙에서 마지막으로 할당이 완료된 공간의 주소

    char * bp;
    //heap의 첫 번째 블럭부터 에필로그 블럭 전까지 
    for (bp = NEXT_BLKP(bp_for_next_fit); GET_SIZE(HDRP(bp)) > 0; bp = NEXT_BLKP(bp))
    {
        //printf("Current Finding Address: %p\n", bp);
        //헤더를 살펴봤는데 할당이 되어있지 않고, asize보다 크기가 큰 블럭을 발견하면
        if (!GET_ALLOC(HDRP(bp)) && (asize <= GET_SIZE(HDRP(bp))))
        {
            //해당 블럭의 포인터 반환
            return bp;
        }
    }

    //next fit으로 했는데 찾지 못한 경우 : 다시 앞에서부터 탐색
    for (bp = heap_listp; bp <= bp_for_next_fit; bp = NEXT_BLKP(bp))
    {
        //헤더를 살펴봤는데 할당이 되어있지 않고, asize보다 크기가 큰 블럭을 발견하면
        if (!GET_ALLOC(HDRP(bp)) && (asize <= GET_SIZE(HDRP(bp))))
        {
            //해당 블럭의 포인터 반환
            return bp;
        }
    }

    //할당 가능한 블럭이 없다면
    return NULL;
}

/*
* place - Place data on usable memory block
*/
static void place(void *bp, size_t asize)
{
    //place할 블럭의 사이즈를 저장하는 변수 csize
    size_t csize = GET_SIZE(HDRP(bp));

    //place할 블럭의 크기와 할당해야 하는 크기의 차이가
    //16바이트 이상일 경우
    //즉, place할 블럭에 데이터를 할당한 후 남은 크기가
    //최소 블럭 크기(16바이트) 이상이면 분할하는 과정
    if ((csize - asize) >= (2 * DSIZE))
    {
        //블럭 포인터의 헤더에 데이터 크기 및 할당 정보 저장
        PUT(HDRP(bp), PACK(asize, 1));
        //푸터에도 저장
        PUT(FTRP(bp), PACK(asize, 1));
        //place한 블럭의 남은 공간으로 넘어가기
        bp = NEXT_BLKP(bp);
       
        //헤더와 푸터를 설정하고, 크기 및 가용 정보 저장
        PUT(HDRP(bp), PACK(csize - asize, 0));
        PUT(FTRP(bp), PACK(csize - asize, 0));
    }
    //분할은 힘들 경우
    else
    {
        //블럭 포인터의 헤더와 푸터에 크기 및 할당 정보 저장
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
    
    //요청받은 크기가 8바이트 이상인 경우
    else
        //최종 할당 크기 : 오버헤드 바이트(헤더,푸터)를 추가하고 인접 8의 배수로 반올림
        //DSIZE - 1을 더하는 이유 : 만약 8의 배수 크기를 요청한다면 내림해줘야 하기 때문에
        //예) 8사이즈 크기를 할당할 때, DSIZE - 1이 아닌 DSIZE를 더한다면
        //16바이트만 할당해야 할 것을 24바이트를 할당해버린다
        asize = DSIZE * ((size + (DSIZE) + (DSIZE - 1)) / DSIZE);

    //asize를 할당 가능한 블럭을 찾았을 경우
    if ((bp = find_fit(asize)) != NULL)
    {
        //해당 블럭에 할당하기
        place(bp, asize);

        //next fit으로 구현한 경우 : current_listp에 bp 저장
        //current_listp = bp;

        //블럭 포인터 리턴
        return bp;
    }

    //asize를 할당 가능한 블럭을 찾지 못한 경우

    //추가 요청할 힙 메모리 공간
    //힙을 확장할 때 최소한 한 페이지 크기(4KB)만큼 요청하면
    //큰 단위로 메모리를 미리 확보해 여러 작은 메모리 요청을 처리할 수 있다
    //그러니까 한 번 확장한 메모리로 여러 번의 메모리 할당 요청을 처리할 수 있게 된다
    //asize가 4KB보다 클 수도 있으니 MAX 함수로 비교하는 것
    extendsize = MAX(asize, CHUNKSIZE);
    //만약 힙 메모리 추가 할당 요청에 실패했다면
    if ((bp = extend_heap(extendsize/WSIZE)) == NULL)
        //NULL 포인터 반환
        return NULL;
    //요청한 블럭을 새로운 가용 블록에 배치
    place(bp, asize);

    //next fit 구현을 위한 값 변경
    bp_for_next_fit = bp;

    //해당 블록 포인터 반환
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
    //재할당 대상 블록의 포인터
    void *oldptr = ptr;
    //재할당한 블록의 포인터
    void *newptr;

    //새로운 블럭을 할당하기 전에
    //1. 굳이 재할당을 하지 않아도 될 정도로 현재 블록이 큰지 : 이 경우 기존 포인터 그대로 반환
    //2. 그렇지 않다면 뒤쪽 블록이 가용하고, 현재 블록의 크기와 다음 블록의 크기를 더한 값이 size보다 크다면
    //뒤쪽 블록과 병합한 후 현재 블록의 포인터를 반환
    //앞쪽 블록과의 병합은 성능 저하를 고려해 진행하지 않음
    if(GET_SIZE(HDRP(oldptr)) >= size + DSIZE)
        return oldptr;
    
    else if (!GET_ALLOC(HDRP(NEXT_BLKP(oldptr))) && GET_SIZE(HDRP(oldptr)) + GET_SIZE(HDRP(NEXT_BLKP(oldptr))) >= size + DSIZE)
    {
        //printf("Condition 1 : oldptr size : %d\n", GET_SIZE(HDRP(oldptr)));
        size_t temp_size = GET_SIZE(HDRP(oldptr));
        //다음 블록의 크기를 size에 더해줌
        temp_size += GET_SIZE(HDRP(NEXT_BLKP(oldptr)));
        //우선 현재 블록의 헤더에 병합 이후의 size를 기록
        PUT(HDRP(oldptr), PACK(temp_size, 1));
        //FTRP의 구현 방식에 의해, 현재 블럭의 size 정보를 푸터가 아닌 헤더에서 가져오게 되고
        //정상적으로 병합된 크기만큼 이동해 푸터를 초기화한다
        PUT(FTRP(oldptr), PACK(temp_size, 1));
        //printf("Condition 1 : after realloc : %d\n", GET_SIZE(HDRP(oldptr)));
        return oldptr;
    }
    //상기한 과정이 모두 이루어지지 않는다면
    //그냥 새로운 블록을 할당하고
    //새로운 블록에 기존 블록의 데이터를 전부 복사
    //복사가 완료되면 기존 블록은 해제
    else
    {
        size_t copySize;
        newptr = mm_malloc(size);
        //만약 새로운 메모리 할당에 실패했다면 NULL 반환
        if (newptr == NULL)
            return NULL;
        copySize = GET_SIZE(HDRP(oldptr));
        if (size < copySize)
            copySize = size;
        memcpy(newptr, oldptr, copySize);
        mm_free(oldptr);
        return newptr;
    }

}