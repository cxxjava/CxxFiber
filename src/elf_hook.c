#ifdef __linux__

#include "elf_hook.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dlfcn.h>
#include <elf.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <unistd.h>
#include <errno.h>
//rename standart types for convenience
#ifdef __x86_64
    #define Elf_Ehdr Elf64_Ehdr
    #define Elf_Shdr Elf64_Shdr
    #define Elf_Sym Elf64_Sym
    #define Elf_Rel Elf64_Rela
    #define ELF_R_SYM ELF64_R_SYM
    #define REL_DYN ".rela.dyn"
    #define REL_PLT ".rela.plt"
#else
    #define Elf_Ehdr Elf32_Ehdr
    #define Elf_Shdr Elf32_Shdr
    #define Elf_Sym Elf32_Sym
    #define Elf_Rel Elf32_Rel
    #define ELF_R_SYM ELF32_R_SYM
    #define REL_DYN ".rel.dyn"
    #define REL_PLT ".rel.plt"
#endif

//==================================================================================================
static int read_header(int d, Elf_Ehdr **header)
{
    *header = (Elf_Ehdr *)malloc(sizeof(Elf_Ehdr));
    if(NULL == *header)
    {
        return errno;
    }

    if (lseek(d, 0, SEEK_SET) < 0)
    {
        free(*header);

        return errno;
    }

    if (read(d, *header, sizeof(Elf_Ehdr)) <= 0)
    {
        free(*header);

        return errno = EINVAL;
    }

    return 0;
}
//--------------------------------------------------------------------------------------------------
static int read_section_table(int d, Elf_Ehdr const *header, Elf_Shdr **table)
{
    size_t size;

    if (NULL == header)
        return EINVAL;

    size = header->e_shnum * sizeof(Elf_Shdr);
    *table = (Elf_Shdr *)malloc(size);
    if(NULL == *table)
    {
        return errno;
    }

    if (lseek(d, header->e_shoff, SEEK_SET) < 0)
    {
        free(*table);

        return errno;
    }

    if (read(d, *table, size) <= 0)
    {
        free(*table);

        return errno = EINVAL;
    }

    return 0;
}
//--------------------------------------------------------------------------------------------------
static int read_string_table(int d, Elf_Shdr const *section, char const **strings)
{
    if (NULL == section)
        return EINVAL;

    *strings = (char const *)malloc(section->sh_size);
    if(NULL == *strings)
    {
        return errno;
    }

    if (lseek(d, section->sh_offset, SEEK_SET) < 0)
    {
        free((void *)*strings);

        return errno;
    }

    if (read(d, (char *)*strings, section->sh_size) <= 0)
    {
        free((void *)*strings);

        return errno = EINVAL;
    }

    return 0;
}
//--------------------------------------------------------------------------------------------------
static int read_symbol_table(int d, Elf_Shdr const *section, Elf_Sym **table)
{
    if (NULL == section)
        return EINVAL;

    *table = (Elf_Sym *)malloc(section->sh_size);
    if(NULL == *table)
    {
        return errno;
    }

    if (lseek(d, section->sh_offset, SEEK_SET) < 0)
    {
        free(*table);

        return errno;
    }

    if (read(d, *table, section->sh_size) <= 0)
    {
        free(*table);

        return errno = EINVAL;
    }

    return 0;
}
//--------------------------------------------------------------------------------------------------
static int read_relocation_table(int d, Elf_Shdr const *section, Elf_Rel **table)
{
    if (NULL == section)
        return EINVAL;

    *table = (Elf_Rel *)malloc(section->sh_size);
    if(NULL == *table)
    {
        return errno;
    }

    if (lseek(d, section->sh_offset, SEEK_SET) < 0)
    {
        free(*table);

        return errno;
    }

    if (read(d, *table, section->sh_size) <= 0)
    {
        free(*table);

        return errno = EINVAL;
    }

    return 0;
}
//--------------------------------------------------------------------------------------------------
static int section_by_index(int d, size_t const index, Elf_Shdr **section)
{
    Elf_Ehdr *header = NULL;
    Elf_Shdr *sections = NULL;
    size_t i;

    *section = NULL;

    if (
        read_header(d, &header) ||
        read_section_table(d, header, &sections)
        )
        return errno;

    if (index < header->e_shnum)
    {
        *section = (Elf_Shdr *)malloc(sizeof(Elf_Shdr));

        if (NULL == *section)
        {
            free(header);
            free(sections);

            return errno;
        }

        memcpy(*section, sections + index, sizeof(Elf_Shdr));
    }
    else
        return EINVAL;

    free(header);
    free(sections);

    return 0;
}
//--------------------------------------------------------------------------------------------------
static int section_by_type(int d, size_t const section_type, Elf_Shdr **section)
{
    Elf_Ehdr *header = NULL;
    Elf_Shdr *sections = NULL;
    size_t i;

    *section = NULL;

    if (
        read_header(d, &header) ||
        read_section_table(d, header, &sections)
        )
        return errno;

    for (i = 0; i < header->e_shnum; ++i)
        if (section_type == sections[i].sh_type)
        {
            *section = (Elf_Shdr *)malloc(sizeof(Elf_Shdr));

            if (NULL == *section)
            {
                free(header);
                free(sections);

                return errno;
            }

            memcpy(*section, sections + i, sizeof(Elf_Shdr));

            break;
        }

    free(header);
    free(sections);

    return 0;
}
//--------------------------------------------------------------------------------------------------
static int section_by_name(int d, char const *section_name, Elf_Shdr **section)
{
    Elf_Ehdr *header = NULL;
    Elf_Shdr *sections = NULL;
    char const *strings = NULL;
    size_t i;

    *section = NULL;

    if (
        read_header(d, &header) ||
        read_section_table(d, header, &sections) ||
        read_string_table(d, &sections[header->e_shstrndx], &strings)
        )
        return errno;

    for (i = 0; i < header->e_shnum; ++i)
        if (!strcmp(section_name, &strings[sections[i].sh_name]))
        {
            *section = (Elf_Shdr *)malloc(sizeof(Elf_Shdr));

            if (NULL == *section)
            {
                free(header);
                free(sections);
                free((void *)strings);

                return errno;
            }

            memcpy(*section, sections + i, sizeof(Elf_Shdr));

            break;
        }

    free(header);
    free(sections);
    free((void *)strings);

    return 0;
}

