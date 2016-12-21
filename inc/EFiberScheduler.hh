/*
 * EFiberScheduler.hh
 *
 *  Created on: 2016-5-17
 *      Author: cxxjava@163.com
 */

#ifndef EFIBERSCHEDULER_HH_
#define EFIBERSCHEDULER_HH_

#include "Efc.hh"
#include "EFiber.hh"
#include "EFiberTimer.hh"
#include "EFiberUtil.hh"

#ifdef CPP11_SUPPORT
#include <functional>
#endif


namespace efc {
namespace eco {

/**
 * Fiber scheduler: only for signal-threading.
 */

class EFiber;
class EIoWaiter;
class EFileContext;
class EFileContextManager;
class SchedulerStub;

class EFiberScheduler: public EObject {
public:
	virtual ~EFiberScheduler();

	/**
	 * New scheduler instance for signal-thread.
	 */
	EFiberScheduler(int maxfd=100000);

	/**
	 * Add a new fiber to this scheduler
	 */
	virtual void schedule(sp<EFiber> fiber);

#ifdef CPP11_SUPPORT
	/**
	 * Add a new lambda function as fiber to this scheduler (c++11)
	 */
	virtual void schedule(std::function<void()> f, int stackSize=1024*1024);
#endif

	/**
	 * Add a new timer to this scheduler
	 */
	virtual sp<EFiberTimer> addtimer(sp<EFiberTimer> timer, llong delay);
	virtual sp<EFiberTimer> addtimer(sp<EFiberTimer> timer, EDate* time);
	virtual sp<EFiberTimer> addtimer(sp<EFiberTimer> timer, llong delay, llong period);
	virtual sp<EFiberTimer> addtimer(sp<EFiberTimer> timer, EDate* firstTime, llong period);

#ifdef CPP11_SUPPORT
	/**
	 * Add a lambda function as timer to this scheduler (c++11)
	 */
	virtual void addtimer(std::function<void()> f, llong delay);
	virtual void addtimer(std::function<void()> f, EDate* time);
	virtual void addtimer(std::function<void()> f, llong delay, llong period);
	virtual void addtimer(std::function<void()> f, EDate* firstTime, llong period);
#endif

	/**
	 * Do schedule and wait all fibers work done.
	 */
	virtual void join();

	/**
	 * Type of fiber schedule balancer
	 * @return SchedulerStub id, 0 always the call join()'s thread.
	 */
	typedef int fiber_schedule_balance_t(EFiber* fiber, int threadNums);

	/**
	 * Do schedule with thread pool and wait all fibers work done.
	 * @param threadNums >= 1
	 * @param callback if null then balance use rol-poling else use the callback return value
	 */
	virtual void join(int threadNums, fiber_schedule_balance_t* callback=null);

	/**
	 *
	 */
	virtual int totalFiberCount();

	/**
	 * Get current active fiber.
	 */
	static EFiber* activeFiber();

	/**
	 * Get current joined scheduler.
	 */
	static EFiberScheduler* currentScheduler();
	static EIoWaiter* currentIoWaiter();

public:
	sp<EFileContext> getFileContext(int fd);
	void delFileContext(int fd);
	void clearFileContexts();

private:
	friend class EFiber;

	int maxEventSetSize;
	int threadNums;

	EFiberConcurrentQueue<EFiber> defaultTaskQueue;
	EA<SchedulerStub*>* schedulerStubs; // created only if threadNums > 1
	fiber_schedule_balance_t* balanceCallback;
	EAtomicCounter balanceIndex;

	EAtomicBoolean hasError;
	EAtomicCounter totalFiberCounter;

	EFileContextManager* hookedFiles;

	static EThreadLocalStorage currScheduler;
	static EThreadLocalStorage currIoWaiter;

	/**
	 *
	 */
	void joinWithThreadBind(EA<SchedulerStub*>* schedulerStubs, int index);
};

} /* namespace eco */
} /* namespace efc */
#endif /* EFIBERSCHEDULER_HH_ */
