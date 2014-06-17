#include "C-kern/konfig.h"
#include "C-kern/api/err.h"
#include "C-kern/api/test/errortimer.h"
#include "C-kern/api/memory/vm.h"
#include "C-kern/api/test/unittest.h"
#include "C-kern/api/time/timevalue.h"
#include "C-kern/api/time/systimer.h"

typedef int (* test_compare_f) (const void * left, const void * right);


/* Reverse a slice of a list in place, from lo up to (exclusive) hi. */
static void
reverse_slice(void **lo, void **hi)
{
    assert(lo && hi);

    --hi;
    while (lo < hi) {
        void *t = *lo;
        *lo = *hi;
        *hi = t;
        ++lo;
        --hi;
    }
}

#define ISLT(X, Y, COMPARE) (COMPARE(X, Y) < 0)

#define IFLT(X, Y)  if (compare(X, Y) < 0)

/* binarysort is the best method for sorting small arrays: it does
   few compares, but can do data movement quadratic in the number of
   elements.
   [lo, hi) is a contiguous slice of a list, and is sorted via
   binary insertion.  This sort is stable.
   On entry, must have lo <= start <= hi, and that [lo, start) is already
   sorted (pass start == lo if you don't know!).
   If islt() complains return -1, else 0.
   Even in case of error, the output slice will be some permutation of
   the input (nothing is lost or duplicated).
*/
static int
binarysort(void **lo, void **hi, void **start, test_compare_f compare)
     /* compare -- comparison function object, or NULL for default */
{
    register void **l, **p, **r;
    register void *pivot;

    assert(lo <= start && start <= hi);
    /* assert [lo, start) is sorted */
    if (lo == start)
        ++start;
    for (; start < hi; ++start) {
        /* set l to where *start belongs */
        l = lo;
        r = start;
        pivot = *r;
        /* Invariants:
         * pivot >= all in [lo, l).
         * pivot  < all in [r, start).
         * The second is vacuously true at the start.
         */
        assert(l < r);
        do {
            p = l + ((r - l) >> 1);
            IFLT(pivot, *p)
                r = p;
            else
                l = p+1;
        } while (l < r);
        assert(l == r);
        /* The invariants still hold, so pivot >= all in [lo, l) and
           pivot < all in [l, start), so pivot belongs at l.  Note
           that if there are elements equal to pivot, l points to the
           first slot after them -- that's why this sort is stable.
           Slide over to make room.
           Caution: using memmove is much slower under MSVC 5;
           we're not usually moving many slots. */
        for (p = start; p > l; --p)
            *p = *(p-1);
        *l = pivot;
    }
    return 0;

}

/*
Return the length of the run beginning at lo, in the slice [lo, hi).  lo < hi
is required on entry.  "A run" is the longest ascending sequence, with

    lo[0] <= lo[1] <= lo[2] <= ...

or the longest descending sequence, with

    lo[0] > lo[1] > lo[2] > ...

Boolean *descending is set to 0 in the former case, or to 1 in the latter.
For its intended use in a stable mergesort, the strictness of the defn of
"descending" is needed so that the caller can safely reverse a descending
sequence without violating stability (strict > ensures there are no equal
elements to get out of order).

Returns -1 in case of error.
*/
static ssize_t
count_run(void **lo, void **hi, test_compare_f compare, int *descending)
{
    ssize_t n;

    assert(lo < hi);
    *descending = 0;
    ++lo;
    if (lo == hi)
        return 1;

    n = 2;
    IFLT(*lo, *(lo-1)) {
        *descending = 1;
        for (lo = lo+1; lo < hi; ++lo, ++n) {
            IFLT(*lo, *(lo-1))
                ;
            else
                break;
        }
    }
    else {
        for (lo = lo+1; lo < hi; ++lo, ++n) {
            IFLT(*lo, *(lo-1))
                break;
        }
    }

    return n;
}

