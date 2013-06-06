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

#include "ctx.h"
#include "usock.h"

#include "../utils/err.h"
#include "../utils/cont.h"
#include "../utils/fast.h"

#define NN_WORKER_MAX_EVENTS 32

/*  The value of this variable is irrelevant. It's used only as a placeholder
    for the address that is used as the 'stop' event ID. */
const int nn_worker_stop = 0;

/*  Private functions. */
static void nn_worker_routine (void *arg);

void nn_worker_task_init (struct nn_worker_task *self,struct nn_fsm *owner)
{
    self->owner = owner;
}

void nn_worker_task_term (struct nn_worker_task *self)
{
}

int nn_worker_init (struct nn_worker *self)
{
    self->cp = CreateIoCompletionPort (INVALID_HANDLE_VALUE, NULL, 0, 0);
    win_assert (self->cp);
    nn_timerset_init (&self->timerset);
    nn_thread_init (&self->thread, nn_worker_routine, self);

    return 0;
}

void nn_worker_term (struct nn_worker *self)
{
    BOOL brc;

    /*  Ask worker thread to terminate. */
    brc = PostQueuedCompletionStatus (self->cp, 0,
        (ULONG_PTR) &nn_worker_stop, NULL);
    win_assert (brc);

    /*  Wait till worker thread terminates. */
    nn_thread_term (&self->thread);

    nn_timerset_term (&self->timerset);
    brc = CloseHandle (self->cp);
    win_assert (brc);
}

void nn_worker_execute (struct nn_worker *self, struct nn_worker_task *task)
{
    BOOL brc;

    brc = PostQueuedCompletionStatus (self->cp, 0, (ULONG_PTR) task, NULL);
    win_assert (brc);
}

void nn_worker_add_timer (struct nn_worker *self, int timeout,
    struct nn_worker_timer *timer)
{
    nn_timerset_add (&((struct nn_worker*) self)->timerset, timeout,
        &timer->hndl);
}

void nn_worker_rm_timer (struct nn_worker *self, struct nn_worker_timer *timer)
{
    nn_timerset_rm (&((struct nn_worker*) self)->timerset, &timer->hndl);
}

HANDLE nn_worker_getcp (struct nn_worker *self)
{
    return self->cp;
}

static void nn_worker_routine (void *arg)
{
    int rc;
    BOOL brc;
    struct nn_worker *self;
    int timeout;
    ULONG count;
    ULONG i;
    struct nn_timerset_hndl *thndl;
    struct nn_worker_timer *timer;
    struct nn_worker_task *task;
    struct nn_usock_op *op;
    OVERLAPPED_ENTRY entries [NN_WORKER_MAX_EVENTS];

    self = (struct nn_worker*) arg;

    while (1) {

        /*  Process all expired timers. */
        while (1) {
            rc = nn_timerset_event (&self->timerset, &thndl);
            if (nn_fast (rc == -EAGAIN))
                break;
            errnum_assert (rc == 0, -rc);
            timer = nn_cont (thndl, struct nn_worker_timer, hndl);
            nn_ctx_enter (timer->owner->ctx);
            timer->owner->fn (timer->owner, timer, NN_WORKER_TIMER_TIMEOUT);
            nn_ctx_leave (timer->owner->ctx);
        }

        /*  Compute the time interval till next timer expiration. */
        timeout = nn_timerset_timeout (&self->timerset);

        /*  Wait for new events and/or timeouts. */
        brc = GetQueuedCompletionStatusEx (self->cp, entries,
            NN_WORKER_MAX_EVENTS, &count, timeout < 0 ? INFINITE : timeout,
            FALSE);
        if (nn_slow (!brc && GetLastError () == WAIT_TIMEOUT))
            continue;
        win_assert (brc);

        for (i = 0; i != count; ++i) {

            /*  Process I/O completion events. */
            if (nn_fast (entries [i].lpOverlapped)) {
printf ("completion\n");
                op = nn_cont (entries [i].lpOverlapped,
                    struct nn_usock_op, olpd);
                nn_ctx_enter (op->usock->fsm.ctx);
                op->usock->fsm.fn (&op->usock->fsm, op, NN_WORKER_IO_EVENT);
                nn_ctx_leave (op->usock->fsm.ctx);
                continue;
            }

            /*  Worker thread shutdown is requested. */
            if (nn_slow (entries [i].lpCompletionKey ==
                  (ULONG_PTR) &nn_worker_stop))
                return;

            /*  Process tasks. */
printf ("task\n");
            task = (struct nn_worker_task*) entries [i].lpCompletionKey;
            nn_ctx_enter (task->owner->ctx);
            task->owner->fn (task->owner, task, NN_WORKER_TASK_EXECUTE);
            nn_ctx_leave (task->owner->ctx);
        }
    }
}