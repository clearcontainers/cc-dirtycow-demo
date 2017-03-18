/*
Copyright (c) 2017 Intel Corporation

Permission is hereby granted, free of charge, to any person obtaining a
copy of this software and associated documentation files (the
"Software"), to deal in the Software without restriction, including
without limitation the rights to use, copy, modify, merge, publish,
distribute, sublicense, and/or sell copies of the Software, and to
permit persons to whom the Software is furnished to do so, subject to
the following conditions:

The above copyright notice and this permission notice shall be included
in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY
CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,
TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE
SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
*/
#ifndef FAILURE
#define FAILURE "[-] "
#endif

#ifndef SUCCESS
#define SUCCESS "[+] "
#endif

#ifndef ELF
#define ELF
#define _GNU_SOURCE
#include <elf.h>
#include <stdio.h>
#include <fcntl.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/auxv.h>
#include <sys/mman.h>

void *vdso_resolve(char *findsym) {
  uint64_t i;
  void *vdso = (void *)getauxval(AT_SYSINFO_EHDR);
  Elf64_Ehdr *elf_header = vdso;
  Elf64_Shdr *section_header = vdso + elf_header->e_shoff;
  Elf64_Shdr *shstrtab_header = (Elf64_Shdr *)((uint64_t)section_header + ((uint64_t)elf_header->e_shstrndx * (uint64_t)elf_header->e_shentsize));
#ifdef DEBUG_SECTIONS
  char *shstrtab = vdso + shstrtab_header->sh_offset;
#endif
  Elf64_Shdr *dynsym = NULL;
  Elf64_Shdr *strtab_header = NULL;
  char *strtab = NULL;
  Elf64_Sym *sym = NULL;

#ifdef DEBUG_WRITE
  write(STDOUT_FILENO, getauxval(AT_SYSINFO_EHDR), size);
#endif

  /*
   * Create array of pointers to sections so we can use st_shndx with symbols
   */
  for (i = 0; i < elf_header->e_shnum; ++i) {
#ifdef DEBUG_SECTIONS
    printf("[%2ld] %-30s %p\n", i, shstrtab + (uint64_t)section_header->sh_name, (void *)section_header->sh_offset);
#endif
    if (section_header->sh_type == SHT_DYNSYM) {
      dynsym = section_header;
    } else if (section_header->sh_type == SHT_STRTAB && section_header != shstrtab_header) {
      strtab_header = section_header;
      strtab = (char *)((uint64_t)elf_header + (uint64_t)strtab_header->sh_offset);
    }
    section_header = (Elf64_Shdr *)((uint64_t)section_header + (uint64_t)elf_header->e_shentsize);
  }

  if (dynsym == NULL || strtab == NULL) {
    return (void *)-1;
  }

  for (
      sym = (Elf64_Sym *)((uint64_t)vdso + (uint64_t)dynsym->sh_offset + (uint64_t)dynsym->sh_entsize);
      (uint64_t)sym - (uint64_t)vdso - (uint64_t)dynsym->sh_offset < (uint64_t)dynsym->sh_size;
      sym = (Elf64_Sym *)((uint64_t)sym + (uint64_t)dynsym->sh_entsize)) {
    if (sym->st_name != 0 && 0 == strncmp(strtab + sym->st_name, findsym, strlen(findsym)) && sym->st_shndx < elf_header->e_shnum) {
      return (void *)((uint64_t)vdso + (uint64_t)sym->st_value);
    }
  }

  return (void *)-1;
}
#endif

#ifdef DEBUG
int main(int argc, char *argv[]) {
  char findsym[] = "__vdso_clock_gettime";
  printf("%s: %p\n", findsym, vdso_resolve(findsym));
}
#endif