/*
Locate the proper position of key in a sorted vector; if the vector contains
an element equal to key, return the position immediately to the left of
the leftmost equal element.  [gallop_right() does the same except returns
the position to the right of the rightmost equal element (if any).]

"a" is a sorted vector with n elements, starting at a[0].  n must be > 0.

"hint" is an index at which to begin the search, 0 <= hint < n.  The closer
hint is to the final result, the faster this runs.

The return value is the int k in 0..n such that

    a[k-1] < key <= a[k]

pretending that *(a-1) is minus infinity and a[n] is plus infinity.  IOW,
key belongs at index k; or, IOW, the first k elements of a should precede
key, and the last n-k should follow key.

Returns -1 on error.  See listsort.txt for info on the method.
*/
static ssize_t
gallop_left(void *key, void **a, ssize_t n, ssize_t hint, test_compare_f compare)
{
    ssize_t ofs;
    ssize_t lastofs;
    ssize_t k;

    assert(a && n > 0 && hint >= 0 && hint < n);

    a += hint;
    lastofs = 0;
    ofs = 1;
    IFLT(*a, key) {
        /* a[hint] < key -- gallop right, until
         * a[hint + lastofs] < key <= a[hint + ofs]
         */
        const ssize_t maxofs = n - hint;             /* &a[n-1] is highest */
        while (ofs < maxofs) {
            IFLT(a[ofs], key) {
                lastofs = ofs;
                ofs = (ofs << 1) + 1;
                if (ofs <= 0)                   /* int overflow */
                    ofs = maxofs;
            }
            else                /* key <= a[hint + ofs] */
                break;
        }
        if (ofs > maxofs)
            ofs = maxofs;
        /* Translate back to offsets relative to &a[0]. */
        lastofs += hint;
        ofs += hint;
    }
    else {
        /* key <= a[hint] -- gallop left, until
         * a[hint - ofs] < key <= a[hint - lastofs]
         */
        const ssize_t maxofs = hint + 1;             /* &a[0] is lowest */
        while (ofs < maxofs) {
            IFLT(*(a-ofs), key)
                break;
            /* key <= a[hint - ofs] */
            lastofs = ofs;
            ofs = (ofs << 1) + 1;
            if (ofs <= 0)               /* int overflow */
                ofs = maxofs;
        }
        if (ofs > maxofs)
            ofs = maxofs;
        /* Translate back to positive offsets relative to &a[0]. */
        k = lastofs;
        lastofs = hint - ofs;
        ofs = hint - k;
    }
    a -= hint;

    assert(-1 <= lastofs && lastofs < ofs && ofs <= n);
    /* Now a[lastofs] < key <= a[ofs], so key belongs somewhere to the
     * right of lastofs but no farther right than ofs.  Do a binary
     * search, with invariant a[lastofs-1] < key <= a[ofs].
     */
    ++lastofs;
    while (lastofs < ofs) {
        ssize_t m = lastofs + ((ofs - lastofs) >> 1);

        IFLT(a[m], key)
            lastofs = m+1;              /* a[m] < key */
        else
            ofs = m;                    /* key <= a[m] */
    }
    assert(lastofs == ofs);             /* so a[ofs-1] < key <= a[ofs] */
    return ofs;

}

