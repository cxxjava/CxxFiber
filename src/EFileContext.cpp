/*
 * EFileContext.cpp
 *
 *  Created on: 2016-5-14
 *      Author: cxxjava@163.com
 */

#include "EFileContext.hh"
#include "EFiber.hh"
#include "EHooker.hh"

#include <sys/fcntl.h>
#include <sys/stat.h>

namespace efc {
namespace eco {

extern "C" {
typedef int (*fcntl_t)(int fd, int cmd, ...);
extern fcntl_t fcntl_f;
} //!C

EFileContext::~EFileContext() {
	EFiber* fiber = EFiber::currentFiber();
	if (!fiber) {
		int flags = fcntl_f(fd, F_GETFL);
		if ((userNonBlocked == true) && !(flags & O_NONBLOCK))
			fcntl_f(fd, F_SETFL, flags | O_NONBLOCK);
		else if ((userNonBlocked == false) && (flags & O_NONBLOCK))
			fcntl_f(fd, F_SETFL, flags & ~O_NONBLOCK);
	}
}

EFileContext::EFileContext(int fd): fd(fd),
		sysNonBlocked(false),
		userNonBlocked(false),
		recvTimeout(0),
		sendTimeout(0) {
	// set non blocking
	int flags = fcntl_f(fd, F_GETFL, 0);
	if (flags == -1) { //error
		throw EFileNotFoundException(__FILE__, __LINE__);
	}
	if ((flags & O_NONBLOCK)) {
		userNonBlocked = true;
	} else {
		fcntl_f(fd, F_SETFL, flags | O_NONBLOCK);
		sysNonBlocked = true;
	}

	if (isSocketFile(fd)) {
		// get SO_RCVTIMEO | SO_SNDTIMEO
		struct timeval tv;
		socklen_t len = sizeof(tv);
		int rv = getsockopt(fd, SOL_SOCKET, SO_RCVTIMEO, &tv, &len);
		if (rv == -1) {
			throw EFileNotFoundException(__FILE__, __LINE__);
		}
		recvTimeout = tv.tv_sec * 1000 + tv.tv_usec / 1000;
		len = sizeof(tv);
		rv = getsockopt(fd, SOL_SOCKET, SO_SNDTIMEO, &tv, &len);
		sendTimeout = tv.tv_sec * 1000 + tv.tv_usec / 1000;
	} else {
		recvTimeout = EInteger::MAX_VALUE;
		sendTimeout = EInteger::MAX_VALUE;
	}
}

int EFileContext::configureNonBlocking() {
	int flags = fcntl_f(fd, F_GETFL);
	return fcntl_f(fd, F_SETFL, flags | O_NONBLOCK);
}

int EFileContext::inFD() {
	return fd;
}

void EFileContext::setSysNonBlock(boolean nonblock) {
	sysNonBlocked = nonblock;
}

boolean EFileContext::isSysNonBlocked() {
	return sysNonBlocked;
}

void EFileContext::setUserNonBlock(boolean nonblock) {
	userNonBlocked = nonblock;
}

boolean EFileContext::isUserNonBlocked() {
	return userNonBlocked;
}

void EFileContext::setRecvTimeout(const timeval* recvTimeout) {
	this->recvTimeout = recvTimeout->tv_sec * 1000 + recvTimeout->tv_usec / 1000;
}

int EFileContext::getRecvTimeout() {
	return (recvTimeout == 0) ? EInteger::MAX_VALUE : recvTimeout;
}

void EFileContext::setSendTimeout(const timeval* sendTimeout) {
	this->sendTimeout = sendTimeout->tv_sec * 1000 + sendTimeout->tv_usec / 1000;
}

int EFileContext::getSendTimeout() {
	return (sendTimeout == 0) ? EInteger::MAX_VALUE : sendTimeout;
}

boolean EFileContext::isStreamFile(int fd) {
	int mode;
	struct stat64 buf;

	if (::fstat64(fd, &buf) >= 0) {
		mode = buf.st_mode;
		if (S_ISFIFO(mode) || S_ISSOCK(mode)) {
			return true;
		}
	}

	return false;
}

boolean EFileContext::isSocketFile(int fd) {
	int mode;
	struct stat64 buf;

	if (::fstat64(fd, &buf) >= 0) {
		mode = buf.st_mode;
		if (S_ISSOCK(mode)) {
			return true;
		}
	}

	return false;
}

//=============================================================================

/**
 * (FD_DEFAULT_CHUNKS * FD_CHUNK_CAPACITY) is the max fd value.
 */
#define FD_DEFAULT_CHUNKS 32
#define FD_CHUNK_CAPACITY 32768

EFileContextManager::EFileContextManager() :
		hookedFiles(
				(ES_ALIGN_UP(fdLimit(FD_DEFAULT_CHUNKS*FD_CHUNK_CAPACITY), FD_CHUNK_CAPACITY)
						/ FD_CHUNK_CAPACITY),
				std::vector < sp<EFileContext> > (FD_CHUNK_CAPACITY)) {
	//
}

EFileContextManager::~EFileContextManager() {
	//
}

sp<EFileContext> EFileContextManager::get(int fd) {
	SpinLock* lock = LOCKFOR((void*)fd);
	int index0 = ES_ALIGN_UP(fd, FD_CHUNK_CAPACITY) / FD_CHUNK_CAPACITY - 1;
	sp<EFileContext> fc;
	lock->lock();
	int index1 = fd % FD_CHUNK_CAPACITY - 1;
	fc = hookedFiles[index0][index1];
	if (fc == null) {
		fc = hookedFiles[index0][index1] = new EFileContext(fd);
	}
	lock->unlock();
	return fc;
}

void EFileContextManager::remove(int fd) {
	SpinLock* lock = LOCKFOR((void*)fd);
	int index0 = ES_ALIGN_UP(fd, FD_CHUNK_CAPACITY) / FD_CHUNK_CAPACITY - 1;
	sp<EFileContext> fc;
	lock->lock();
	int index1 = fd % FD_CHUNK_CAPACITY - 1;
	fc = hookedFiles[index0][index1];
	if (fc != null) {
		fc.reset();
	}
	lock->unlock();
}

void EFileContextManager::clear() {
	hookedFiles.clear();
}

} /* namespace eco */
} /* namespace efc */
