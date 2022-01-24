#include <string.h>
#include "memlib.h"
#include "mm.h"

team_t team = {"c", "s", "a", "p", "p"};

#define WSIZE (sizeof(size_t) *    1)
#define DSIZE (sizeof(size_t) *    2)
#define CSIZE (sizeof(size_t) * 1024)

#define MAXIMUM(x, y) ((x) > (y) ? (x) : (y))
#define MINIMUM(x, y) ((x) < (y) ? (x) : (y))
#define CEILING(x, y) (((x) + ((y) - 1)) / (y) * (y))

#define PACK(size, allc) ((size) | (allc))

#define GET(p)    (*(size_t *)(p))
#define PUT(p, v) (*(size_t *)(p) = (v))

#define GET_SIZE(bp) (GET(bp - WSIZE) & ~0x7)
#define GET_ALLC(bp) (GET(bp - WSIZE) &  0x1)

#define HDRP(bp) ((bp) - WSIZE)
#define FTRP(bp) ((bp) + GET_SIZE(bp) - DSIZE)

#define NEXT_BLKP(bp) ((bp) + (GET((bp) - WSIZE) & ~0x7))
#define PREV_BLKP(bp) ((bp) - (GET((bp) - DSIZE) & ~0x7))

#define NEXT_BLKP_ALLC(bp) (GET(FTRP(bp) + WSIZE) & 0x1)
#define PREV_BLKP_ALLC(bp) (GET(HDRP(bp) - WSIZE) & 0x1)

#define NEXT_LNKP(bp) (*((void **)bp + 0))
#define PREV_LNKP(bp) (*((void **)bp + 1))

static void *head_list;

static void *extend(size_t size);
static void *coalesce(void *bp);
static void  split(void *bp, size_t size);
static void *findfit(size_t size);
static void  insert(void *bp);
static void  delete(void *bp);

int mm_init() {
    if ((head_list = mem_sbrk(12 * WSIZE)) == (void *)-1) return -1;
    PUT(head_list +  0 * WSIZE, 0);
    PUT(head_list +  1 * WSIZE, 0);
    PUT(head_list +  2 * WSIZE, 0);
    PUT(head_list +  3 * WSIZE, 0);
    PUT(head_list +  4 * WSIZE, 0);
    PUT(head_list +  5 * WSIZE, 0);
    PUT(head_list +  6 * WSIZE, 0);
    PUT(head_list +  7 * WSIZE, 0);
    PUT(head_list +  8 * WSIZE, 0);
    PUT(head_list + 10 * WSIZE, 1);
    PUT(head_list + 11 * WSIZE, 1);
    return 0;
}

void *mm_malloc(size_t size) {
    if (!size) return NULL;
    size_t asize = CEILING(size, DSIZE) + DSIZE;
    void *bp;
    if ((bp = findfit(asize))) delete(bp);
    else {
        size_t esize = MAXIMUM(asize, CSIZE);
        if (!(bp = extend(esize))) return NULL;
        bp = coalesce(bp);
    }
    split(bp, asize);
    return bp;
}

void mm_free(void *bp) { insert(coalesce(bp)); }

void *mm_realloc(void *obp, size_t nsize) {
    if (!obp) return mm_malloc(nsize);
    if (!nsize) { mm_free(obp); return NULL; }
    size_t osize = GET_SIZE(obp);
    if (nsize == osize) return obp;
    osize = MINIMUM(nsize, osize);
    void *nbp = mm_malloc(nsize);
    memcpy(nbp, obp, osize);
    mm_free(obp);
    return nbp;
}

void *extend(size_t size) {
    void *bp;
    if ((bp = mem_sbrk(size)) == (void *)-1) return NULL;
    PUT(HDRP(bp), PACK(size, 0));
    PUT(FTRP(bp), PACK(size, 0));
    PUT(HDRP(NEXT_BLKP(bp)), PACK(0, 1));
    return bp;
}

void *coalesce(void *bp) {
    size_t size = GET_SIZE(bp);
    if (!PREV_BLKP_ALLC(bp)) { delete(PREV_BLKP(bp)); size += GET_SIZE(PREV_BLKP(bp)); }
    if (!NEXT_BLKP_ALLC(bp)) { delete(NEXT_BLKP(bp)); size += GET_SIZE(NEXT_BLKP(bp)); }
    PUT(!PREV_BLKP_ALLC(bp) ? HDRP(PREV_BLKP(bp)) : HDRP(bp), PACK(size, 0));
    PUT(!NEXT_BLKP_ALLC(bp) ? FTRP(NEXT_BLKP(bp)) : FTRP(bp), PACK(size, 0));
    return !PREV_BLKP_ALLC(bp) ? PREV_BLKP(bp) : bp;
}

void split(void *bp, size_t asize) {
    size_t csize = GET_SIZE(bp);
    if (csize - asize >= 2 * DSIZE) {
        PUT(HDRP(bp), PACK(asize, 1));
        PUT(FTRP(bp), PACK(asize, 1));
        PUT(HDRP(NEXT_BLKP(bp)), PACK(csize-asize, 0));
        PUT(FTRP(NEXT_BLKP(bp)), PACK(csize-asize, 0));
        insert(NEXT_BLKP(bp));
    } else {
        PUT(HDRP(bp), PACK(csize, 1));
        PUT(FTRP(bp), PACK(csize, 1));
    }
}

static int getindex(size_t size);

void *findfit(size_t size) {
    for (int i = getindex(size); i < 9; ++i) {
        void *pp = head_list + i * WSIZE;
        while ((pp = NEXT_LNKP(pp)))
            if (GET_SIZE(pp) >= size)
                return pp;
    }
    return NULL;
}

void insert(void *bp) {
    size_t size = GET_SIZE(bp);
    void *hp = head_list + getindex(size) * WSIZE;
    NEXT_LNKP(bp) = NEXT_LNKP(hp);
    if (NEXT_LNKP(hp))
        PREV_LNKP(NEXT_LNKP(hp)) = bp;
    PREV_LNKP(bp) = hp;
    NEXT_LNKP(hp) = bp;
}

void delete(void *bp) {
    if (NEXT_LNKP(bp))
        PREV_LNKP(NEXT_LNKP(bp)) = PREV_LNKP(bp);
    NEXT_LNKP(PREV_LNKP(bp)) = NEXT_LNKP(bp);
}

int getindex(size_t size) {
    if      (size <=   32) return 0;
    else if (size <=   64) return 1;
    else if (size <=  128) return 2;
    else if (size <=  256) return 3;
    else if (size <=  512) return 4;
    else if (size <= 1024) return 5;
    else if (size <= 2048) return 6;
    else if (size <= 4096) return 7;
    else                   return 8;
}
