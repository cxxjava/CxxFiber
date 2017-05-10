#include "main.hh"
#include "Eco.hh"

#include <sys/resource.h>

#define LOG(fmt,...) ESystem::out->println(fmt, ##__VA_ARGS__)

#define FIBER_THREADS 4
#define MAX_CONNECTIONS 2
#define MAX_IDLETIME 60000
EFiberScheduler scheduler;
struct ThreadData: public EObject {
	struct Sock: public EObject {
		sp<ESocket> socket;
		long lastActiveTime;

		Sock(sp<ESocket> s, long t): socket(s), lastActiveTime(t) {}
	};
	EHashMap<int, Sock*> sockMap;

	void updateLastActiveTime(sp<ESocket> socket, long time) {
		Sock* sock = sockMap.get(socket->getFD());
		if (sock) {
			sock->lastActiveTime = time;
		}
	}
};

static EA<ThreadData*> gThreadData(FIBER_THREADS, false);

static volatile int gStopFlag = 0;

static EAtomicCounter gConnections;

static void SigHandler(int sig) {
	scheduler.interrupt();
	gStopFlag = 1;
}

static int balance_callback(EFiber* fiber, int threadNums) {
	int fid = fiber->getId();
	if (fid == 0) { // 0 is the first fiber.
		return 0;   // 0 is the join()'s thread
	} else {
		long id = fiber->getTag();
		if (id > 0) {
			return (int)id;
		} else {
			return fid % (threadNums - 1) + 1; // balance to other's threads.
		}
	}
}

static void schedule_callback(int threadIndex,
		EFiberScheduler::SchedulePhase schedulePhase, EThread* currentThread,
		EFiber* currentFiber) {
	switch (schedulePhase) {
	case EFiberScheduler::SCHEDULE_BEFORE: {
		LOG("SCHEDULE_BEFORE index=%d", threadIndex);
		gThreadData[threadIndex] = new ThreadData();
		break;
	}
	case EFiberScheduler::SCHEDULE_AFTER: {
		LOG("SCHEDULE_AFTER index=%d", threadIndex);
		delete gThreadData[threadIndex];
		break;
	}
	default:
//		LOG("index=%d, phase=%d, thread=%d, fiber=%d",
//				threadIndex, schedulePhase, currentThread->getId(),
//				currentFiber ? currentFiber->getId() : -1);
		break;
	}
}

MAIN_IMPL(testeco_echoserver) {
	printf("main()\n");

//	eso_proc_detach(TRUE, 0, 0, 0);

	ESystem::init(argc, argv);

	eso_signal(SIGINT, SigHandler);

	printf("inited.\n");

//	rlimit of = {100000, 100000};
//	if (-1 == setrlimit(RLIMIT_NOFILE, &of)) {
//		perror("setrlimit");
//		exit(1);
//	}

	try {
//		EFiberScheduler scheduler;
		
		scheduler.schedule([&](){
			EServerSocket ss;
			ss.setReuseAddress(true);
			ss.bind(8888, 8192);
			ss.setSoTimeout(30000);

			while (!gStopFlag) {
				try {
					// accept
					sp<ESocket> socket = ss.accept();
					if (socket != null) {
						// reach the max connections.
						if (gConnections.value() >= MAX_CONNECTIONS) {
							socket->close();
							EThread::yield(); //?
							continue;
						}

						scheduler.schedule([=](){
							gConnections++;

							try {
								socket->setTcpNoDelay(true);
								socket->setSoLinger(true, 0);
								socket->setReceiveBufferSize(256*1024);
								socket->setSoTimeout(30000);

								// update accept time
								ThreadData* threadData = gThreadData[EFiber::currentFiber()->getThreadIndex()];
								threadData->sockMap.put(socket->getFD(), new ThreadData::Sock(socket, ESystem::currentTimeMillis()));

								char buf[512] = {0};
								// read
								EInputStream* is = socket->getInputStream();
								EOutputStream* os = socket->getOutputStream();

								while (!gStopFlag) {
									try {
										// read
										int n = is->read(buf, sizeof(buf));
										if (n == -1) { //EOF
											threadData->sockMap.remove(socket->getFD());
											socket->close();
											LOG("socket closed");
											break;
										} else {
											// update read time
											threadData->updateLastActiveTime(socket, ESystem::currentTimeMillis());

											LOG("read buf=%s", buf);

											// write
//											os->write(buf, n);
											os->write("HTTP/1.1 200 OK\r\nContent-Length: 11\r\nContent-Type: text/html\r\n\r\nHello,world", 75);
										}
									} catch (ESocketTimeoutException& e) {
										LOG("read timeout");
									}

#if 0
									socket->close();
									break;
#endif
								}
							} catch (EIOException& e) {
								e.printStackTrace();
							} catch (...) {
							}

							gConnections--;
						});
					}
				} catch (ESocketTimeoutException& e) {
					LOG("accept timeout");
				}
			}
		}/*, 32*1024*/);

		// create leader fiber per-thread for idle socket clean.
		for (int i=1; i<FIBER_THREADS; i++) {
			sp<EFiber> workThreadLeadFiber = scheduler.schedule([&](){
				LOG("I'm leader fiber. thread id=%ld", EThread::currentThread()->getId());

				try {
					ThreadData* threadData = gThreadData[EFiber::currentFiber()->getThreadIndex()];

					while (!gStopFlag) {
						long currTime = ESystem::currentTimeMillis();
						sp<EIterator<ThreadData::Sock*> > iter = threadData->sockMap.values()->iterator();
						while (iter->hasNext()) {
							ThreadData::Sock* sock = iter->next();

							if (currTime - sock->lastActiveTime > MAX_IDLETIME) {
								//shutdown socket by server.
								LOG("socket shutdown1");
								sock->socket->shutdown();
								LOG("socket shutdown2");
							}
						}

						sleep(3);
					}
				} catch (EThrowable& t) {
					t.printStackTrace();
				}

			});
			workThreadLeadFiber->setTag(i);
		}

		// schedule callback
		scheduler.setScheduleCallback(schedule_callback);

		// balance callback
#ifdef CPP11_SUPPORT
		scheduler.setBalanceCallback([](EFiber* fiber, int threadNums){
			int fid = fiber->getId();
			if (fid == 0) { // 0 is the first fiber.
				return 0;   // 0 is the join()'s thread
			} else {
				long id = fiber->getTag();
				if (id > 0) {
					return (int)id;
				} else {
					return fid % (threadNums - 1) + 1; // balance to other's threads.
				}
			}
		});
#else
		scheduler.setBalanceCallback(balance_callback);
#endif

		scheduler.join(FIBER_THREADS);
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
