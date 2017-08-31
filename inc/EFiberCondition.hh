/*
 * EFiberCondition.hh
 *
 *  Created on: 2016-5-24
 *      Author: cxxjava@163.com
 */

#ifndef EFIBERCONDITION_HH_
#define EFIBERCONDITION_HH_

#include "./EFiberBlocker.hh"

namespace efc {
namespace eco {

/**
 *
 */

class EFiberCondition: public EObject {
public:
	void await();
	void signal();
	void signalAll();

private:
	EConcurrentLinkedQueue<EFiberBlocker> waiters;
};

} /* namespace eco */
} /* namespace efc */
#endif /* EFIBERCONDITION_HH_ */
