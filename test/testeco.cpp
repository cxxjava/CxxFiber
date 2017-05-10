#include "main.hh"
#include "Eco.hh"

#include "EFiberUtil.hh"

#include <fcntl.h>
#include <poll.h>
#include <sys/select.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <sys/ioctl.h>
#include <signal.h>
#include <netdb.h>
#include <sys/stat.h>
#ifdef __linux__
#include <sys/epoll.h>
#include <linux/version.h>
#include <sys/sendfile.h>
#endif
#ifdef __APPLE__
#include <sys/event.h>
#endif

#define LOG(fmt,...) ESystem::out->println(fmt, ##__VA_ARGS__)

static const char* g_ip = "0.0.0.0";
static const uint16_t g_port = 8888;

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

class MySubFiber: public EFiber {
public:
	virtual ~MySubFiber() {
		LOG("~MySubFiber()");
	}
	MySubFiber() {
	}
	MySubFiber(int stackSize): EFiber(stackSize) {
	}
	virtual void run() {
		for (int i=0; i<20; i++) {
			LOG("MySubFiber run(%d)", i);

			errno = 5555;
			EFiber::yield();
			LOG("errno5=%d", errno);
		}
	}
};

class MyFiber: public EFiber {
public:
	virtual ~MyFiber() {
		LOG("~MyFiber()");
	}
	MyFiber() {
	}
	MyFiber(int stackSize): EFiber(stackSize) {
	}
	virtual void run() {
		LOG("fiber=%s", this->toString().c_str());

		for (int i=0; i<200; i++) {
			LOG("MyFiber run(%d)", i);

			errno = 9999;
			EFiber::yield();
			LOG("errno9=%d", errno);

//			throw EException(__FILE__, __LINE__, "e0");

			try {
				throw EException(__FILE__, __LINE__, "e1");
			} catch (EException& e) {
				EFiber::yield();
				LOG("e1");
////				try {
//				throw EException(__FILE__, __LINE__, "e2");
////				} catch (...) {
////					LOG("...");
////				}
			}
		}

		LOG("end of MyFiber::run()...");
	}
};

static void test_one_thread() {
	EFiberScheduler scheduler;
	scheduler.schedule(new MyFiber());
	scheduler.schedule(new MyFiber());
	scheduler.schedule(new MyFiber());
	scheduler.schedule(new MyFiber());
	scheduler.join();

	LOG("end of test_one_thread().");
}

static void test_multi_thread() {
	EFiberScheduler scheduler;

	for (int i=0; i<5; i++) {
		scheduler.schedule(new MyFiber());

		scheduler.schedule([](){});
	}

	scheduler.join(3);

	LOG("test_multi_thread() finished!");
}

static void go_func_test1() {
	LOG("go_func_test1");
}

static void go_func_test2(EFiberScheduler* scheduler) {
	LOG("go_func_test2, scheduler hashCode: %d", scheduler->hashCode());
}

static void test_c11schedule() {
	EFiberScheduler* scheduler = new EFiberScheduler();
	scheduler->schedule(new MyFiber());
	scheduler->schedule(new MyFiber());
	scheduler->schedule(new MyFiber());
	scheduler->schedule(new MyFiber());

#ifdef CPP11_SUPPORT

	scheduler->schedule(go_func_test1);

	scheduler->schedule(std::bind(&go_func_test2, scheduler));

	scheduler->schedule(
		[](){
			EFiber* fiber = EFiber::currentFiber();
			LOG("fiber2=%s", fiber->toString().c_str());

			for (int i=0; i<1000; i++) {
				errno = 1111;
				EFiber::yield();
				LOG("i=%d, errno1=%d", i, errno);
			}
		}
	);

	sp<EThread> ths = EThreadX::execute([&]() {
		EFiberScheduler* scheduler = new EFiberScheduler();

		scheduler->schedule(new MyFiber());

		scheduler->join();

		delete scheduler;
	});

	// mast be before thread join().
	scheduler->join();

	ths->join();

	delete scheduler;

	LOG("fiber2 finished!");
#else
	scheduler->join();
	LOG("fiber1 finished!");
#endif
}

