/*
 * Copyright 1998-1999 Vrije Universiteit, Amsterdam, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in
 * the top level of the DASLIB distribution.
 */

/*
 * DASLIB authors:
 *	Rutger Hofman	<rutger@cs.vu.nl>
 *	Kees Verstoep	<versto@cs.vu.nl>
 */

#ifndef __DASLIB_PTHREAD_H__
#define __DASLIB_PTHREAD_H__

#include <pthread.h>

/* Allow inlining of pthread_mutex_(un)lock but only for the
 * case of a contentionless lock, and only for PTHREAD_MUTEX_FAST mutexes.
 * Stole code from the pthread implementation glibc-linuxthreads-2.2.5.
 */

#ifdef NDEBUG
#define	DAS_PTHREAD_MUTEX_INLINE
#endif

/* It seems that pthread.h chose to forget some prototypes */
int pthread_mutexattr_setkind_np(pthread_mutexattr_t *attr, int kind);
int pthread_mutexattr_getkind_np(const pthread_mutexattr_t *attr, int *kind);


#ifdef DAS_PTHREAD_MUTEX_INLINE

#ifdef __GNUC__

#include <errno.h>

int __pthread_mutex_lock(pthread_mutex_t *mutex);
int __pthread_mutex_unlock(pthread_mutex_t *mutex);
int __pthread_mutex_init(pthread_mutex_t *mutex,
			 const pthread_mutexattr_t *mutex_attr);

static inline int
das_pthread_compare_and_swap(long int *p, long int oldval, long int newval)
{
    char ret;
    long int readval;

    __asm__ __volatile__ ("lock; cmpxchgl %3, %1; sete %0"
			  : "=q" (ret), "=m" (*p), "=a" (readval)
			  : "r" (newval), "m" (*p), "a" (oldval)
			  : "memory");
    return ret;
}


static inline int
das_pthread_trylock(struct _pthread_fastlock *lock)
{
    return lock->__status == 0 &&
	   das_pthread_compare_and_swap(&lock->__status, 0, 1);
}


static inline int
das_pthread_tryunlock(struct _pthread_fastlock *lock)
{
    long int	oldstatus;

    while ((oldstatus = lock->__status) == 1) {
	if (das_pthread_compare_and_swap(&lock->__status, oldstatus, 0)) {
	    return 1;
	}
    }

    return 0;
}


static inline int
das_pthread_mutex_init(pthread_mutex_t *mutex, const pthread_mutexattr_t *attr)
{
    pthread_mutexattr_t *a = (pthread_mutexattr_t *)attr;
    pthread_mutexattr_t t;
    int		r;

    if (attr != NULL) {
	if (a->__mutexkind != PTHREAD_MUTEX_ADAPTIVE_NP) {
	    return EINVAL;
	}
    } else {
	a = &t;
	pthread_mutexattr_init(a);
	pthread_mutexattr_setkind_np(a, PTHREAD_MUTEX_ADAPTIVE_NP);
    }

    r = __pthread_mutex_init(mutex, a);

    if (a == &t) {
	pthread_mutexattr_destroy(a);
    }

    return r;
}


#define pthread_mutex_lock(__m) \
    (das_pthread_trylock(&(__m)->__m_lock) ? 0 : __pthread_mutex_lock(__m))

#define pthread_mutex_unlock(__m) \
    (das_pthread_tryunlock(&(__m)->__m_lock) ? 0 : __pthread_mutex_unlock(__m))

#define pthread_mutex_init(__mutex, __attr) \
    das_pthread_mutex_init(__mutex, __attr)


#else
#  error Need GNU C for asm and inlining
#endif

#endif

void das_pthread_init(int *argc, char **argv);
void das_pthread_end(void);

#endif
