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
	/**
	 * Schedule phase for per-thread.
	 */
	enum SchedulePhase {
		SCHEDULE_BEFORE = 0,
		SCHEDULE_IDLE = 1,
		FIBER_BEFORE = 2,
		FIBER_AFTER = 3,
		SCHEDULE_AFTER = 4
	};

	/**
	 * Type of the callback for per-thread schedule.
	 */
	typedef void thread_schedule_callback_t(int threadIndex,
			SchedulePhase schedulePhase, EThread* currentThread,
			EFiber* currentFiber);

	/**
	 * Type of fiber schedule balancer
	 *
	 * @return SchedulerStub id (threadIndex), 0 always the call join()'s thread.
	 */
	typedef int fiber_schedule_balance_t(EFiber* fiber, int threadNums);

public:
	virtual ~EFiberScheduler();

	/**
	 * New scheduler instance for signal-thread.
	 */
	EFiberScheduler();
	EFiberScheduler(int maxfd);

	/**
	 * Add a new fiber to this scheduler
	 */
	virtual void schedule(sp<EFiber> fiber);

#ifdef CPP11_SUPPORT
	/**
	 * Add a new lambda function as fiber to this scheduler (c++11)
	 */
	virtual sp<EFiber> schedule(std::function<void()> f, int stackSize=1024*1024);
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
	 * Set the callback for scheduler looping
	 *
	 * @param callback Schedule callback at different phase.
	 */
#ifdef CPP11_SUPPORT
	virtual void setScheduleCallback(std::function<void(int threadIndex,
			SchedulePhase schedulePhase, EThread* currentThread,
			EFiber* currentFiber)> callback);
#else
	virtual void setScheduleCallback(thread_schedule_callback_t* callback);
#endif

	/**
	 * Set the callback for schedule balance
	 *
	 * @param balancer if null then balance use rol-poling else use the callback return value
	 */
#ifdef CPP11_SUPPORT
	virtual void setBalanceCallback(std::function<int(EFiber* fiber, int threadNums)> balancer);
#else
	virtual void setBalanceCallback(fiber_schedule_balance_t* balancer);
#endif

	/**
	 * Do schedule and wait all fibers work done.
	 */
	virtual void join();

	/**
	 * Do schedule with thread pool and wait all fibers work done.
	 *
	 * @param threadNums >= 1
	 */
	virtual void join(int threadNums);

	/**
	 *
	 */
	virtual void interrupt();
	virtual boolean isInterrupted();

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
#ifdef CPP11_SUPPORT
	std::function<int(EFiber* fiber, int threadNums)> balanceCallback;
#else
	fiber_schedule_balance_t* balanceCallback;
#endif
	EAtomicCounter balanceIndex;

	EAtomicBoolean hasError;
	EAtomicCounter totalFiberCounter;

	EFileContextManager* hookedFiles;

	volatile boolean interrupted;

#ifdef CPP11_SUPPORT
	std::function<void(int threadIndex,
			SchedulePhase schedulePhase, EThread* currentThread,
			EFiber* currentFiber)> scheduleCallback;
#else
	thread_schedule_callback_t* scheduleCallback;
#endif

	static EThreadLocalStorage currScheduler;
	static EThreadLocalStorage currIoWaiter;

	/**
	 *
	 */
	void joinWithThreadBind(EA<SchedulerStub*>* schedulerStubs, int index,
			EThread* currentThread);
};

} /* namespace eco */
} /* namespace efc */
#endif /* EFIBERSCHEDULER_HH_ */