static void test_nesting() {
#ifdef CPP11_SUPPORT
	EFiberScheduler scheduler;
	scheduler.schedule([&]() {
		char ss[100];
		sprintf(ss, "lllllllllllllllllllllll");
		LOG("parent fiber, s=%s", ss);

		EFiber* f = EFiber::currentFiber();
		LOG("f->toString(): %s", f->toString().c_str());

		for (int i=0; i<100; i++) {
			scheduler.schedule([&]() {
//				LOG("sub fiber, s=%s", ss); //error! ss out of scope.

				for (int i=0; i<100; i++) {
					EFiber* f = EFiber::currentFiber();
					LOG("sf->toString(): %s", f->toString().c_str());

					EFiber::yield();
				}

			}, 400*1024);
		}
	});
	scheduler.join(3);
#endif

	LOG("end of test_nesting().");
}

static void test_channel() {
#ifdef CPP11_SUPPORT
	EFiberChannel<EString> channel(0);
	EFiberScheduler scheduler;
	scheduler.schedule([&]() {
		for (int i=0; i<100; i++) {
			channel.write(new EString("channel data"));
		}
	});
	scheduler.schedule([&]() {
		sp<EString> s;
		for (int i=0; i<200; i++) {
			if (channel.tryRead(s)) {
				LOG("recv s=%s", s->c_str());
			} else {
				LOG("recv fail.");
			}
		}
	});
	scheduler.join();
#endif
}

static void test_channel_one_thread() {
#ifdef CPP11_SUPPORT
	EFiberChannel<EString> channel(0);
	EFiberScheduler scheduler;
	scheduler.schedule([&]() {
		char ss[100];
		sprintf(ss, "lllllllllllllllllllllll");
		LOG("parent fiber, s=%s", ss);

		EFiber* f = EFiber::currentFiber();
		LOG("f->toString(): %s", f->toString().c_str());

		for (int i=0; i<100; i++) {
			scheduler.schedule([&]() {
				//LOG("sub fiber, s=%s", ss); //error! ss out of scope.

				for (int i=0; i<100; i++) {
					EFiber* f = EFiber::currentFiber();
					LOG("sf->toString(): %s", f->toString().c_str());

					channel.write(new EString("channel data"));

//					EFiber::yield();

					LOG("after of write.");
				}

			}, 40*1024);
		}

		sp<EString> s;
		for (int i=0; i<100*100; i++) {
			s = channel.read();
			LOG("recv s=%s", s->c_str());
		}
	});
	scheduler.join();
#endif
}

#define RUNTIMES 100
#define FLIP_READ_WRITE 1
#define BOTH_IS_FIBER 0
#define BOTH_IS_THREAD 1

static void test_channel_multi_thread() {
#ifdef CPP11_SUPPORT
	EFiberScheduler scheduler;

	// channel.
	EFiberChannel<EString> channel(1);

	scheduler.schedule([&]() {
		for (int i=0; i<RUNTIMES; i++) {
			scheduler.schedule([&]() {
				//LOG("sub fiber, s=%s", ss); //error! ss out of scope.

				for (int j=0; j<RUNTIMES; j++) {
#if FLIP_READ_WRITE
					sp<EString> s = channel.read();
					if (s != null) {
						LOG("recv s=%s", s->c_str());
					}

//					LOG("after of read.");
#else
					channel.write(new EString("channel data"));

					LOG("after of write.");
#endif
				}

//				LOG("after of fiber.");
			});
		}
	});

	sp<EThread> ths = EThreadX::execute([&]() {
#if BOTH_IS_FIBER
		EFiberScheduler scheduler;

		scheduler.schedule([&]() {
			for (int i=0; i<RUNTIMES*RUNTIMES; i++) {
#if FLIP_READ_WRITE
				channel.write(new EString("channel data"));
#else
				sp<EString> s = channel.read();
				if (s != null) {
					LOG("recv s=%s", s->c_str());
				}
#endif
			}
		});

		scheduler.join();
#else
#if FLIP_READ_WRITE
		for (int i=0; i<RUNTIMES*RUNTIMES; i++) {
			channel.write(new EString("channel data"));

//			LOG("after of write.");
		}
#else
		for (int i=0; i<RUNTIMES*RUNTIMES; i++) {
			sp<EString> s = channel.read();
			if (s != null) {
				LOG("recv s=%s", s->c_str());
			}

			LOG("after of read.");
		}
#endif
#endif

#if BOTH_IS_THREAD
		for (int i=0; i<RUNTIMES; i++) {
			channel.write(new EString("channel data2"));
		}
#endif

		LOG("end of threadx.");
	});

	sp<EThread> ths2 = EThreadX::execute([&]() {
#if BOTH_IS_THREAD
		for (int i=0; i<RUNTIMES; i++) {
			sp<EString> s = channel.read();
			if (s != null) {
//				LOG("recv2 s=%s", s->c_str());
			}
		}
#endif
	});

	// io wait
	scheduler.schedule([&]() {
		int i = 0;
		while (i++ < 50) {
			EFiber::sleep(5);
			LOG("sleep 3s.");
		}
	});

	scheduler.join(4);
	ths->join();
	ths2->join();
#endif

	LOG("end of test_channel_multi_thread().");
}

