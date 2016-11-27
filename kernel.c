#include <stdio.h>
#include <string.h>
#include <taihen.h>
#include <psp2kern/kernel/modulemgr.h>
#include <psp2kern/kernel/threadmgr.h>
#include <psp2kern/kernel/sysmem.h>
#include <psp2kern/io/fcntl.h>
#include "elf.h"

#define DUMP_PATH "ux0:dump/"
#define LOG_FILE DUMP_PATH "kplugin_log.txt"

static void log_reset();
static void log_write(const char *buffer, size_t length);

#define LOG(...) \
	do { \
		char buffer[256]; \
		snprintf(buffer, sizeof(buffer), ##__VA_ARGS__); \
		log_write(buffer, strlen(buffer)); \
	} while (0)

static void dump_region(const char *filename, void *addr, unsigned int size)
{
	SceUID fd;

	if (!(fd = sceIoOpenForDriver(filename, SCE_O_WRONLY | SCE_O_CREAT | SCE_O_TRUNC, 6))) {
		LOG("Error opening %s\n", filename);
		return;
	}

	sceIoWriteForDriver(fd, addr, size);

	sceIoCloseForDriver(fd);
}

void _start() __attribute__ ((weak, alias ("module_start")));

#define MOD_LIST_SIZE 0x80


void doDump(SceKernelModuleInfo *info) {
	char filename[2048] = {0};
	int i;
	int fout;
	Elf32_Ehdr ehdr;
	Elf32_Phdr phdr;
	Elf32_Off offset;

	snprintf(filename, sizeof(filename), "%s/%s.elf",
		 DUMP_PATH, info->module_name);

	LOG("Dumping %s\n", filename);

	if (!(fout = sceIoOpenForDriver(filename, SCE_O_CREAT | SCE_O_WRONLY, 0777))) {
		LOG("Failed to open the file for writing.\n");
		return;
	}

	ehdr.e_ident[EI_MAG0] = ELFMAG0;
	ehdr.e_ident[EI_MAG1] = ELFMAG1;
	ehdr.e_ident[EI_MAG2] = ELFMAG2;
	ehdr.e_ident[EI_MAG3] = ELFMAG3;
	ehdr.e_ident[EI_CLASS] = ELFCLASS32;
	ehdr.e_ident[EI_DATA] = ELFDATA2LSB;
	ehdr.e_ident[EI_VERSION] = EV_CURRENT;
	ehdr.e_ident[EI_OSABI] = ELFOSABI_ARM_AEABI;
	ehdr.e_ident[EI_ABIVERSION] = 0;
	memset(ehdr.e_ident + EI_PAD, 0, EI_NIDENT - EI_PAD);
	ehdr.e_type = ET_CORE;
	ehdr.e_machine = EM_ARM;
	ehdr.e_version = EV_CURRENT;
	ehdr.e_entry = (Elf32_Addr)info->module_start;
	ehdr.e_phoff = sizeof (ehdr);
	ehdr.e_flags = EF_ARM_HASENTRY
		       | EF_ARM_ABI_FLOAT_HARD
		       | EF_ARM_EABI_VER5;
	ehdr.e_ehsize = sizeof (ehdr);
	ehdr.e_phentsize = sizeof (Elf32_Phdr);
	ehdr.e_shentsize = sizeof (Elf32_Shdr);
	ehdr.e_shnum = 0;
	ehdr.e_shstrndx = 0;

	ehdr.e_shoff = 0;
	ehdr.e_phnum = 0;
	for (i = 0; i < 4; ++i) {
		if (info->segments[i].vaddr == NULL)
			continue;

		++ehdr.e_phnum;
	}

	sceIoWriteForDriver (fout, &ehdr, sizeof (ehdr));

	offset = sizeof (ehdr) + ehdr.e_phnum * sizeof(phdr);
	phdr.p_type = PT_LOAD;
	phdr.p_paddr = 0;
	phdr.p_align = 1;
	for (i = 0; i < 4; ++i) {
		if (info->segments[i].vaddr == NULL)
			continue;

		phdr.p_flags = info->segments[i].perms;
		phdr.p_offset = offset;
		phdr.p_vaddr = (Elf32_Addr)info->segments[i].vaddr;
		phdr.p_memsz = info->segments[i].memsz;
		phdr.p_filesz = phdr.p_memsz;

		sceIoWriteForDriver (fout, &phdr, sizeof (phdr));

		offset += phdr.p_filesz;
	}

	for (i = 0; i < 4; ++i) {
		if (info->segments[i].vaddr == NULL) {
			LOG("Segment #%x is empty, skipping\n", i);
			continue;
		}

		sceIoWriteForDriver(fout, info->segments[i].vaddr, info->segments[i].memsz);
	}

	sceIoCloseForDriver(fout);

	snprintf(filename, sizeof(filename), "%s/%s_info.bin",
		 DUMP_PATH, info->module_name);

	LOG("Dumping %s\n", filename);

	if (!(fout = sceIoOpenForDriver(filename, SCE_O_CREAT | SCE_O_WRONLY, 0777))) {
		LOG("Failed to open the file for writing.\n");
		return;
	}

	sceIoWriteForDriver (fout, info, sizeof (*info));
	sceIoCloseForDriver (fout);
}


int module_start(SceSize argc, const void *args)
{
	int i, j;
	int ret;
	size_t num;
	SceKernelModuleInfo modinfo;
	SceUID modlist[MOD_LIST_SIZE];

	log_reset();

	LOG("kplugin by xerpi\n");

	memset(modlist, 0, sizeof(modlist));

	num = MOD_LIST_SIZE;
	ret = sceKernelGetModuleListForKernel(KERNEL_PID, 0x80000001, 1, modlist, &num);
	if (ret < 0)
		LOG("Error getting the module list\n");

	LOG("Found %d modules.\n", num);

	for (i = 0; i < num; i++) {
		memset(&modinfo, 0, sizeof(modinfo));

		ret = sceKernelGetModuleInfoForKernel(KERNEL_PID, modlist[i], &modinfo);
		if (ret < 0) {
			LOG("Error getting the module info for module: %d\n", i);
			continue;
		}

		LOG("Module %d name: %s\n", i, modinfo.module_name);

		for (j = 0; j < 4; j++) {
			char path[128];
			SceKernelSegmentInfo *seginfo = &modinfo.segments[j];

			if (seginfo->size != sizeof(*seginfo))
				continue;

			snprintf(path, sizeof(path), DUMP_PATH "%s_0x%08X_seg%d.bin",
				modinfo.module_name, (uintptr_t)seginfo->vaddr, j);

			dump_region(path, seginfo->vaddr, seginfo->memsz);
		}
		doDump(&modinfo);
	}

	return SCE_KERNEL_START_SUCCESS;
}

int module_stop(SceSize argc, const void *args)
{
	return SCE_KERNEL_STOP_SUCCESS;
}

void log_reset()
{
	SceUID fd = sceIoOpenForDriver(LOG_FILE,
		SCE_O_WRONLY | SCE_O_CREAT | SCE_O_TRUNC, 6);
	if (fd < 0)
		return;

	sceIoCloseForDriver(fd);
}

void log_write(const char *buffer, size_t length)
{
	extern int sceIoMkdirForDriver(const char *, int);
	sceIoMkdirForDriver(DUMP_PATH, 6);

	SceUID fd = sceIoOpenForDriver(LOG_FILE,
		SCE_O_WRONLY | SCE_O_CREAT | SCE_O_APPEND, 6);
	if (fd < 0)
		return;

	sceIoWriteForDriver(fd, buffer, length);
	sceIoCloseForDriver(fd);
}
