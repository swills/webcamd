/*-
 * Copyright (c) 2009 Hans Petter Selasky. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

static pthread_cond_t sema_cond;

void
init_waitqueue_head(wait_queue_head_t *q)
{
	memset(q, 0, sizeof(*q));
}

void
uninit_waitqueue_head(wait_queue_head_t *q)
{
}

void
wake_up(wait_queue_head_t *q)
{
	atomic_lock();
	pthread_cond_broadcast(&sema_cond);
	atomic_unlock();
}

void
wake_up_all(wait_queue_head_t *q)
{
	atomic_lock();
	pthread_cond_broadcast(&sema_cond);
	atomic_unlock();
}

void
wake_up_nr(wait_queue_head_t *q, uint32_t nr)
{
	atomic_lock();
	pthread_cond_broadcast(&sema_cond);
	atomic_unlock();
}

void
__wait_event(wait_queue_head_t *q)
{
	pthread_cond_wait(&sema_cond, atomic_get_lock());
}

void
__wait_get_timeout(uint64_t timeout, struct timespec *ts)
{
	timeout++;

	timeout = (1000000000ULL / HZ) * (uint64_t)timeout;

	ts[0].tv_nsec = timeout % 1000000000ULL;
	ts[0].tv_sec = timeout / 1000000000ULL;

	clock_gettime(CLOCK_REALTIME, ts + 1);

	ts[0].tv_sec += ts[1].tv_sec;
	ts[0].tv_nsec += ts[1].tv_nsec;

	if (ts[0].tv_nsec >= 1000000000) {
		ts[0].tv_sec++;
		ts[0].tv_nsec -= 1000000000;
	}
}

int
__wait_event_timed(wait_queue_head_t *q, struct timespec *ts)
{
	int err;

	err = pthread_cond_timedwait(&sema_cond, atomic_get_lock(), ts);

	return (err == ETIMEDOUT);
}

void
sema_init(struct semaphore *sem, int32_t value)
{
	memset(sem, 0, sizeof(*sem));
	sem->value = value;
}

void
sema_uninit(struct semaphore *sem)
{
}

void
up(struct semaphore *sem)
{
	atomic_lock();
	sem->value++;
	if (sem->value == 1)
		pthread_cond_broadcast(&sema_cond);
	atomic_unlock();
}

void
down(struct semaphore *sem)
{
	atomic_lock();
	while (sem->value <= 0)
		pthread_cond_wait(&sema_cond, atomic_get_lock());
	sem->value--;
	atomic_unlock();
}

void
poll_wait(struct file *filp, wait_queue_head_t *wq, poll_table * p)
{
	if (p && wq) {
		/* XXX register file in polling table */
	}
}

void
init_completion(struct completion *x)
{
	x->done = 0;
	init_waitqueue_head(&x->wait);
}

void
uninit_completion(struct completion *x)
{
	uninit_waitqueue_head(&x->wait);
}

void
wait_for_completion(struct completion *x)
{
	atomic_lock();
	while (x->done == 0) {
		__wait_event(&x->wait);
	}
	x->done--;
	atomic_unlock();
}

uint64_t
wait_for_completion_timeout(struct completion *x,
    uint64_t timeout)
{
	struct timespec ts[2];
	uint64_t ret = timeout;

	__wait_get_timeout(ret, ts);

	atomic_lock();
	while (x->done == 0) {
		if (__wait_event_timed(&x->wait, ts)) {
			ret = 0;
			break;
		}
	}
	x->done--;
	atomic_unlock();

	return (ret);
}

void
complete(struct completion *x)
{
	atomic_lock();
	x->done++;
	pthread_cond_broadcast(&sema_cond);
	atomic_unlock();
}

void
schedule(void)
{
	usleep(1);
}

struct funcdata {
	threadfn_t *func;
	void   *data;
};

static void *
kthread_wrapper(void *arg)
{
	struct funcdata fd = *(struct funcdata *)arg;

	free(arg);

	pthread_setcancelstate(PTHREAD_CANCEL_ENABLE, NULL);
	pthread_setcanceltype(PTHREAD_CANCEL_ASYNCHRONOUS, NULL);

	fd.func(fd.data);

	pthread_exit(NULL);
}

struct task_struct *
kthread_run(threadfn_t *func, void *data, char *fmt,...)
{
	pthread_t ptd;
	struct funcdata *fd = malloc(sizeof(*fd));

	if (fd == NULL)
		return (ERR_PTR(-ENOMEM));

	fd->func = func;
	fd->data = data;

	if (pthread_create(&ptd, NULL, kthread_wrapper, fd)) {
		free(fd);
		return (ERR_PTR(-ENOMEM));
	}
	return ((struct task_struct *)ptd);
}

int
kthread_stop(struct task_struct *k)
{
	pthread_cancel((pthread_t)k);
	pthread_join((pthread_t)k, NULL);
	return (0);
}

int
kthread_should_stop(void)
{
	pthread_testcancel();		/* XXX workaround */
	return (0);
}

int
try_to_freeze(void)
{
	return (0);
}

int
freezing(struct task_struct *p)
{
	return (0);
}

void
set_freezable(void)
{

}

int
thread_init(void)
{
	pthread_cond_init(&sema_cond, NULL);
	return (0);
}

void
thread_exit(void)
{
	pthread_cond_destroy(&sema_cond);
}

module_init(thread_init)
module_exit(thread_exit)