static void test_mutex() {
#ifdef CPP11_SUPPORT
	EFiberMutex mutex;
	EFiberScheduler scheduler;

	int value = 0;

	mutex.lock();

	scheduler.schedule([&]() {
		EFiber::sleep(1000);

		boolean r = mutex.tryLock(1, ETimeUnit::SECONDS);
		LOG("tryLock=%d...", r);
		if (r) {
			mutex.unlock();
		}

		EFiber::sleep(3000);

		LOG("1...");
		for (int i=0; i<3000; i++) {
			SYNCBLOCK(&mutex) {
				LOG("-- %d", value++);
			}}
		}
	});

//	for (int i=0; i<100; i++) {
	scheduler.schedule([&]() {
		EFiber::sleep(3000);

		LOG("2...");
		mutex.unlock();
		LOG("3...");

		EFiber::sleep(3000);

		for (int i=0; i<300; i++) {
			SYNCBLOCK(&mutex) {
				LOG("++ %d", value++);
			}}
		}
	});
//	}
	scheduler.join();

	LOG("end of test_mutex().");
#endif
}

static void test_mutex_multi_thread() {
#ifdef CPP11_SUPPORT
	EFiberMutex mutex;
	EFiberScheduler scheduler;

	int value = 0;

	mutex.lock();

#if 0
	scheduler.schedule([&]() {
		EFiber::sleep(1000);

		boolean r = mutex.tryLock(1, ETimeUnit::SECONDS);
		LOG("tryLock=%d...", r);
		if (r) {
			mutex.unlock();
		}

		EFiber::sleep(3000);

		LOG("1...");
		for (int i=0; i<300; i++) {
			SYNCBLOCK(&mutex) {
				LOG("-- %d", value++);
			}}
		}
	});

	sp<EThreadX> thread = EThreadX::execute([&]() {
		EThread::sleep(2000);

		LOG("2...");
		mutex.unlock();
		LOG("3...");

		EThread::sleep(3000);

		for (int i=0; i<300; i++) {
			SYNCBLOCK(&mutex) {
				LOG("++ %d", value++);
			}}
		}
	});
#else
	scheduler.schedule([&]() {
		EFiber::sleep(1000);

		boolean r = mutex.tryLock(1, ETimeUnit::SECONDS);
		LOG("tryLock1=%d...", r);
		if (r) {
			mutex.unlock();
		}

		mutex.lock();
		EFiber::sleep(2000);
		mutex.unlock();

		LOG("1...");
		for (int i=0; i<300; i++) {
			SYNCBLOCK(&mutex) {
				LOG("-- %d", value++);
			}}
		}
	});

	sp<EThreadX> thread = EThreadX::execute([&]() {
		sleep(2);

		LOG("2...");
		mutex.unlock();
		LOG("3...");

		sleep(1);

		boolean r = mutex.tryLock(1, ETimeUnit::SECONDS);
		LOG("tryLock2=%d...", r);
		if (r) {
			mutex.unlock();
		}

		for (int i=0; i<300; i++) {
			SYNCBLOCK(&mutex) {
				LOG("++ %d", value++);
			}}
		}
	});
#endif

	scheduler.join();
	thread->join();

	LOG("end of test_mutex_multi_thread().");
#endif
}

static void test_condition() {
#ifdef CPP11_SUPPORT
	EFiberScheduler scheduler;
	EFiberCondition condition;

	for (int i=0; i<10; i++) {
		scheduler.schedule([&]() {
			LOG("await, i=%d", i);
			condition.await();
			LOG("wakeup, i=%d", i);
		});
	}

#if 1
	for (int i=0; i<10; i++) {
		scheduler.schedule([&]() {
			EFiber::sleep(5000);
			condition.signal();
		});
	}
#else
	scheduler.schedule([&]() {
		EFiber::sleep(5000);
		condition.signalAll();
	});
#endif

	scheduler.join();
#endif

	LOG("end of test_condition().");
}

static void test_sleep() {
#ifdef CPP11_SUPPORT
	EFiberScheduler scheduler;

	scheduler.schedule([]() {
		LOG("sleep 1 second.");
		EFiber::sleep(1000);
	});

	scheduler.join();
#endif

	LOG("end of test_sleep().");
}

