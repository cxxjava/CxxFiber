/*
 * EFiberLocal.hh
 *
 *  Created on: 2016-5-24
 *      Author: cxxjava@163.com
 */

#ifndef EFIBERLOCAL_HH_
#define EFIBERLOCAL_HH_

#include "./EFiber.hh"

namespace efc {
namespace eco {

/**
 *
 */

typedef void (*fiber_destroyed_callback_t)(void* data);

struct EFiberLocalKeyWrap {
	EFiberLocalKeyWrap(fiber_destroyed_callback_t cb): callback(cb) {}
	fiber_destroyed_callback_t callback;
};

template<typename E>
class EFiberLocal: public EObject {
public:
	EFiberLocal(): keyWrap(null) {
	}
	EFiberLocal(fiber_destroyed_callback_t cb): keyWrap(cb) {
	}

	E get() {
		EFiber* currFiber = EFiber::currentFiber();
		if (!currFiber) {
			return null;
		}
		return (E)(eso_hash_get(currFiber->localValues, &keyWrap, sizeof(this)));
	}

	E set(E e) {
		EFiber* currFiber = EFiber::currentFiber();
		if (!currFiber) {
			return null;
		}
		return (E)(eso_hash_set(currFiber->localValues, &keyWrap, sizeof(this), e));
	}

	E remove() {
		EFiber* currFiber = EFiber::currentFiber();
		if (!currFiber) {
			return null;
		}
		return (E)(eso_hash_set(currFiber->localValues, &keyWrap, sizeof(this), NULL));
	}

private:
	friend class EFiber;

	EFiberLocalKeyWrap keyWrap;
};

} /* namespace eco */
} /* namespace efc */
#endif /* EFIBERLOCAL_HH_ */
