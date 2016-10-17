/*
 * PS Vita 3.60 module dump
 * Based on https://github.com/xyzz/vita-modump/blob/master/main.c
 * Credits goes to xyz (original vita-modump), st4rk, smoke and theflow
 * it's very buggy
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <psp2/moduleinfo.h>
#include <psp2/io/fcntl.h>
#include <psp2/io/dirent.h>
#include <psp2/kernel/threadmgr.h>
#include <psp2/kernel/modulemgr.h>
#include <psp2/kernel/processmgr.h>
#include <psp2/sysmodule.h>
#include <psp2/io/stat.h>
#include <psp2/types.h>
#include "elf.h"

#include "graphics.h"

#define MAX_LOADED_MODULES 256
#define MAX_PATH_LENGTH 1024

const char *outDir = "ux0:/dump";

int removePath(char *path, uint32_t *value, uint32_t max, void (* SetProgress)(uint32_t value, uint32_t max), int (* cancelHandler)()) {
	SceUID dfd = sceIoDopen(path);
	if (dfd >= 0) {
		int res = 0;

		do {
			SceIoDirent dir;
			memset(&dir, 0, sizeof(SceIoDirent));

			res = sceIoDread(dfd, &dir);
			if (res > 0) {
				if (strcmp(dir.d_name, ".") == 0 || strcmp(dir.d_name, "..") == 0)
					continue;

				char *new_path = malloc(strlen(path) + strlen(dir.d_name) + 2);
				snprintf(new_path, MAX_PATH_LENGTH, "%s/%s", path, dir.d_name);

				if (SCE_S_ISDIR(dir.d_stat.st_mode)) {
					int ret = removePath(new_path, value, max, SetProgress, cancelHandler);
					if (ret <= 0) {
						free(new_path);
						sceIoDclose(dfd);
						return ret;
					}
				} else {
					int ret = sceIoRemove(new_path);
					if (ret < 0) {
						free(new_path);
						sceIoDclose(dfd);
						return ret;
					}

					if (value)
						(*value)++;

					if (SetProgress)
						SetProgress(value ? *value : 0, max);

					if (cancelHandler && cancelHandler()) {
						free(new_path);
						sceIoDclose(dfd);
						return 0;
					}
				}

				free(new_path);
			}
		} while (res > 0);

		sceIoDclose(dfd);

		int ret = sceIoRmdir(path);
		if (ret < 0)
			return ret;

		if (value)
			(*value)++;

		if (SetProgress)
			SetProgress(value ? *value : 0, max);

		if (cancelHandler && cancelHandler()) {
			return 0;
		}
	} else {
		int ret = sceIoRemove(path);
		if (ret < 0)
			return ret;

		if (value)
			(*value)++;

		if (SetProgress)
			SetProgress(value ? *value : 0, max);

		if (cancelHandler && cancelHandler()) {
			return 0;
		}
	}

	return 1;
}

void doDump(SceUID id, SceKernelModuleInfo *info) {
	char filename[2048] = {0};
	int i;
	int fout;
	Elf32_Ehdr ehdr;
	Elf32_Phdr phdr;
	Elf32_Off offset;

	snprintf(filename, sizeof(filename), "%s/%s.elf",
		 outDir, info->module_name);

	psvDebugScreenPrintf("Dumping %s\n", filename);

	if (!(fout = sceIoOpen(filename, SCE_O_CREAT | SCE_O_WRONLY, 0777))) {
		psvDebugScreenPrintf("Failed to open the file for writing.\n");
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

	sceIoWrite (fout, &ehdr, sizeof (ehdr));

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

		sceIoWrite (fout, &phdr, sizeof (phdr));

		offset += phdr.p_filesz;
	}

	for (i = 0; i < 4; ++i) {
		if (info->segments[i].vaddr == NULL) {
			psvDebugScreenPrintf("Segment #%x is empty, skipping\n", i);
			continue;
		}

		sceIoWrite(fout, info->segments[i].vaddr, info->segments[i].memsz);
	}

	sceIoClose(fout);

	snprintf(filename, sizeof(filename), "%s/%s_info.bin",
		 outDir, info->module_name);

	psvDebugScreenPrintf("Dumping %s\n", filename);

	if (!(fout = sceIoOpen(filename, SCE_O_CREAT | SCE_O_WRONLY, 0777))) {
		psvDebugScreenPrintf("Failed to open the file for writing.\n");
		return;
	}

	sceIoWrite (fout, info, sizeof (*info));
	sceIoClose (fout);
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

		sceKernelUnloadModule(moduleId, 0, NULL);
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
	//int mList[MAX_LOADED_MODULES];
	//int nLoaded = MAX_LOADED_MODULES;

	psvDebugScreenInit();
	psvDebugScreenPrintf("Starting module dump...\n");


	int result = 0;
	
	if ((result = sceIoOpen(outDir,SCE_O_RDONLY, 0777)) < 0) {
		result = sceIoMkdir(outDir, 0777);

		if (result == 0) {
			psvDebugScreenPrintf("The output dir was created\n");
		} else {
			psvDebugScreenPrintf("Directory exists\nDeleting and recreating...\n");
			removePath(outDir, NULL, 0, NULL, NULL);
			sceIoMkdir(outDir, 0777);
			goto _continue_;
		}
	}

_continue_:

	/*psvDebugScreenPrintf("Loading addtional modules\n");

	newModules();

	if ((result = sceKernelGetModuleList(0xFF, mList, &nLoaded)) < 0) {
		psvDebugScreenPrintf("Failed to get module list, result: %08x\n", result);
		goto _exit_;
	}

	psvDebugScreenPrintf("Total modules loaded: %d\n", nLoaded);

	SceUID i = 0;

	for (i = 0; i < nLoaded; ++i)
		dumpModule(mList[i]);
	*/
	psvDebugScreenPrintf("Dumping modules by name\n");
	
	//dumpModuleByPath("1e6c89bb6fd70485", "pd0:app/NPXS10007/sce_module/libc.suprx");
	//dumpModuleByPath("3c3b85ca044fab22", "pd0:app/NPXS10007/sce_module/libfios2.suprx");
	//dumpModuleByPath("9ce10e890f276561", "pd0:app/NPXS10007/sce_module/libult.suprx");
	
	//251 USERMODULES
	dumpModuleByPath("2800000000028005", "os0:us/avcodec_us.suprx");
	dumpModuleByPath("280000000002800a", "os0:us/driver_us.suprx");
	dumpModuleByPath("280000000002802a", "os0:us/libgpu_es4.suprx");
	dumpModuleByPath("2800000000028030", "os0:us/libgxm_es4.suprx");
	dumpModuleByPath("2800000000028034", "os0:us/libkernel.suprx");
	dumpModuleByPath("2800000000000015", "os0:ue/safemode.self");
	dumpModuleByPath("2800000000000027", "vs0:/app/NPXS10000/eboot.bin");
	dumpModuleByPath("2800000000000026", "vs0:/app/NPXS10001/eboot.bin");
	dumpModuleByPath("280000000002001f", "vs0:/app/NPXS10001/np_party_app.suprx");
	dumpModuleByPath("280000000000001c", "vs0:/app/NPXS10002/eboot.bin");
	dumpModuleByPath("2800000000000025", "vs0:/app/NPXS10003/eboot.bin");
	dumpModuleByPath("2800000000000018", "vs0:/app/NPXS10004/eboot.bin");
	dumpModuleByPath("280000000000001a", "vs0:/app/NPXS10006/eboot.bin");
	dumpModuleByPath("280000000000001b", "vs0:/app/NPXS10008/eboot.bin");
	dumpModuleByPath("2800000000000011", "vs0:/app/NPXS10009/eboot.bin");
	dumpModuleByPath("280000000000001e", "vs0:/app/NPXS10010/eboot.bin");
	dumpModuleByPath("2800000000000012", "vs0:/app/NPXS10012/eboot.bin");
	dumpModuleByPath("280000000000700a", "vs0:/app/NPXS10013/eboot.bin");
	dumpModuleByPath("2800000000020045", "vs0:/app/NPXS10013/gaikai-player.suprx");
	dumpModuleByPath("2800000000020046", "vs0:/app/NPXS10013/libSceSecondScreen.suprx");
	dumpModuleByPath("2800000000000020", "vs0:/app/NPXS10014/eboot.bin");
	dumpModuleByPath("2800000000000010", "vs0:/app/NPXS10015/eboot.bin");
	dumpModuleByPath("2800000000020024", "vs0:/app/NPXS10015/system_settings_core.suprx");
	dumpModuleByPath("2800000000000019", "vs0:/app/NPXS10018/eboot.bin");
	dumpModuleByPath("2800000000000029", "vs0:/app/NPXS10021/eboot.bin");
	dumpModuleByPath("2800000000020026", "vs0:/app/NPXS10021/tel_reg.suprx");
	dumpModuleByPath("2800000000000016", "vs0:/app/NPXS10023/eboot.bin");
	dumpModuleByPath("280000000000002b", "vs0:/app/NPXS10024/eboot.bin");
	dumpModuleByPath("280000000000002c", "vs0:/app/NPXS10025/eboot.bin");
	dumpModuleByPath("280000000000002d", "vs0:/app/NPXS10026/eboot.bin");
	dumpModuleByPath("2800000000000022", "vs0:/app/NPXS10027/eboot.bin");
	dumpModuleByPath("2800000000000013", "vs0:/app/NPXS10028/eboot.bin");
	dumpModuleByPath("2808000000000101", "vs0:/app/NPXS10028/pcff.skprx");
	dumpModuleByPath("280000000000002a", "vs0:/app/NPXS10029/eboot.bin");
	dumpModuleByPath("280000000000002e", "vs0:/app/NPXS10031/eboot.bin");
	dumpModuleByPath("280000000000002f", "vs0:/app/NPXS10032/eboot.bin");
	dumpModuleByPath("2800000000000031", "vs0:/app/NPXS10036/eboot.bin");
	dumpModuleByPath("2800000000000021", "vs0:/app/NPXS10063/eboot.bin");
	dumpModuleByPath("2800000000000034", "vs0:/app/NPXS10065/eboot.bin");
	dumpModuleByPath("2800000000020030", "vs0:/app/NPXS10065/grief_report_dialog.suprx");
	dumpModuleByPath("2800000000000035", "vs0:/app/NPXS10072/eboot.bin");
	dumpModuleByPath("2800000000020035", "vs0:/app/NPXS10072/email_engine.suprx");
	dumpModuleByPath("2800000000000037", "vs0:/app/NPXS10073/eboot.bin");
	dumpModuleByPath("2800000000000036", "vs0:/app/NPXS10077/eboot.bin");
	dumpModuleByPath("2800000000000040", "vs0:/app/NPXS10078/eboot.bin");
	dumpModuleByPath("2800000000000039", "vs0:/app/NPXS10079/eboot.bin");
	dumpModuleByPath("2800000000000041", "vs0:/app/NPXS10080/eboot.bin");
	dumpModuleByPath("2800000000000038", "vs0:/app/NPXS10081/eboot.bin");
	//This one doesn't work, no idea why
	//dumpModuleByPath("2800800000000015", "vs0:/app/NPXS10082/spawn.self");
	dumpModuleByPath("2800000000008005", "vs0:/app/NPXS10083/eboot.bin");
	dumpModuleByPath("2800000000008005", "vs0:/app/NPXS10084/eboot.bin");
	dumpModuleByPath("2800000000000044", "vs0:/app/NPXS10085/eboot.bin");
	dumpModuleByPath("2800000000000042", "vs0:/app/NPXS10091/eboot.bin");
	dumpModuleByPath("2800000000000043", "vs0:/app/NPXS10092/eboot.bin");
	dumpModuleByPath("2800000000000046", "vs0:/app/NPXS10094/eboot.bin");
	dumpModuleByPath("2800000000000045", "vs0:/app/NPXS10095/eboot.bin");
	dumpModuleByPath("280000000000700a", "vs0:/app/NPXS10098/eboot.bin");
	dumpModuleByPath("2800000000020045", "vs0:/app/NPXS10098/gaikai-player.suprx");
	dumpModuleByPath("2800000000000047", "vs0:/app/NPXS10100/eboot.bin");
	dumpModuleByPath("2800000000000048", "vs0:/app/NPXS10101/eboot.bin");
	dumpModuleByPath("280000000002807f", "vs0:/data/external/webcore/jx_web_filtering.suprx");
	dumpModuleByPath("2800000000028097", "vs0:/data/external/webcore/ScePsp2Compat.suprx");
	dumpModuleByPath("2800000000028099", "vs0:/data/external/webcore/SceWebKitModule.suprx");
	dumpModuleByPath("280000000002807e", "vs0:/data/external/webcore/vita_jsextobj.suprx");
	dumpModuleByPath("2800000000028001", "vs0:/sys/external/activity_db.suprx");
	dumpModuleByPath("2800000000028002", "vs0:/sys/external/adhoc_matching.suprx");
	dumpModuleByPath("2800000000028003", "vs0:/sys/external/apputil.suprx");
	dumpModuleByPath("28000000000280a5", "vs0:/sys/external/apputil_ext.suprx");
	dumpModuleByPath("2800000000028004", "vs0:/sys/external/audiocodec.suprx");
	dumpModuleByPath("28000000000280ac", "vs0:/sys/external/avcdec_for_player.suprx");
	dumpModuleByPath("2800000000028086", "vs0:/sys/external/bgapputil.suprx");
	dumpModuleByPath("2800000000028007", "vs0:/sys/external/bXCe.suprx");
	dumpModuleByPath("2800000000028008", "vs0:/sys/external/common_gui_dialog.suprx");
	dumpModuleByPath("2800000000028080", "vs0:/sys/external/dbrecovery_utility.suprx");
	dumpModuleByPath("2800000000028009", "vs0:/sys/external/dbutil.suprx");
	dumpModuleByPath("28000000000280ae", "vs0:/sys/external/friend_select.suprx");
	dumpModuleByPath("2800000000028087", "vs0:/sys/external/incoming_dialog.suprx");
	dumpModuleByPath("280000000002800c", "vs0:/sys/external/ini_file_processor.suprx");
	dumpModuleByPath("2800000000028083", "vs0:/sys/external/libatrac.suprx");
	dumpModuleByPath("280000000002800e", "vs0:/sys/external/libc.suprx");
	dumpModuleByPath("280000000002800f", "vs0:/sys/external/libcdlg.suprx");
	dumpModuleByPath("28000000000280aa", "vs0:/sys/external/libcdlg_calendar_review.suprx");
	dumpModuleByPath("2800000000028079", "vs0:/sys/external/libcdlg_cameraimport.suprx");
	dumpModuleByPath("2800000000028010", "vs0:/sys/external/libcdlg_checkout.suprx");
	dumpModuleByPath("2800000000017003", "vs0:/sys/external/libcdlg_companion.suprx");
	dumpModuleByPath("280000000002807c", "vs0:/sys/external/libcdlg_compat.suprx");
	dumpModuleByPath("2800000000028090", "vs0:/sys/external/libcdlg_cross_controller.suprx");
	dumpModuleByPath("2800000000028011", "vs0:/sys/external/libcdlg_friendlist.suprx");
	dumpModuleByPath("28000000000280a8", "vs0:/sys/external/libcdlg_friendlist2.suprx");
	dumpModuleByPath("28000000000280b1", "vs0:/sys/external/libcdlg_game_custom_data.suprx");
	dumpModuleByPath("28000000000280b2", "vs0:/sys/external/libcdlg_game_custom_data_impl.suprx");
	dumpModuleByPath("2800000000028012", "vs0:/sys/external/libcdlg_ime.suprx");
	dumpModuleByPath("28000000000280af", "vs0:/sys/external/libcdlg_invitation.suprx");
	dumpModuleByPath("28000000000280b0", "vs0:/sys/external/libcdlg_invitation_impl.suprx");
	dumpModuleByPath("2800000000028013", "vs0:/sys/external/libcdlg_main.suprx");
	dumpModuleByPath("2800000000028015", "vs0:/sys/external/libcdlg_msg.suprx");
	dumpModuleByPath("280000000002809f", "vs0:/sys/external/libcdlg_near.suprx");
	dumpModuleByPath("2800000000028017", "vs0:/sys/external/libcdlg_netcheck.suprx");
	dumpModuleByPath("2800000000028089", "vs0:/sys/external/libcdlg_npeula.suprx");
	dumpModuleByPath("28000000000280a9", "vs0:/sys/external/libcdlg_npprofile2.suprx");
	dumpModuleByPath("280000000002801a", "vs0:/sys/external/libcdlg_np_message.suprx");
	dumpModuleByPath("2800000000028082", "vs0:/sys/external/libcdlg_np_sns_fb.suprx");
	dumpModuleByPath("280000000002801b", "vs0:/sys/external/libcdlg_np_trophy_setup.suprx");
	dumpModuleByPath("280000000002801c", "vs0:/sys/external/libcdlg_photoimport.suprx");
	dumpModuleByPath("280000000002801d", "vs0:/sys/external/libcdlg_photoreview.suprx");
	dumpModuleByPath("2800000000020040", "vs0:/sys/external/libcdlg_pocketstation.suprx");
	dumpModuleByPath("2800000000017004", "vs0:/sys/external/libcdlg_remote_osk.suprx");
	dumpModuleByPath("280000000002801e", "vs0:/sys/external/libcdlg_savedata.suprx");
	dumpModuleByPath("2800000000028096", "vs0:/sys/external/libcdlg_twitter.suprx");
	dumpModuleByPath("280000000002809a", "vs0:/sys/external/libcdlg_tw_login.suprx");
	dumpModuleByPath("28000000000280b3", "vs0:/sys/external/libcdlg_videoimport.suprx");
	dumpModuleByPath("2800000000028020", "vs0:/sys/external/libclipboard.suprx");
	dumpModuleByPath("2800000000028023", "vs0:/sys/external/libdbg.suprx");
	dumpModuleByPath("2800000000028024", "vs0:/sys/external/libfiber.suprx");
	dumpModuleByPath("2800000000028026", "vs0:/sys/external/libfios2.suprx");
	dumpModuleByPath("2800000000028027", "vs0:/sys/external/libg729.suprx");
	dumpModuleByPath("28000000000280a1", "vs0:/sys/external/libgameupdate.suprx");
	dumpModuleByPath("280000000002807a", "vs0:/sys/external/libhandwriting.suprx");
	dumpModuleByPath("2800000000028032", "vs0:/sys/external/libhttp.suprx");
	dumpModuleByPath("2800000000028033", "vs0:/sys/external/libime.suprx");
	dumpModuleByPath("2800000000028088", "vs0:/sys/external/libipmi_nongame.suprx");
	dumpModuleByPath("2800000000028036", "vs0:/sys/external/liblocation.suprx");
	dumpModuleByPath("280000000002809c", "vs0:/sys/external/liblocation_extension.suprx");
	dumpModuleByPath("2800000000028092", "vs0:/sys/external/liblocation_factory.suprx");
	dumpModuleByPath("2800000000028091", "vs0:/sys/external/liblocation_internal.suprx");
	dumpModuleByPath("280000000002002c", "vs0:/sys/external/libmln.suprx");
	dumpModuleByPath("2800000000024003", "vs0:/sys/external/libmlnapplib.suprx");
	dumpModuleByPath("2800000000020031", "vs0:/sys/external/libmlndownloader.suprx");
	dumpModuleByPath("2800000000028078", "vs0:/sys/external/libnaac.suprx");
	dumpModuleByPath("2800000000028037", "vs0:/sys/external/libnet.suprx");
	dumpModuleByPath("2800000000028038", "vs0:/sys/external/libnetctl.suprx");
	dumpModuleByPath("2800000000028039", "vs0:/sys/external/libngs.suprx");
	dumpModuleByPath("280000000002803a", "vs0:/sys/external/libpaf.suprx");
	dumpModuleByPath("2800000000020039", "vs0:/sys/external/libpaf_web_map_view.suprx");
	dumpModuleByPath("280000000002803c", "vs0:/sys/external/libperf.suprx");
	dumpModuleByPath("280000000002803d", "vs0:/sys/external/libpgf.suprx");
	dumpModuleByPath("280000000002803e", "vs0:/sys/external/libpvf.suprx");
	dumpModuleByPath("2800000000028043", "vs0:/sys/external/librudp.suprx");
	dumpModuleByPath("2800000000028044", "vs0:/sys/external/libsas.suprx");
	dumpModuleByPath("280000000002809e", "vs0:/sys/external/libsceavplayer.suprx");
	dumpModuleByPath("2800000000028093", "vs0:/sys/external/libSceBeisobmf.suprx");
	dumpModuleByPath("2800000000028094", "vs0:/sys/external/libSceBemp2sys.suprx");
	dumpModuleByPath("2800000000017002", "vs0:/sys/external/libSceCompanionUtil.suprx");
	dumpModuleByPath("280000000002808d", "vs0:/sys/external/libSceDtcpIp.suprx");
	dumpModuleByPath("2800000000028045", "vs0:/sys/external/libSceFt2.suprx");
	dumpModuleByPath("2800000000028046", "vs0:/sys/external/libscejpegarm.suprx");
	dumpModuleByPath("2800000000028047", "vs0:/sys/external/libscejpegencarm.suprx");
	dumpModuleByPath("28000000000280ad", "vs0:/sys/external/libSceJson.suprx");
	dumpModuleByPath("2800000000028076", "vs0:/sys/external/libscemp4.suprx");
	dumpModuleByPath("28000000000280a4", "vs0:/sys/external/libSceMp4Rec.suprx");
	dumpModuleByPath("2800000000028048", "vs0:/sys/external/libSceMusicExport.suprx");
	dumpModuleByPath("280000000002809b", "vs0:/sys/external/libSceNearDialogUtil.suprx");
	dumpModuleByPath("2800000000028049", "vs0:/sys/external/libSceNearUtil.suprx");
	dumpModuleByPath("280000000002804a", "vs0:/sys/external/libScePhotoExport.suprx");
	dumpModuleByPath("2800000000028081", "vs0:/sys/external/libScePromoterUtil.suprx");
	dumpModuleByPath("280000000002804b", "vs0:/sys/external/libSceScreenShot.suprx");
	dumpModuleByPath("280000000002804c", "vs0:/sys/external/libSceShutterSound.suprx");
	dumpModuleByPath("280000000002804d", "vs0:/sys/external/libSceSqlite.suprx");
	dumpModuleByPath("280000000002808a", "vs0:/sys/external/libSceTelephonyUtil.suprx");
	dumpModuleByPath("28000000000280a3", "vs0:/sys/external/libSceTeleportClient.suprx");
	dumpModuleByPath("28000000000280a2", "vs0:/sys/external/libSceTeleportServer.suprx");
	dumpModuleByPath("2800000000024001", "vs0:/sys/external/libSceVideoExport.suprx");
	dumpModuleByPath("2800000000020034", "vs0:/sys/external/libSceVideoSearchEmpr.suprx");
	dumpModuleByPath("280000000002804f", "vs0:/sys/external/libSceXml.suprx");
	dumpModuleByPath("2800000000028050", "vs0:/sys/external/libshellsvc.suprx");
	dumpModuleByPath("2800000000028051", "vs0:/sys/external/libssl.suprx");
	dumpModuleByPath("2800000000028052", "vs0:/sys/external/libsulpha.suprx");
	dumpModuleByPath("2800000000028053", "vs0:/sys/external/libsystemgesture.suprx");
	dumpModuleByPath("2800000000028054", "vs0:/sys/external/libult.suprx");
	dumpModuleByPath("2800000000028055", "vs0:/sys/external/libvoice.suprx");
	dumpModuleByPath("2800000000028056", "vs0:/sys/external/libvoiceqos.suprx");
	dumpModuleByPath("2800000000028057", "vs0:/sys/external/livearea_util.suprx");
	dumpModuleByPath("28000000000280a0", "vs0:/sys/external/mail_api_for_local_libc.suprx");
	dumpModuleByPath("2800000000028058", "vs0:/sys/external/near_profile.suprx");
	dumpModuleByPath("2800000000028085", "vs0:/sys/external/notification_util.suprx");
	dumpModuleByPath("2800000000028059", "vs0:/sys/external/np_activity.suprx");
	dumpModuleByPath("280000000002805a", "vs0:/sys/external/np_activity_sdk.suprx");
	dumpModuleByPath("280000000002805b", "vs0:/sys/external/np_basic.suprx");
	dumpModuleByPath("280000000002805c", "vs0:/sys/external/np_commerce2.suprx");
	dumpModuleByPath("280000000002805d", "vs0:/sys/external/np_common.suprx");
	dumpModuleByPath("28000000000280a7", "vs0:/sys/external/np_common_ps4.suprx");
	dumpModuleByPath("280000000002805e", "vs0:/sys/external/np_friend_privacylevel.suprx");
	dumpModuleByPath("280000000002805f", "vs0:/sys/external/np_kdc.suprx");
	dumpModuleByPath("2800000000028060", "vs0:/sys/external/np_manager.suprx");
	dumpModuleByPath("2800000000028061", "vs0:/sys/external/np_matching2.suprx");
	dumpModuleByPath("2800000000028062", "vs0:/sys/external/np_message.suprx");
	dumpModuleByPath("2800000000028074", "vs0:/sys/external/np_message_contacts.suprx");
	dumpModuleByPath("2800000000028075", "vs0:/sys/external/np_message_dialog_impl.suprx");
	dumpModuleByPath("2800000000028095", "vs0:/sys/external/np_message_padding.suprx");
	dumpModuleByPath("2800000000028063", "vs0:/sys/external/np_party.suprx");
	dumpModuleByPath("2800000000028064", "vs0:/sys/external/np_ranking.suprx");
	dumpModuleByPath("280000000002808f", "vs0:/sys/external/np_signaling.suprx");
	dumpModuleByPath("2800000000028084", "vs0:/sys/external/np_sns_facebook.suprx");
	dumpModuleByPath("2800000000028065", "vs0:/sys/external/np_trophy.suprx");
	dumpModuleByPath("2800000000028066", "vs0:/sys/external/np_tus.suprx");
	dumpModuleByPath("2800000000028067", "vs0:/sys/external/np_utility.suprx");
	dumpModuleByPath("28000000000280a6", "vs0:/sys/external/np_webapi.suprx");
	dumpModuleByPath("280000000002808b", "vs0:/sys/external/party_member_list.suprx");
	dumpModuleByPath("280000000002808e", "vs0:/sys/external/psmkdc.suprx");
	dumpModuleByPath("280000000002808c", "vs0:/sys/external/pspnet_adhoc.suprx");
	dumpModuleByPath("28000000000280ab", "vs0:/sys/external/signin_ext.suprx");
	dumpModuleByPath("280000000002806e", "vs0:/sys/external/sqlite.suprx");
	dumpModuleByPath("280000000002806f", "vs0:/sys/external/store_checkout_plugin.suprx");
	dumpModuleByPath("2800000000024002", "vs0:/sys/external/trigger_util.suprx");
	dumpModuleByPath("2800000000028073", "vs0:/sys/external/web_ui_plugin.suprx");
	dumpModuleByPath("2800000000020002", "vs0:/vsh/common/app_settings.suprx");
	dumpModuleByPath("2800000000020003", "vs0:/vsh/common/auth_plugin.suprx");
	dumpModuleByPath("2800000000020004", "vs0:/vsh/common/av_content_handler.suprx");
	dumpModuleByPath("280000000002002a", "vs0:/vsh/common/backup_restore.suprx");
	dumpModuleByPath("2800000000020006", "vs0:/vsh/common/content_operation.suprx");
	dumpModuleByPath("2800000000020007", "vs0:/vsh/common/dbrecovery_plugin.suprx");
	dumpModuleByPath("2800000000020008", "vs0:/vsh/common/dbsetup.suprx");
	dumpModuleByPath("280000000002003c", "vs0:/vsh/common/libBEAVCorePlayer.suprx");
	dumpModuleByPath("280000000002000f", "vs0:/vsh/common/libFflMp4.suprx");
	dumpModuleByPath("2800000000020044", "vs0:/vsh/common/libical.suprx");
	dumpModuleByPath("2800000000020043", "vs0:/vsh/common/libicalss.suprx");
	dumpModuleByPath("280000000002002c", "vs0:/vsh/common/libmarlin.suprx");
	dumpModuleByPath("2800000000020013", "vs0:/vsh/common/libmarlindownloader.suprx");
	dumpModuleByPath("280000000002002d", "vs0:/vsh/common/libmarlin_pb.suprx");
	dumpModuleByPath("2800000000020014", "vs0:/vsh/common/libmtp.suprx");
	dumpModuleByPath("280000000002003a", "vs0:/vsh/common/libmtphttp.suprx");
	dumpModuleByPath("280000000002003b", "vs0:/vsh/common/libmtphttp_wrapper.suprx");
	dumpModuleByPath("2800000000020037", "vs0:/vsh/common/libSenvuabsFFsdk.suprx");
	dumpModuleByPath("2800000000020016", "vs0:/vsh/common/libvideoprofiler.suprx");
	dumpModuleByPath("2800000000020038", "vs0:/vsh/common/mail_api_for_local.suprx");
	dumpModuleByPath("2800000000020001", "vs0:/vsh/common/mms/AACPromoter.suprx");
	dumpModuleByPath("2800000000020005", "vs0:/vsh/common/mms/bmp_promoter.suprx");
	dumpModuleByPath("280000000002000b", "vs0:/vsh/common/mms/gif_promoter.suprx");
	dumpModuleByPath("280000000002000e", "vs0:/vsh/common/mms/jpeg_promoter.suprx");
	dumpModuleByPath("280000000002001a", "vs0:/vsh/common/mms/meta_gen.suprx");
	dumpModuleByPath("280000000002001b", "vs0:/vsh/common/mms/Mp3Promoter.suprx");
	dumpModuleByPath("280000000002001c", "vs0:/vsh/common/mms/MsvPromoter.suprx");
	dumpModuleByPath("2800000000020020", "vs0:/vsh/common/mms/png_promoter.suprx");
	dumpModuleByPath("2800000000020022", "vs0:/vsh/common/mms/RiffPromoter.suprx");
	dumpModuleByPath("2800000000020023", "vs0:/vsh/common/mms/SensMe.suprx");
	dumpModuleByPath("2800000000020027", "vs0:/vsh/common/mms/tiff_promoter.suprx");
	dumpModuleByPath("280000000002001d", "vs0:/vsh/common/mtpr3.suprx");
	dumpModuleByPath("280000000002001e", "vs0:/vsh/common/mtp_client.suprx");
	dumpModuleByPath("280000000002002f", "vs0:/vsh/common/np_grief_report.suprx");
	dumpModuleByPath("280000000002003f", "vs0:/vsh/game/gamecard_installer_plugin.suprx");
	dumpModuleByPath("2800000000020009", "vs0:/vsh/game/gamedata_plugin.suprx");
	dumpModuleByPath("2800000000000014", "vs0:/vsh/initialsetup/initialsetup.self");
	dumpModuleByPath("280000000002003e", "vs0:/vsh/online_storage/online_storage_plugin.suprx");
	dumpModuleByPath("2800000000020029", "vs0:/vsh/shell/auth_reset_plugin.suprx");
	dumpModuleByPath("280000000002002e", "vs0:/vsh/shell/idu_update_plugin.suprx");
	dumpModuleByPath("280000000002000c", "vs0:/vsh/shell/ime_plugin.suprx");
	dumpModuleByPath("280000000002000d", "vs0:/vsh/shell/impose_net_plugin.suprx");
	dumpModuleByPath("280000000002003d", "vs0:/vsh/shell/liblocation_dolce_provider.suprx");
	dumpModuleByPath("2800000000020010", "vs0:/vsh/shell/liblocation_permission.suprx");
	dumpModuleByPath("2800000000020018", "vs0:/vsh/shell/livespace_db.suprx");
	dumpModuleByPath("2800000000020019", "vs0:/vsh/shell/location_dialog_plugin.suprx");
	dumpModuleByPath("2800000000000001", "vs0:/vsh/shell/shell.self");
	dumpModuleByPath("2800000000020025", "vs0:/vsh/shell/telephony/initial_check/tel_initial_check_plugin.suprx");

	
	
	psvDebugScreenPrintf("Done\n");

_exit_:
	
	while (1) {}

	sceKernelExitProcess(0);
	return 0;
}