static void fiber_destroyed_callback(void* data) {
	if (data) {
		EString* s = (EString*)data;
		delete s;
	}
}

static void test_local() {
#ifdef CPP11_SUPPORT
	EFiberScheduler scheduler;
	EFiberLocal<EString*> localValue;
	EFiberLocal<EString*> localValue2(fiber_destroyed_callback);

	for (int i=0; i<100; i++) {
		scheduler.schedule([&, i]() {
			EString x = EString::formatOf("fiber %d data", i);
			LOG("x=%s", x.c_str());
			localValue.set(new EString(x));
			EString* s = localValue.get();
			LOG("s=%s", s->c_str());
			delete localValue.remove();

			localValue2.set(new EString("freed at call back."));
			LOG("s=%s", localValue2.get()->c_str());
		});
	}

	scheduler.join();
#endif

	LOG("end of test_local().");
}

static void test_hook_connect1() {
#ifdef CPP11_SUPPORT
	EFiberScheduler scheduler;

	scheduler.schedule([]() {
		int fd = socket(AF_INET, SOCK_STREAM, 0);

		sockaddr_in addr;
		memset(&addr, 0, sizeof(addr));
		addr.sin_family = AF_INET;
		addr.sin_port = htons(8096);
		addr.sin_addr.s_addr = inet_addr("61.135.169.121");
		if (-1 == connect(fd, (sockaddr*)&addr, sizeof(addr))) {
			LOG("connect error:%s\n", strerror(errno));
			return;
		} else {
			LOG("connect success.");
		}

		close(fd);
	});

	scheduler.join();
#endif
}

static void test_hook_connect2() {
#ifdef CPP11_SUPPORT
	EFiberScheduler scheduler;

	for (int i=0; i<100; i++) {
		scheduler.schedule([]() {
			int fd = socket(AF_INET, SOCK_STREAM, 0);

			sockaddr_in addr;
			memset(&addr, 0, sizeof(addr));
			addr.sin_family = AF_INET;
			addr.sin_port = htons(80);
			addr.sin_addr.s_addr = inet_addr("61.135.169.121");
			if (-1 == connect(fd, (sockaddr*)&addr, sizeof(addr))) {
				LOG("connect error:%s\n", strerror(errno));
				return;
			} else {
				LOG("connect success.");
			}

			close(fd);
		});
	}

	scheduler.join(3);
#endif
}

static void test_hook_poll() {
#ifdef CPP11_SUPPORT
	EFiberScheduler scheduler;

	scheduler.schedule([]() {
		int fd = eso_net_socket(AF_INET, SOCK_STREAM, 0);
		int result = eso_net_connect(fd, "61.135.169.121", 80, 50); //poll in eso_net_connect().
		LOG("result=%d", result);
		eso_net_close(fd);
	});

	scheduler.join();
#endif
}

#ifdef TEST_HOOK_SELECT
static void test_hook_select() {
#ifdef CPP11_SUPPORT
	EFiberScheduler scheduler;

	scheduler.schedule([]() {
		int fd = eso_net_socket(AF_INET, SOCK_STREAM, 0);
		int result = eso_net_connect(fd, "61.135.169.121", 80, 50);
		LOG("connect result=%d", result);

		eso_net_write(fd, "HTTP/1.1 200 OK\r\nContent-Length: 11\r\nContent-Type: text/html\r\n\r\nHello,world", 75);

		fd_set readset, writeset, exceptset;
		FD_ZERO(&readset);
		FD_ZERO(&writeset);
		FD_ZERO(&exceptset);
		FD_SET(fd, &readset);
		FD_SET(fd, &writeset);
		FD_SET(fd, &exceptset);
		struct timeval tv = {5, 0};
//		result = select(fd+1, &readset, &writeset, &exceptset, &tv);
		result = select(fd+1, &readset, NULL, NULL, &tv);
		LOG("select result=%d, errno=%d", result, errno);
		if (FD_ISSET(fd, &readset)) {
			char buf[1024];
			result = eso_net_read(fd, buf, sizeof(buf));
			LOG("read result=%d, s=%s", result, buf);
		}
		if (FD_ISSET(fd, &writeset)) {
			LOG("select result=%d, can write", result);
		}

		eso_net_close(fd);
	});

	scheduler.join();
#endif
}
#endif

static void test_hook_sleep() {
#ifdef CPP11_SUPPORT
	EFiberScheduler scheduler;

	scheduler.schedule([]() {
		while (1) {
			sleep(10);
			LOG("xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx");
		}
	});

	for (int i=0; i<10; i++) {
		scheduler.schedule([]() {
			while (1) {
				LOG("z");
				EFiber::yield();
			}
		});
	}

	scheduler.join();
#endif
}

