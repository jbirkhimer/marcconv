/*
 *  marc_mem.c
 *
 *  Wrapper for malloc(), realloc(), and free()
 *
 *  Keeps track of alloc and free operations and allows
 *  to audit memory operations and find anomalies.
 *
 *  The marc_end() function logs statistics to the log file
 *  which has a blow by blow account of the sequence of memory
 *  calls to do the marcconv program's work.
 *
 */
#include <stdio.h>
#include <fcntl.h>
#include <string.h>
#include <errno.h>
#include <stdarg.h>
#include <stdlib.h>
#include <unistd.h>
#include <mcheck.h>

#define LIST_SIZE 1000000     //  Static size of array of struct to track allocations.
#define MLOG "marc_mem.log"   //  Log file.

/*
 *  Represents a single memory allocation.
 */
struct mem_item {
    void *ptr;      //  Pointer to memory block.
    int size;       //  Block size.
    int id;         //  originationg code line.
};

typedef struct mem_item Mitem ;  // Memory allocations recorded here.
typedef unsigned int uint;
typedef unsigned char uchar;


/*
 *  Global variables start with 'g_'
 */
Mitem  g_mem_list[LIST_SIZE];             //  Allocation list.

Mitem *g_next = (Mitem *) &g_mem_list[0]; //  Pointer to next free Mitem.

Mitem *g_last_free = NULL;                //  Last Mitem vacated by marc_dealloc.

int g_marc_mem_init_done = 0;             //  Need to open log file.

FILE *g_log;                              //  Log file pointer.

/*
 *  Stats.
 */
int g_total_alloc;    //  Number of alloc calls.
int g_total_free;     //  Number of dealloc calls.
int g_max_idx;        //  Highest list index used.
int g_errs;           //  Number of errors.
int g_min_cnt;        //  Minumum allocation size.
int g_max_cnt;        //  Maximu allocation size.

/*
 *  Function prototypes.
 */
Mitem *find_Mitem(void *);
Mitem *get_Mitem(void);
Mitem *get_free(Mitem *);
int get_idx(Mitem *);
void marc_mem_init();
void *marc_alloc(int, int);
void marc_dealloc(void *, int);
void *marc_realloc(void *, int, int);
void marc_end();
void mlog(const char *format, ...);
void no_op(enum mcheck_status status) {}

/*
 *  External globals.
 */
extern int g_recnum ;    //  Number of record being processed.


/*
 *  Open log file, init GNU extension to check if a malloc'ed chunck is free-able or not.
 */
void
marc_mem_init()
{
    if ((g_log = fopen(MLOG, "w")) == NULL) {
        printf("====>>> %d: marc_mem_init: Cannot open log file %s\n", g_recnum, MLOG);
        exit(1);
    }
    mcheck(&no_op);
    setbuf(stdout, NULL);
    setbuf(g_log, NULL);
    g_marc_mem_init_done = 1;
    mlog("%d: marc_mem_init: list at %d\n", g_recnum, g_mem_list);
}


/*
 *  Wrapper for malloc.
 *
 *  Extra parameter 'id' identifies the line of code the alloc
 *  comes from, which is tagged with  '// TAG:<id>.'
 */
void *
marc_alloc(int count, int id)
{
    struct mem_item *p;
    void *retval = NULL;

    if (!g_marc_mem_init_done) {
        marc_mem_init();
    }

    if (count < g_min_cnt) {
        g_min_cnt = count;
    }

    if (count > g_max_cnt) {
        g_max_cnt = count;
    }

    mlog("%d: marc_alloc:START - count: %d id: %d\n", g_recnum, count, id);

    if (count < 1 || count > 100000) {
        mlog("====>>> %d: marc_alloc: stupid count (%d)\n", g_recnum, count);
        g_errs++;
        return retval;
    }

    /*
     *  Get allocation tracking object.
     */

    if ((p = get_Mitem()) == NULL) {
        return retval;
    }


    if ((p->ptr = malloc(count)) == NULL) {
        mlog("====>>> %d: marc_alloc: bad malloc %s\n", g_recnum, strerror(errno));
        g_errs++;
        return retval;
    }
    p->size = count;
    p->id   = id;
    retval  = p->ptr;

    mlog("%d:A: %d - %d: %d %d\n", g_recnum, get_idx(p), retval, count, id);

    mlog("%d: marc_alloc:END - count: %d id: %d\n\n", g_recnum, count, id);
    return retval;
}


/*
 *  Wrapper for free().
 *
 * (marc_free() was taken.)
 *
 *  Extra parameter 'id' identifies the line of code the alloc
 *  comes from, which is tagged with  '// TAG:<id>.'
 */
