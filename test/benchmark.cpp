#include "main.hh"
#include "Eco.hh"

#define LOG(fmt,...) ESystem::out->println(fmt, ##__VA_ARGS__)

static void test_scheduling_performance() {
#ifdef CPP11_SUPPORT
	EFiberDebugger::getInstance().debugOn(EFiberDebugger::NONE);

	EFiberScheduler scheduler;

	llong t1 = ESystem::currentTimeMillis();

	int times = 1000000;

#if 1
	int count = times;

	for (int i=0; i<10; i++) {
		scheduler.schedule([&]() {
			while (count-- > 0) {
				EFiber::yield();
			}
		});
	}

	scheduler.join();
#else
	EAtomicCounter count(times);

	for (int i=0; i<10; i++) {
		scheduler.schedule([&]() {
			while (count-- > 0) {
				EFiber::yield();
			}
		});
	}

	scheduler.join(4);
#endif

	llong t2 = ESystem::currentTimeMillis();

	LOG("switch 10 fibers run %ld times, cost %ld ms\nper second op times: %f", times, t2 - t1, ((double)times)/(t2-t1)*1000);
#endif
}

MAIN_IMPL(testeco_benchmark) {
	printf("main()\n");

	ESystem::init(argc, argv);

	printf("inited.\n");

	int i = 0;
	try {
		boolean loop = EBoolean::parseBoolean(ESystem::getProgramArgument("loop"));

//		EFiberDebugger::getInstance().debugOn(EFiberDebugger::SCHEDULER);

		do {
			test_scheduling_performance();
		} while (1);
	}
	catch (EException& e) {
		e.printStackTrace();
	}
	catch (...) {
		printf("catch all...\n");
	}

	printf("exit...\n");

	ESystem::exit(0);

	return 0;
}
