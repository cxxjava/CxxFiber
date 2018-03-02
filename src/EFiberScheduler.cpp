/*
 * EFiberScheduler.cpp
 *
 *  Created on: 2016-5-17
 *      Author: cxxjava@163.com
 */

#include "./EContext.hh"
#include "./EIoWaiter.hh"
#include "./EFileContext.hh"
#include "../inc/EFiberScheduler.hh"
#include "../inc/EFiber.hh"
#include "../inc/EFiberBlocker.hh"
#include "../inc/EFiberDebugger.hh"

#include <sys/resource.h>

namespace efc {
namespace eco {

//=============================================================================

static long fdLimit(int deflim) {
	struct rlimit rlim;
	if (getrlimit(RLIMIT_NOFILE, &rlim) < 0)
		return deflim;
	long limit = rlim.rlim_cur;
	return (limit < 0 ? deflim : rlim.rlim_cur);
}

//=============================================================================

EThreadLocalStorage EFiberScheduler::currScheduler;
EThreadLocalStorage EFiberScheduler::currIoWaiter;

class SchedulerStub: public EObject {
public:
	EFiberConcurrentQueue<EFiber> taskQueue;
	EIoWaiter ioWaiter;
	EIoWaiter* volatile hungIoWaiter;
	SchedulerStub(int maxEventSetSize) :
			ioWaiter(maxEventSetSize), hungIoWaiter(null) {
	}
};

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
	delete schedulerStubs;
	delete hookedFiles;
}

EFiberScheduler::EFiberScheduler() :
		maxEventSetSize(fdLimit(FD_DEFAULT_CHUNKS * FD_CHUNK_CAPACITY)),
		threadNums(1),
		schedulerStubs(null),
		balanceCallback(null),
		scheduleCallback(null),
		hookedFiles(new EFileContextManager(maxEventSetSize)),
		interrupted(false) {
	//
}

EFiberScheduler::EFiberScheduler(int maxfd) :
		maxEventSetSize(ES_MAX(maxfd, fdLimit(FD_DEFAULT_CHUNKS * FD_CHUNK_CAPACITY))),
		threadNums(1),
		schedulerStubs(null),
		balanceCallback(null),
		scheduleCallback(null),
		hookedFiles(new EFileContextManager(maxEventSetSize)),
		interrupted(false) {
	//
}

void EFiberScheduler::schedule(sp<EFiber> fiber) {
	totalFiberCounter++;

	fiber->state = EFiber::RUNNABLE;
	fiber->setScheduler(this);

	if (!schedulerStubs) {
		defaultTaskQueue.add(new sp<EFiber>(fiber));
	} else {
		int index = 0;
		if (balanceCallback) {
			index = balanceCallback(fiber.get(), threadNums);
		} else {
			index = (balanceIndex++) % schedulerStubs->length();
		}
		SchedulerStub* ss = schedulerStubs->getAt(index);
		ss->taskQueue.add(new sp<EFiber>(fiber));
		EIoWaiter* iw = ss->hungIoWaiter;
		if (iw) {
			iw->signal();
		}
	}
}

#ifdef CPP11_SUPPORT
sp<EFiber> EFiberScheduler::schedule(std::function<void()> f, int stackSize) {
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

	sp<EFiber> fiber(new Fiber(f, stackSize));
	this->schedule(fiber);
	return fiber;
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

#ifdef CPP11_SUPPORT
void EFiberScheduler::setScheduleCallback(std::function<void(int threadIndex,
		SchedulePhase schedulePhase, EThread* currentThread,
		EFiber* currentFiber)> callback) {
	this->scheduleCallback = callback;
}
#else
void EFiberScheduler::setScheduleCallback(thread_schedule_callback_t* callback) {
	this->scheduleCallback = callback;
}
#endif

#ifdef CPP11_SUPPORT
void EFiberScheduler::setBalanceCallback(std::function<int(EFiber* fiber, int threadNums)> balancer) {
	this->balanceCallback = balancer;
}
#else
void EFiberScheduler::setBalanceCallback(fiber_schedule_balance_t* balancer) {
	this->balanceCallback = balancer;
}
#endif

void EFiberScheduler::join() {
	EThread* currentThread = EThread::currentThread();
#ifdef CPP11_SUPPORT
	std::function<void(int threadIndex,
			SchedulePhase schedulePhase, EThread* currentThread,
			EFiber* currentFiber)> callback = scheduleCallback;
#else
	thread_schedule_callback_t* callback = scheduleCallback;
#endif
	// create io waiter.
	long currentThreadID = currentThread->getId();
	EIoWaiter ioWaiter(maxEventSetSize);
	SchedulerLocal schedulerLocal(this);

	currScheduler.set(&schedulerLocal);
	currIoWaiter.set(&ioWaiter);

	if (scheduleCallback) {
		scheduleCallback(0, SCHEDULE_BEFORE, currentThread, NULL);
	}

	for (;;) {
		int total = totalFiberCounter.value();

		sp<EFiber>* fiber_ = defaultTaskQueue.poll();
		if (!fiber_) {
			if (total > 0) {
				/**
				 * inactive fibers is BLOCKED or WAITING!
				 */
				int events = ioWaiter.onceProcessEvents(3000);

				if (interrupted) {
					ioWaiter.interrupt();
				}

				if (scheduleCallback) {
					scheduleCallback(0, SCHEDULE_IDLE, currentThread, NULL);
				}

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

			if (interrupted) {
				ioWaiter.interrupt();
			}
		}

		if (!fiber->boundQueue) {
			fiber->boundQueue = &defaultTaskQueue;
		}
		fiber->boundThreadID = currentThreadID;

		// bind io waiter
		fiber->setIoWaiter(&ioWaiter);

#ifdef DEBUG
		llong t1 = ESystem::nanoTime();
#endif

		schedulerLocal.currFiber = fiber;
		if (scheduleCallback) {
			scheduleCallback(0, FIBER_BEFORE, currentThread, fiber);
		}
		fiber->context->swapIn();
		if (scheduleCallback) {
			scheduleCallback(0, FIBER_AFTER, currentThread, fiber);
		}
		schedulerLocal.currFiber = null;

#ifdef DEBUG
		llong t2 = ESystem::nanoTime();
		ECO_DEBUG(EFiberDebugger::SCHEDULER, "fiter run time: %lldns, %s", t2 - t1, currentThread->toString().c_str());
#endif

		switch (fiber->state) {
		case EFiber::RUNNABLE:
		{
			defaultTaskQueue.add(fiber_);
		}
			break;
		case EFiber::BLOCKED:
		{
			ES_ASSERT(fiber->blocker);
			if (!fiber->blocker->swapOut(fiber))
				defaultTaskQueue.add(fiber_);
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

	if (scheduleCallback) {
		scheduleCallback(0, SCHEDULE_AFTER, currentThread, NULL);
	}
}

void EFiberScheduler::join(int threadNums) {

	if (threadNums < 1) {
		throw EIllegalArgumentException(__FILE__, __LINE__, "threadNums < 1");
	}
	if (threadNums == 1) {
		join(); //!
		return;
	}
	// if (threadNums == 1) then ignore balanceCallback.
	this->threadNums = threadNums;

	// create thread local scheduler stub
	schedulerStubs = new EA<SchedulerStub*>(threadNums);
	for (int i=0; i<threadNums; i++) {
		schedulerStubs->setAt(i, new SchedulerStub(maxEventSetSize));
	}

	// reset error.
	hasError.set(false);

	// multi-threading.
	class Worker: public EThread {
	public:
		EFiberScheduler* scheduler;
		int index;
	public:
		virtual void run() {
			try {
				scheduler->joinWithThreadBind(scheduler->schedulerStubs, index, this);
			} catch (...) {
				scheduler->hasError.set(true);
			}
		}
	};

	/**
	 * Don't forget current thread self to work together.
	 */

	// create other threads.
	EA<sp<Worker> > pool(threadNums - 1);
	for (int i=0; i<threadNums-1; i++) {
		Worker* worker = new Worker();
		worker->scheduler = this;
		worker->index = i + 1; // 0 is for current thread.
		pool[i] = worker;
	}

	// dispatch fibers to each thread.
	sp<EFiber>* fiber_;
	int index = 0;
	while ((fiber_ = defaultTaskQueue.poll()) != null) {
		int i = 0;
		if (balanceCallback) {
			i = balanceCallback((*fiber_).get(), threadNums);
		} else {
			i = (index++) % threadNums;
		}
		schedulerStubs->getAt(i)->taskQueue.add(fiber_);
	}

	// worker run.
	for (int i=0; i<pool.length(); i++) {
		pool[i]->start();
	}

	// current thread work.
	joinWithThreadBind(schedulerStubs, 0, EThread::currentThread());

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

void EFiberScheduler::joinWithThreadBind(EA<SchedulerStub*>* schedulerStubs,
		int index, EThread* currentThread) {
	SchedulerStub* stub = schedulerStubs->getAt(index);
	EIoWaiter* ioWaiter = &stub->ioWaiter;
	EFiberConcurrentQueue<EFiber>* localQueue = &stub->taskQueue;

	long currentThreadID = currentThread->getId();
	SchedulerLocal schedulerLocal(this);

	currScheduler.set(&schedulerLocal);
	currIoWaiter.set(ioWaiter);

	if (scheduleCallback) {
		scheduleCallback(index, SCHEDULE_BEFORE, currentThread, NULL);
	}

	for (;;) {
		int total = totalFiberCounter.value();

		// try get from thread local queue.
		sp<EFiber>* fiber_ = localQueue->poll();
		if (!fiber_) {
			if (total > 0) {
				/**
				 * inactive fibers is BLOCKED or WAITING!
				 */
				stub->hungIoWaiter = ioWaiter;
				int events = ioWaiter->onceProcessEvents(3000);
				stub->hungIoWaiter = null;

				if (interrupted) {
					ioWaiter->interrupt();
				}

				if (scheduleCallback) {
					scheduleCallback(index, SCHEDULE_IDLE, currentThread, NULL);
				}

				continue;
			} else {
				break;
			}
		}
		EFiber* fiber = (*fiber_).get();

		if (ioWaiter->getWaitersCount() > 0) {
			// io waiter process.
			int events = ioWaiter->onceProcessEvents();
			ECO_DEBUG(EFiberDebugger::SCHEDULER, "return the number of events: %d", events);

			if (interrupted) {
				ioWaiter->interrupt();
			}
		}

		fiber->setThreadIndex(index);

		if (!fiber->boundQueue) {
			fiber->boundQueue = localQueue;
		}
		fiber->boundThreadID = currentThreadID;

		// bind io waiter
		fiber->setIoWaiter(ioWaiter);

#ifdef DEBUG
		llong t1 = ESystem::nanoTime();
#endif

		schedulerLocal.currFiber = fiber;
		if (scheduleCallback) {
			scheduleCallback(index, FIBER_BEFORE, currentThread, fiber);
		}
		fiber->context->swapIn();
		if (scheduleCallback) {
			scheduleCallback(index, FIBER_AFTER, currentThread, fiber);
		}
		schedulerLocal.currFiber = null;

#ifdef DEBUG
		llong t2 = ESystem::nanoTime();
		ECO_DEBUG(EFiberDebugger::SCHEDULER, "fiter run time: %lldns, %s", t2 - t1, currentThread->toString().c_str());
#endif

		switch (fiber->state) {
		case EFiber::RUNNABLE:
		{
			// add to thread local queue.
			localQueue->add(fiber_);
		}
			break;
		case EFiber::BLOCKED:
		{
			ES_ASSERT(fiber->blocker);
			if (!fiber->blocker->swapOut(fiber)) {
				// add to thread local queue.
				localQueue->add(fiber_);
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
	for (int i=0; i<schedulerStubs->length(); i++) {
		EIoWaiter* iw = &schedulerStubs->getAt(i)->ioWaiter;
		if (iw != ioWaiter) {
			iw->signal();
		}
	}

	if (scheduleCallback) {
		scheduleCallback(index, SCHEDULE_AFTER, currentThread, NULL);
	}
}

void EFiberScheduler::interrupt() {
	interrupted = true;

	//active all ioWatier.
	if (threadNums > 1 && schedulerStubs) {
		for (int i=0; i<schedulerStubs->length(); i++) {
			schedulerStubs->getAt(i)->ioWaiter.signal();
		}
	} else {
		EIoWaiter* iw = currentIoWaiter();
		if (iw) iw->signal();
	}
}

boolean EFiberScheduler::isInterrupted() {
	return interrupted;
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
	return hookedFiles->get(fd);
}

void EFiberScheduler::delFileContext(int fd) {
	hookedFiles->remove(fd);
}

void EFiberScheduler::clearFileContexts() {
	hookedFiles->clear();
}

} /* namespace eco */
} /* namespace efc */
