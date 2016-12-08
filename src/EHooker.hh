/*
 * EHooker.hh
 *
 *  Created on: 2016-5-12
 *      Author: cxxjava@163.com
 */

#ifndef EHOOKER_HH_
#define EHOOKER_HH_

#include "Efc.hh"

namespace efc {
namespace eco {

/**
 *
 */

class EHooker {
public:
	DECLARE_STATIC_INITZZ

public:
	static boolean isInterrupted(boolean ClearInterrupted);
	static llong interruptEscapedTime();

	template <typename F>
	static ssize_t comm_io_on_fiber(F fn, const char* name, int event, int fd, ...);
};

} /* namespace eco */
} /* namespace efc */
#endif /* EHOOKER_HH_ */