#ifdef __cplusplus
extern "C"
{
#endif

int rebind_symbols_image(char const *module_filename, void *handle,
		struct rebinding rebindings[], size_t rebindings_nel)
{
    int descriptor;  //file descriptor of shared module

    Elf_Shdr
    *dynsym = NULL,  // ".dynsym" section header
    *rel_plt = NULL,  // ".rel.plt" section header
    *rel_dyn = NULL;  // ".rel.dyn" section header

    Elf_Rel
    *rel_plt_table = NULL,  //array with ".rel.plt" entries
    *rel_dyn_table = NULL;  //array with ".rel.dyn" entries

    size_t
    i,
    rel_plt_amount,  // amount of ".rel.plt" entries
    rel_dyn_amount,  // amount of ".rel.dyn" entries
    *name_address = NULL;  //address of relocation for symbol named "name"

    void *original = NULL;  //address of the symbol being substituted

    void *module_address = NULL;

    Elf_Shdr *strings_section = NULL;
	char const *strings = NULL;
	Elf_Sym *symbols = NULL;
	size_t amount;
	Elf_Sym *found = NULL;

    if (NULL == module_filename || NULL == handle)
        return -1;

    descriptor = open(module_filename, O_RDONLY);

    if (descriptor < 0)
        return -1;

	// get base address
	if (section_by_type(descriptor, SHT_DYNSYM, &dynsym) ||  //get ".dynsym" section
		section_by_index(descriptor, dynsym->sh_link, &strings_section) ||
		read_string_table(descriptor, strings_section, &strings) ||
		read_symbol_table(descriptor, dynsym, &symbols))
	{
		free(strings_section);
		free((void *)strings);
		free(symbols);
		free(dynsym);

		close(descriptor);

		return errno;
	}

	amount = dynsym->sh_size / sizeof(Elf_Sym);

	/* Trick to get the module base address in a portable way:
	 *   Find the first GLOBAL or WEAK symbol in the symbol table,
	 *   look this up with dlsym, then return the difference as the base address
	 */
	for (i = 0; i < amount; ++i)
	{
		switch(ELF32_ST_BIND(symbols[i].st_info)) {
		case STB_GLOBAL:
		case STB_WEAK:
			found = &symbols[i];
			break;
		default: // Not interested in this symbol
			break;
		}
	}
	if (found != NULL)
	{
		const char *name = &strings[found->st_name];
		void *sym = dlsym(handle, name);
		if(sym != NULL)
			module_address = (void*)((size_t)sym - found->st_value);
	}

	free(strings_section);
	free((void *)strings);
	free(dynsym);

	if (!module_address) {
		free(symbols);
		close(descriptor);
		return -1;
	}

    if (
        section_by_name(descriptor, REL_PLT, &rel_plt) ||  //get ".rel.plt" (for 32-bit) or ".rela.plt" (for 64-bit) section
        section_by_name(descriptor, REL_DYN, &rel_dyn)  //get ".rel.dyn" (for 32-bit) or ".rela.dyn" (for 64-bit) section
       )
    {  //if something went wrong
        free(rel_plt);
        free(rel_dyn);
        free(symbols);
        close(descriptor);
        return -1;
    }

    close(descriptor);

    rel_plt_table = (Elf_Rel *)(((size_t)module_address) + rel_plt->sh_addr);  //init the ".rel.plt" array
    rel_plt_amount = rel_plt->sh_size / sizeof(Elf_Rel);  //and get its size

    rel_dyn_table = (Elf_Rel *)(((size_t)module_address) + rel_dyn->sh_addr);  //init the ".rel.dyn" array
    rel_dyn_amount = rel_dyn->sh_size / sizeof(Elf_Rel);  //and get its size

    //release the data used
    free(rel_plt);
    free(rel_dyn);

    //now we've got ".rel.plt" (needed for PIC) table and ".rel.dyn" (for non-PIC) table and the symbol's index
    for (i = 0; i < rel_plt_amount; ++i)  //lookup the ".rel.plt" table
    {
    	uint j;
    	uint16_t ndx = ELF_R_SYM(rel_plt_table[i].r_info);
    	char* name = (char*)((size_t)module_address + symbols[ndx].st_name);
    	for (j = 0; j < rebindings_nel; j++) {
			if (strcmp(name, rebindings[j].name) == 0) {
				if (rebindings[j].replaced != NULL) {
					original = (void *)*(size_t *)(((size_t)module_address) + rel_plt_table[i].r_offset);  //save the original function address
					*(rebindings[j].replaced) = original;
				}
				*(size_t *)(((size_t)module_address) + rel_plt_table[i].r_offset) = (size_t)rebindings[j].replacement;  //and replace it with the substitutional
				break; //the target symbol appears in ".rel.plt" only once
			}
		}
    }

	free(symbols);

    return 0;
}
#ifdef __cplusplus
}
#endif

#endif //#!__linux__
