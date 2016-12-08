/*
 * EFileContext.hh
 *
 *  Created on: 2016-5-14
 *      Author: cxxjava@163.com
 */

#ifndef EFILECONTEXT_HH_
#define EFILECONTEXT_HH_

#include "EObject.hh"

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

} /* namespace eco */
} /* namespace efc */
#endif /* EFILECONTEXT_HH_ */
