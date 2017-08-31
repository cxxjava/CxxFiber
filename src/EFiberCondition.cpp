/*
 * EFiberCondition.cpp
 *
 *  Created on: 2016-5-24
 *      Author: cxxjava@163.com
 */

#include "../inc/EFiberCondition.hh"

namespace efc {
namespace eco {

void EFiberCondition::await() {
	sp<EFiberBlocker> fb = new EFiberBlocker(0);
	waiters.add(fb);
	fb->wait();
}

void EFiberCondition::signal() {
	sp<EFiberBlocker> fb = waiters.poll();
	if (fb != null) {
		fb->wakeUp();
	}
}

void EFiberCondition::signalAll() {
	sp<EConcurrentIterator<EFiberBlocker> > iter = waiters.iterator();
	while (iter->hasNext()) {
		sp<EFiberBlocker> fb = iter->next();
		fb->wakeUp();
	}
}

} /* namespace eco */
} /* namespace efc */