void
marc_dealloc(void *ptr, int id)
{
    Mitem *p;

    mlog("%d: marc_dealloc:START - ptr: %d id: %d\n", g_recnum, ptr, id);

    if (!g_marc_mem_init_done) {
        printf("====>>> %d: Calling FREE BEFORE MALLOC... ARGGGG!\n", g_recnum) ;
        mlog("====>>> %d: marc_dealloc:Calling FREE BEFORE MALLOC... ARGGGG! - ptr: %d id: %d\n", g_recnum, ptr, id);
        return;
    }


    if (!ptr) {
        mlog("====>>> %d: marc_dealloc: ZERO FREE ptr: id: %d\n", g_recnum, id);
        g_errs++;
        return;
    }

    p = find_Mitem(ptr) ;

    if (p) {
        if (mprobe(p->ptr) == MCHECK_FREE) {
            mlog("====>>> %d: marc_dealloc: BAD MCHECK FREE ptr: %d: count: %d id: %d\n", g_recnum, p->ptr, p->size, p->id);
            g_errs++;
        } else {
            free(p->ptr);
            mlog("%d:F: %d: - %d: %d %d\n", g_recnum, get_idx(p), p->ptr, p->size, p->id);
            p->size = 0;
            p->id = 0;
            p->ptr = NULL;
            g_last_free = p;
        }
    } else {
        mlog("====>>> %d: marc_dealloc: PTR NOT FOUND ptr: %d id: %d\n", g_recnum, ptr, id);
        g_errs++;
    }
    mlog("%d: marc_dealloc:END - ptr: %d id: %d\n\n", g_recnum, ptr, id);
}


/*
 *  Wrapper for realloc().
 *
 *  Extra parameter 'id' identifies the line of code the alloc
 *  comes from, which is tagged with  '// TAG:<id>.'
 */
void *
marc_realloc(void *ptr, int count, int id)
{
    Mitem *p;
    void *retval = NULL;

    if (!g_marc_mem_init_done) {
        marc_mem_init();
    }

    if (count < g_min_cnt) {
        g_min_cnt = count;
    }

    if (count > g_max_cnt) {
        g_max_cnt = count;
    }

    if (g_recnum == 1523368 && count == 27309) {
        mlog(">>> ptrx = Ox%x\n", ptr);
        mlog(">>>%d: marc_REALLOC:START - ptr: %d count: %d id: %d\n", g_recnum, ptr, count, id);
        if (ptr) {
            p = find_Mitem(ptr) ;
            if (p) {
                mlog(">>> %d: marc_REALLOC: Found: ptr %d size: %d id: %d idx: %d\n", g_recnum, p->ptr, p->size, p->id, get_idx(p));
            } else {
                mlog(">>>====>>> %d: marc_REALLOC: PTR NOT FOUND ptr: %d count: %d id: %d\n", g_recnum, ptr, count, id);
                g_errs++;
                return retval;
            }
            mlog(">>>BEFORE FREE() of %d (%x)\n", p->ptr, p->ptr);
            free(ptr);
            mlog(">>>AFTER  FREE() of %d (%x)\n", p->ptr, p->ptr);
            p->ptr = malloc(count);
            mlog(">>>AFTER  MALLOC()\n") ;
            p->size = count;
            p->id = id;
            return p->ptr;
        }
    }
    mlog("%d: marc_REALLOC:START - ptr: %d count: %d id: %d\n", g_recnum, ptr, count, id);

    if (count < 1 || count > 100000) {
        mlog("====>>> %d: marc_realloc: stupid count (%d)\n", g_recnum, count);
        g_errs++;
        return retval;
    }

    if (ptr) {
        p = find_Mitem(ptr) ;
        if (p) {
            mlog("%d: marc_REALLOC: Found: ptr %d size: %d id: %d idx: %d\n", g_recnum, p->ptr, p->size, p->id, get_idx(p));
        } else {
            mlog("====>>> %d: marc_REALLOC: PTR NOT FOUND ptr: %d count: %d id: %d\n", g_recnum, ptr, count, id);
            g_errs++;
            return retval;
        }
        if (mprobe(p->ptr) == MCHECK_FREE) {
            mlog("====>>> %d: marc_REALLOC: BAD MCHECK FREE ptr: %d count: %d id: %d\n", g_recnum, p->ptr, p->size, p->id);
            g_errs++;
            return retval;
        }
    } else {
        mlog("%d: marc_REALLOC: ZERO FREE ptr: id: %d\n", g_recnum, id);
        if ((p = get_Mitem()) == NULL) {
            return retval;
        }
    }

    if ((p->ptr = realloc(ptr, count)) == NULL) {
        mlog("====>>> %d: marc_REALLOC: bad realloc %s\n", g_recnum, strerror(errno));
        g_errs++;
        return retval;
    }

    p->size = count;
    p->id = id;
    retval = p->ptr;
    mlog("%d:R: %d: -  %d %d %d\n", g_recnum, get_idx(p), p->ptr, p->size, p->id);
    mlog("%d: marc_REALLOC:END - ptr: %d count: %d id: %d\n\n", g_recnum, ptr, count, id);

    return retval;
}


