#ifdef __linux__

//@see: https://github.com/shoumikhin/ELF-Hook

#pragma once

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C"
{
#endif

/*
 * A structure representing a particular intended rebinding from a symbol
 * name to its replacement
 */
struct rebinding {
	const char *name;
	void *replacement;
	void **replaced;
};

/*
 * Rebinds as above, but only in the specified image.
 */
int rebind_symbols_image(char const *library_filename, void *handle,
		struct rebinding rebindings[], size_t rebindings_nel);

#ifdef __cplusplus
}
#endif

#endif //#!__linux__
