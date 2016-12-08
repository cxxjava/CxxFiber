/* A simple event-driven programming library. Originally I wrote this code
 * for the Jim's event-loop (Jim is a Tcl interpreter) but later translated
 * it in form of a library for easy reuse.
 *
 * Copyright (c) 2006-2012, Salvatore Sanfilippo <antirez at gmail dot com>
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 *   * Redistributions of source code must retain the above copyright notice,
 *     this list of conditions and the following disclaimer.
 *   * Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 *   * Neither the name of Redis nor the names of its contributors may be used
 *     to endorse or promote products derived from this software without
 *     specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef __ECO_POLL_H__
#define __ECO_POLL_H__

#include "es_types.h"
#include "es_status.h"
#include "es_config.h"

#ifdef __cplusplus
extern "C" {
#endif

//mask
#define ECO_POLL_NONE 0
#define ECO_POLL_READABLE 1
#define ECO_POLL_WRITABLE 2
#define ECO_POLL_ALL_MASKS (ECO_POLL_READABLE|ECO_POLL_WRITABLE)

//flags
#define ECO_POLL_FILE_EVENTS 1
#define ECO_POLL_TIME_EVENTS 2
#define ECO_POLL_ALL_EVENTS (ECO_POLL_FILE_EVENTS|ECO_POLL_TIME_EVENTS)

#define ECO_POLL_NOMORE -1

typedef struct co_poll_t co_poll_t;

/* Types and data structures */
typedef void coFileProc(co_poll_t *poll, int fd, void *clientData, int mask);
typedef int coTimeProc(co_poll_t *poll, es_int64_t id, void *clientData);
typedef void coEventFinalizerProc(co_poll_t *poll, void *clientData);

/* File event structure */
typedef struct coFileEvent {
    int mask; /* one of ECO_POLL_(READABLE|WRITABLE) */
    coFileProc *rfileProc;
    coFileProc *wfileProc;
    void *clientData;
} coFileEvent;

/* Time event structure */
typedef struct coTimeEvent {
	es_int64_t id; /* time event identifier. */
    long when_sec; /* seconds */
    long when_ms; /* milliseconds */
    coTimeProc *timeProc;
    coEventFinalizerProc *finalizerProc;
    void *clientData;
    struct coTimeEvent *next;
} coTimeEvent;

/* A fired event */
typedef struct coFiredEvent {
    int fd;
    int mask;
} coFiredEvent;

/* State of an event based program */
struct co_poll_t {
    int maxfd;   /* highest file descriptor currently registered */
    int setsize; /* max number of file descriptors tracked */
    es_int64_t timeEventNextId;
    time_t lastTime;     /* Used to detect system clock skew */
    coFileEvent *events; /* Registered events */
    coFiredEvent *fired; /* Fired events */
    coTimeEvent *timeEventHead;
    void *apidata; /* This is used for polling API specific data */
};

/* export api */
co_poll_t* eco_poll_create(int setsize);
void eco_poll_destroy(co_poll_t** ppoll);

/* Resize the maximum set size of the event loop.
 * If the requested set size is smaller than the current set size, but
 * there is already a file descriptor in use that is >= the requested
 * set size minus one, -1 is returned and the operation is not
 * performed at all.
 *
 * Otherwise 0 is returned and the operation is successful. */
int eco_poll_resize(co_poll_t *poll, int setsize);

/* Process every pending time event, then every pending file event
 * (that may be registered by time event callbacks just processed).
 * Without special flags the function sleeps until some file event
 * fires, or when the next time event occurs (if any).
 *
 * If flags is 0, the function does nothing and returns.
 * if flags has ECO_POLL_ALL_EVENTS set, all the kind of events are processed.
 * if flags has ECO_POLL_FILE_EVENTS set, file events are processed.
 * if flags has ECO_POLL_TIME_EVENTS set, time events are processed.
 *
 * timeout >0 wait some time | ==0 ASAP | <0 block indefinitely
 *
 * The function returns the number of events processed. */
int eco_poll_process_events(co_poll_t* poll, int flags, es_int64_t timeout);

es_status_t eco_poll_file_event_create(co_poll_t* poll, int fd, int mask,
		coFileProc *proc, void *clientData);
es_status_t eco_poll_file_event_update(co_poll_t *poll, int fd, int mask,
        coFileProc *proc, void *clientData);
void eco_poll_file_event_delete(co_poll_t* poll, int fd, int mask);
int eco_poll_get_file_events(co_poll_t* poll, int fd);

es_int64_t eco_poll_time_event_create(co_poll_t* poll, es_int64_t milliseconds,
		coTimeProc *proc, void *clientData,
		coEventFinalizerProc *finalizerProc);
es_status_t eco_poll_time_event_delete(co_poll_t* poll, es_int64_t id);

/* Wait for milliseconds until the given file descriptor becomes
 * writable/readable/exception */
int eco_poll_wait(int fd, int mask, es_int64_t milliseconds);

/* Return the current poll type. */
char *eco_poll_api_name(void);

/* Return the current set size. */
int eco_poll_get_size(co_poll_t *poll);

/* Return native poll fd. */
int eco_poll_getfd(co_poll_t* poll);

#ifdef __cplusplus
}
#endif

#endif //!__ECO_POLL_H__
