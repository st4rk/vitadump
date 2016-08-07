/*
 * PS Vita 3.60 module dump
 * Based on https://github.com/xyzz/vita-modump/blob/master/main.c
 * Credits goes to xyz (original vita-modump), st4rk and smoke
 * it's very buggy
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <psp2/moduleinfo.h>
#include <psp2/io/fcntl.h>
#include <psp2/kernel/threadmgr.h>
#include <psp2/kernel/modulemgr.h>
#include <psp2/kernel/processmgr.h>
#include <psp2/sysmodule.h>
#include <psp2/io/stat.h>
#include <psp2/types.h>

#include "graphics.h"

#define MAX_LOADED_MODULES 256

const char *outDir = "ux0:/dump";

void doDump(SceUID id, SceKernelModuleInfo *info) {
	char filename[2048] = {0};
	int i;
	int fout;
	for (i = 0; i < 4; ++i) {

		if (info->segments[i].vaddr == NULL) {
			psvDebugScreenPrintf("Segment #%x is empty, skipping\n", i);
			continue;
		}

		snprintf(filename, sizeof(filename), "%s/0x%08x_0x%08x_%s_%d.bin", outDir, id, (unsigned)info->segments[i].vaddr, info->module_name, i);
		psvDebugScreenPrintf("Dumping segment #%x to %s\n", i, filename);

		if (!(fout = sceIoOpen(filename, SCE_O_CREAT | SCE_O_WRONLY, 0777))) {
			psvDebugScreenPrintf("Failed to open the file for writing.\n");
			continue;
		}

		sceIoWrite(fout, info->segments[i].vaddr, info->segments[i].memsz);
		sceIoClose(fout);
	}
}

/*
 * Thanks to flatz
 */
void dumpModuleByPath(char *module_name, char *path) {
	int result = 0;
	SceUID moduleId = 0;
	SceKernelModuleInfo info;

	psvDebugScreenPrintf("Module: %s. Attempting to load: '%s'.\n", module_name, path);
	moduleId = sceKernelLoadModule(path, 0, NULL);
	if (moduleId > 0) {
		psvDebugScreenPrintf("The module was loaded with success\n");
		info.size = sizeof(info);

		if ((result = sceKernelGetModuleInfo(moduleId, &info)) < 0) {
		psvDebugScreenPrintf("Failed to get module information, ID: %d, result: 0x%08x\n", moduleId, result);
		} else {
			doDump(moduleId, &info);
		}

		//sceKernelUnloadModule(moduleId, 0, NULL);
	} else {
		psvDebugScreenPrintf("Failed to load module: 0x%08X\n", moduleId);
	}
}

void dumpModule(SceUID id) {
	int result = 0;
	SceKernelLMOption opt;
	int reloadModule = 0;
	SceKernelModuleInfo info;


	if ((result = sceKernelGetModuleInfo(id, &info)) < 0) {
		psvDebugScreenPrintf("Failed to get module information, ID: %d, result: 0x%08x\n", id, result);
		return;
	}

	psvDebugScreenPrintf("Module: %s. Attempting to reload: '%s'.\n", info.module_name, info.path);

	if (strncmp(info.path, "ux0:/patch", 11) == 0) {
		char newPath[2048] = {0};
		char *modulePath   = strchr(info.path + 11, '/');
		strcpy(newPath, "app0:");
		strcpy(newPath+5, modulePath);

		psvDebugScreenPrintf("Module path for reloading changed to: %s\n", newPath);

		reloadModule = sceKernelLoadModule(newPath, 0, &opt);
	} else {
		reloadModule = sceKernelLoadModule(info.path, 0, &opt);
	}


	if (reloadModule > 0) {
		psvDebugScreenPrintf("The module was reloaded with success\n");
		info.size = sizeof(info);

		if ((result = sceKernelGetModuleInfo(reloadModule, &info)) < 0) {
			psvDebugScreenPrintf("Failed to get info for the reloaded module, result: %08x\n", result);
		} else {
			doDump(reloadModule, &info);
		}

		//sceKernelUnloadModule(reloadModule, 0, &opt);
	} else {
		psvDebugScreenPrintf("Failed to reload module, NID will remain poisoned\n");
		doDump(id, &info);
	}

}	

void newModules() {
	SceUInt16 i = 0;

	for (i = 0; i < 0xFF; i++) {
		int result = sceSysmoduleLoadModule(i);

		if (result != 0)
			psvDebugScreenPrintf("Error while loading module %d result: %08x\n", i, result);
	}
}

int main(int argc, char **argv) {
	int mList[MAX_LOADED_MODULES];
	int nLoaded = MAX_LOADED_MODULES;

	psvDebugScreenInit();
	psvDebugScreenPrintf("Starting module dump...\n");


	int result = 0;
	
	if ((result = sceIoOpen(outDir,SCE_O_RDONLY, 0777)) < 0) {
		result = sceIoMkdir(outDir, 0777);

		if (result == 0) {
			psvDebugScreenPrintf("The output dir was created\n");
		} else {
			psvDebugScreenPrintf("Directory exists\n");
			goto _continue_;
		}
	}

_continue_:

	psvDebugScreenPrintf("Loading addtional modules\n");

	newModules();

	if ((result = sceKernelGetModuleList(0xFF, mList, &nLoaded)) < 0) {
		psvDebugScreenPrintf("Failed to get module list, result: %08x\n", result);
		goto _exit_;
	}

	psvDebugScreenPrintf("Total modules loaded: %d\n", nLoaded);

	SceUID i = 0;

	for (i = 0; i < nLoaded; ++i)
		dumpModule(mList[i]);

	psvDebugScreenPrintf("Dumping modules by name\n");
	dumpModuleByPath("SceWebKit.bin", "vs0:data/external/webcore/SceWebKitModule.suprx");
	dumpModuleByPath("SceLibC.bin", "vs0:sys/external/libc.suprx");


	psvDebugScreenPrintf("Done\n");

_exit_:
	
	while (1) {}

	sceKernelExitProcess(0);
	return 0;
}