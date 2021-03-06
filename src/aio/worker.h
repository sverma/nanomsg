/*
    Copyright (c) 2013 250bpm s.r.o.

    Permission is hereby granted, free of charge, to any person obtaining a copy
    of this software and associated documentation files (the "Software"),
    to deal in the Software without restriction, including without limitation
    the rights to use, copy, modify, merge, publish, distribute, sublicense,
    and/or sell copies of the Software, and to permit persons to whom
    the Software is furnished to do so, subject to the following conditions:

    The above copyright notice and this permission notice shall be included
    in all copies or substantial portions of the Software.

    THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
    IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
    FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
    THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
    LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
    FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS
    IN THE SOFTWARE.
*/

#ifndef NN_WORKER_INCLUDED
#define NN_WORKER_INCLUDED

#if !defined NN_HAVE_WINDOWS

#include "../utils/queue.h"
#include "../utils/mutex.h"
#include "../utils/thread.h"
#include "../utils/efd.h"

#include "poller.h"
#include "timerset.h"

/*  Base class for users of worker object. It defined event sink for events
    generated by the worker. */

struct nn_worker_callback;
struct nn_worker_poller;

struct nn_worker_callback_vfptr {
    void (*callback) (struct nn_worker_callback *self,
       void *source, int type, struct nn_worker_poller *poller);
};

struct nn_worker_callback {
    const struct nn_worker_callback_vfptr *vfptr;
};

void nn_worker_callback_init (struct nn_worker_callback *self,
    const struct nn_worker_callback_vfptr *vfptr);
void nn_worker_callback_term (struct nn_worker_callback *self);

/*  Poller provided by the worker object to the user. */

#define NN_WORKER_FD_IN NN_POLLER_IN
#define NN_WORKER_FD_OUT NN_POLLER_OUT
#define NN_WORKER_FD_ERR NN_POLLER_ERR

struct nn_worker_fd {
    struct nn_worker_callback *callback;
    struct nn_poller_hndl hndl;
};

void nn_worker_fd_init (struct nn_worker_fd *self,
    struct nn_worker_callback *callback);
void nn_worker_fd_term (struct nn_worker_fd *self);

#define NN_WORKER_TIMER_TIMEOUT 1

struct nn_worker_timer {
    struct nn_worker_callback *callback;
    struct nn_timerset_hndl hndl;
};

void nn_worker_timer_init (struct nn_worker_timer *self,
    struct nn_worker_callback *callback);
void nn_worker_timer_term (struct nn_worker_timer *self);

struct nn_worker_poller;

void nn_worker_poller_add_fd (struct nn_worker_poller *self,
    int s, struct nn_worker_fd *fd);
void nn_worker_poller_rm_fd(struct nn_worker_poller *self,
    struct nn_worker_fd *fd);
void nn_worker_poller_set_in (struct nn_worker_poller *self,
    struct nn_worker_fd *fd);
void nn_worker_poller_reset_in (struct nn_worker_poller *self,
    struct nn_worker_fd *fd);
void nn_worker_poller_set_out (struct nn_worker_poller *self,
    struct nn_worker_fd *fd);
void nn_worker_poller_reset_out (struct nn_worker_poller *self,
    struct nn_worker_fd *fd);
void nn_worker_poller_add_timer (struct nn_worker_poller *self,
    int timeout, struct nn_worker_timer *timer);
void nn_worker_poller_rm_timer (struct nn_worker_poller *self,
    struct nn_worker_timer *timer);

/*  The worker object itself. */

#define NN_WORKER_TASK_EXECUTE 1

struct nn_worker_task {
    struct nn_worker_callback *callback;
    struct nn_queue_item item;
};

void nn_worker_task_init (struct nn_worker_task *self,
    struct nn_worker_callback *callback);
void nn_worker_task_term (struct nn_worker_task *self);

struct nn_worker {
    struct nn_mutex sync;
    struct nn_queue tasks;
    struct nn_queue_item stop;
    struct nn_efd efd;
    struct nn_poller poller;
    struct nn_poller_hndl efd_hndl;
    struct nn_timerset timerset;
    struct nn_thread thread;
};

int nn_worker_init (struct nn_worker *self);
void nn_worker_term (struct nn_worker *self);
void nn_worker_execute (struct nn_worker *self, struct nn_worker_task *task);

#endif

#endif

