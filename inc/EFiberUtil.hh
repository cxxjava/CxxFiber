/*
 * EFiberUtil.hh
 *
 *  Created on: 2016-6-5
 *      Author: cxxjava@163.com
 */

#ifndef EFIBERUTIL_HH_
#define EFIBERUTIL_HH_

#include "Efc.hh"

#ifdef CPP11_SUPPORT
#include <atomic>
#endif
#include <sys/resource.h>

namespace efc {
namespace eco {

//=============================================================================

static long fdLimit(int deflim) {
	struct rlimit rlim;
	if (getrlimit(RLIMIT_NOFILE, &rlim) < 0)
		return -1;
	long limit = rlim.rlim_cur;
	return (limit < 0 ? deflim : rlim.rlim_cur);
}

//=============================================================================

#ifdef CPP11_SUPPORT
class SpinLock {
private:
	volatile std::atomic_flag lck;

public:
	SpinLock() {
		lck.clear();
	}

	ALWAYS_INLINE void lock() {
		while (std::atomic_flag_test_and_set_explicit(&lck,
				std::memory_order_acquire))
			;
	}

	ALWAYS_INLINE void unlock() {
		std::atomic_flag_clear_explicit(&lck, std::memory_order_release);
	}
};
#else
#define SpinLock ESpinLock
#endif //!CPP11_SUPPORT

//=============================================================================

template<typename E, typename LOCK=SpinLock>
class EFiberConcurrentQueue {
public:
	typedef struct node_t {
	    sp<E>* volatile value;
	    node_t* volatile next;

	    node_t(): value(null), next(null) {}
	} NODE;

public:
	~EFiberConcurrentQueue() {
		delete head;
	}

	EFiberConcurrentQueue() {
		NODE* node = new NODE();
		head = tail = node;
	}

	void add(sp<E>* e) {
		NODE* node = (*e)->packing;
		node->value = e;
		node->next = null;
		tl.lock();
			tail->next = node;
			tail = node;
		tl.unlock();
	}

	sp<E>* poll() {
		sp<E>* v;
		NODE* node = null;
		hl.lock();
			node = head;
			NODE* new_head = node->next;
			if (new_head == null) {
				hl.unlock();
				return null;
			}
			v = new_head->value;
			head = new_head;
		hl.unlock();
		{
			node->value = v;
			(*v)->packing = node;
		}
		return v;
	}

private:
	NODE *head;
	NODE *tail;
	LOCK hl;
	LOCK tl;
};

//=============================================================================

#define LOCKFOR(p) SpinLockPool<0>::lockFor(p)

#define POLLSIZE 41

template< int I > class SpinLockPool
{
private:
    static SpinLock* pool_[POLLSIZE];

public:
    static SpinLock* lockFor(void const* pv)
    {
    	es_size_t i = reinterpret_cast<es_size_t>(pv) % POLLSIZE;
        return pool_[i];
    }
};

#define SPINLOCK_INIT new SpinLock()

template< int I > SpinLock* SpinLockPool< I >::pool_[POLLSIZE] =
{
	SPINLOCK_INIT, SPINLOCK_INIT, SPINLOCK_INIT, SPINLOCK_INIT, SPINLOCK_INIT,
	SPINLOCK_INIT, SPINLOCK_INIT, SPINLOCK_INIT, SPINLOCK_INIT, SPINLOCK_INIT,
	SPINLOCK_INIT, SPINLOCK_INIT, SPINLOCK_INIT, SPINLOCK_INIT, SPINLOCK_INIT,
	SPINLOCK_INIT, SPINLOCK_INIT, SPINLOCK_INIT, SPINLOCK_INIT, SPINLOCK_INIT,
	SPINLOCK_INIT, SPINLOCK_INIT, SPINLOCK_INIT, SPINLOCK_INIT, SPINLOCK_INIT,
	SPINLOCK_INIT, SPINLOCK_INIT, SPINLOCK_INIT, SPINLOCK_INIT, SPINLOCK_INIT,
	SPINLOCK_INIT, SPINLOCK_INIT, SPINLOCK_INIT, SPINLOCK_INIT, SPINLOCK_INIT,
	SPINLOCK_INIT, SPINLOCK_INIT, SPINLOCK_INIT, SPINLOCK_INIT, SPINLOCK_INIT,
	SPINLOCK_INIT
};

//=============================================================================

} /* namespace eco */
} /* namespace efc */
#endif /* EFIBERUTIL_HH_ */
