/*
 * EFiberDebugger.cpp
 *
 *  Created on: 2016-5-20
 *      Author: cxxjava@163.com
 */

#include "EFiberDebugger.hh"

namespace efc {
namespace eco {

#if USE_ELOG
sp<ELogger> EFiberDebugger::logger = ELoggerManager::getLogger("eco");
#endif

EFiberDebugger::EFiberDebugger(): debugFlags(0) {
}

void EFiberDebugger::debugOn(int flags) {
	debugFlags |= flags;
}

void EFiberDebugger::debugOff(int flags) {
	debugFlags &= ~flags;
}

boolean EFiberDebugger::isDebugOn(int flags) {
	return (debugFlags & flags);
}

EFiberDebugger& EFiberDebugger::getInstance() {
	static EFiberDebugger debugger;
	return debugger;
}

} /* namespace eco */
} /* namespace efc */