static void sigfunc(int sig_no) {
	LOG("signaled.");
	int temp = 1000;
	while (temp-- > 0)
	;
}

static void test_hook_signal() {
	EFiberScheduler scheduler;

	signal(SIGINT, sigfunc);

#ifdef CPP11_SUPPORT
	scheduler.schedule([]() {
		while (1) {
			sleep(10);
			LOG("xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx");
		}
	});

	scheduler.join();
#endif
}

static void test_hook_fcntl() {
	struct flock _lock;
	_lock.l_type = F_WRLCK;
	_lock.l_whence = SEEK_SET;
	_lock.l_start = 0;
	_lock.l_len = 0;
	int fd = open( "/tmp/t",O_CREAT|O_RDWR,S_IRWXU|S_IRGRP|S_IWGRP|S_IRWXO );
	int ret = fcntl( fd,F_SETLK,&_lock );
	LOG("ret=%d", ret);
	ret = fcntl(fd, F_GETLK,&_lock );
	LOG("ret=%d", ret);
	close(fd);

#ifdef CPP11_SUPPORT
	EFiberScheduler scheduler;
	scheduler.schedule([&]() {
		struct flock _lock;
		_lock.l_type = F_WRLCK;
		_lock.l_whence = SEEK_SET;
		_lock.l_start = 0;
		_lock.l_len = 0;
		int fd = open( "/tmp/t2",O_CREAT|O_RDWR,S_IRWXU|S_IRGRP|S_IWGRP|S_IRWXO );
		int ret = fcntl( fd,F_SETLK,&_lock );
		LOG("ret=%d", ret);
		ret = fcntl(fd, F_GETLK,&_lock );
		LOG("ret=%d", ret);
		close(fd);
	});
	scheduler.join();
#endif
}

static void test_hook_nonblocking() {
	int flags;
	int fd = socket(AF_INET, SOCK_STREAM, 0);

	flags = fcntl(fd, F_GETFL, 0);
	if (flags & O_NONBLOCK) {
		LOG("O_NONBLOCK");
	}
	fcntl(fd, F_SETFL, flags | O_NONBLOCK);

	flags = fcntl(fd, F_GETFL, 0);
	if (flags & O_NONBLOCK) {
		LOG("O_NONBLOCK");
	}
	fcntl(fd, F_SETFL, flags & ~O_NONBLOCK);

	flags = fcntl(fd, F_GETFL, 0);
	if (flags & O_NONBLOCK) {
		LOG("O_NONBLOCK");
	}

#ifdef CPP11_SUPPORT
	EFiberScheduler scheduler;
	scheduler.schedule([&]() {
		fcntl(fd, F_SETFL, flags | O_NONBLOCK);

		flags = fcntl(fd, F_GETFL, 0);
		if (flags & O_NONBLOCK) {
			LOG("O_NONBLOCK");
		}

		fcntl(fd, F_SETFL, flags & ~O_NONBLOCK);
		int on = 1; ioctl(fd, FIONBIO, &on);

		//====================

		sockaddr_in addr;
		memset(&addr, 0, sizeof(addr));
		addr.sin_family = AF_INET;
		addr.sin_port = htons(80);
		addr.sin_addr.s_addr = inet_addr("61.135.169.121");
		if (-1 == connect(fd, (sockaddr*)&addr, sizeof(addr))) {
			LOG("connect error:%s\n", strerror(errno));
			return;
		} else {
			LOG("connect success.");
		}

		flags = fcntl(fd, F_GETFL, 0);
		if (flags & O_NONBLOCK) {
			LOG("O_NONBLOCK");
		}

		ENetWrapper::configureBlocking(fd, false);

		//close(fd);
	});
	scheduler.join();
#endif

	flags = ::fcntl(fd, F_GETFL);
	LOG("flags=%d", flags);
	if (flags & O_NONBLOCK) {
		LOG("O_NONBLOCK");
	}

	close(fd);
}

