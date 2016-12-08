#include "main.hh"
#include "Eco.hh"

#define LOG(fmt,...) ESystem::out->println(fmt, ##__VA_ARGS__)

volatile int gStopFlag = 0;

static void SigHandler(int sig) {
	gStopFlag = 1;
}

MAIN_IMPL(testeco_echoserver) {
	printf("main()\n");

//	eso_proc_detach(TRUE, 0, 0, 0);

	ESystem::init(argc, argv);

	eso_signal(SIGINT, SigHandler);

	printf("inited.\n");

	try {
		EFiberScheduler scheduler;
		
		scheduler.schedule([&](){
			EServerSocket ss;
			ss.setReuseAddress(true);
			ss.bind(8888);

			while (!gStopFlag) {
				// accept
				sp<ESocket> socket = ss.accept();
				if (socket != null) {
					scheduler.schedule([=](){
						try {
							char buf[512] = {0};
							// read
							EInputStream* is = socket->getInputStream();
							int n = is->read(buf, sizeof(buf));
							LOG("read buf=%s", buf);

							// write
							EOutputStream* os = socket->getOutputStream();
							os->write(buf, n);
//							os->write("HTTP/1.1 200 OK\r\nContent-Length: 11\r\nContent-Type: text/html\r\n\r\nHello,world", 75);
						} catch (EIOException& e) {
							e.printStackTrace();
						} catch (...) {
						}
					});
				}
			}
		});

		scheduler.join(4);
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
