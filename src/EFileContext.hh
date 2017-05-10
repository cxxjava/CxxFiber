/*
 * EFileContext.hh
 *
 *  Created on: 2016-5-14
 *      Author: cxxjava@163.com
 */

#ifndef EFILECONTEXT_HH_
#define EFILECONTEXT_HH_

#include "Efc.hh"
#include <vector>

namespace efc {
namespace eco {

/**
 *
 */

class EFileContext: public EObject {
public:
	virtual ~EFileContext();

	EFileContext(int fd);

	int configureNonBlocking();
	int inFD();

	void setSysNonBlock(boolean nonblock);
	boolean isSysNonBlocked();
	void setUserNonBlock(boolean nonblock);
	boolean isUserNonBlocked();

	void setRecvTimeout(const timeval* recvTimeout);
	int getRecvTimeout();
	void setSendTimeout(const timeval* sendTimeout);
	int getSendTimeout();

	static boolean isStreamFile(int fd);
	static boolean isSocketFile(int fd);

private:
	int fd;
	boolean sysNonBlocked;
	boolean userNonBlocked;
	int recvTimeout;
	int sendTimeout;
};

//=============================================================================

/**
 *
 */

/**
 * (FD_DEFAULT_CHUNKS * FD_CHUNK_CAPACITY) is the max fd value.
 */
#define FD_DEFAULT_CHUNKS 32
#define FD_CHUNK_CAPACITY 32768

class EFileContextManager {
public:
	EFileContextManager(int maxfd);
	~EFileContextManager();

	sp<EFileContext> get(int fd);
	void remove(int fd);
	void clear();

private:
	std::vector<std::vector<sp<EFileContext> > > hookedFiles;
};

} /* namespace eco */
} /* namespace efc */
#endif /* EFILECONTEXT_HH_ */
