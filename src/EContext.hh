/*
 * EContext.hh
 *
 *  Created on: 2016-5-1
 *      Author: cxxjava@163.com
 */

#ifndef ECONTEXT_HH_
#define ECONTEXT_HH_

#ifdef __APPLE__
//@see: "error ucontext routines are deprecated, and require _XOPEN_SOURCE to be defined"
//@see: http://boost-users.boost.narkive.com/oBZ9TxFN/coroutine-bus-error-on-mac-os-x-10-5
#define _XOPEN_SOURCE
#endif

#include "Efc.hh"

#include <ucontext.h>

namespace efc {
namespace eco {

/**
 *
 */

class EFiber;

class EContext {
public:
	virtual ~EContext();

	EContext(EFiber* fiber);

	boolean swapIn();
	boolean swapOut();

	static inline void* getOrignContext();
	static void cleanOrignContext();

private:
	friend class EFiber;

#ifdef __x86_64__
	es_uint64_t env[10];
#endif
	ucontext_t* context;

	EFiber* fiber;
	char* stackAddr; /* Base of stack's allocated memory */
	int errno_; /* Global errno */

	static EThreadLocalStorage threadLocal;

	static void fiber_worker(void* arg);
};

} /* namespace eco */
} /* namespace efc */
#endif /* ECONTEXT_HH_ */
