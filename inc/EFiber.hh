/*
 * EFiber.hh
 *
 *  Created on: 2016-5-17
 *      Author: cxxjava@163.com
 */

#ifndef EFIBER_HH_
#define EFIBER_HH_

#include "Efc.hh"
#include "EFiberUtil.hh"

namespace efc {
namespace eco {

/**
 *
 */

class EContext;
class EIoWaiter;
class EFiberBlocker;
class EFiberScheduler;
template<typename E>
class EFiberLocal;

abstract class EFiber: public ERunnable, public enable_shared_from_this<EFiber> {
public:
	enum State {
		NEW,
		RUNNABLE,
		BLOCKED,
		WAITING,
		TERMINATED
	};

	static const int DEFAULT_STACK_SIZE = 1024*1024; //1M
#ifdef __linux__
	static const int MIN_STACK_SIZE = 8192;
#endif
#ifdef __APPLE__
	static const int MIN_STACK_SIZE = 32768;
#endif

public:

	/**
	 * The action to be performed by this fiber task.
	 */
	virtual void run() = 0;

public:
	virtual ~EFiber();

	/**
	 *
	 */
	EFiber& setName(const char* name);

	/**
	 *
	 */
	const char* getName();

	/**
	 *
	 */
	void cancel();

	/**
	 *
	 */
	boolean isCanceled();

	/**
	 *
	 */
	boolean isAlive();

	/**
	 *
	 */
	int getStackSize();

	/**
	 *
	 */
	int getId();

	/**
	 *
	 */
	State getState();

	/**
	 *
	 */
	EFiberScheduler* getScheduler();

	/**
	 *
	 */
	EIoWaiter* getIoWaiter();

	/**
	 *
	 */
	boolean isWaitTimeout();

	/**
	 *
	 */
	virtual EStringBase toString();

	/**
	 *
	 */
	static void yield();

	/**
	 *
	 */
	static void sleep(llong millis) THROWS(EInterruptedException);

	/**
	 *
	 */
	static void sleep(llong millis, int nanos) THROWS(EInterruptedException);

	/**
	 *
	 */
	static EFiber* currentFiber();

protected:
	friend class EContext;
	friend class EFiberScheduler;
	friend class EFiberBlocker;
	friend class EIoWaiter;
	template<typename E>
	friend class EFiberLocal;
	template<typename E, typename LOCK>
	friend class EFiberConcurrentQueue;

	/* Fiber state */
	volatile State state;

	/* Fiber ID */
	int fid;
	/* Fiber name */
	EString name;

	int stackSize;
	EContext* context; /* Fiber's context */

	sp<EFiber> parent; /* Keep parent for sub fiber */

	EFiberScheduler* scheduler; /* Bound scheduler */
	EIoWaiter* iowaiter; /* Bound iowaiter */

	EFiberConcurrentQueue<EFiber>* boundQueue; /* Thread bound queue */
	long boundThreadID;

	EFiberBlocker* blocker;

	boolean isIoWaitTimeout;
	boolean canceled;

	es_hash_t* localValues;

	EFiberConcurrentQueue<EFiber>::NODE* packing;

	/**
	 * Constructor
	 */
	EFiber();
	EFiber(int stackSize);

	void setScheduler(EFiberScheduler* scheduler);
	void setIoWaiter(EIoWaiter* iowaiter);
	void setState(State state);

	void swapIn();
	void swapOut();
};

} /* namespace eco */
} /* namespace efc */
#endif /* EFIBER_HH_ */
