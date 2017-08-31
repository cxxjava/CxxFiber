/*
 * EFiber.cpp
 *
 *  Created on: 2016-5-17
 *      Author: cxxjava@163.com
 */

#include "./EContext.hh"
#include "./EIoWaiter.hh"
#include "../inc/EFiber.hh"
#include "../inc/EFiberLocal.hh"
#include "../inc/EFiberScheduler.hh"
#include "../inc/EFiberBlocker.hh"
#include "../inc/EFiberDebugger.hh"

namespace efc {
namespace eco {

static EAtomicCounter idCounter(0);

static const char StateName[][15] = {
	"NEW",
	"RUNNABLE",
	"BLOCKED",
	"WAITING",
	"TERMINATED"
};

EFiber::~EFiber() {
	delete packing;
	delete context;
	/*
	 * call back for user has a chance to free all fiber local data.
	 */
	es_hash_index_t *hi;
	void *key;
	void *val;
	for (hi = eso_hash_first(localValues); hi; hi = eso_hash_next(hi)) {
		eso_hash_this(hi, &key, NULL, &val);
		EFiberLocalKeyWrap* kw = (EFiberLocalKeyWrap*)key;
		if (kw) {
			kw->callback(val);
		}
	}
	eso_hash_free(&localValues);
	ECO_DEBUG(EFiberDebugger::FIBER, "delete fiber#%d", fid);
}

EFiber::EFiber(int size):
		state(NEW),
		fid(idCounter++),
		tag(ES_LONG_MIN_VALUE),
		stackSize(ES_MAX(size, MIN_STACK_SIZE)),
		context(null),
		scheduler(null),
		iowaiter(null),
		boundQueue(null),
		boundThreadID(-1),
		blocker(null),
		isIoWaitTimeout(false),
		canceled(false),
		packing(null),
		threadIndex(0) {
	EFiber* cf = currentFiber();
	if (cf) parent = cf->shared_from_this();
	context = new EContext(this);
	localValues = eso_hash_make(1, NULL);
	packing = new EFiberConcurrentQueue<EFiber>::NODE();
	ECO_DEBUG(EFiberDebugger::FIBER, "new fiber#%d: %d", fid, stackSize);
}

void EFiber::setName(const char* name) {
	this->name = name;
}

const char* EFiber::getName() {
	return name.isEmpty() ? "null" : name.c_str();
}

void EFiber::setTag(long tag) {
	this->tag = tag;
}

const long EFiber::getTag() {
	return tag;
}

void EFiber::cancel() {
	canceled = true;
}

boolean EFiber::isCanceled() {
	return canceled;
}

boolean EFiber::isAlive() {
	return (state == RUNNABLE);
}

int EFiber::getStackSize() {
	return stackSize;
}

int EFiber::getId() {
	return fid;
}

int EFiber::getThreadIndex() {
	return threadIndex;
}

EFiber::State EFiber::getState() {
	return state;
}

EFiberScheduler* EFiber::getScheduler() {
	return scheduler;
}

EIoWaiter* EFiber::getIoWaiter() {
	return iowaiter;
}

boolean EFiber::isWaitTimeout() {
	boolean b = isIoWaitTimeout;
	isIoWaitTimeout = false;
	return b;
}

void EFiber::yield() {
	EFiber* f = EFiber::currentFiber();
	if (f != null) {
		f->context->swapOut();
	}
}

EStringBase EFiber::toString() {
	return EStringBase::formatOf("Fiber[%s,%d,%s,%d]", getName(), getId(), StateName[state], stackSize);
}

void EFiber::sleep(llong millis) {
	if (millis < 0) {
		throw EIllegalArgumentException(__FILE__, __LINE__, "timeout value is negative");
	}

	EIoWaiter* ioWaiter = EFiberScheduler::currentIoWaiter();
	if (ioWaiter) {
		sp<EFiber> self = EFiber::currentFiber()->shared_from_this();
		llong id = ioWaiter->setupTimer(millis, self);
		ioWaiter->swapOut(self);
		ioWaiter->cancelTimer(id);
	}
}

void EFiber::sleep(llong millis, int nanos) {
	if (millis < 0) {
		throw EIllegalArgumentException(__FILE__, __LINE__, "timeout value is negative");
	}

	if (nanos < 0 || nanos > 999999) {
		throw EIllegalArgumentException(__FILE__, __LINE__,
				"nanosecond timeout value out of range");
	}

	if (nanos >= 500000 || (nanos != 0 && millis == 0)) {
		millis++;
	}

	sleep(millis);
}

EFiber* EFiber::currentFiber() {
	return EFiberScheduler::activeFiber();
}

void EFiber::setScheduler(EFiberScheduler* scheduler) {
	this->scheduler = scheduler;
}

void EFiber::setIoWaiter(EIoWaiter* iowaiter) {
	this->iowaiter = iowaiter;
}

void EFiber::setState(State state) {
	this->state = state;
}

void EFiber::setThreadIndex(int index) {
	this->threadIndex = index;
}

void EFiber::swapIn() {
	int _state_ = state;
	if (state == EFiber::WAITING || state == EFiber::BLOCKED) {
		blocker = null;
		state = EFiber::RUNNABLE;
		boundQueue->add(this->packing->value);

		if (EThread::currentThread()->getId() != boundThreadID
				&& _state_ == EFiber::BLOCKED) {
			iowaiter->signal();
		}
	}
}

void EFiber::swapOut() {
	context->swapOut();
}

} /* namespace eco */
} /* namespace efc */
