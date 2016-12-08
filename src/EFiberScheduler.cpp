/*
 * EFiberScheduler.cpp
 *
 *  Created on: 2016-5-17
 *      Author: cxxjava@163.com
 */

#include "EFiberScheduler.hh"
#include "EContext.hh"
#include "EFiber.hh"
#include "EIoWaiter.hh"
#include "EFileContext.hh"
#include "EFiberBlocker.hh"
#include "EFiberDebugger.hh"

namespace efc {
namespace eco {

//=============================================================================

EThreadLocalStorage EFiberScheduler::currScheduler;
EThreadLocalStorage EFiberScheduler::currIoWaiter;

struct SchedulerLocal {
	EFiberScheduler* scheduler;
	EFiber* currFiber;

	SchedulerLocal(EFiberScheduler* fs): scheduler(fs), currFiber(null) {}
};

class IoWaiterFiber: public EFiber {
public:
	IoWaiterFiber(EIoWaiter* iw): ioWaiter(iw) {
	}
	virtual void run() {
		ioWaiter->loopProcessEvents();
	}
private:
	EIoWaiter* ioWaiter;
};

class TimberFiber: public EFiber {
private:
	sp<EFiberTimer> timer;
	llong delay;
	llong period;
public:
	TimberFiber(sp<EFiberTimer>& t, llong d): timer(t), delay(d), period(0) {
	}
	TimberFiber(sp<EFiberTimer>& t, llong d, llong p): timer(t), delay(d), period(p) {
	}
	void init() {
		timer->setFiber(this);
	}
	virtual void run() {
		EFiber::sleep(delay);
		if (period > 0) {
			while (!canceled) {
				EFiber::sleep(period);
				timer->run();
			}
		} else {
			if (!canceled) timer->run();
		}
	}
};

class Timer: public EFiberTimer {
public:
	Timer(std::function<void()> f): func(f) {
	}
	virtual void run() {
		func();
	}
private:
	std::function<void()> func;
};

//=============================================================================

EFiberScheduler::~EFiberScheduler() {
	//
}

EFiberScheduler::EFiberScheduler(int maxfd): maxEventSetSize(maxfd) {
	//
}

void EFiberScheduler::schedule(sp<EFiber> fiber) {
	totalFiberCounter++;

	fiber->state = EFiber::RUNNABLE;
	fiber->setScheduler(this);

	taskQueue.add(new sp<EFiber>(fiber));
}

#ifdef CPP11_SUPPORT
void EFiberScheduler::schedule(std::function<void()> f, int stackSize) {
	class Fiber: public EFiber {
	public:
		Fiber(std::function<void()> f, int stackSize):
			EFiber(stackSize), func(f) {
		}
		virtual void run() {
			func();
		}
	private:
		std::function<void()> func;
	};

	this->schedule(new Fiber(f, stackSize));
}
#endif

sp<EFiberTimer> EFiberScheduler::addtimer(sp<EFiberTimer> timer, llong delay) {
	if (delay > 0) {
		sp<TimberFiber> tf = new TimberFiber(timer, delay);
		tf->init();
		this->schedule(tf);
	}
	return timer;
}

sp<EFiberTimer> EFiberScheduler::addtimer(sp<EFiberTimer> timer, EDate* time) {
	llong delay = time->getTime() - ESystem::currentTimeMillis();
	if (delay > 0) {
		sp<TimberFiber> tf = new TimberFiber(timer, delay);
		tf->init();
		this->schedule(tf);
	}
	return timer;
}

sp<EFiberTimer> EFiberScheduler::addtimer(sp<EFiberTimer> timer, llong delay, llong period) {
	if (delay > 0) {
		sp<TimberFiber> tf = new TimberFiber(timer, delay, period);
		tf->init();
		this->schedule(tf);
	}
	return timer;
}

sp<EFiberTimer> EFiberScheduler::addtimer(sp<EFiberTimer> timer, EDate* firstTime, llong period) {
	llong delay = firstTime->getTime() - ESystem::currentTimeMillis();
	if (delay > 0) {
		sp<TimberFiber> tf = new TimberFiber(timer, delay, period);
		tf->init();
		this->schedule(tf);
	}
	return timer;
}

#ifdef CPP11_SUPPORT
void EFiberScheduler::addtimer(std::function<void()> f, llong delay) {
	this->addtimer(new Timer(f), delay);
}

void EFiberScheduler::addtimer(std::function<void()> f, EDate* time) {
	this->addtimer(new Timer(f), time);
}

void EFiberScheduler::addtimer(std::function<void()> f, llong delay, llong period) {
	this->addtimer(new Timer(f), delay, period);
}

void EFiberScheduler::addtimer(std::function<void()> f, EDate* firstTime, llong period) {
	this->addtimer(new Timer(f), firstTime, period);
}
#endif

void EFiberScheduler::join() {
	long currentThreadID = EThread::currentThread()->getId();
	EIoWaiter ioWaiter(maxEventSetSize);
	SchedulerLocal schedulerLocal(this);

	currScheduler.set(&schedulerLocal);
	currIoWaiter.set(&ioWaiter);

	for (;;) {
		int total = totalFiberCounter.value();

		sp<EFiber>* fiber_ = taskQueue.poll();
		if (!fiber_) {
			if (total > 0) {
				/**
				 * inactive fibers is BLOCKED or WAITING!
				 */
				int events = ioWaiter.onceProcessEvents(3000);

				continue;
			} else {
				break;
			}
		}
		EFiber* fiber = (*fiber_).get();

		if (ioWaiter.getWaitersCount() > 0) {
			// io waiter process.
			int events = ioWaiter.onceProcessEvents();
			ECO_DEBUG(EFiberDebugger::SCHEDULER, "return the number of events: %d", events);
		}

		if (!fiber->boundQueue) {
			fiber->boundQueue = &taskQueue;
		}
		fiber->boundThreadID = currentThreadID;

		// bind io waiter
		fiber->setIoWaiter(&ioWaiter);

#ifdef DEBUG
		llong t1 = ESystem::nanoTime();
#endif

		schedulerLocal.currFiber = fiber;
		fiber->context->swapIn();
		schedulerLocal.currFiber = null;

#ifdef DEBUG
		llong t2 = ESystem::nanoTime();
		ECO_DEBUG(EFiberDebugger::SCHEDULER, "fiter run time: %lldns, %s", t2 - t1, EThread::currentThread()->toString().c_str());
#endif

		switch (fiber->state) {
		case EFiber::RUNNABLE:
		{
			taskQueue.add(fiber_);
		}
			break;
		case EFiber::BLOCKED:
		{
			ES_ASSERT(fiber->blocker);
			if (!fiber->blocker->swapOut(fiber))
				taskQueue.add(fiber_);
		}
			break;
		case EFiber::TERMINATED:
			delete fiber_;
			totalFiberCounter--;
			break;
		default:
			//
			break;
		}
	}

	currIoWaiter.set(null);
	currScheduler.set(null);

	// do some clean.
	clearFileContexts();
	EContext::cleanOrignContext();
}

void EFiberScheduler::join(int threadNums) {
	if (threadNums < 1) {
		throw EIllegalArgumentException(__FILE__, __LINE__, "threadNums < 1");
	}
	if (threadNums == 1) {
		join(); //!
		return;
	}

	hasError.set(false);

	// multi-threading.
	class Worker: public EThread {
	public:
		EFiberScheduler* scheduler;
		EFiberConcurrentQueue<EFiber > localQueue;
		EA<EIoWaiter*>& ioWaiters;
		EIoWaiter* ioWaiter;
	public:
		Worker(EA<EIoWaiter*>& iws): ioWaiters(iws) { }
		virtual void run() {
			try {
				scheduler->joinWithThreadBind(localQueue, ioWaiter, ioWaiters);
			} catch (...) {
				scheduler->hasError.set(true);
			}
		}
	};

	EA<EIoWaiter*> ioWaiters(threadNums);
	for (int i=0; i<threadNums; i++) {
		ioWaiters[i] = new EIoWaiter(maxEventSetSize);
	}

	/**
	 * Don't forget current thread self to work together.
	 */

	// create other threads.
	ea<Worker> pool(threadNums - 1);
	for (int i=0; i<threadNums-1; i++) {
		Worker* worker = new Worker(ioWaiters);
		worker->scheduler = this;
		worker->ioWaiter = ioWaiters[i+1]; // 0 is for current thread.
		pool[i] = worker;
	}

	// dispatch fibers to each thread.
	sp<EFiber>* fiber_;
	int index = 0;
	while ((fiber_ = taskQueue.poll()) != null) {
		int i = (index++) % threadNums;
		if (i == 0) {
			localQueue.add(fiber_);
		} else {
			pool[i-1]->localQueue.add(fiber_);
		}
	}

	// worker run.
	for (int i=0; i<pool.length(); i++) {
		pool[i]->start();
	}

	// current thread work.
	joinWithThreadBind(localQueue, ioWaiters[0], ioWaiters);

	// wait other threads work finished.
	for (int i=0; i<pool.length(); i++) {
		pool[i]->join();
	}

	// do some clean.
	clearFileContexts();

	if (hasError.get()) {
		throw ERuntimeException(__FILE__, __LINE__, "join fail");
	}
}

void EFiberScheduler::joinWithThreadBind(EFiberConcurrentQueue<EFiber >& localQueue, EIoWaiter* ioWaiter, EA<EIoWaiter*>& ioWaiters) {
	long currentThreadID = EThread::currentThread()->getId();
	SchedulerLocal schedulerLocal(this);

	currScheduler.set(&schedulerLocal);
	currIoWaiter.set(ioWaiter);

	for (;;) {
		int total = totalFiberCounter.value();

		// try get from threads shared queue.
		sp<EFiber>* fiber_ = taskQueue.poll();
		if (!fiber_) {
			// try get from thread local queue.
			fiber_ = localQueue.poll();
			if (!fiber_) {
				if (total > 0) {
					/**
					 * inactive fibers is BLOCKED or WAITING!
					 */
					int events = ioWaiter->onceProcessEvents(3000);

					continue;
				} else {
					break;
				}
			}
		}
		EFiber* fiber = (*fiber_).get();

		if (ioWaiter->getWaitersCount() > 0) {
			// io waiter process.
			int events = ioWaiter->onceProcessEvents();
			ECO_DEBUG(EFiberDebugger::SCHEDULER, "return the number of events: %d", events);
		}

		if (!fiber->boundQueue) {
			fiber->boundQueue = &localQueue;
		}
		fiber->boundThreadID = currentThreadID;

		// bind io waiter
		fiber->setIoWaiter(ioWaiter);

#ifdef DEBUG
		llong t1 = ESystem::nanoTime();
#endif

		schedulerLocal.currFiber = fiber;
		fiber->context->swapIn();
		schedulerLocal.currFiber = null;

#ifdef DEBUG
		llong t2 = ESystem::nanoTime();
		ECO_DEBUG(EFiberDebugger::SCHEDULER, "fiter run time: %lldns, %s", t2 - t1, EThread::currentThread()->toString().c_str());
#endif

		switch (fiber->state) {
		case EFiber::RUNNABLE:
		{
			// add to thread local queue.
			localQueue.add(fiber_);
		}
			break;
		case EFiber::BLOCKED:
		{
			ES_ASSERT(fiber->blocker);
			if (!fiber->blocker->swapOut(fiber)) {
				// add to thread local queue.
				localQueue.add(fiber_);
			}
		}
			break;
		case EFiber::TERMINATED:
			delete fiber_;
			totalFiberCounter--;
			break;
		default:
			//
			break;
		}
	}

	currIoWaiter.set(null);
	currScheduler.set(null);

	// do some clean.
	EContext::cleanOrignContext();

	// try to notify another iowaiter in the same scheduler group.
	for (int i=0; i<ioWaiters.length(); i++) {
		if (ioWaiters[i] != ioWaiter) {
			ioWaiters[i]->signal();
		}
	}
}

int EFiberScheduler::totalFiberCount() {
	return totalFiberCounter.value();
}

EFiber* EFiberScheduler::activeFiber() {
	SchedulerLocal* sl = static_cast<SchedulerLocal*>(currScheduler.get());
	return sl ? sl->currFiber : null;
}

EFiberScheduler* EFiberScheduler::currentScheduler() {
	SchedulerLocal* sl = static_cast<SchedulerLocal*>(currScheduler.get());
	return sl ? sl->scheduler : null;
}

EIoWaiter* EFiberScheduler::currentIoWaiter() {
	return static_cast<EIoWaiter*>(currIoWaiter.get());
}

sp<EFileContext> EFiberScheduler::getFileContext(int fd) {
	sp<EFileContext> fdctx = hookedFiles.get(fd);
	if (!fdctx) {
		try {
			fdctx = new EFileContext(fd);
		} catch (...) {
			return null; // fcntl error.
		}
		hookedFiles.put(fd, fdctx);
	}
	return fdctx;
}

void EFiberScheduler::delFileContext(int fd) {
	hookedFiles.remove(fd);
}

void EFiberScheduler::clearFileContexts() {
	ECO_DEBUG(EFiberDebugger::SCHEDULER, "clear %d file contexts.", hookedFiles.size());
	hookedFiles.clear();
}

} /* namespace eco */
} /* namespace efc */