/*
Exactly like gallop_left(), except that if key already exists in a[0:n],
finds the position immediately to the right of the rightmost equal value.

The return value is the int k in 0..n such that

    a[k-1] <= key < a[k]

or -1 if error.

The code duplication is massive, but this is enough different given that
we're sticking to "<" comparisons that it's much harder to follow if
written as one routine with yet another "left or right?" flag.
*/
static ssize_t
gallop_right(void *key, void **a, ssize_t n, ssize_t hint, test_compare_f compare)
{
    ssize_t ofs;
    ssize_t lastofs;
    ssize_t k;

    assert(a && n > 0 && hint >= 0 && hint < n);

    a += hint;
    lastofs = 0;
    ofs = 1;
    IFLT(key, *a) {
        /* key < a[hint] -- gallop left, until
         * a[hint - ofs] <= key < a[hint - lastofs]
         */
        const ssize_t maxofs = hint + 1;             /* &a[0] is lowest */
        while (ofs < maxofs) {
            IFLT(key, *(a-ofs)) {
                lastofs = ofs;
                ofs = (ofs << 1) + 1;
                if (ofs <= 0)                   /* int overflow */
                    ofs = maxofs;
            }
            else                /* a[hint - ofs] <= key */
                break;
        }
        if (ofs > maxofs)
            ofs = maxofs;
        /* Translate back to positive offsets relative to &a[0]. */
        k = lastofs;
        lastofs = hint - ofs;
        ofs = hint - k;
    }
    else {
        /* a[hint] <= key -- gallop right, until
         * a[hint + lastofs] <= key < a[hint + ofs]
        */
        const ssize_t maxofs = n - hint;             /* &a[n-1] is highest */
        while (ofs < maxofs) {
            IFLT(key, a[ofs])
                break;
            /* a[hint + ofs] <= key */
            lastofs = ofs;
            ofs = (ofs << 1) + 1;
            if (ofs <= 0)               /* int overflow */
                ofs = maxofs;
        }
        if (ofs > maxofs)
            ofs = maxofs;
        /* Translate back to offsets relative to &a[0]. */
        lastofs += hint;
        ofs += hint;
    }
    a -= hint;

    assert(-1 <= lastofs && lastofs < ofs && ofs <= n);
    /* Now a[lastofs] <= key < a[ofs], so key belongs somewhere to the
     * right of lastofs but no farther right than ofs.  Do a binary
     * search, with invariant a[lastofs-1] <= key < a[ofs].
     */
    ++lastofs;
    while (lastofs < ofs) {
        ssize_t m = lastofs + ((ofs - lastofs) >> 1);

        IFLT(key, a[m])
            ofs = m;                    /* key < a[m] */
        else
            lastofs = m+1;              /* a[m] <= key */
    }
    assert(lastofs == ofs);             /* so a[ofs-1] <= key < a[ofs] */
    return ofs;
}

/* The maximum number of entries in a MergeState's pending-runs stack.
 * This is enough to sort arrays of size up to about
 *     32 * phi ** MAX_MERGE_PENDING
 * where phi ~= 1.618.  85 is ridiculouslylarge enough, good for an array
 * with 2**64 elements.
 */
#define MAX_MERGE_PENDING 85

/* When we get into galloping mode, we stay there until both runs win less
 * often than MIN_GALLOP consecutive times.  See listsort.txt for more info.
 */
#define MIN_GALLOP 7

/* Avoid malloc for small temp arrays. */
#define MERGESTATE_TEMP_SIZE 256

/* One MergeState exists on the stack per invocation of mergesort.  It's just
 * a convenient way to pass state around among the helper functions.
 */
struct s_slice {
    void **base;
    ssize_t len;
};

typedef struct s_MergeState {
    /* The user-supplied comparison function. or NULL if none given. */
    test_compare_f compare;

    /* This controls when we get *into* galloping mode.  It's initialized
     * to MIN_GALLOP.  merge_lo and merge_hi tend to nudge it higher for
     * random data, and lower for highly structured data.
     */
    ssize_t min_gallop;

    /* 'a' is temp storage to help with merges.  It contains room for
     * alloced entries.
     */
    void **a;       /* may point to temparray below */
    ssize_t alloced;

    /* A stack of n pending runs yet to be merged.  Run #i starts at
     * address base[i] and extends for len[i] elements.  It's always
     * true (so long as the indices are in bounds) that
     *
     *     pending[i].base + pending[i].len == pending[i+1].base
     *
     * so we could cut the storage for this, but it's a minor amount,
     * and keeping all the info explicit simplifies the code.
     */
    int n;
    struct s_slice pending[MAX_MERGE_PENDING];

    /* 'a' points to this when possible, rather than muck with malloc. */
    void *temparray[MERGESTATE_TEMP_SIZE];
} MergeState;

/* Conceptually a MergeState's constructor. */
static void
merge_init(MergeState *ms, test_compare_f compare)
{
    assert(ms != NULL);
    ms->compare = compare;
    ms->a = ms->temparray;
    ms->alloced = MERGESTATE_TEMP_SIZE;
    ms->n = 0;
    ms->min_gallop = MIN_GALLOP;
}

/* Free all the temp memory owned by the MergeState.  This must be called
 * when you're done with a MergeState, and may be called before then if
 * you want to free the temp memory early.
 */
static void
merge_freemem(MergeState *ms)
{
    assert(ms != NULL);
    if (ms->a != ms->temparray)
        free(ms->a);
    ms->a = ms->temparray;
    ms->alloced = MERGESTATE_TEMP_SIZE;
}

/* Ensure enough temp memory for 'need' array slots is available.
 * Returns 0 on success and -1 if the memory can't be gotten.
 */
