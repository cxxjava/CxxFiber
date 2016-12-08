/*
 * EFiberMutex.cpp
 *
 *  Created on: 2016-5-9
 *      Author: cxxjava@163.com
 */

#include "EFiberMutex.hh"
#include "EFiber.hh"

namespace efc {
namespace eco {

EFiberMutex::~EFiberMutex() {
}

EFiberMutex::EFiberMutex(): blocker(1, 1) {
}

void EFiberMutex::lock() {
	blocker.wait();
}

void EFiberMutex::lockInterruptibly() {
	throw EUnsupportedOperationException(__FILE__, __LINE__);
}

boolean EFiberMutex::tryLock() {
	return blocker.tryWait();
}

boolean EFiberMutex::tryLock(llong time, ETimeUnit* unit) {
	return blocker.tryWait(time, unit);
}

void EFiberMutex::unlock() {
	blocker.wakeUp();
}

ECondition* EFiberMutex::newCondition() {
	throw EUnsupportedOperationException(__FILE__, __LINE__);
}

boolean EFiberMutex::isLocked() {
	return !blocker.isWaking();
}

} /* namespace eco */
} /* namespace efc */
