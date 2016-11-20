#include <stdio.h>
#include <string.h>
#include <taihen.h>
#include <psp2kern/kernel/modulemgr.h>
#include <psp2kern/kernel/threadmgr.h>
#include <psp2kern/kernel/sysmem.h>
#include <psp2kern/io/fcntl.h>

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