static int
merge_getmem(MergeState *ms, ssize_t need)
{
    assert(ms != NULL);
    if (need <= ms->alloced)
        return 0;
    /* Don't realloc!  That can cost cycles to copy the old data, but
     * we don't care what's in the block.
     */
    merge_freemem(ms);
    if ((size_t)need > SSIZE_MAX / sizeof(void*)) {
        printf("Out of memory\n");
        return -1;
    }
    ms->a = (void **)malloc((size_t)need * sizeof(void*));
    if (ms->a) {
        ms->alloced = need;
        return 0;
    }
    printf("Out of memory\n");
    merge_freemem(ms);          /* reset to sane state */
    return -1;
}
#define MERGE_GETMEM(MS, NEED) ((NEED) <= (MS)->alloced ? 0 :   \
                                merge_getmem(MS, NEED))

/* Merge the na elements starting at pa with the nb elements starting at pb
 * in a stable way, in-place.  na and nb must be > 0, and pa + na == pb.
 * Must also have that *pb < *pa, that pa[na-1] belongs at the end of the
 * merge, and should have na <= nb.  See listsort.txt for more info.
 * Return 0 if successful, -1 if error.
 */
static ssize_t
merge_lo(MergeState *ms, void **pa, ssize_t na,
                         void **pb, ssize_t nb)
{
    ssize_t k;
    test_compare_f compare;
    void **dest;
    int result = -1;            /* guilty until proved innocent */
    ssize_t min_gallop;

    assert(ms && pa && pb && na > 0 && nb > 0 && pa + na == pb);
    if (MERGE_GETMEM(ms, na) < 0)
        return -1;
    memcpy(ms->a, pa, (size_t)na * sizeof(void*));
    dest = pa;
    pa = ms->a;

    *dest++ = *pb++;
    --nb;
    if (nb == 0)
        goto Succeed;
    if (na == 1)
        goto CopyB;

    min_gallop = ms->min_gallop;
    compare = ms->compare;
    for (;;) {
        ssize_t acount = 0;          /* # of times A won in a row */
        ssize_t bcount = 0;          /* # of times B won in a row */

        /* Do the straightforward thing until (if ever) one run
         * appears to win consistently.
         */
        for (;;) {
            assert(na > 1 && nb > 0);
            k = ISLT(*pb, *pa, compare);
            if (k) {
                if (k < 0)
                    goto Fail;
                *dest++ = *pb++;
                ++bcount;
                acount = 0;
                --nb;
                if (nb == 0)
                    goto Succeed;
                if (bcount >= min_gallop)
                    break;
            }
            else {
                *dest++ = *pa++;
                ++acount;
                bcount = 0;
                --na;
                if (na == 1)
                    goto CopyB;
                if (acount >= min_gallop)
                    break;
            }
        }

        /* One run is winning so consistently that galloping may
         * be a huge win.  So try that, and continue galloping until
         * (if ever) neither run appears to be winning consistently
         * anymore.
         */
        ++min_gallop;
        do {
            assert(na > 1 && nb > 0);
            min_gallop -= min_gallop > 1;
            ms->min_gallop = min_gallop;
            k = gallop_right(*pb, pa, na, 0, compare);
            acount = k;
            if (k) {
                if (k < 0)
                    goto Fail;
                memcpy(dest, pa, (size_t)k * sizeof(void *));
                dest += k;
                pa += k;
                na -= k;
                if (na == 1)
                    goto CopyB;
                /* na==0 is impossible now if the comparison
                 * function is consistent, but we can't assume
                 * that it is.
                 */
                if (na == 0)
                    goto Succeed;
            }
            *dest++ = *pb++;
            --nb;
            if (nb == 0)
                goto Succeed;

            k = gallop_left(*pa, pb, nb, 0, compare);
            bcount = k;
            if (k) {
                if (k < 0)
                    goto Fail;
                memmove(dest, pb, (size_t)k * sizeof(void *));
                dest += k;
                pb += k;
                nb -= k;
                if (nb == 0)
                    goto Succeed;
            }
            *dest++ = *pa++;
            --na;
            if (na == 1)
                goto CopyB;
        } while (acount >= MIN_GALLOP || bcount >= MIN_GALLOP);
        ++min_gallop;           /* penalize it for leaving galloping mode */
        ms->min_gallop = min_gallop;
    }
Succeed:
    result = 0;
Fail:
    if (na)
        memcpy(dest, pa, (size_t)na * sizeof(void*));
    return result;
CopyB:
    assert(na == 1 && nb > 0);
    /* The last element of pa belongs at the end of the merge. */
    memmove(dest, pb, (size_t)nb * sizeof(void *));
    dest[nb] = *pa;
    return 0;
}

