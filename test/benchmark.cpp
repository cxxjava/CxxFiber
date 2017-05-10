#include "main.hh"
#include "Eco.hh"

#define LOG(fmt,...) ESystem::out->println(fmt, ##__VA_ARGS__)

//=============================================================================
//linux: per second op times: 10638297.872340
//os x : per second op times: 14492753.623188

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

//=============================================================================
//wrk -t12 -c100 -d30s -T30s -H "Connection: close" http://127.0.0.1:9988/
//linux: Requests/sec: 246052
//os x : Requests/sec: 53310

#include <assert.h>
#include <unistd.h>
#include <sys/resource.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#ifdef __linux__
#include <linux/version.h>
#endif

static const char* g_ip = "0.0.0.0";
static const uint16_t g_port = 8888;
static int thread_count = 8;
static int tps = 0;

static inline int atomic_add32(volatile int * mem, int val)
{
	int ret;
	asm volatile
		(
		"lock\n\t xadd %1, %0"
		: "+m"( *mem ), "=r"( ret )
		: "1"( val )
		: "memory", "cc"
		);

	return ret;
}

static inline int do_write(int fd, const char * b, int l)
{
	int w = 0, t = 0;
	for (;;) {
		t = ::write(fd, b + w, l - w);
		if (t < 1) {
//			LOG("%s:%u, msg=%s", __FUNCTION__, __LINE__, strerror(errno));
			return -1;
		}
		w += t;
		if (l == w) return l;
	}

	return -1;
}

static inline int do_read(int fd, char * b, int l)
{
	int r = 0, t = 0;
	for (;;) {
		t = ::read(fd, b + r, l - r);
		if (t < 1) {
//			LOG("%s:%u, msg=%s", __FUNCTION__, __LINE__, strerror(errno));
			return -1;
		}
		r += t;
		return r;
	}

	return -1;
}

static void server(EFiberScheduler& scheduler)
{
	int ret;
	int accept_fd = socket(AF_INET, SOCK_STREAM, 0);
	assert(accept_fd >= 0);

#ifdef __APPLE__
	int v = 1;
	ret = setsockopt(accept_fd, SOL_SOCKET, SO_REUSEPORT, &v, sizeof(v));
#else
#if LINUX_VERSION_CODE>= KERNEL_VERSION(3,9,0)
	int v = 1;
	ret = setsockopt(accept_fd, SOL_SOCKET, SO_REUSEPORT, &v, sizeof(v));
#endif
#endif

	sockaddr_in addr;
	addr.sin_family = AF_INET;
	addr.sin_port = htons(g_port);
	addr.sin_addr.s_addr = inet_addr(g_ip);
	ret = bind(accept_fd, (sockaddr*)&addr, sizeof(addr));
	assert(ret == 0);

	ret = listen(accept_fd, 8192);
	assert(ret == 0);
	for (;;) {
		socklen_t addr_len = sizeof(addr);
		int s = accept(accept_fd, (sockaddr*)&addr, &addr_len);
		if (s < 0) {
			perror("accept error:");
			continue;
		}

		int size = 256 * 1024;
		setsockopt(s, SOL_SOCKET, SO_RCVBUF, (char *)&size, sizeof(size));

		int flag = 1;
		setsockopt(s, IPPROTO_TCP, TCP_NODELAY, (char *)&flag, sizeof(flag));

		struct linger li = { 1, 0 };
		setsockopt(s, SOL_SOCKET, SO_LINGER, (char *)&li, sizeof(li));

		scheduler.schedule([s]() {
			while (1) {
				int rsize = 1024;
				char rbuf[rsize];

				ssize_t rn = do_read(s, rbuf, rsize);
				if (rn < 0) {
					shutdown(s, 0x02);
					close(s);
					break;
				}

 				memcpy(rbuf, "HTTP/1.1 200 OK\r\nContent-Length: 11\r\nContent-Type: text/html\r\n\r\nHello,world", 75);
				rsize = 75;
				rn = do_write(s, rbuf, rsize);
				if (rn < 0) {
					shutdown(s, 0x02);
					close(s);
					return ;
				}

				atomic_add32(&tps, 1);
			}
		});
	}
}

static void show_status()
{
	while (1) {
		printf("tps : %d\n", atomic_add32(&tps, -1 * tps));
		sleep(1);
	}
}

static int balance_callback(EFiber* fiber, int threadNums) {
	int fid = fiber->getId();
	if (fid == 0) { // 0 is the first fiber.
		return 0;   // 0 is the join()'s thread
	} else {
		return fid % (threadNums - 1) + 1; // balance to other's threads.
	}
}

#define SET_RLIMIT    0
#define USE_SIG_BLOCK 0

static void test_iohooking_performance() {
#if SET_RLIMIT
	rlimit of = {1000000, 1000000};
	if (-1 == setrlimit(RLIMIT_NOFILE, &of)) {
		perror("setrlimit");
		exit(1);
	}
#endif

#if USE_SIG_BLOCK
	sigset_t new_mask, old_mask, wait_mask;
	sigfillset(&new_mask);
	pthread_sigmask(SIG_BLOCK, &new_mask, &old_mask);
#endif

	sp<EThreadX> thd = EThreadX::execute([](){
		show_status();
	});

	EFiberScheduler scheduler;
	scheduler.schedule([&](){
		server(scheduler);
	});
	scheduler.join(thread_count);
//	scheduler.join(thread_count, balance_callback);
	thd->join();

#if USE_SIG_BLOCK
	pthread_sigmask(SIG_SETMASK, &old_mask, 0);
	sigemptyset(&wait_mask);
	sigaddset(&wait_mask, SIGINT);
	sigaddset(&wait_mask, SIGQUIT);
	sigaddset(&wait_mask, SIGTERM);
	sigaddset(&wait_mask, SIGUSR1);
	sigaddset(&wait_mask, SIGUSR2);
	pthread_sigmask(SIG_BLOCK, &wait_mask, 0);
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
//			test_scheduling_performance();
			test_iohooking_performance();
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
