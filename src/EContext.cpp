/*
 * EContext.cpp
 *
 *  Created on: 2016-5-1
 *      Author: cxxjava@163.com
 */

#include "EContext.hh"
#include "EFiber.hh"
#include "EFiberDebugger.hh"

namespace efc {
namespace eco {

#if defined(__x86_64__)
# if defined(__AVX__)
#  define CLOBBER \
        , "ymm0", "ymm1", "ymm2", "ymm3", "ymm4", "ymm5", "ymm6", "ymm7",\
        "ymm8", "ymm9", "ymm10", "ymm11", "ymm12", "ymm13", "ymm14", "ymm15"
# else
#  define CLOBBER
# endif

# define SETJMP(ctx) ({\
    int ret;\
    asm("lea     LJMPRET%=(%%rip), %%rcx\n\t"\
        "xor     %%rax, %%rax\n\t"\
        "mov     %%rbx, (%%rdx)\n\t"\
        "mov     %%rbp, 8(%%rdx)\n\t"\
        "mov     %%r12, 16(%%rdx)\n\t"\
        "mov     %%rsp, 24(%%rdx)\n\t"\
        "mov     %%r13, 32(%%rdx)\n\t"\
        "mov     %%r14, 40(%%rdx)\n\t"\
        "mov     %%r15, 48(%%rdx)\n\t"\
        "mov     %%rcx, 56(%%rdx)\n\t"\
        "mov     %%rdi, 64(%%rdx)\n\t"\
        "mov     %%rsi, 72(%%rdx)\n\t"\
        "LJMPRET%=:\n\t"\
        : "=a" (ret)\
        : "d" (ctx)\
        : "memory", "rcx", "r8", "r9", "r10", "r11",\
          "xmm0", "xmm1", "xmm2", "xmm3", "xmm4", "xmm5", "xmm6", "xmm7",\
          "xmm8", "xmm9", "xmm10", "xmm11", "xmm12", "xmm13", "xmm14", "xmm15"\
          CLOBBER\
          );\
    ret;\
})

# define LONGJMP(ctx) \
    asm("movq   (%%rax), %%rbx\n\t"\
	    "movq   8(%%rax), %%rbp\n\t"\
	    "movq   16(%%rax), %%r12\n\t"\
	    "movq   24(%%rax), %%rdx\n\t"\
	    "movq   32(%%rax), %%r13\n\t"\
	    "movq   40(%%rax), %%r14\n\t"\
	    "mov    %%rdx, %%rsp\n\t"\
	    "movq   48(%%rax), %%r15\n\t"\
	    "movq   56(%%rax), %%rdx\n\t"\
	    "movq   64(%%rax), %%rdi\n\t"\
	    "movq   72(%%rax), %%rsi\n\t"\
	    "jmp    *%%rdx\n\t"\
        : : "a" (ctx) : "rdx" \
    )
#endif

//=============================================================================

EThreadLocalStorage EContext::threadLocal;

void* EContext::getOrignContext() {
#ifdef __x86_64__
	void* env = threadLocal.get();
	if (!env) {
		env = malloc(sizeof(es_uint64_t) * 10);
		threadLocal.set(env);
	}
	return env;
#else
	ucontext_t* ctx = (ucontext_t*)threadLocal.get();
	if (!ctx) {
		ctx = new ucontext_t;
		threadLocal.set(ctx);
	}
	return ctx;
#endif
}

void EContext::cleanOrignContext() {
#ifdef __x86_64__
	void* env = threadLocal.get();
	if (env) {
		free(env);
		threadLocal.set(NULL);
	}
#else
	ucontext_t* ctx = (ucontext_t*)threadLocal.get();
	if (ctx) {
		delete ctx;
		threadLocal.set(NULL);
	}
#endif
}

//=============================================================================

EContext::~EContext() {
	if (stackAddr) {
		free(stackAddr);
	}
	if (context) {
		free(context);
	}
}

EContext::EContext(EFiber* f): fiber(f) {
	context = (ucontext_t*)malloc(sizeof(ucontext_t));

	/* do a reasonable initialization */
	sigset_t zero;
	memset(context, 0, sizeof(ucontext_t));
	sigemptyset(&zero);
	sigprocmask(SIG_BLOCK, &zero, &context->uc_sigmask);

	/* must initialize with current context */
	if (getcontext(context) == -1) {
		throw ERuntimeException(__FILE__, __LINE__, "getcontext");
	}

	int stackSize = fiber->stackSize;
#ifdef DEBUG
	stackAddr = (char*)calloc(1, stackSize); //for calc max stack.
	//@see: http://embeddedgurus.com/stack-overflow/2009/03/computing-your-stack-size/
#else
	stackAddr = (char*)malloc(stackSize);
#endif

	/* call makecontext to do the real work. */
	/* leave a few words open on both ends */
	char* alignedAddr = (char*)ES_ALIGN_DEFAULT((llong)stackAddr);
	context->uc_stack.ss_sp = alignedAddr; //memory aligned
	context->uc_stack.ss_size = stackSize - (alignedAddr - stackAddr);
	context->uc_link = NULL;

	makecontext(context, (void(*)(void))&fiber_worker, 1, fiber);
}

boolean EContext::swapIn() {
	// restore
	errno = errno_;

#ifdef __x86_64__
	/* use setcontext() for the initial jump, as it allows us to set up
	 * a stack, but continue with longjmp() as it's much faster.
	 */
	if (SETJMP(getOrignContext()) == 0) {
		/* context just be used once for set up a stack, which will
		 * be freed in fiber_worker.
		 */
		if (context != NULL)
			setcontext(context);
		else
			LONGJMP(env);
	}
	return true;
#else
	return (swapcontext((ucontext_t*)getOrignContext(), context) == 0);
#endif
}

boolean EContext::swapOut() {
#ifdef DEBUG
	//calc max stack size!!!
	if (EFiberDebugger::getInstance().isDebugOn(EFiberDebugger::FIBER)) {
		char* pcurr = stackAddr;
		char* pend = stackAddr + fiber->stackSize;
		while (pcurr < pend && *pcurr++ == 0) {
		}
		int n = pend - pcurr;
		ECO_DEBUG(EFiberDebugger::FIBER, "======= max stack size: %d =======", n);
	}
#endif

	//keep it
	errno_ = errno;

#ifdef __x86_64__
	if (SETJMP(env) == 0) {
		LONGJMP(getOrignContext());
	}
	return true;
#else
	return (swapcontext(context, (ucontext_t*)getOrignContext()) == 0);
#endif
}

void EContext::fiber_worker(void* arg) {
	EFiber* fiber = (EFiber*)arg;

#ifdef __x86_64__
	/* when using setjmp/longjmp, the context just be used only once */
	if (fiber->context->context != NULL) {
		free(fiber->context->context);
		fiber->context->context = NULL;
	}
#endif

	try {

		fiber->run();

	} catch (EThrowable& t) {
		t.printStackTrace();
		throw;
	} catch (...) {
		//TODO...
		throw;
	}

	fiber->state = EFiber::TERMINATED;
	fiber->context->swapOut(); //EFiber::yield();
}

} /* namespace eco */
} /* namespace efc */