/* Merge the na elements starting at pa with the nb elements starting at pb
 * in a stable way, in-place.  na and nb must be > 0, and pa + na == pb.
 * Must also have that *pb < *pa, that pa[na-1] belongs at the end of the
 * merge, and should have na >= nb.  See listsort.txt for more info.
 * Return 0 if successful, -1 if error.
 */
static ssize_t
merge_hi(MergeState *ms, void **pa, ssize_t na, void **pb, ssize_t nb)
{
    ssize_t k;
    test_compare_f compare;
    void **dest;
    int result = -1;            /* guilty until proved innocent */
    void **basea;
    void **baseb;
    ssize_t min_gallop;

    assert(ms && pa && pb && na > 0 && nb > 0 && pa + na == pb);
    if (MERGE_GETMEM(ms, nb) < 0)
        return -1;
    dest = pb + nb - 1;
    memcpy(ms->a, pb, (size_t)nb * sizeof(void*));
    basea = pa;
    baseb = ms->a;
    pb = ms->a + nb - 1;
    pa += na - 1;

    *dest-- = *pa--;
    --na;
    if (na == 0)
        goto Succeed;
    if (nb == 1)
        goto CopyA;

    min_gallop = ms->min_gallop;
    compare = ms->compare;
    for (;;) {
        ssize_t acount = 0;          /* # of times A won in a row */
        ssize_t bcount = 0;          /* # of times B won in a row */

        /* Do the straightforward thing until (if ever) one run
         * appears to win consistently.
         */
        for (;;) {
            assert(na > 0 && nb > 1);
            k = ISLT(*pb, *pa, compare);
            if (k) {
                if (k < 0)
                    goto Fail;
                *dest-- = *pa--;
                ++acount;
                bcount = 0;
                --na;
                if (na == 0)
                    goto Succeed;
                if (acount >= min_gallop)
                    break;
            }
            else {
                *dest-- = *pb--;
                ++bcount;
                acount = 0;
                --nb;
                if (nb == 1)
                    goto CopyA;
                if (bcount >= min_gallop)
                    break;
            }
        }

        /* One run is winning so consistently that galloping may
         * be a huge win.  So try that, and continue galloping until
         * (if ever) neither run appears to be winning consistently
         * anymore.
         */
        ++min_gallop;
        do {
            assert(na > 0 && nb > 1);
            min_gallop -= min_gallop > 1;
            ms->min_gallop = min_gallop;
            k = gallop_right(*pb, basea, na, na-1, compare);
            if (k < 0)
                goto Fail;
            k = na - k;
            acount = k;
            if (k) {
                dest -= k;
                pa -= k;
                memmove(dest+1, pa+1, (size_t)k * sizeof(void *));
                na -= k;
                if (na == 0)
                    goto Succeed;
            }
            *dest-- = *pb--;
            --nb;
            if (nb == 1)
                goto CopyA;

            k = gallop_left(*pa, baseb, nb, nb-1, compare);
            if (k < 0)
                goto Fail;
            k = nb - k;
            bcount = k;
            if (k) {
                dest -= k;
                pb -= k;
                memcpy(dest+1, pb+1, (size_t)k * sizeof(void *));
                nb -= k;
                if (nb == 1)
                    goto CopyA;
                /* nb==0 is impossible now if the comparison
                 * function is consistent, but we can't assume
                 * that it is.
                 */
                if (nb == 0)
                    goto Succeed;
            }
            *dest-- = *pa--;
            --na;
            if (na == 0)
                goto Succeed;
        } while (acount >= MIN_GALLOP || bcount >= MIN_GALLOP);
        ++min_gallop;           /* penalize it for leaving galloping mode */
        ms->min_gallop = min_gallop;
    }
Succeed:
    result = 0;
Fail:
    if (nb)
        memcpy(dest-(nb-1), baseb, (size_t)nb * sizeof(void*));
    return result;
CopyA:
    assert(nb == 1 && na > 0);
    /* The first element of pb belongs at the front of the merge. */
    dest -= na;
    pa -= na;
    memmove(dest+1, pa+1, (size_t)na * sizeof(void *));
    *dest = *pb;
    return 0;
}