static void test_hook_read_write() {
//	signal(SIGINT, sigfunc);

#define MULTITHREAD 1

#ifdef CPP11_SUPPORT
	EFiberScheduler scheduler;
#if MULTITHREAD
	for (int i=0; i<100; i++) {
#endif
	scheduler.schedule([&]() {
		int fd = socket(AF_INET, SOCK_STREAM, 0);

		int timeout = 10000;
		struct timeval tv;
		tv.tv_sec = timeout / 1000;
		tv.tv_usec = (timeout % 1000) * 1000;
		ENetWrapper::setOption(fd, ESocketOptions::_SO_TIMEOUT, &tv, sizeof(tv));

		sockaddr_in addr;
		memset(&addr, 0, sizeof(addr));
		addr.sin_family = AF_INET;
		addr.sin_port = htons(8096);
		addr.sin_addr.s_addr = inet_addr("127.0.0.1");
		if (-1 == connect(fd, (sockaddr*)&addr, sizeof(addr))) {
			LOG("connect error:%s\n", strerror(errno));
			return;
		} else {
			LOG("connect success.");
		}

		const char* req = "GET / HTTP/1.1\r\nHost: localhost:8096\r\n\r\n";
		int written = write(fd, req, strlen(req));
		if (written != strlen(req)) {
			LOG("write error.");
		}
		char buf[152] = {0};
		int readed = read(fd, buf, sizeof(buf));
		LOG("readed=%d, errno=%d, buf=%s", readed, errno, buf);

		// write big data.
		es_data_t* wbuf = eso_mmalloc(102400000);
		eso_mmemfill(wbuf, '1');
		written = write(fd, (char *)wbuf, eso_mnode_size(wbuf));
		eso_mfree(wbuf);

		close(fd);
	});
#if MULTITHREAD
	}
	scheduler.join(3);
#else
	scheduler.join();
#endif
#endif
}

static void test_hook_pipe() {
#ifdef CPP11_SUPPORT
	es_pipe_t* pipe = eso_pipe_create();

	EFiberScheduler scheduler;
	scheduler.schedule([&]() {

		void* buf[32];
		int r = eso_fread(buf, sizeof(buf), pipe->in);
		LOG("r=%d, s=%s", r, buf);

	});
	scheduler.schedule([&]() {
		sleep(3);
		eso_fwrite("123456", 6, pipe->out);
		LOG("w");
	});
	scheduler.join();

	eso_pipe_destroy(&pipe);
#endif
}

static void test_timer() {
	class Timer1: public EFiberTimer {
	public:
		virtual void run() {
			LOG("timer1 run...");
		}
	};

	class Timer2: public EFiberTimer {
	public:
		virtual void run() {
			LOG("timer2 run...");
		}
	};

	EFiberScheduler scheduler;

	sp<EFiberTimer> t1 = scheduler.addtimer(new Timer1(), 3000, 1000);
	sp<EFiberTimer> t2 = scheduler.addtimer(new Timer2(), 1000, 1000);

#ifdef CPP11_SUPPORT
	scheduler.schedule([&]() {
		EFiber::sleep(10000);
		t2->cancel();
		EFiber::sleep(10000);
		t1->cancel();
	});

	scheduler.addtimer([]() {
		LOG("www");
	}, 1, 2000);
#endif
	scheduler.join();
}

static void test_hook_kqueue() {
#ifdef CPP11_SUPPORT
	EFiberScheduler scheduler;
	scheduler.schedule([&]() {

#ifdef __APPLE__
	int fd = socket(AF_INET, SOCK_STREAM, 0);

	sockaddr_in addr;
	memset(&addr, 0, sizeof(addr));
	addr.sin_family = AF_INET;
	addr.sin_port = htons(8096);
	addr.sin_addr.s_addr = inet_addr("127.0.0.1");
	if (-1 == connect(fd, (sockaddr*)&addr, sizeof(addr))) {
		LOG("connect error:%s\n", strerror(errno));
		return;
	} else {
		LOG("connect success.");
	}

	const char* req = "GET / HTTP/1.1\r\nHost: localhost:8096\r\n\r\n";
	int written = write(fd, req, strlen(req));
	if (written != strlen(req)) {
		LOG("write error.");
	}

	ENetWrapper::configureBlocking(fd, false);

	int kq = kqueue();

	struct timespec tv = {3, 0};
	struct kevent ke;
	struct kevent ke_;
	EV_SET(&ke, fd, EVFILT_READ, EV_ADD, 0, 0, NULL);
	int n = kevent(kq, &ke, 1, &ke_, 1, &tv);
	if (n > 0) {
		char buf[152] = {0};
		int readed = read(fd, buf, sizeof(buf));
		LOG("readed=%d, errno=%d, buf=%s", readed, errno, buf);
	}

	close(fd);
	close(kq);
#endif
	});

	scheduler.join();
#endif
}

