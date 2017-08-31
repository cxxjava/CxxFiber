/*
 * EIoWaiter.cpp
 *
 *  Created on: 2016-5-10
 *      Author: cxxjava@163.com
 */

#include "./EIoWaiter.hh"
#include "../inc/EFiberDebugger.hh"

#include <sys/resource.h>

namespace efc {
namespace eco {

extern "C" {
typedef ssize_t (*read_t)(int fd, void *buf, size_t count);
extern read_t read_f;

typedef ssize_t (*write_t)(int fd, const void *buf, size_t count);
extern write_t write_f;
} //!C

static void pipeEventProc(co_poll_t *poll, int fd, void *clientData, int mask) {
	char buf[32];
	int n;
	RESTARTABLE(read_f(fd, buf, sizeof(buf)), n);

	ECO_DEBUG(EFiberDebugger::WAITING, "io waiter signaled.");
}

//=============================================================================

EIoWaiter::~EIoWaiter() {
	eco_poll_destroy(&poll);
	eso_pipe_destroy(&pipe);
}

EIoWaiter::EIoWaiter(int iosetSize): waiters(0) {
	poll = eco_poll_create(iosetSize);
	pipe = eso_pipe_create();

	// register pipe for poll wakeup.
	eco_poll_file_event_update(poll, eso_fileno(pipe->in), ECO_POLL_READABLE, pipeEventProc, NULL);
}

void EIoWaiter::loopProcessEvents() {
	for (;;) {
		EFiber::yield(); //!

		int events = eco_poll_process_events(poll, ECO_POLL_ALL_EVENTS, 0);
		ECO_DEBUG(EFiberDebugger::WAITING, "get %d ready events", events);
	}
}

int EIoWaiter::onceProcessEvents(int timeout) {
	int events = eco_poll_process_events(poll, ECO_POLL_ALL_EVENTS, timeout);
	ECO_DEBUG(EFiberDebugger::WAITING, "get %d ready events, timeout=%d", events, timeout);
	return events;
}

void EIoWaiter::fileEventProc(co_poll_t *poll, int fd, void *clientData, int mask) {
	sp<EFiber> fiber = *(sp<EFiber>*)clientData;
	fiber->swapIn(); // resume!
}

void EIoWaiter::setFileEvent(int fd, int mask, sp<EFiber> fiber) {
	sp<EFiber>* f = new sp<EFiber>(fiber);
	es_status_t status = eco_poll_file_event_create(poll, fd, mask, fileEventProc, f);
	if (status != ES_SUCCESS) {
		delete f;
	}

	waiters++;

	fiber->state = EFiber::WAITING; // will be hang!
	ECO_DEBUG(EFiberDebugger::WAITING, "fiber[%s] will waiting.", fiber->toString().c_str());
}

void EIoWaiter::delFileEvent(int fd, int mask) {
	coFileEvent *fe = NULL;
	if (fd >= poll->setsize) return;
	fe = &poll->events[fd];
	if (fe) {
		sp<EFiber>* f = (sp<EFiber>*)fe->clientData;
		delete f;
	}
	eco_poll_file_event_delete(poll, fd, mask);

	waiters--;
}

int EIoWaiter::getFileEvent(int fd) {
	return eco_poll_get_file_events(poll, fd);
}

int EIoWaiter::timeEventProc(co_poll_t *poll, llong id, void *clientData) {
	sp<EFiber> fiber = *(sp<EFiber>*)clientData;
	fiber->isIoWaitTimeout = true;
	fiber->swapIn(); // resume!
	ECO_DEBUG(EFiberDebugger::WAITING, "fiber[%s] resume.", fiber->toString().c_str());
	return ECO_POLL_NOMORE;
}

void EIoWaiter::timeEventFinalizerProc(co_poll_t *poll, void *clientData) {
	sp<EFiber>* f = (sp<EFiber>*)clientData;
	delete f;
}

llong EIoWaiter::setupTimer(llong timeout, sp<EFiber> fiber) {
	sp<EFiber>* f = new sp<EFiber>(fiber);
	fiber->isIoWaitTimeout = false;

	waiters++;

	return eco_poll_time_event_create(poll, timeout, timeEventProc, f,  timeEventFinalizerProc);
}

void EIoWaiter::cancelTimer(llong id) {
	eco_poll_time_event_delete(poll, id);

	waiters--;
}

int EIoWaiter::getNativePollHandle() {
	return eco_poll_getfd(poll);
}

int EIoWaiter::getWaitersCount() {
	return waiters;
}

void EIoWaiter::interrupt() {
	eco_poll_time_fire_all(poll);
}

boolean EIoWaiter::swapOut(sp<EFiber>& fiber) {
	fiber->setState(EFiber::WAITING);
	EFiber::yield(); // paused!
	ECO_DEBUG(EFiberDebugger::WAITING, "fiber[%s] paused.", fiber->toString().c_str());
	return true;
}

void EIoWaiter::signal() {
	int fd = eso_fileno(pipe->out);
	int n;
	RESTARTABLE(write_f(fd, "\0xF1", 1), n);
}

} /* namespace eco */
} /* namespace efc */
