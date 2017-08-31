/*
 * EFiberChannel.hh
 *
 *  Created on: 2016-5-5
 *      Author: cxxjava@163.com
 */

#ifndef EFIBER_CHANNEL_HH_
#define EFIBER_CHANNEL_HH_

#include "./EFiber.hh"
#include "./EFiberBlocker.hh"

namespace efc {
namespace eco {

/**
 *
 */

template<typename E>
class EFiberChannel: public EObject {
public:
	virtual ~EFiberChannel() {
	}

	EFiberChannel(int capacity = 0): writer(capacity) {
	}

	sp<E> read() {
		writer.wakeUp();
		reader.wait();
		return dataQueue.poll();
	}

	void write(sp<E> o) {
		writer.wait();
		dataQueue.add(o);
		reader.wakeUp();
	}

	boolean tryRead(sp<E>& o) {
		writer.wakeUp();
		while (!reader.tryWait()) {
			if (writer.tryWait()) {
				return false;
			} else {
				EFiber::yield();
			}
		}
		o = dataQueue.poll();
		return true;
	}

	boolean tryWrite(sp<E> o) {
		if (!writer.tryWait())
			return false;
		dataQueue.add(o);
		reader.wakeUp();
		return true;
	}

	boolean isEmpty() {
		return !reader.isWaking();
	}

private:
	EConcurrentLiteQueue<E> dataQueue;
	EFiberBlocker reader;
	EFiberBlocker writer;
};

} /* namespace eco */
} /* namespace efc */
#endif /* EFIBER_CHANNEL_HH_ */