/* Merge the two runs at stack indices i and i+1.
 * Returns 0 on success, -1 on error.
 */
static ssize_t
merge_at(MergeState *ms, ssize_t i)
{
    void **pa, **pb;
    ssize_t na, nb;
    ssize_t k;
    test_compare_f compare;

    assert(ms != NULL);
    assert(ms->n >= 2);
    assert(i >= 0);
    assert(i == ms->n - 2 || i == ms->n - 3);

    pa = ms->pending[i].base;
    na = ms->pending[i].len;
    pb = ms->pending[i+1].base;
    nb = ms->pending[i+1].len;
    assert(na > 0 && nb > 0);
    assert(pa + na == pb);

    /* Record the length of the combined runs; if i is the 3rd-last
     * run now, also slide over the last run (which isn't involved
     * in this merge).  The current run i+1 goes away in any case.
     */
    ms->pending[i].len = na + nb;
    if (i == ms->n - 3)
        ms->pending[i+1] = ms->pending[i+2];
    --ms->n;

    /* Where does b start in a?  Elements in a before that can be
     * ignored (already in place).
     */
    compare = ms->compare;
    k = gallop_right(*pb, pa, na, 0, compare);
    if (k < 0)
        return -1;
    pa += k;
    na -= k;
    if (na == 0)
        return 0;

    /* Where does a end in b?  Elements in b after that can be
     * ignored (already in place).
     */
    nb = gallop_left(pa[na-1], pb, nb, nb-1, compare);
    if (nb <= 0)
        return nb;

    /* Merge what remains of the runs, using a temp array with
     * min(na, nb) elements.
     */
    if (na <= nb)
        return merge_lo(ms, pa, na, pb, nb);
    else
        return merge_hi(ms, pa, na, pb, nb);
}

/* Examine the stack of runs waiting to be merged, merging adjacent runs
 * until the stack invariants are re-established:
 *
 * 1. len[-3] > len[-2] + len[-1]
 * 2. len[-2] > len[-1]
 *
 * See listsort.txt for more info.
 *
 * Returns 0 on success, -1 on error.
 */
static int
merge_collapse(MergeState *ms)
{
    struct s_slice *p = ms->pending;

    assert(ms);
    while (ms->n > 1) {
        ssize_t n = ms->n - 2;
        if (n > 0 && p[n-1].len <= p[n].len + p[n+1].len) {
            if (p[n-1].len < p[n+1].len)
                --n;
            if (merge_at(ms, n) < 0)
                return -1;
        }
        else if (p[n].len <= p[n+1].len) {
                 if (merge_at(ms, n) < 0)
                        return -1;
        }
        else
            break;
    }
    return 0;
}

/* Regardless of invariants, merge all runs on the stack until only one
 * remains.  This is used at the end of the mergesort.
 *
 * Returns 0 on success, -1 on error.
 */
static int
merge_force_collapse(MergeState *ms)
{
    struct s_slice *p = ms->pending;

    assert(ms);
    while (ms->n > 1) {
        ssize_t n = ms->n - 2;
        if (n > 0 && p[n-1].len < p[n+1].len)
            --n;
        if (merge_at(ms, n) < 0)
            return -1;
    }
    return 0;
}

/* Compute a good value for the minimum run length; natural runs shorter
 * than this are boosted artificially via binary insertion.
 *
 * If n < 64, return n (it's too small to bother with fancy stuff).
 * Else if n is an exact power of 2, return 32.
 * Else return an int k, 32 <= k <= 64, such that n/k is close to, but
 * strictly less than, an exact power of 2.
 *
 * See listsort.txt for more info.
 */
static size_t
merge_compute_minrun(size_t n)
{
    size_t r = 0;           /* becomes 1 if any 1 bits are shifted off */

    while (n >= 64) {
        r |= n & 1;
        n >>= 1;
    }
    return n + r;
}

