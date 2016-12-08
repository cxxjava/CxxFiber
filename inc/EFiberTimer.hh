/*
 * EFiberTimer.hh
 *
 *  Created on: 2016-5-21
 *      Author: cxxjava@163.com
 */

#ifndef EFIBERTIMER_HH_
#define EFIBERTIMER_HH_

#include "Efc.hh"
#include "EFiber.hh"

namespace efc {
namespace eco {

/**
 *
 */

abstract class EFiberTimer: public ERunnable {
public:

	/**
	 *
	 */
	void cancel();

private:
	friend class TimberFiber;

	wp<EFiber> fiber;

	void setFiber(EFiber* fiber);
};

} /* namespace eco */
} /* namespace efc */
#endif /* EFIBERTIMER_HH_ */