static void test_hook_gethostbyname() {
#ifdef CPP11_SUPPORT
	EFiberScheduler scheduler;

	scheduler.schedule([&]() {
		while (true) {
			LOG("xxx");
			const char* name = "x2.xxx.xxx";
			struct hostent *hp = gethostbyname(name);
			if (hp) {
				struct in_addr **addrp = (struct in_addr **) hp->h_addr_list;
				EInetAddress ia(name, (*addrp)->s_addr);
				LOG("xxx %s", ia.toString().c_str());
			}
			//EFiber::yield();
		}
	});

	scheduler.schedule([&]() {
		while (true) {
		EFiber::sleep(100);
		LOG("yyy");
		}
	});

	scheduler.join();
#endif
}

static void test_hook_sendfile() {
	EFiberScheduler scheduler;
	scheduler.schedule([&](){
		int ret;
		int accept_fd = socket(AF_INET, SOCK_STREAM, 0);
		ES_ASSERT(accept_fd >= 0);

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
		ES_ASSERT(ret == 0);

		ret = listen(accept_fd, 8192);
		ES_ASSERT(ret == 0);
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

					const char* filename = "xxx.zip";
					struct stat filestat;
					stat(filename, &filestat);
					int fd = open(filename, O_RDONLY);
					off_t off = 0;
					off_t num = filestat.st_size;
#ifdef __APPLE__
					int result = sendfile(fd, s, 0, &num, NULL, 0);
#else
					int result = sendfile(s, fd, &off, num);
#endif
					close(fd);
					printf("reuslt=%d, fd=%d, num=%d, errno=%d, error=%s\n", result, fd, num, errno, strerror(errno));
				}
			});
		}
	});
	scheduler.join();
}

static void test_not_hook_file() {
#ifdef CPP11_SUPPORT
	EFiberScheduler scheduler;
	scheduler.schedule([&]() {
		es_file_t* pfile = eso_fopen("./test_file.txt", "r+");
//		int fd = ::open("./test_file.txt", O_RDWR);
		int r = ::write(eso_fileno(pfile), (void*)"1234567890", 10);
		eso_fclose(pfile);
//		::close(fd);
	});
	scheduler.join();
#endif
}

static void test_nio() {
	ESocketChannel* socketChannel = ESocketChannel::open();
	socketChannel->configureBlocking(false );
	ESelector* selector = ESelector::open();
	socketChannel->register_(selector, ESelectionKey::OP_CONNECT);
	EInetSocketAddress SERVER_ADDRESS("localhost", 8096);
//	EInetSocketAddress SERVER_ADDRESS("10.211.55.8", 8899);
	socketChannel->connect(&SERVER_ADDRESS);

	ESet<ESelectionKey*>* selectionKeys;
	sp<EIterator<ESelectionKey*> > iterator;
	ESelectionKey* selectionKey;
	ESocketChannel* client;
	int count = 0;
	EIOByteBuffer sendbuffer(512);
	EIOByteBuffer receivebuffer(512);

	int nn = 0;
	do {
		nn++;

		selector->select();
		selectionKeys = selector->selectedKeys();
		iterator = selectionKeys->iterator();
		while (iterator->hasNext()) {
			selectionKey = iterator->next();
			if (selectionKey->isConnectable()) {
				LOG("client connect");
				client = (ESocketChannel*) selectionKey->channel();
				if (client->isConnectionPending()) {
					client->finishConnect();
					LOG("connect finished!");
					sendbuffer.clear();
					const char* req = "GET / HTTP/1.1\r\nHost: localhost:8096\r\n\r\n";
					sendbuffer.put(req, strlen(req));
					sendbuffer.flip();
					client->write(&sendbuffer);
				}
				client->register_(selector, ESelectionKey::OP_READ);
			} else if (selectionKey->isReadable()) {
				client = (ESocketChannel*) selectionKey->channel();
				receivebuffer.clear();
				try {
					count = client->read(&receivebuffer);
				} catch (...) {
					client->close();
				}
				if(count>0){
					receivebuffer.flip();
					LOG("recev server:%s", receivebuffer.current());
				}
				selectionKey->cancel();
			}
		}
		selectionKeys->clear();
	} while (nn < 2);

	selector->close();
	delete selector;

	socketChannel->close();
	delete socketChannel;
}

