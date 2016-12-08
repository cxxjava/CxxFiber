/*
 * EFiberMutex.hh
 *
 *  Created on: 2016-5-9
 *      Author: cxxjava@163.com
 */

#ifndef EFIBERMUTEX_HH_
#define EFIBERMUTEX_HH_

#include "EFiberBlocker.hh"

namespace efc {
namespace eco {

/**
 * Mutex for fiber.
 */

class EFiberMutex: public ELock {
public:
	virtual ~EFiberMutex();
	EFiberMutex();

	virtual void lock();
	virtual void lockInterruptibly() THROWS(EUnsupportedOperationException);
	virtual boolean tryLock();
	virtual boolean tryLock(llong time, ETimeUnit* unit=ETimeUnit::MILLISECONDS);
	virtual void unlock();
	virtual ECondition* newCondition() THROWS(EUnsupportedOperationException);

	virtual boolean isLocked();

private:
	EFiberBlocker blocker;
};

} /* namespace eco */
} /* namespace efc */
#endif /* EFIBERMUTEX_HH_ */
