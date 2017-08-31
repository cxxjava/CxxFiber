/*
 * EFiberTimer.cpp
 *
 *  Created on: 2016-5-21
 *      Author: cxxjava@163.com
 */

#include "../inc/EFiberTimer.hh"
#include "../inc/EFiberScheduler.hh"

namespace efc {
namespace eco {

void EFiberTimer::cancel() {
	EFiberScheduler* fs = EFiberScheduler::currentScheduler();
	if (fs) {
		sp<EFiber> fiber = this->fiber.lock();
		if (fiber != null) {
			fiber->cancel();
		}
	}
}

void EFiberTimer::setFiber(EFiber* fiber) {
	this->fiber = fiber->shared_from_this();
}

} /* namespace eco */
} /* namespace efc */
