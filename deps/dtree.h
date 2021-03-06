/*  Dtree -- distributed dynamic scheduler
 *
 *  implementation
 *
 */

#ifndef _DTREE_H
#define _DTREE_H

#include <mpi.h>
#include <stdint.h>


/*  test-and-test-and-set lock macros
 */
#define lock_init(l)                    \
    (l) = 0

#define lock_acquire(l)                 \
    do {                                \
        while ((l))                     \
            cpupause();                 \
    } while (__sync_lock_test_and_set(&(l), 1))
#define lock_release(l)                 \
    (l) = 0


/*  thread yield and delay
 */
#if (__MIC__)
# define cpupause()     _mm_delay_64(100)
//# define waitcycles(c)  _mm_delay_64((c))
# define waitcycles(c) {                \
      uint64_t s=_rdtsc();              \
      while ((_rdtsc()-s)<(c))          \
          _mm_delay_64(100);            \
  }
#else
# define cpupause()     _mm_pause()
# define waitcycles(c) {                \
      uint64_t s=_rdtsc();              \
      while ((_rdtsc()-s)<(c))          \
          _mm_pause();                  \
  }
#endif


/*  dtree debugging and profiling
 */
#ifdef DEBUG_DTREE
#ifndef TRACE_DTREE
#define TRACE_DTREE        1
#endif
#endif

#ifdef TRACE_DTREE

#if TRACE_DTREE == 1
#define TRACE(dt,x...)          \
    if ((dt)->my_rank == 0 || (dt)->my_rank == (dt)->num_ranks-1) \
        fprintf(stderr, x)
#elif TRACE_DTREE == 2
#define TRACE(dt,x...)          \
    if ((dt)->my_rank < 18 || (dt)->my_rank > (dt)->num_ranks-19) \
        fprintf(stderr, x)
#elif TRACE_DTREE == 3
#define TRACE(dt,x...)          \
    fprintf(stderr, x)
#else
#define TRACE(dt,x...)
#endif

#else

#define TRACE(x...)

#endif

#ifdef PROFILE_DTREE
enum {
    TIME_GETWORK, TIME_MPIWAIT, TIME_MPISEND, TIME_RUN, NTIMES
};

char *times_names[] = {
    "getwork", "mpiwait", "mpisend", "runtree", ""
};

typedef struct thread_timing_tag {
    uint64_t last, min, max, total, count;
} thread_timing_t;
#endif


/*  Dtree container
 */
typedef struct dtree_tag {
    /* tree structure */
    int8_t              num_levels, my_level;
    int                 parent, *children, num_children;
    double              tot_children;

    /* MPI info */
    int                 my_rank, num_ranks;
    int16_t             *children_req_bufs;
    MPI_Request         parent_req, *children_reqs;

    /* work distribution policy */
    int16_t             parents_work;
    double              first, rest;
    double              *distrib_fractions;
    int16_t             min_distrib;

    /* work items */
    int64_t             first_work_item, last_work_item, next_work_item;
    int64_t volatile    work_lock __attribute((aligned(8)));

    /* for heterogeneous clusters */
    double              node_mul;

    /* concurrent calls in from how many threads? */
    int                 num_threads;
    int                 (*threadid)();
#ifdef PROFILE_DTREE
    thread_timing_t     **times;
#endif

} dtree_t;


/* Dtree interface
 */
int     dtree_init(int argc, char **argv);
int     dtree_shutdown(void);
int     dtree_create(int fan_out, int64_t num_work_items,
            int can_parent, int parents_work,
            double node_mul, int num_threads, int (*threadid)(),
            double first, double rest, int16_t min_distrib,
            dtree_t **dt, int *is_parent);
void    dtree_destroy(dtree_t *dt);

/* call to get initial work allocation; before dtree_run() is called */
int64_t dtree_initwork(dtree_t *dt, int64_t *first_item, int64_t *last_item);

/* get a block of work */
int64_t dtree_getwork(dtree_t *dt, int64_t *first_item, int64_t *last_item);

/* call from a thread repeatedly until it returns 0 */
int     dtree_run(dtree_t *dt);

/* number of participating nodes */
int     dtree_nnodes();

/* this node's unique identifier */
int     dtree_nodeid();

/* utility helpers */
uint64_t rdtsc();
void     cpu_pause();

#endif  /* _DTREE_H */

