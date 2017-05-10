/*
 * EIoWaiter.hh
 *
 *  Created on: 2016-5-10
 *      Author: cxxjava@163.com
 */

#ifndef EIOWAITER_HH_
#define EIOWAITER_HH_

#include "Efc.hh"
#include "EFiber.hh"
#include "eco_ae.h"

namespace efc {
namespace eco {

/**
 *
 */

class EIoWaiter {
public:
	virtual ~EIoWaiter();

	EIoWaiter(int iosetSize);

	/**
	 *
	 */
	void loopProcessEvents();
	int onceProcessEvents(int timeout=0);

	/**
	 *
	 */
	void setFileEvent(int fd, int mask, sp<EFiber> fiber);
	void delFileEvent(int fd, int mask);
	int getFileEvent(int fd);

	/**
	 *
	 */
	llong setupTimer(llong timeout, sp<EFiber> fiber);
	void cancelTimer(llong id);

	/**
	 *
	 */
	int getNativePollHandle();
	int getWaitersCount();

	/**
	 *
	 */
	void interrupt();

	/**
	 *
	 */
	boolean swapOut(sp<EFiber>& fiber);

	/**
	 *
	 */
	void signal();

private:
	co_poll_t* poll;
	es_pipe_t* pipe;
	int waiters;

	static void fileEventProc(co_poll_t *poll, int fd, void *clientData, int mask);
	static int  timeEventProc(co_poll_t *poll, llong id, void *clientData);
	static void timeEventFinalizerProc(co_poll_t *poll, void *clientData);
};

} /* namespace eco */
} /* namespace efc */
#endif /* EIOWAITER_HH_ */