static int
listsort(size_t len, void * a[len], test_compare_f compare)
{
    MergeState ms;
    void **lo, **hi;
    size_t nremaining;
    size_t minrun;
    int result = -1;            /* guilty until proved innocent */

    merge_init(&ms, compare);

    nremaining = len;
    if (nremaining < 2)
        goto succeed;

    /* March over the array once, left to right, finding natural runs,
     * and extending short natural runs to minrun elements.
     */
    lo = a;
    hi = lo + nremaining;
    minrun = merge_compute_minrun(nremaining);
    do {
        int descending;
        ssize_t n;

        /* Identify next run. */
        n = count_run(lo, hi, compare, &descending);
        if (n < 0)
            goto fail;
        if (descending)
            reverse_slice(lo, lo + n);
        /* If short, extend to min(minrun, nremaining). */
        if ((size_t)n < minrun) {
            const size_t force = nremaining <= minrun ?
                              nremaining : minrun;
            if (binarysort(lo, lo + force, lo + n, compare) < 0)
                goto fail;
            n = (ssize_t) force;
        }
        /* Push run onto pending-runs stack, and maybe merge. */
        assert(ms.n < MAX_MERGE_PENDING);
        ms.pending[ms.n].base = lo;
        ms.pending[ms.n].len = n;
        ++ms.n;
        if (merge_collapse(&ms) < 0)
            goto fail;
        /* Advance to find next run. */
        lo += n;
        nremaining -= (size_t)n;
         // printf("nremaining %d\n", nremaining );
    } while (nremaining);
    assert(lo == hi);
    // printf("merge_force_collapse\n");
    if (merge_force_collapse(&ms) < 0)
        goto fail;
    assert(ms.n == 1);
    assert(ms.pending[0].base == a);
    assert(ms.pending[0].len  == (ssize_t)len);

succeed:
    result = 0;
fail:

    merge_freemem(&ms);

    return result;
}



static uint64_t s_compare_count = 0;

static int test_compare(const void * left, const void * right)
{
   ++s_compare_count;
   // works cause all pointer values are positive integer values ==> no overflow if subtracted
   if ((uintptr_t)left < (uintptr_t)right)
      return -1;
   else if ((uintptr_t)left > (uintptr_t)right)
      return +1;
   return 0;
}

static int test_compare2(const void * left, const void * right)
{
   ++s_compare_count;
   // works cause all pointer values are positive integer values ==> no overflow if subtracted
   if (*(const uintptr_t*)left < *(const uintptr_t*)right)
      return -1;
   else if (*(const uintptr_t*)left > *(const uintptr_t*)right)
      return +1;
   return 0;
}

static int test_sort(void)
{
   vmpage_t       vmpage;
   void        ** a      = 0;
   const unsigned len    = 500000;

   // prepare
   s_compare_count = 0;
   TEST(0 == init_vmpage(&vmpage, len * sizeof(void*)));
   a = (void**) vmpage.addr;

   for (uintptr_t i = 0; i < len; ++i) {
      a[i] = (void*) i;
   }

   srandom(123456);
   for (unsigned i = 0; i < len; ++i) {
      unsigned r = ((unsigned) random()) % len;
      void * t = a[r];
      a[r] = a[i];
      a[i] = t;
   }

   systimer_t timer;
   TEST(0 == init_systimer(&timer, sysclock_MONOTONIC));
   TEST(0 == startinterval_systimer(timer, &(struct timevalue_t){.nanosec = 1000000}));
   listsort(len, a, test_compare);
   uint64_t mergetime_ms;
   TEST(0 == expirationcount_systimer(timer, &mergetime_ms));
   TEST(0 == free_systimer(&timer));

   printf("time = %u\n", (unsigned) mergetime_ms);

   for (uintptr_t i = 0; i < len; ++i) {
      TEST(a[i] == (void*) i);
   }

   printf("compare_count = %llu\n", s_compare_count);

   // unprepare
   a = 0;
   TEST(0 == free_vmpage(&vmpage));

   return 0;
ONABORT:
   if (a) free_vmpage(&vmpage);
   return EINVAL;
}

int unittest_sort_testsort(void)
{
   if (test_sort())           goto ONABORT;

   return 0;
ONABORT:
   return EINVAL;
}
