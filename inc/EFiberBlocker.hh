/*
 * EFiberBlocker.hh
 *
 *  Created on: 2016-5-9
 *      Author: cxxjava@163.com
 */

#ifndef EFIBERBLOCKER_HH_
#define EFIBERBLOCKER_HH_

#include "Efc.hh"

namespace efc {
namespace eco {

/**
 *
 */

class EFiber;

class EFiberBlocker: public EObject {
public:
	virtual ~EFiberBlocker();

	EFiberBlocker(uint capacity=0, uint limit=-1);

	void wait();
	boolean tryWait();
	boolean tryWait(llong time, ETimeUnit* unit=ETimeUnit::MILLISECONDS);
	boolean wakeUp();
	boolean isWaking();

private:
	friend class EFiberScheduler;

	class SyncObj: public EQueueEntry, public ESynchronizeable {
	};

	uint wakeup_;
	uint limit_;

	EConcurrentIntrusiveDeque<EQueueEntry> waitQueue;
	sp<SyncObj> sync_;

	boolean swapOut(EFiber* fiber);
};

} /* namespace eco */
} /* namespace efc */
#endif /* EFIBERBLOCKER_HH_ */