static void test_sslsocket() {
	char buffer[4096];
	int ret;
	ESSLSocket *socket = new ESSLSocket();
	socket->setReceiveBufferSize(10240);
	socket->connect("www.baidu.com", 443, 3000);
	socket->setSoTimeout(3000);
	char *get_str = "GET / HTTP/1.1\r\n"
					"Accept: image/gif, image/jpeg, image/pjpeg, image/pjpeg, application/x-shockwave-flash, application/msword, application/vnd.ms-excel, application/vnd.ms-powerpoint, application/xaml+xml, application/x-ms-xbap, application/x-ms-application, */*\r\n"
					"Accept-Language: zh-cn\r\n"
					"User-Agent: Mozilla/4.0 (compatible; MSIE 8.0; Windows NT 5.1; Trident/4.0; .NET4.0C; .NET4.0E; .NET CLR 2.0.50727)\r\n"
					"Accept-Encoding: gzip, deflate\r\n"
					"Host: www.baidu.com\r\n"
					"Connection: Close\r\n" //"Connection: Keep-Alive\r\n"
					"Cookie: BAIDUID=72CBD0B204EC83BF3C5C0FA7A9C89637:FG=1\r\n\r\n";
	EOutputStream *sos = socket->getOutputStream();
	EInputStream *sis = socket->getInputStream();
	sos->write(get_str, strlen(get_str));
	LOG("socket available=[%d]", sis->available());
	try {
		while ((ret = sis->read(buffer, sizeof(buffer))) > 0) {
			LOG("socket ret=[%d], available=[%d]", ret, sis->available());
			LOG("socket read=[%s]", buffer);
		}
	} catch (...) {
	}
	sis->close();
	sos->close();
	socket->close();
	delete socket;
}

static void test_sslserversocket() {
	ESSLServerSocket *serverSocket = new ESSLServerSocket();
	serverSocket->setSSLParameters(null,
			"./certs/tests-cert.pem",
			"./certs/tests-key.pem",
			null, null);
	serverSocket->setReuseAddress(true);
	serverSocket->bind(8443);
	LOG("serverSocket=%s", serverSocket->toString().c_str());
	int count = 0;
	char buffer[11];
	while (count < 10) {
		try {
			ESSLSocket *clientSocket = serverSocket->accept();
			count++;
			EInetSocketAddress *isar = clientSocket->getRemoteSocketAddress();
			EInetSocketAddress *isal = clientSocket->getLocalSocketAddress();
			LOG("socket rip=[%s], rport=%d", isar->getHostName(), isar->getPort());
			LOG("socket lip=[%s], lport=%d", isal->getHostName(), isal->getPort());
			try {
				EInputStream *sis = clientSocket->getInputStream();
				eso_memset(buffer, 0, sizeof(buffer) - 1);
				sis->read(buffer, sizeof(buffer));
				LOG("socket read=[%s]", buffer);
			} catch (EIOException &e) {
				LOG("read e=%s", e.toString().c_str());
			}
			delete clientSocket;
		} catch (...) {
			LOG("accept error.");
		}
	}
	delete serverSocket;
}

static void test_efc_in_fiber() {
#ifdef CPP11_SUPPORT
	EFiberScheduler scheduler;

	scheduler.schedule([&]() {
		while (true) {
//			test_nio();
//			test_sslsocket();
			test_sslserversocket();
		}
	});

	scheduler.schedule([&]() {
		while (true) {
			sleep(1);
			LOG("xxx");
			EFiber::yield();
		}
	});

	scheduler.schedule([&]() {
		while (true) {
			sleep(5);
			LOG("yyy");
			EFiber::yield();
		}
	});

	scheduler.join();
#endif
}

MAIN_IMPL(testeco) {
	printf("main()\n");

	ESystem::init(argc, argv);

	printf("inited.\n");

	int i = 0;
	try {
		boolean loop = EBoolean::parseBoolean(ESystem::getProgramArgument("loop"));

//		EFiberDebugger::getInstance().debugOn(EFiberDebugger::SCHEDULER);

		do {
//			test_one_thread();
//			test_multi_thread();
//			test_c11schedule();
//			test_nesting();
//			test_channel();
//			test_channel_one_thread();
//			test_channel_multi_thread();
//			test_mutex();
//			test_mutex_multi_thread();
//			test_condition();
//			test_sleep();
//			test_timer();
//			test_local();
//			test_hook_connect1();
//			test_hook_connect2();
//			test_hook_poll();
//			test_hook_select();
//			test_hook_sleep();
//			test_hook_signal(); //todo:
//			test_hook_fcntl();
//			test_hook_nonblocking();
//			test_not_hook_file();
//			test_hook_pipe();
//			test_hook_kqueue();
//			test_hook_gethostbyname();
//			test_nio();
			test_not_hook_file();
//			test_nio();
//			test_sslsocket();
//			test_nio_in_fiber();

//		} while (++i < 5);
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
