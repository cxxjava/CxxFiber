/*
 * EHooker.cpp
 *
 *  Created on: 2016-5-12
 *      Author: cxxjava@163.com
 */

#include "./EHooker.hh"
#include "./EIoWaiter.hh"
#include "./EFileContext.hh"
#include "../inc/EFiberLocal.hh"
#include "../inc/EFiberScheduler.hh"
#include "eco_ae.h"

#include <dlfcn.h>
#include <poll.h>
#include <signal.h>
#include <unistd.h>
#include <resolv.h>
#include <netdb.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/fcntl.h>
#include <sys/ioctl.h>
#include <sys/uio.h>
#ifdef __linux__
#include <bits/signum.h> //__SIGRTMAX
#include <sys/epoll.h>
#include <sys/sendfile.h>
#endif
#ifdef __APPLE__
#include <sys/event.h>
#endif

namespace efc {
namespace eco {

extern "C" {

typedef void (*sig_t) (int);
typedef void (*signal_t)(int sig, sig_t func);

typedef unsigned int (*sleep_t)(unsigned int seconds);

typedef int (*usleep_t)(useconds_t usec);

typedef int (*nanosleep_t)(const struct timespec *req, struct timespec *rem);

typedef int (*close_t)(int);

typedef int (*fcntl_t)(int fd, int cmd, ...);

typedef int (*ioctl_t)(int fd, unsigned long int request, ...);

typedef int (*setsockopt_t)(int sockfd, int level, int optname,
        const void *optval, socklen_t optlen);

typedef int (*dup2_t)(int oldfd, int newfd);

typedef int (*poll_t)(struct pollfd *fds, nfds_t nfds, int timeout);

typedef int (*select_t)(int nfds, fd_set *readfds, fd_set *writefds,
        fd_set *exceptfds, struct timeval *timeout);

typedef int (*connect_t)(int fd, const struct sockaddr *addr, socklen_t addrlen);

typedef int (*accept_t)(int sockfd, struct sockaddr *addr, socklen_t *addrlen);

typedef ssize_t (*read_t)(int fd, void *buf, size_t count);

typedef ssize_t (*readv_t)(int fd, const struct iovec *iov, int iovcnt);

typedef ssize_t (*recv_t)(int sockfd, void *buf, size_t len, int flags);

typedef ssize_t (*recvfrom_t)(int sockfd, void *buf, size_t len, int flags,
        struct sockaddr *src_addr, socklen_t *addrlen);

typedef ssize_t (*recvmsg_t)(int sockfd, struct msghdr *msg, int flags);

typedef ssize_t (*write_t)(int fd, const void *buf, size_t count);

typedef ssize_t (*writev_t)(int fd, const struct iovec *iov, int iovcnt);

typedef ssize_t (*send_t)(int sockfd, const void *buf, size_t len, int flags);

typedef ssize_t (*sendto_t)(int sockfd, const void *buf, size_t len, int flags,
        const struct sockaddr *dest_addr, socklen_t addrlen);

typedef ssize_t (*sendmsg_t)(int sockfd, const struct msghdr *msg, int flags);

typedef size_t (*fread_t)(void *ptr, size_t size, size_t nitems, FILE *stream);

typedef size_t (*fwrite_t)(const void *ptr, size_t size, size_t nitems, FILE *stream);

typedef ssize_t (*pread_t)(int fd, void *buf, size_t count, off_t offset);

typedef ssize_t (*pwrite_t)(int fd, const void *buf, size_t count, off_t offset);

#ifdef __linux__
typedef int (*dup3_t)(int oldfd, int newfd, int flags);
typedef hostent* (*gethostbyname_t)(const char *name);
typedef res_state (*__res_state_t)();
typedef int (*__poll_t)(struct pollfd fds[], nfds_t nfds, int timeout);

typedef int (*epoll_wait_t)(int epfd, struct epoll_event *events,
                     int maxevents, int timeout);
typedef ssize_t (*sendfile_t)(int out_fd, int in_fd, off_t *offset, size_t count);
#endif

#ifdef __APPLE__
typedef int (*kevent_t)(int kq, const struct kevent *changelist, int nchanges,
		struct kevent *eventlist, int nevents, const struct timespec *timeout);

typedef int (*kevent64_t)(int kq, const struct kevent64_s *changelist,
		int nchanges, struct kevent64_s *eventlist, int nevents,
		unsigned int flags, const struct timespec *timeout);
typedef int (*sendfile_t)(int fd, int s, off_t offset, off_t *len, struct sf_hdtr *hdtr, int flags);
#endif

//=============================================================================

static signal_t signal_f = NULL;
static sleep_t sleep_f = NULL;
static usleep_t usleep_f = NULL;
static nanosleep_t nanosleep_f = NULL;
static close_t close_f = NULL;
/*static*/ fcntl_t fcntl_f = NULL;
static ioctl_t ioctl_f = NULL;
static setsockopt_t setsockopt_f = NULL;
static dup2_t dup2_f = NULL;
static poll_t poll_f = NULL;
static select_t select_f = NULL;
static connect_t connect_f = NULL;
static accept_t accept_f = NULL;
/*static*/ read_t read_f = NULL;
static readv_t readv_f = NULL;
static recv_t recv_f = NULL;
static recvfrom_t recvfrom_f = NULL;
static recvmsg_t recvmsg_f = NULL;
/*static*/ write_t write_f = NULL;
static writev_t writev_f = NULL;
static send_t send_f = NULL;
static sendto_t sendto_f = NULL;
static sendmsg_t sendmsg_f = NULL;
static fread_t fread_f = NULL;
static fwrite_t fwrite_f = NULL;
static pread_t pread_f = NULL;
static pwrite_t pwrite_f = NULL;
static sendfile_t sendfile_f = NULL;
#ifdef __linux__
static dup3_t dup3_f = NULL;
static gethostbyname_t gethostbyname_f = NULL;
static __res_state_t __res_state_f = NULL;
static __poll_t __poll_f = NULL;
/*static*/ epoll_wait_t epoll_wait_f = NULL;
#endif
#ifdef __APPLE__
/*static*/ kevent_t kevent_f = NULL;
static kevent64_t kevent64_f = NULL;
#endif

} //!C

//=============================================================================

//@see: http://docs.oracle.com/cd/E19253-01/819-7050/chapter3-24/

DEFINE_STATIC_INITZZ_BEGIN(EHooker)
signal_f = (signal_t)dlsym(RTLD_NEXT, "signal");
sleep_f = (sleep_t)dlsym(RTLD_NEXT, "sleep");
usleep_f = (usleep_t)dlsym(RTLD_NEXT, "usleep");
nanosleep_f = (nanosleep_t)dlsym(RTLD_NEXT, "nanosleep");
close_f = (close_t)dlsym(RTLD_NEXT, "close");
fcntl_f = (fcntl_t)dlsym(RTLD_NEXT, "fcntl");
ioctl_f = (ioctl_t)dlsym(RTLD_NEXT, "ioctl");
setsockopt_f = (setsockopt_t)dlsym(RTLD_NEXT, "setsockopt");
dup2_f = (dup2_t)dlsym(RTLD_NEXT, "dup2");
poll_f = (poll_t)dlsym(RTLD_NEXT, "poll");
select_f = (select_t)dlsym(RTLD_NEXT, "select");
connect_f = (connect_t)dlsym(RTLD_NEXT, "connect");
accept_f = (accept_t)dlsym(RTLD_NEXT, "accept");
read_f = (read_t)dlsym(RTLD_NEXT, "read");
readv_f = (readv_t)dlsym(RTLD_NEXT, "readv");
recv_f = (recv_t)dlsym(RTLD_NEXT, "recv");
recvfrom_f = (recvfrom_t)dlsym(RTLD_NEXT, "recvfrom");
recvmsg_f = (recvmsg_t)dlsym(RTLD_NEXT, "recvmsg");
write_f = (write_t)dlsym(RTLD_NEXT, "write");
writev_f = (writev_t)dlsym(RTLD_NEXT, "writev");
send_f = (send_t)dlsym(RTLD_NEXT, "send");
sendto_f = (sendto_t)dlsym(RTLD_NEXT, "sendto");
sendmsg_f = (sendmsg_t)dlsym(RTLD_NEXT, "sendmsg");
fread_f = (fread_t)dlsym(RTLD_NEXT, "fread");
fwrite_f = (fwrite_t)dlsym(RTLD_NEXT, "fwrite");
pread_f = (pread_t)dlsym(RTLD_NEXT, "pread");
pwrite_f = (pwrite_t)dlsym(RTLD_NEXT, "pwrite");
sendfile_f = (sendfile_t)dlsym(RTLD_NEXT, "sendfile");
#ifdef __linux__
dup3_f = (dup3_t)dlsym(RTLD_NEXT, "dup3");
gethostbyname_f = (gethostbyname_t)dlsym(RTLD_NEXT, "gethostbyname");
__res_state_f = (__res_state_t)dlsym(RTLD_NEXT,"__res_state");
__poll_f = (__poll_t)dlsym(RTLD_NEXT, "__poll");
epoll_wait_f = (epoll_wait_t)dlsym(RTLD_NEXT, "epoll_wait");
#endif
#ifdef __APPLE__
kevent_f = (kevent_t)dlsym(RTLD_NEXT, "kevent");
kevent64_f = (kevent64_t)dlsym(RTLD_NEXT, "kevent64");
#endif
DEFINE_STATIC_INITZZ_END

//=============================================================================

extern "C" {

static boolean process_signaled = false;
static llong interrupt_escaped_time = 0L;
static EThreadLocalStorage thread_signaled;

#ifndef __SIGRTMAX
#define __SIGRTMAX 64
#endif

static sig_t sigfunc_map[__SIGRTMAX] = {0};

static void sigfunc(int sig_no) {
	process_signaled = true;
	llong t1 = ESystem::currentTimeMillis();

	ES_ASSERT(sig_no < __SIGRTMAX);
	if (sig_no < __SIGRTMAX) {
		sig_t func = sigfunc_map[sig_no];
		if (func) {
			func(sig_no);
		}
	} else {
		fprintf(stderr, "sig_no >= %d\n", __SIGRTMAX);
	}

	llong t2 = ESystem::currentTimeMillis();
	interrupt_escaped_time = t2 - t1;
}

static uint32_t PollEvent2EEvent(short events)
{
    uint32_t e = 0;
    if (events & POLLIN)   e |= ECO_POLL_READABLE;
    if (events & POLLOUT)  e |= ECO_POLL_WRITABLE;
    return e;
}

static short EEvent2PollEvent(uint32_t events)
{
    short e = 0;
    if (events & ECO_POLL_READABLE)  e |= POLLIN;
    if (events & ECO_POLL_WRITABLE) e |= POLLOUT;
    return e;
}

} //!C

//=============================================================================

extern "C" {

sig_t signal(int sig, sig_t func)
{
	EHooker::_initzz_();

	ES_ASSERT(sig < __SIGRTMAX);
	if (sig < __SIGRTMAX) {
		if ((long)func > 512) { //special defined sig_t is always less 512 ?
			sigfunc_map[sig] = func;
			signal_f(sig, sigfunc);
		} else {
			signal_f(sig, func);
		}
		return func;
	}
	fprintf(stderr, "sig_no > %d\n", __SIGRTMAX);
	return NULL;
}

unsigned int sleep(unsigned int seconds)
{
	EHooker::_initzz_();

	EFiberScheduler* scheduler = EFiberScheduler::currentScheduler();
	if (!scheduler) {
		return sleep_f(seconds);
	}

	llong milliseconds = seconds * 1000;
	EFiber::sleep(milliseconds);
	return 0;
}

int usleep(useconds_t usec) {
	EHooker::_initzz_();

	EFiberScheduler* scheduler = EFiberScheduler::currentScheduler();
	if (!scheduler) {
		return usleep_f(usec);
	}

	llong milliseconds = usec / 1000;
	EFiber::sleep(milliseconds);
	return 0;
}

int nanosleep(const struct timespec *req, struct timespec *rem) {
	EHooker::_initzz_();

	if (!req) {
		errno = EINVAL;
		return -1;
	}

	EFiberScheduler* scheduler = EFiberScheduler::currentScheduler();
	if (!scheduler) {
		return nanosleep_f(req, rem);
	}

	llong milliseconds = req->tv_sec * 1000 + req->tv_nsec / 1000000;
	EFiber::sleep(milliseconds);
	return 0;
}

int close(int fd)
{
	EHooker::_initzz_();

	EFiberScheduler* scheduler = EFiberScheduler::currentScheduler();
	if (scheduler && EFileContext::isStreamFile(fd)) {
		scheduler->delFileContext(fd);
	}
	return close_f(fd);
}

int fcntl(int fd, int cmd, ...)
{
	EHooker::_initzz_();

	va_list args;
	va_start(args, cmd);
	void* arg = va_arg(args, void*);
	va_end(args);

	EFiberScheduler* scheduler = EFiberScheduler::currentScheduler();
	if (scheduler && EFileContext::isStreamFile(fd)) {
		sp<EFileContext> fdctx = scheduler->getFileContext(fd);
		if (!fdctx) {
			return -1;
		}

		if (cmd == F_SETFL) {
			int flags = (int)((long)(arg));
			fdctx->setUserNonBlock(flags & O_NONBLOCK);
			return 0;
		}

		if (cmd == F_GETFL) {
			int flags = fcntl_f(fd, cmd);
			if (fdctx->isUserNonBlocked())
				return flags | O_NONBLOCK;
			else
				return flags & ~O_NONBLOCK;
		}
	}

	return fcntl_f(fd, cmd, arg);
}

int ioctl(int fd, unsigned long int request, ...)
{
	EHooker::_initzz_();

	va_list va;
	va_start(va, request);
	void* arg = va_arg(va, void*);
	va_end(va);

	EFiberScheduler* scheduler = EFiberScheduler::currentScheduler();
	if (scheduler && (request == FIONBIO) && EFileContext::isStreamFile(fd)) {
		sp<EFileContext> fdctx = scheduler->getFileContext(fd);
		if (!fdctx) {
			return -1;
		}
		boolean nonblock = !!*(int*)arg;
		fdctx->setUserNonBlock(nonblock);
		return 0;
	}

	return ioctl_f(fd, request, arg);
}

int setsockopt(int sockfd, int level, int optname, const void *optval, socklen_t optlen)
{
	EHooker::_initzz_();

	if (level == SOL_SOCKET) {
		if (optname == SO_RCVTIMEO || optname == SO_SNDTIMEO) {
			EFiberScheduler* scheduler = EFiberScheduler::currentScheduler();
			if (scheduler && EFileContext::isStreamFile(sockfd)) {
				sp<EFileContext> fdctx = scheduler->getFileContext(sockfd);
				if (!fdctx) {
					return -1;
				}
				if (optname == SO_RCVTIMEO)
					fdctx->setRecvTimeout((const timeval*)optval);
				if (optname == SO_SNDTIMEO)
					fdctx->setSendTimeout((const timeval*)optval);
			}
		}
	}

	return setsockopt_f(sockfd, level, optname, optval, optlen);
}

int dup2(int oldfd, int newfd)
{
	EHooker::_initzz_();

	if (oldfd == newfd) {
		return 0;
	}

	EFiberScheduler* scheduler = EFiberScheduler::currentScheduler();
	if (scheduler && EFileContext::isStreamFile(newfd)) {
		scheduler->delFileContext(newfd);
	}

	return dup2_f(oldfd, newfd);
}

#ifdef __linux__
int dup3(int oldfd, int newfd, int flags)
{
	EHooker::_initzz_();

	if (oldfd == newfd) {
		errno = EINVAL;
		return -1;
	}

	EFiberScheduler* scheduler = EFiberScheduler::currentScheduler();
	if (scheduler && EFileContext::isStreamFile(newfd)) {
		scheduler->delFileContext(newfd);
	}
	return dup3_f(oldfd, newfd, flags);
}
#endif

int poll(struct pollfd *fds, nfds_t nfds, int timeout)
{
	EHooker::_initzz_();

	EFiberScheduler* scheduler = EFiberScheduler::currentScheduler();
	if (!scheduler) {
		return poll_f(fds, nfds, timeout);
	}

	if (timeout == 0)
		return poll_f(fds, nfds, timeout);

	boolean invalide_all = true;
	for (nfds_t i = 0; i < nfds; ++i) {
		invalide_all &= (fds[i].fd < 0);
	}
	if (invalide_all) {
		EFiber::sleep(timeout);
		return 0;
	}

#if 1
	// try once at immediately.
	int ret = poll_f(fds, nfds, 0);
	if (ret != 0) {
		return ret;
	}
#endif

	EIoWaiter* ioWaiter = EFiberScheduler::currentIoWaiter();
	sp<EFiber> fiber = EFiber::currentFiber()->shared_from_this();

	boolean none = true;
	for (nfds_t i = 0; i < nfds; ++i) {
		if (fds[i].fd < 0) continue;
		ioWaiter->setFileEvent(fds[i].fd, PollEvent2EEvent(fds[i].events), fiber);
		none = false;
	}

	if (none) {
		errno = 0;
		return nfds;
	}

	llong timerID = -1;
	if (timeout > 0) {
		timerID = ioWaiter->setupTimer(timeout, fiber);
	}

	ioWaiter->swapOut(fiber); // pause the fiber.

	if (timerID != -1) {
		ioWaiter->cancelTimer(timerID);
	}

	if (fiber->isWaitTimeout()) {
		for (nfds_t i = 0; i < nfds; ++i) {
			ioWaiter->delFileEvent(fds[i].fd, ECO_POLL_ALL_EVENTS);
		}
		return 0;
	} else {
		int n = 0;
		for (nfds_t i = 0; i < nfds; ++i) {
			fds[i].revents = EEvent2PollEvent(ioWaiter->getFileEvent(fds[i].fd));
			ioWaiter->delFileEvent(fds[i].fd, ECO_POLL_ALL_EVENTS);
			if (fds[i].revents) n++;
		}
		errno = 0;
		return n;
	}
}

int select(int nfds, fd_set *readfds, fd_set *writefds,
        fd_set *exceptfds, struct timeval *timeout)
{
	EHooker::_initzz_();

	EFiberScheduler* scheduler = EFiberScheduler::currentScheduler();
	if (!scheduler) {
		return select_f(nfds, readfds, writefds, exceptfds, timeout);
	}

	llong milliseconds = ELLong::MAX_VALUE;
	if (timeout)
		milliseconds = timeout->tv_sec * 1000 + timeout->tv_usec / 1000;

	if (milliseconds == 0)
		return select_f(nfds, readfds, writefds, exceptfds, timeout);

	if (nfds == 0 || (!readfds && !writefds && !exceptfds)) {
		EFiber::sleep(milliseconds);
		return 0;
	}

	nfds = ES_MIN(nfds, FD_SETSIZE);

#if 1
	// try once at immediately.
	timeval zero_tv = {0, 0};
	int ret = select_f(nfds, readfds, writefds, exceptfds, &zero_tv);
	if (ret != 0) {
		return ret;
	}
#endif

	//FIXME: to support exceptfds.

	EIoWaiter* ioWaiter = EFiberScheduler::currentIoWaiter();
	sp<EFiber> fiber = EFiber::currentFiber()->shared_from_this();

	EArrayList<int> pfds;
	short events = 0;
	for (int fd = 0; fd < nfds; fd++) {
		if (readfds && FD_ISSET(fd, readfds)) {
			events = ECO_POLL_READABLE;
		}
		if (writefds && FD_ISSET(fd, writefds)) {
			events = ECO_POLL_WRITABLE;
		}

		if (events == 0) {
			continue;
		}

		ioWaiter->setFileEvent(fd, events, fiber);
		pfds.add(fd);
	}

	// clear the old.
	if (readfds) FD_ZERO(readfds);
	if (writefds) FD_ZERO(writefds);
	if (exceptfds) FD_ZERO(exceptfds);

	llong timerID = -1;
	if (milliseconds > 0) {
		timerID = ioWaiter->setupTimer(milliseconds, fiber);
	}

	ioWaiter->swapOut(fiber); // pause the fiber.

	if (timerID != -1) {
		ioWaiter->cancelTimer(timerID);
	}

	if (fiber->isWaitTimeout()) {
		for (int i = 0; i < pfds.size(); i++) {
			int fd = pfds.getAt(i);
			ioWaiter->delFileEvent(fd, ECO_POLL_ALL_EVENTS);
		}
		return 0;
	} else {
		int n = 0;
		for (int i = 0; i < pfds.size(); i++) {
			int fd = pfds.getAt(i);
			events = ioWaiter->getFileEvent(fd);
			if (events) n++;
			if (readfds && (events & ECO_POLL_READABLE)) {
				FD_SET(fd, readfds);
			}
			if (writefds && (events & ECO_POLL_WRITABLE)) {
				FD_SET(fd, writefds);
			}
			ioWaiter->delFileEvent(fd, ECO_POLL_ALL_EVENTS);
		}
		errno = 0;
		return n;
	}
}

int connect(int fd, const struct sockaddr *addr, socklen_t addrlen)
{
	EHooker::_initzz_();

	EFiberScheduler* scheduler = EFiberScheduler::currentScheduler();
	if (!scheduler /*|| !EFileContext::isStreamFile(fd)*/) {
		return connect_f(fd, addr, addrlen);
	}

	sp<EFileContext> fdctx = scheduler->getFileContext(fd);
	if (!fdctx) {
		//errno = EBADF;
		return -1;
		//return connect_f(fd, addr, addrlen);
	}
	if (fdctx->isUserNonBlocked()) {
		return connect_f(fd, addr, addrlen);
	}

	int n = connect_f(fd, addr, addrlen);
	if (n == 0) {
		// success immediately
		return 0;
	} else if (n == -1 && errno == EINPROGRESS) {
		// waiting
		EIoWaiter* ioWaiter = EFiberScheduler::currentIoWaiter();
		sp<EFiber> fiber = EFiber::currentFiber()->shared_from_this();
		ioWaiter->setFileEvent(fd, ECO_POLL_WRITABLE, fiber);
		ioWaiter->swapOut(fiber); // pause the fiber.
		ioWaiter->delFileEvent(fd, ECO_POLL_WRITABLE);

		// re-check
		int err = 0;
		socklen_t optlen = sizeof(int);
		if (getsockopt(fd, SOL_SOCKET, SO_ERROR, (void *)&err, &optlen) == -1) {
			return -1;
		}

		if (!err)
			return 0;
		else {
			errno = err;
			return -1;
		}
	} else {
		// error
		return n;
	}
}

int accept(int sockfd, struct sockaddr *addr, socklen_t *addrlen)
{
#ifdef CPP11_SUPPORT
	return EHooker::comm_io_on_fiber(accept_f, "accept", POLLIN, sockfd, sockfd, addr, addrlen);
#else
	return EHooker::comm_io_on_fiber(accept_f, "accept", POLLIN, sockfd, addr, addrlen);
#endif
}

ssize_t read(int fd, void *buf, size_t count)
{
#ifdef CPP11_SUPPORT
	return EHooker::comm_io_on_fiber(read_f, "read", POLLIN, fd, fd, buf, count);
#else
	return EHooker::comm_io_on_fiber(read_f, "read", POLLIN, fd, buf, count);
#endif
}

ssize_t readv(int fd, const struct iovec *iov, int iovcnt)
{
#ifdef CPP11_SUPPORT
	return EHooker::comm_io_on_fiber(readv_f, "readv", POLLIN, fd, fd, iov, iovcnt);
#else
	return EHooker::comm_io_on_fiber(readv_f, "readv", POLLIN, fd, iov, iovcnt);
#endif
}

ssize_t recv(int sockfd, void *buf, size_t len, int flags)
{
#ifdef CPP11_SUPPORT
	return EHooker::comm_io_on_fiber(recv_f, "recv", POLLIN, sockfd, sockfd, buf, len, flags);
#else
	return EHooker::comm_io_on_fiber(recv_f, "recv", POLLIN, sockfd, buf, len, flags);
#endif
}

ssize_t recvfrom(int sockfd, void *buf, size_t len, int flags,
        struct sockaddr *src_addr, socklen_t *addrlen)
{
#ifdef CPP11_SUPPORT
	return EHooker::comm_io_on_fiber(recvfrom_f, "recvfrom", POLLIN, sockfd, sockfd, buf, len, flags, src_addr, addrlen);
#else
	return EHooker::comm_io_on_fiber(recvfrom_f, "recvfrom", POLLIN, sockfd, buf, len, flags, src_addr, addrlen);
#endif
}

ssize_t recvmsg(int sockfd, struct msghdr *msg, int flags)
{
#ifdef CPP11_SUPPORT
	return EHooker::comm_io_on_fiber(recvmsg_f, "recvmsg", POLLIN, sockfd, sockfd, msg, flags);
#else
	return EHooker::comm_io_on_fiber(recvmsg_f, "recvmsg", POLLIN, sockfd, msg, flags);
#endif
}

ssize_t write(int fd, const void *buf, size_t count)
{
#ifdef CPP11_SUPPORT
	return EHooker::comm_io_on_fiber(write_f, "write", POLLOUT, fd, fd, buf, count);
#else
	return EHooker::comm_io_on_fiber(write_f, "write", POLLOUT, fd, buf, count);
#endif
}

ssize_t writev(int fd, const struct iovec *iov, int iovcnt)
{
#ifdef CPP11_SUPPORT
	return EHooker::comm_io_on_fiber(writev_f, "writev", POLLOUT, fd, fd, iov, iovcnt);
#else
	return EHooker::comm_io_on_fiber(writev_f, "writev", POLLOUT, fd, iov, iovcnt);
#endif
}

ssize_t send(int sockfd, const void *buf, size_t len, int flags)
{
#ifdef CPP11_SUPPORT
	return EHooker::comm_io_on_fiber(send_f, "send", POLLOUT, sockfd, sockfd, buf, len, flags);
#else
	return EHooker::comm_io_on_fiber(send_f, "send", POLLOUT, sockfd, buf, len, flags);
#endif
}

ssize_t sendto(int sockfd, const void *buf, size_t len, int flags,
        const struct sockaddr *dest_addr, socklen_t addrlen)
{
#ifdef CPP11_SUPPORT
	return EHooker::comm_io_on_fiber(sendto_f, "sendto", POLLOUT, sockfd, sockfd, buf, len, flags, dest_addr, addrlen);
#else
	return EHooker::comm_io_on_fiber(sendto_f, "sendto", POLLOUT, sockfd, buf, len, flags, dest_addr, addrlen);
#endif
}

ssize_t sendmsg(int sockfd, const struct msghdr *msg, int flags)
{
#ifdef CPP11_SUPPORT
	return EHooker::comm_io_on_fiber(sendmsg_f, "sendmsg", POLLOUT, sockfd, sockfd, msg, flags);
#else
	return EHooker::comm_io_on_fiber(sendmsg_f, "sendmsg", POLLOUT, sockfd, msg, flags);
#endif
}

ssize_t pread(int fd, void *buf, size_t count, off_t offset)
{
#ifdef CPP11_SUPPORT
	return EHooker::comm_io_on_fiber(pread_f, "pread", POLLIN, fd, fd, buf, count, offset);
#else
	return EHooker::comm_io_on_fiber(pread_f, "pread", POLLIN, fd, buf, count, offset);
#endif
}

ssize_t pwrite(int fd, const void *buf, size_t count, off_t offset)
{
#ifdef CPP11_SUPPORT
	return EHooker::comm_io_on_fiber(pwrite_f, "pwrite", POLLOUT, fd, fd, buf, count, offset);
#else
	return EHooker::comm_io_on_fiber(pwrite_f, "pwrite", POLLOUT, fd, buf, count, offset);
#endif
}

size_t fread(void *ptr, size_t size, size_t nitems, FILE *stream)
{
	return EHooker::comm_io_on_fiber(fread_f, "fread", POLLIN, eso_fileno(stream), ptr, size, nitems, stream);
}

size_t fwrite(const void *ptr, size_t size, size_t nitems, FILE *stream)
{
	return EHooker::comm_io_on_fiber(fwrite_f, "fwrite", POLLOUT, eso_fileno(stream), ptr, size, nitems, stream);
}

//=============================================================================

#ifdef __linux__
struct hostbuf_wrap
{
	struct hostent host;
	char* buffer;
	size_t iBufferSize;
	int host_errno;
};

static void fiber_destroyed_callback_to_free_res_state(void* data) {
	if (data) {
		res_state w = (res_state)data;
		if (w) free(w);
	}
}
static void fiber_destroyed_callback_to_free_hostbuf_wrap(void* data) {
	if (data) {
		hostbuf_wrap* w = (hostbuf_wrap*)data;
		if (w->buffer) free(w->buffer);
		free(w);
	}
}
static EFiberLocal<res_state> res_state_local(fiber_destroyed_callback_to_free_res_state);
static EFiberLocal<hostbuf_wrap*> hostbuf_wrap_local(fiber_destroyed_callback_to_free_hostbuf_wrap);

res_state __res_state()
{
	EHooker::_initzz_();

	EFiberScheduler* scheduler = EFiberScheduler::currentScheduler();
	if (!scheduler) {
		return __res_state_f();
	}

	res_state rs = res_state_local.get();
	if (!rs) {
		rs = (res_state)malloc(sizeof(struct __res_state));
		res_state_local.set(rs);
	}
	return rs;
}

struct hostent *gethostbyname(const char *name)
{
	if (!name) {
		return NULL;
	}

	EHooker::_initzz_();

	EFiberScheduler* scheduler = EFiberScheduler::currentScheduler();
	if (!scheduler) {
		return gethostbyname_f(name);
	}

	hostbuf_wrap* hw = hostbuf_wrap_local.get();
	if (!hw) {
		hw = (hostbuf_wrap*)calloc(1, sizeof(hostbuf_wrap));
		hostbuf_wrap_local.set(hw);
	}
	if (hw->buffer && hw->iBufferSize > 1024) {
		free(hw->buffer);
		hw->buffer = NULL;
	}
	if (!hw->buffer) {
		hw->buffer = (char*)malloc(1024);
		hw->iBufferSize = 1024;
	}

	struct hostent *host = &hw->host;
	struct hostent *result = NULL;
	int *h_errnop = &(hw->host_errno);

	RETRY:
#ifdef __GLIBC__
	gethostbyname_r(name, host, hw->buffer, hw->iBufferSize, &result, h_errnop);
#else
	result = gethostbyname_r(name, host, hw->buffer, hw->iBufferSize, h_errnop);
#endif
	while (result == NULL && errno == ERANGE) {
		hw->iBufferSize = hw->iBufferSize * 2;
		hw->buffer = (char*)realloc(hw->buffer, hw->iBufferSize);
		goto RETRY;
	}
	return result;
}

int __poll(struct pollfd fds[], nfds_t nfds, int timeout)
{
	return poll(fds, nfds, timeout);
}

int epoll_wait(int epfd, struct epoll_event *events,
                     int maxevents, int timeout)
{
	EHooker::_initzz_();

	EFiberScheduler* scheduler = EFiberScheduler::currentScheduler();
	if (!scheduler || timeout == 0) {
		return epoll_wait_f(epfd, events, maxevents, timeout);
	}

	// waiting for events signal

	EIoWaiter* ioWaiter = EFiberScheduler::currentIoWaiter();
	sp<EFiber> fiber = EFiber::currentFiber()->shared_from_this();

	ioWaiter->setFileEvent(epfd, ECO_POLL_READABLE, fiber);

	llong milliseconds = (timeout > 0) ? timeout : EInteger::MAX_VALUE;
	llong timerID = ioWaiter->setupTimer(milliseconds, fiber);

	ioWaiter->swapOut(fiber); // pause the fiber.

	ioWaiter->cancelTimer(timerID);
	ioWaiter->delFileEvent(epfd, ECO_POLL_ALL_EVENTS);

	if (fiber->isWaitTimeout()) {
		errno = EINVAL;
		return -1;
	} else {
		return epoll_wait_f(epfd, events, maxevents, 0);
	}
}

ssize_t sendfile(int out_fd, int in_fd, off_t *offset, size_t count)
{
	EHooker::_initzz_();

	EFiberScheduler* scheduler = EFiberScheduler::currentScheduler();
	if (!scheduler || !EFileContext::isStreamFile(out_fd)) {
		return sendfile_f(out_fd, in_fd, offset, count);
	}

	sp<EFileContext> fdctx = scheduler->getFileContext(out_fd);
	if (!fdctx) {
		return -1;
	}

	boolean isUNB = fdctx->isUserNonBlocked();
	if (isUNB) {
		return sendfile_f(out_fd, in_fd, offset, count);
	}

	int milliseconds = fdctx->getSendTimeout();
	if (milliseconds == 0) milliseconds = -1;
	off_t offset_ = offset ? *offset : 0;

RETRY:
	pollfd pfd;
	pfd.fd = out_fd;
	pfd.events = POLLOUT;
	pfd.revents = 0;

	ssize_t ret = poll(&pfd, 1, milliseconds);
	if (ret == 1) { //success
		ret = sendfile_f(out_fd, in_fd, &offset_, count);
		if (ret > 0) count -= ret;
		if ((ret >= 0 && count > 0)
				|| (ret == -1 && (errno == EAGAIN || errno == EWOULDBLOCK))) {
			if (count > 0) goto RETRY;
		}
	} else if (ret == 0) { //timeout
		ret = -1;
		errno = isUNB ? EAGAIN : ETIMEDOUT;
	}

	if (offset) *offset = offset_;
	if (ret == 0) errno = 0;

	return ret;
}

#else // __APPLE__

// gethostbyname() is asynchronous already, @see libinfo/mdns_module.c:_mdns_search():kqueue!

int kevent(int kq, const struct kevent *changelist, int nchanges,
		struct kevent *eventlist, int nevents, const struct timespec *timeout)
{
	EHooker::_initzz_();

	EFiberScheduler* scheduler = EFiberScheduler::currentScheduler();
	if (!scheduler) {
		return kevent_f(kq, changelist, nchanges, eventlist, nevents, timeout);
	}

	if ((timeout && timeout->tv_sec == 0 && timeout->tv_nsec == 0) || !eventlist || nevents == 0) {
		// try at immediately.
		return kevent_f(kq, changelist, nchanges, eventlist, nevents, timeout);
	}

	// waiting for events signal

	if (changelist && nchanges > 0) {
		kevent_f(kq, changelist, nchanges, NULL, 0, NULL);
	}

	EIoWaiter* ioWaiter = EFiberScheduler::currentIoWaiter();
	sp<EFiber> fiber = EFiber::currentFiber()->shared_from_this();

	ioWaiter->setFileEvent(kq, ECO_POLL_READABLE, fiber);

	llong milliseconds = timeout ? (timeout->tv_sec * 1000 + timeout->tv_nsec / 1000000) : EInteger::MAX_VALUE;
	llong timerID = ioWaiter->setupTimer(milliseconds, fiber);

	ioWaiter->swapOut(fiber); // pause the fiber.

	ioWaiter->cancelTimer(timerID);
	ioWaiter->delFileEvent(kq, ECO_POLL_ALL_EVENTS);

	if (fiber->isWaitTimeout()) {
		errno = EINVAL;
		return -1;
	} else {
		struct timespec zerotv = {0, 0};
		return kevent_f(kq, NULL, 0, eventlist, nevents, &zerotv);
	}
}

int kevent64(int kq, const struct kevent64_s *changelist, int nchanges,
		struct kevent64_s *eventlist, int nevents, unsigned int flags,
		const struct timespec *timeout)
{
	EHooker::_initzz_();

	EFiberScheduler* scheduler = EFiberScheduler::currentScheduler();
	if (!scheduler) {
		return kevent64_f(kq, changelist, nchanges, eventlist, nevents, flags, timeout);
	}

	if ((timeout && timeout->tv_sec == 0 && timeout->tv_nsec == 0) || !eventlist || nevents == 0) {
		// try once at immediately.
		return kevent64_f(kq, changelist, nchanges, eventlist, nevents, flags, timeout);
	}

	// waiting for events signal

	if (changelist && nchanges > 0) {
		kevent64_f(kq, changelist, nchanges, NULL, 0, flags, NULL);
	}

	EIoWaiter* ioWaiter = EFiberScheduler::currentIoWaiter();
	sp<EFiber> fiber = EFiber::currentFiber()->shared_from_this();

	ioWaiter->setFileEvent(kq, ECO_POLL_READABLE, fiber);

	llong milliseconds = timeout ? (timeout->tv_sec * 1000 + timeout->tv_nsec / 1000000) : EInteger::MAX_VALUE;
	llong timerID = ioWaiter->setupTimer(milliseconds, fiber);

	ioWaiter->swapOut(fiber); // pause the fiber.

	ioWaiter->cancelTimer(timerID);
	ioWaiter->delFileEvent(kq, ECO_POLL_ALL_EVENTS);

	if (fiber->isWaitTimeout()) {
		errno = EINVAL;
		return -1;
	} else {
		struct timespec zerotv = {0, 0};
		return kevent64_f(kq, NULL, 0, eventlist, nevents, flags, &zerotv);
	}
}

int sendfile(int fd, int s, off_t offset, off_t *len, struct sf_hdtr *hdtr, int flags)
{
	EHooker::_initzz_();

	EFiberScheduler* scheduler = EFiberScheduler::currentScheduler();
	if (!scheduler || !EFileContext::isStreamFile(s)) {
		return sendfile_f(fd, s, offset, len, hdtr, flags);
	}

	sp<EFileContext> fdctx = scheduler->getFileContext(s);
	if (!fdctx) {
		return -1;
	}

	boolean isUNB = fdctx->isUserNonBlocked();
	if (isUNB) {
		return sendfile_f(fd, s, offset, len, hdtr, flags);
	}

	int milliseconds = fdctx->getSendTimeout();
	if (milliseconds == 0) milliseconds = -1;
	off_t inlen = len ? *len : 0;
	off_t outlen;

RETRY:
	pollfd pfd;
	pfd.fd = s;
	pfd.events = POLLOUT;
	pfd.revents = 0;

	int ret = poll(&pfd, 1, milliseconds);
	if (ret == 1) { //success
		outlen = inlen;
		ret = sendfile_f(fd, s, offset, &outlen, hdtr, flags);
		if (ret == -1 && (errno == EAGAIN || errno == EWOULDBLOCK)) {
			if (outlen > 0) {
				offset += outlen;
				inlen -= outlen;
			}
			if (inlen > 0) goto RETRY;
		}
	} else if (ret == 0) { //timeout
		ret = -1;
		errno = isUNB ? EAGAIN : ETIMEDOUT;
	}

	if (ret == 0) errno = 0;

	return ret;
}

#endif

} //!C

//=============================================================================

boolean EHooker::isInterrupted(boolean ClearInterrupted) {
	boolean r = process_signaled;
	if (ClearInterrupted) {
		process_signaled = false;
	}
	return r;
}

llong EHooker::interruptEscapedTime() {
	return interrupt_escaped_time;
}

#ifdef CPP11_SUPPORT

template <typename F, typename ... Args>
ssize_t EHooker::comm_io_on_fiber(F fn, const char* name, int event, int fd, Args&&... args) {
	EHooker::_initzz_();

	EFiberScheduler* scheduler = EFiberScheduler::currentScheduler();
	if (!scheduler || !EFileContext::isStreamFile(fd)) {
		return fn(std::forward<Args>(args)...);
	}

	sp<EFileContext> fdctx = scheduler->getFileContext(fd);
	if (!fdctx) {
		return -1;
	}

	boolean isUNB = fdctx->isUserNonBlocked();
	if (isUNB) {
		return fn(std::forward<Args>(args)...);
	}

	ssize_t ret = -1;

	if ((event & POLLOUT) == POLLOUT) {
		// try once at immediately.
		ret = fn(std::forward<Args>(args)...);
		if (ret >= 0) { //success?
			goto SUCCESS;
		}
	}

	// try io wait.
	{
		int milliseconds = (event == POLLIN) ? fdctx->getRecvTimeout() : fdctx->getSendTimeout();
		if (milliseconds == 0) milliseconds = -1;

RETRY:
		pollfd pfd;
		pfd.fd = fd;
		pfd.events = event;
		pfd.revents = 0;

		ret = poll(&pfd, 1, milliseconds);
		if (ret == 1) { //success
			ret = fn(std::forward<Args>(args)...);
			if (ret == -1 && (errno == EAGAIN || errno == EWOULDBLOCK)) {
				goto RETRY;
			}
	    } else if (ret == 0) { //timeout
	        ret = -1;
	        errno = isUNB ? EAGAIN : ETIMEDOUT;
	    }
	}

SUCCESS:
	if (ret >= 0 && ((intptr_t)fn == (intptr_t)accept_f)) {
		// if listen socket is non-blocking then accepted socket also non-blocking,
		// we need reset it to back.
		int socket = (int)ret;
		int flags = fcntl_f(socket, F_GETFL);
		fcntl_f(socket, F_SETFL, flags & ~O_NONBLOCK);
	}

	return ret;
}

#else //!

template <typename F>
static ssize_t call_fn(EFileContext* fdctx, F fn, int fd, va_list _args) {
	ssize_t ret = -1;
	va_list args;
#ifdef va_copy
	va_copy(args, _args);
#else
	args = _args;
#endif

	if ((intptr_t)fn == (intptr_t)accept_f) {
		struct sockaddr* addr = va_arg(args, struct sockaddr*);
		socklen_t* addrlen = va_arg(args, socklen_t*);
		int socket = accept_f(fd, addr, addrlen);
		if (socket >= 0 && fdctx != null && !fdctx->isUserNonBlocked()) {
			// if listen socket is non-blocking then accepted socket also non-blocking,
			// we need reset it to back.
			int flags = fcntl_f(socket, F_GETFL);
			fcntl_f(socket, F_SETFL, flags & ~O_NONBLOCK);
		}
		ret = socket;
	}
	else if ((intptr_t)fn == (intptr_t)read_f) {
		void* buf = va_arg(args, void*);
		size_t count = va_arg(args, size_t);
		ret = read_f(fd, buf, count);
	}
	else if ((intptr_t)fn == (intptr_t)readv_f) {
		struct iovec* iov = va_arg(args, struct iovec*);
		int iovcnt = va_arg(args, int);
		ret = readv_f(fd, iov, iovcnt);
	}
	else if ((intptr_t)fn == (intptr_t)recv_f) {
		void *buf = va_arg(args, void*);
		size_t len = va_arg(args, size_t);
		int flags = va_arg(args, int);
		ret = recv_f(fd, buf, len, flags);
	}
	else if ((intptr_t)fn == (intptr_t)recvfrom_f) {
		void *buf = va_arg(args, void*);
		size_t len = va_arg(args, size_t);
		int flags = va_arg(args, int);
		struct sockaddr *src_addr = va_arg(args, struct sockaddr*);
		socklen_t *addrlen = va_arg(args, socklen_t*);
		ret = recvfrom_f(fd, buf, len, flags, src_addr, addrlen);
	}
	else if ((intptr_t)fn == (intptr_t)recvmsg_f) {
		struct msghdr *msg = va_arg(args, struct msghdr*);
		int flags = va_arg(args, int);
		ret = recvmsg_f(fd, msg, flags);
	}
	else if ((intptr_t)fn == (intptr_t)write_f) {
		void* buf = va_arg(args, void*);
		size_t count = va_arg(args, size_t);
		ret = write_f(fd, buf, count);
	}
	else if ((intptr_t)fn == (intptr_t)writev_f) {
		struct iovec* iov = va_arg(args, struct iovec*);
		int iovcnt = va_arg(args, int);
		ret = writev_f(fd, iov, iovcnt);
	}
	else if ((intptr_t)fn == (intptr_t)send_f) {
		void *buf = va_arg(args, void*);
		size_t len = va_arg(args, size_t);
		int flags = va_arg(args, int);
		ret = send_f(fd, buf, len, flags);
	}
	else if ((intptr_t)fn == (intptr_t)sendto_f) {
		void *buf = va_arg(args, void*);
		size_t len = va_arg(args, size_t);
		int flags = va_arg(args, int);
		struct sockaddr *dest_addr = va_arg(args, struct sockaddr*);
		socklen_t addrlen = va_arg(args, socklen_t);
		ret = sendto_f(fd, buf, len, flags, dest_addr, addrlen);
	}
	else if ((intptr_t)fn == (intptr_t)sendmsg_f) {
		struct msghdr *msg = va_arg(args, struct msghdr*);
		int flags = va_arg(args, int);
		ret = sendmsg_f(fd, msg, flags);
	}
	else if ((intptr_t)fn == (intptr_t)fread_f) { //fread
		void *ptr = va_arg(args, void*);
		size_t size = va_arg(args, size_t);
		size_t nitems = va_arg(args, size_t);
		FILE *stream  = va_arg(args, FILE*);
		ret = fread_f(ptr, size, nitems, stream);
	}
	else if ((intptr_t)fn == (intptr_t)fwrite_f) { //fwrite
		void *ptr = va_arg(args, void*);
		size_t size = va_arg(args, size_t);
		size_t nitems = va_arg(args, size_t);
		FILE *stream  = va_arg(args, FILE*);
		ret = fwrite_f(ptr, size, nitems, stream);
	}
	else if ((intptr_t)fn == (intptr_t)pread_f) {
		void *buf = va_arg(args, void*);
		size_t count = va_arg(args, size_t);
		off_t offset = va_arg(args, off_t);
		ret = pread_f(fd, buf, count, offset);
	}
	else if ((intptr_t)fn == (intptr_t)pwrite_f) {
		void *buf = va_arg(args, void*);
		size_t count = va_arg(args, size_t);
		off_t offset = va_arg(args, off_t);
		ret = pwrite_f(fd, buf, count, offset);
	}
	else {
#ifdef va_copy
		va_end(args);
#endif
		throw EUnsupportedOperationException(__FILE__, __LINE__);
	}

#ifdef va_copy
	va_end(args);
#endif
	return ret;
}

template <typename F>
ssize_t EHooker::comm_io_on_fiber(F fn, const char* name, int event, int fd, ...) {
	EHooker::_initzz_();

	ssize_t ret;
	va_list args;

	va_start(args, fd);

	EFiberScheduler* scheduler = EFiberScheduler::currentScheduler();
	if (!scheduler || !EFileContext::isStreamFile(fd)) {
		ret = call_fn(null, fn, fd, args);
		va_end(args);
		return ret;
	}

	sp<EFileContext> fdctx = scheduler->getFileContext(fd);
	if (!fdctx) {
		va_end(args);
		return -1;
	}

	boolean isUNB = fdctx->isUserNonBlocked();
	if (isUNB) {
		ret = call_fn(null, fn, fd, args);
		va_end(args);
		return ret;
	}

	if ((event & POLLOUT) == POLLOUT) {
		// try once at immediately.
		ret = call_fn(fdctx.get(), fn, fd, args);
		if (ret >= 0) { //success?
			va_end(args);
			return ret;
		}
	}

	int milliseconds = (event == POLLIN) ? fdctx->getRecvTimeout() : fdctx->getSendTimeout();
	if (milliseconds == 0) milliseconds = -1;

RETRY:
	pollfd pfd;
	pfd.fd = fd;
	pfd.events = event;
	pfd.revents = 0;

	ret = poll(&pfd, 1, milliseconds);
	if (ret == 1) { //success
		ret = call_fn(fdctx.get(), fn, fd, args);
		if (ret == -1 && (errno == EAGAIN || errno == EWOULDBLOCK)) {
			goto RETRY;
		}
    } else if (ret == 0) { //timeout
        ret = -1;
        errno = isUNB ? EAGAIN : ETIMEDOUT;
    }

	va_end(args);
	return ret;
}

#endif //!CPP11_SUPPORT

} /* namespace eco */
} /* namespace efc */
