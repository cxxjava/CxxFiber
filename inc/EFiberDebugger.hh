/*
 * EFiberDebugger.hh
 *
 *  Created on: 2016-5-20
 *      Author: cxxjava@163.com
 */

#ifndef EFIBERDEBUGGER_HH_
#define EFIBERDEBUGGER_HH_

#define USE_ELOG 0 // 0-printf | 1-CxxLog4j

#include "Efc.hh"
#if USE_ELOG
#include "ELog.hh"
#endif

namespace efc {
namespace eco {

/**
 *
 */

class EFiberDebugger {
public:
	static const int NONE = 0X00;
	static const int FIBER = 0X01;
	static const int SCHEDULER = 0X02;
	static const int BLOCKER = 0X04;
	static const int WAITING = 0X08;
	static const int ALL = 0XFF;

public:
	static EFiberDebugger& getInstance();

	void debugOn(int flags);
	void debugOff(int flags);
	boolean isDebugOn(int flags);

	void log(const char* _file_, int _line_, const char* msg) {
	#if USE_ELOG
		logger->debug(_file_, _line_, msg);
	#else
		ECalendar cal(ESystem::currentTimeMillis(), ESystem::localTimeZone());
		fprintf(stdout, "[%s][%s][%s:%d] %s\n",
				cal.toString("%Y%m%d %H:%M:%S,%s").c_str(),
				EThread::currentThread()->toString().c_str(),
				eso_filepath_name_get(_file_), _line_,
				msg);
		fflush(stdout);
	#endif
	}

private:
	#if USE_ELOG
	static sp<ELogger> logger;
	#endif

	int debugFlags;
	EFiberDebugger();
};

#ifdef DEBUG
#define ECO_DEBUG(type, fmt, ...) \
    do { \
    	::efc::eco::EFiberDebugger& debugger = ::efc::eco::EFiberDebugger::getInstance(); \
        if (debugger.isDebugOn(type)) { \
        	EString s = EString::formatOf(fmt, ##__VA_ARGS__); \
        	debugger.log(__FILE__, __LINE__, s.c_str()); \
        } \
    } while(0)
#else
#define ECO_DEBUG(type, fmt, ...)
#endif

} /* namespace eco */
} /* namespace efc */
#endif /* EFIBERDEBUGGER_HH_ */
