#include "main.hh"
#include "Eco.hh"

#include <sys/resource.h>

#define LOG(fmt,...) ESystem::out->println(fmt, ##__VA_ARGS__)

volatile int gStopFlag = 0;

static void SigHandler(int sig) {
	gStopFlag = 1;
}

static int balance_callback(EFiber* fiber, int threadNums) {
	int fid = fiber->getId();
	if (fid == 0) { // 0 is the first fiber.
		return 0;   // 0 is the join()'s thread
	} else {
		return fid % (threadNums - 1) + 1; // balance to other's threads.
	}
}

MAIN_IMPL(testeco_echoserver) {
	printf("main()\n");

//	eso_proc_detach(TRUE, 0, 0, 0);

	ESystem::init(argc, argv);

//	eso_signal(SIGINT, SigHandler);

	printf("inited.\n");

	rlimit of = {1000000, 1000000};
	if (-1 == setrlimit(RLIMIT_NOFILE, &of)) {
		perror("setrlimit");
		exit(1);
	}


	try {
		EFiberScheduler scheduler;
		
		scheduler.schedule([&](){
			EServerSocket ss;
			ss.setReuseAddress(true);
			ss.bind(8888, 8192);

			while (!gStopFlag) {
				// accept
				sp<ESocket> socket = ss.accept();
				if (socket != null) {
					socket->setTcpNoDelay(true);
					socket->setSoLinger(true, 0);
					socket->setReceiveBufferSize(256*1024);

					scheduler.schedule([=](){
						try {
							char buf[512] = {0};
							// read
							EInputStream* is = socket->getInputStream();
							EOutputStream* os = socket->getOutputStream();

							while (true) {
								// read
								int n = is->read(buf, sizeof(buf));
								if (n == -1) { //EOF
									socket->close();
									break;
								} else {
//									LOG("read buf=%s", buf);
								}

								// write
//								os->write(buf, n);
								os->write("HTTP/1.1 200 OK\r\nContent-Length: 11\r\nContent-Type: text/html\r\n\r\nHello,world", 75);

								socket->close();
								break;
							}
						} catch (EIOException& e) {
							e.printStackTrace();
						} catch (...) {
						}
					});
				}
			}
		}/*, 32*1024*/);

		scheduler.join(4, balance_callback);
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
