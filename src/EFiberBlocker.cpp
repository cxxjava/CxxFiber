/*
 * EFiberBlocker.cpp
 *
 *  Created on: 2016-5-9
 *      Author: cxxjava@163.com
 */

#include "EFiberBlocker.hh"
#include "EIoWaiter.hh"
#include "EFiberScheduler.hh"
#include "EFiberDebugger.hh"

namespace efc {
namespace eco {

EFiberBlocker::~EFiberBlocker() {
#ifdef DEBUG
	if (!sync_->getLock()->tryLock()) {
		ECO_DEBUG(EFiberDebugger::BLOCKER, "fiber blocker#%p is locked.", this);
	}

	if (!waitQueue.size() > 0) {
		ECO_DEBUG(EFiberDebugger::BLOCKER, "fiber blocker#%p is waiting.", this);
	}
#endif
}

EFiberBlocker::EFiberBlocker(uint capacity, uint limit):
		wakeup_(capacity),
		limit_(limit),
		sync_(new SyncObj) {
	ECO_DEBUG(EFiberDebugger::BLOCKER, "fiber blocker#%p created.", this);
}

void EFiberBlocker::wait() {
	EFiber* fiber = EFiber::currentFiber();
	if (fiber) {
		 // waiter is a fiber.
		SYNCHRONIZED(sync_.get()) {
			if (wakeup_ > 0) {
				ECO_DEBUG(EFiberDebugger::BLOCKER, "wait immediately done.");
				wakeup_--;
				return ;
			}
		}}

		fiber->blocker = this;
		fiber->state = EFiber::BLOCKED; // will be hang!
		EFiber::yield();
	} else {
		// waiter is a thread.
		SYNCHRONIZED(sync_.get()) {
			if (wakeup_ > 0) {
				ECO_DEBUG(EFiberDebugger::BLOCKER, "wait immediately done.");
				wakeup_--;
				return ;
			}

			waitQueue.add(sync_);
			ECO_DEBUG(EFiberDebugger::BLOCKER, "thread[%s] will waiting.", EThread::currentThread()->toString().c_str());
			sync_->wait();
		}}
	}
}

boolean EFiberBlocker::tryWait() {
	SYNCHRONIZED(sync_.get()) {
		if (wakeup_ == 0)
			return false;

		wakeup_--;
		ECO_DEBUG(EFiberDebugger::BLOCKER, "try wait success.");
		return true;
	}}
}

boolean EFiberBlocker::tryWait(llong time, ETimeUnit* unit) {
	EFiber* fiber = EFiber::currentFiber();
	if (fiber) {
		 // waiter is a fiber.
		SYNCHRONIZED(sync_.get()) {
			if (wakeup_ > 0) {
				ECO_DEBUG(EFiberDebugger::BLOCKER, "wait immediately done.");
				wakeup_--;
				return true;
			}
		}}

		// add timeout check
		EIoWaiter* ioWaiter = EFiberScheduler::currentIoWaiter();

		llong timeout = unit->toMillis(time);
		llong timerID = -1;
		if (timeout > 0) {
			timerID = ioWaiter->setupTimer(timeout, fiber->shared_from_this());
		}

		fiber->blocker = this;
		fiber->state = EFiber::BLOCKED; // will be hang!
		EFiber::yield();

		if (timerID != -1) {
			ioWaiter->cancelTimer(timerID);
		}

		if (fiber->isWaitTimeout()) {
			return !waitQueue.remove(fiber);
		}
		return true;
	} else {
		// waiter is a thread.
		SYNCHRONIZED(sync_.get()) {
			if (wakeup_ > 0) {
				ECO_DEBUG(EFiberDebugger::BLOCKER, "wait immediately done.");
				wakeup_--;
				return true;
			}

			waitQueue.add(sync_);
			ECO_DEBUG(EFiberDebugger::BLOCKER, "thread[%s] will waiting.", EThread::currentThread()->toString().c_str());
			boolean r = sync_->getCondition()->await(time, unit);
			if (!r) {
				return !waitQueue.remove(sync_.get());
			}
			return true;
		}}
	}
}

boolean EFiberBlocker::wakeUp() {
	sp<EQueueEntry> waiter;

	SYNCHRONIZED(sync_.get()) {
		waiter = waitQueue.poll();
		if (!waiter) {
			if (wakeup_ >= limit_) {
				ECO_DEBUG(EFiberDebugger::BLOCKER, "wake up failed.");
				return false;
			}
			wakeup_++;
			ECO_DEBUG(EFiberDebugger::BLOCKER, "wake up #%u.", wakeup_);
			return true;
		}

		sp<ESynchronizeable> sync = dynamic_pointer_cast<ESynchronizeable>(waiter);
		if (sync != null) { // waiter is a thread.
			sync->notify();
			ECO_DEBUG(EFiberDebugger::BLOCKER, "wake up a thread waiter.");
			return true;
		}
	}}

	// if waiter is not a thread that's the fiber.
	sp<EFiber> fiber = dynamic_pointer_cast<EFiber>(waiter);
	ECO_DEBUG(EFiberDebugger::BLOCKER, "wake up a fiber[%s].", fiber->toString().c_str());
	fiber->swapIn(); // resume!
	return true;
}

boolean EFiberBlocker::isWaking() {
	SYNCHRONIZED(sync_.get()) {
		return wakeup_ > 0;
	}}
}

boolean EFiberBlocker::swapOut(EFiber* fiber) {
	SYNCHRONIZED(sync_.get()) {
		if (wakeup_ > 0) {
			wakeup_--;
			return false;
		}
		ECO_DEBUG(EFiberDebugger::BLOCKER, "fiber[%s] will waiting.", fiber->toString().c_str());
		waitQueue.add(fiber->shared_from_this());
		return true; // hung!
	}}
}

} /* namespace eco */
} /* namespace efc */