/*
 *  Wrapper for calloc().
 *
 *  Extra parameter 'id' identifies the line of code the alloc
 *  comes from, which is tagged with  '// TAG:<id>.'
 */
void *
marc_calloc(int count, int nitems, int id)
{
    Mitem *p;
    int total_count;
    void *retval = NULL;

    if (!g_marc_mem_init_done) {
        marc_mem_init();
    }

    total_count = count * nitems;

    if (total_count < g_min_cnt) {
        g_min_cnt = count;
    }

    if (total_count > g_max_cnt) {
        g_max_cnt = count;
    }

    mlog("%d: marc_CALLOC:START - count: %d nitems: %d id: %d\n", g_recnum, count, nitems, id);

    if (total_count < 1 || total_count > 100000) {
        mlog("====>>> %d: marc_calloc: stupid count (%d)\n", g_recnum, total_count);
        g_errs++;
        return retval;
    }


    if ((p = get_Mitem()) == NULL) {
        return retval;
    }

    if ((p->ptr = calloc(count, nitems)) == NULL) {
        mlog("====>>> %d: marc_calloc: bad calloc %s\n", g_recnum, strerror(errno));
        g_errs++;
        return retval;
    }
    p->size = total_count;
    p->id   = id;
    retval  = p->ptr;

    mlog("%d:C: %d - %d: %d %d\n", g_recnum, get_idx(p), retval, total_count, id);

    mlog("%d: marc_CALLOC:END - count: %d nitems: %d id: %d\n\n", g_recnum, count, nitems, id);

    return retval;
}


void
marc_end()
{
    mlog("END: max_idx = %d\ntotal_allocs: %d\ntotal_frees: %d\nerrors: %d\nmin_cnt: %d\nmax_cnt: %d",
            g_max_idx, g_total_alloc, g_total_free, g_errs, g_min_cnt, g_max_cnt);
    fclose(g_log);
}


Mitem *
find_Mitem(void *ptr)
{
    int i;
    Mitem *p;
    Mitem *retval = NULL;

    for (i = 0; i < LIST_SIZE; i++) {
        p = &g_mem_list[i];
        if (p->ptr == ptr) {
            retval = p;
            break;
        }
    }
    return retval;
}


/*
 *  Get the next free Mitem object.
 *
 *  Returns NULL if none free.
 */
Mitem *
get_Mitem()
{
    Mitem *retval = NULL;

    /*
     *  Reuse a Mitem if available or take the next free one in the list.
     */
    if (g_last_free) {
        retval = g_last_free;
        g_last_free = get_free(g_last_free);
    } else {
        if (g_next >= &g_mem_list[LIST_SIZE]) {
            mlog("====>>> %d: get_Mitem: Out of mem_item objects. Bye!\n", g_recnum);
            g_errs++;
        } else {
            retval = g_next++;
            g_max_idx++;
            mlog("%d: get_Mitem: Returning Mitem index %d\n", g_recnum, get_idx(retval));
        }
    }

    return retval;
}


/*
 *  Look for a free Mitem lower in the list.
 *  (Produced by two dealloc without intervening alloc.)
 */
Mitem *
get_free(Mitem *p)
{
    Mitem *retval = NULL;

    while (--p >= g_mem_list) {
        if (p->ptr == NULL) {
            retval = p;
            break;
        }
    }
    return retval;
}


/*
 *  Get the index in the Mitem list from a Mitem pointer.
 */
int
get_idx(Mitem *p)
{
    int retval = -1;

    if (p >= &g_mem_list[0] && p <= &g_mem_list[LIST_SIZE-1] && !(((uint) p - (uint) g_mem_list) % sizeof(Mitem))) {
        retval = ((uint) p - (uint) g_mem_list) / sizeof(Mitem);
    } else {
        mlog("====>>> %d: get_idx: Bad pointer to index: g_mem_list = %d p = %d sizeof(Mitem) = %d\n", g_recnum, g_mem_list, p, sizeof(Mitem));
    }
    return retval;
}


void
mlog(const char *format, ...)
{
  va_list args;

    va_start(args, format);
    vfprintf(g_log, format, args);
    va_end(args);
}

