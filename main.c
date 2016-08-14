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
	
	
	
	/*
	TESTKIT ONLY
	vs0:app/NPXS10095/stitch_core_prx.suprx->2800000000020041 
	vs0:app/NPXS10095/stitch_prx.suprx->2800000000020042 
	vs0:vsh/shell/liblocation_provider.suprx->2800000000020011 
	*/
	/*
	dumpModuleByPath("2808000000020002", "os0:kd/acmgr.skprx");
	dumpModuleByPath("2808000000020007", "os0:kd/authmgr.skprx");
	dumpModuleByPath("2808000000020060", "os0:kd/bootimage.skprx");
	dumpModuleByPath("280800000002000d", "os0:kd/buserror.skprx");
	dumpModuleByPath("2808000000020064", "os0:kd/crashdump.skprx");
	dumpModuleByPath("2808000000020014", "os0:kd/display.skprx");
	dumpModuleByPath("2808000000020015", "os0:kd/dmacmgr.skprx");
	dumpModuleByPath("280800000002005c", "os0:kd/enum_wakeup.skprx");
	dumpModuleByPath("2808000000020017", "os0:kd/excpmgr.skprx");
	dumpModuleByPath("2808000000020018", "os0:kd/exfatfs.skprx");
	dumpModuleByPath("280800000002005e", "os0:kd/gcauthmgr.skprx");
	dumpModuleByPath("280800000002001f", "os0:kd/gpucoredump_es4.skprx");
	dumpModuleByPath("2808000000020022", "os0:kd/hdmi.skprx");
	dumpModuleByPath("2808000000020025", "os0:kd/intrmgr.skprx");
	dumpModuleByPath("2808000000020026", "os0:kd/iofilemgr.skprx");
	dumpModuleByPath("280800000002006e", "os0:kd/krm.skprx");
	dumpModuleByPath("2808000000020066", "os0:kd/lcd.skprx");
	dumpModuleByPath("2808000000020028", "os0:kd/lowio.skprx");
	dumpModuleByPath("2808000000000201", "os0:kd/magicgate.skprx");
	dumpModuleByPath("280800000002005f", "os0:kd/marlin_hci.skprx");
	dumpModuleByPath("2808000000000201", "os0:kd/mgkeymgr.skprx");
	dumpModuleByPath("2808000000000202", "os0:kd/mgvideo.skprx");
	dumpModuleByPath("280800000002002a", "os0:kd/modulemgr.skprx");
	dumpModuleByPath("280800000002002c", "os0:kd/msif.skprx");
	dumpModuleByPath("2808000000020031", "os0:kd/oled.skprx");
	dumpModuleByPath("2808000000000100", "os0:kd/pcbc.skprx");
	dumpModuleByPath("2808000000020036", "os0:kd/processmgr.skprx");
	dumpModuleByPath("2808000000020039", "os0:kd/rtc.skprx");
	dumpModuleByPath("280800000002003b", "os0:kd/sdif.skprx");
	dumpModuleByPath("280800000002003c", "os0:kd/sdstor.skprx");
	dumpModuleByPath("280800000002003e", "os0:kd/smsc_proxy.skprx");
	dumpModuleByPath("280800000002003d", "os0:kd/sm_comm.skprx");
	dumpModuleByPath("280800000002003f", "os0:kd/ss_mgr.skprx");
	dumpModuleByPath("2808000000020043", "os0:kd/syscon.skprx");
	dumpModuleByPath("2808000000020044", "os0:kd/sysmem.skprx");
	dumpModuleByPath("2808000000020046", "os0:kd/sysstatemgr.skprx");
	dumpModuleByPath("2808000000020047", "os0:kd/systimer.skprx");
	dumpModuleByPath("2808000000020048", "os0:kd/threadmgr.skprx");
	dumpModuleByPath("2809000000000008", "os0:kd/usbdev_serial.skprx");
	dumpModuleByPath("2808000000020063", "os0:kd/usbpspcm.skprx");
	dumpModuleByPath("2808000000020061", "os0:kd/usbstor.skprx");
	dumpModuleByPath("2808000000000203", "os0:kd/usbstormg.skprx");
	dumpModuleByPath("2808000000020062", "os0:kd/usbstorvstor.skprx");
	dumpModuleByPath("2808000000020052", "os0:kd/vipimg.skprx");
	dumpModuleByPath("2808000000020054", "os0:kd/vnzimg.skprx");
	dumpModuleByPath("2808000000020057", "os0:kd/wlanbt_robin_img_ax.skprx");
	dumpModuleByPath("280800000002005b", "os0:psp2bootconfig.skprx");
	dumpModuleByPath("2808000000020059", "os0:psp2config_dolce.skprx");
	dumpModuleByPath("2808000000020059", "os0:psp2config_vita.skprx");
	dumpModuleByPath("2e0000000000000a", "os0:sm/act_sm.self");
	dumpModuleByPath("2e00000000000007", "os0:sm/aimgr_sm.self");
	dumpModuleByPath("2e00000000000008", "os0:sm/compat_sm.self");
	dumpModuleByPath("2e0000000000000c", "os0:sm/encdec_w_portability_sm.self");
	dumpModuleByPath("2e0000000000000d", "os0:sm/gcauthmgr_sm.self");
	dumpModuleByPath("2e0000000000000b", "os0:sm/mgkm_sm.self");
	dumpModuleByPath("2e00000000000006", "os0:sm/pm_sm.self");
	dumpModuleByPath("2e00000000000005", "os0:sm/qaf_sm.self");
	dumpModuleByPath("2e0000000000000e", "os0:sm/rmauth_sm.self");
	dumpModuleByPath("2e00000000000010", "os0:sm/spkg_verifier_sm_w_key_2.self");
	dumpModuleByPath("2e00000000000004", "os0:sm/update_service_sm.self");
	dumpModuleByPath("2e0000000000000f", "os0:sm/utoken_sm.self");
	dumpModuleByPath("2800000000000015", "os0:ue/safemode.self");
	*/
	dumpModuleByPath("2800000000028005", "os0:us/avcodec_us.suprx");
	dumpModuleByPath("280000000002800a", "os0:us/driver_us.suprx");
	dumpModuleByPath("280000000002802a", "os0:us/libgpu_es4.suprx");
	dumpModuleByPath("2800000000028030", "os0:us/libgxm_es4.suprx");
	dumpModuleByPath("2800000000028034", "os0:us/libkernel.suprx");
	//dumpModuleByPath("1e6c89bb6fd70485", "pd0:app/NPXS10007/sce_module/libc.suprx");
	//dumpModuleByPath("3c3b85ca044fab22", "pd0:app/NPXS10007/sce_module/libfios2.suprx");
	//dumpModuleByPath("9ce10e890f276561", "pd0:app/NPXS10007/sce_module/libult.suprx");
	dumpModuleByPath("280000000002001f", "vs0:app/NPXS10001/np_party_app.suprx");
	dumpModuleByPath("2800000000020045", "vs0:app/NPXS10013/gaikai-player.suprx");
	dumpModuleByPath("2800000000020046", "vs0:app/NPXS10013/libSceSecondScreen.suprx");
	dumpModuleByPath("2800000000020024", "vs0:app/NPXS10015/system_settings_core.suprx");
	dumpModuleByPath("2800000000020026", "vs0:app/NPXS10021/tel_reg.suprx");
	//dumpModuleByPath("2808000000000101", "vs0:app/NPXS10028/pcff.skprx");
	dumpModuleByPath("2800000000020030", "vs0:app/NPXS10065/grief_report_dialog.suprx");
	dumpModuleByPath("2800000000020035", "vs0:app/NPXS10072/email_engine.suprx");
	//dumpModuleByPath("2800800000000015", "vs0:app/NPXS10082/spawn.self");
	dumpModuleByPath("2800000000020045", "vs0:app/NPXS10098/gaikai-player.suprx");
	dumpModuleByPath("280000000002807f", "vs0:data/external/webcore/jx_web_filtering.suprx");
	dumpModuleByPath("2800000000028097", "vs0:data/external/webcore/ScePsp2Compat.suprx");
	dumpModuleByPath("2800000000028099", "vs0:data/external/webcore/SceWebKitModule.suprx");
	dumpModuleByPath("280000000002807e", "vs0:data/external/webcore/vita_jsextobj.suprx");
	dumpModuleByPath("2800000000028001", "vs0:sys/external/activity_db.suprx");
	dumpModuleByPath("2800000000028002", "vs0:sys/external/adhoc_matching.suprx");
	dumpModuleByPath("2800000000028003", "vs0:sys/external/apputil.suprx");
	dumpModuleByPath("28000000000280a5", "vs0:sys/external/apputil_ext.suprx");
	dumpModuleByPath("2800000000028004", "vs0:sys/external/audiocodec.suprx");
	dumpModuleByPath("28000000000280ac", "vs0:sys/external/avcdec_for_player.suprx");
	dumpModuleByPath("2800000000028086", "vs0:sys/external/bgapputil.suprx");
	dumpModuleByPath("2800000000028007", "vs0:sys/external/bXCe.suprx");
	dumpModuleByPath("2800000000028008", "vs0:sys/external/common_gui_dialog.suprx");
	dumpModuleByPath("2800000000028080", "vs0:sys/external/dbrecovery_utility.suprx");
	dumpModuleByPath("2800000000028009", "vs0:sys/external/dbutil.suprx");
	dumpModuleByPath("28000000000280ae", "vs0:sys/external/friend_select.suprx");
	dumpModuleByPath("2800000000028087", "vs0:sys/external/incoming_dialog.suprx");
	dumpModuleByPath("280000000002800c", "vs0:sys/external/ini_file_processor.suprx");
	dumpModuleByPath("2800000000028083", "vs0:sys/external/libatrac.suprx");
	dumpModuleByPath("280000000002800e", "vs0:sys/external/libc.suprx");
	dumpModuleByPath("280000000002800f", "vs0:sys/external/libcdlg.suprx");
	dumpModuleByPath("28000000000280aa", "vs0:sys/external/libcdlg_calendar_review.suprx");
	dumpModuleByPath("2800000000028079", "vs0:sys/external/libcdlg_cameraimport.suprx");
	dumpModuleByPath("2800000000028010", "vs0:sys/external/libcdlg_checkout.suprx");
	dumpModuleByPath("2800000000017003", "vs0:sys/external/libcdlg_companion.suprx");
	dumpModuleByPath("280000000002807c", "vs0:sys/external/libcdlg_compat.suprx");
	dumpModuleByPath("2800000000028090", "vs0:sys/external/libcdlg_cross_controller.suprx");
	dumpModuleByPath("2800000000028011", "vs0:sys/external/libcdlg_friendlist.suprx");
	dumpModuleByPath("28000000000280a8", "vs0:sys/external/libcdlg_friendlist2.suprx");
	dumpModuleByPath("28000000000280b1", "vs0:sys/external/libcdlg_game_custom_data.suprx");
	dumpModuleByPath("28000000000280b2", "vs0:sys/external/libcdlg_game_custom_data_impl.suprx");
	dumpModuleByPath("2800000000028012", "vs0:sys/external/libcdlg_ime.suprx");
	dumpModuleByPath("28000000000280af", "vs0:sys/external/libcdlg_invitation.suprx");
	dumpModuleByPath("28000000000280b0", "vs0:sys/external/libcdlg_invitation_impl.suprx");
	dumpModuleByPath("2800000000028013", "vs0:sys/external/libcdlg_main.suprx");
	dumpModuleByPath("2800000000028015", "vs0:sys/external/libcdlg_msg.suprx");
	dumpModuleByPath("280000000002809f", "vs0:sys/external/libcdlg_near.suprx");
	dumpModuleByPath("2800000000028017", "vs0:sys/external/libcdlg_netcheck.suprx");
	dumpModuleByPath("2800000000028089", "vs0:sys/external/libcdlg_npeula.suprx");
	dumpModuleByPath("28000000000280a9", "vs0:sys/external/libcdlg_npprofile2.suprx");
	dumpModuleByPath("280000000002801a", "vs0:sys/external/libcdlg_np_message.suprx");
	dumpModuleByPath("2800000000028082", "vs0:sys/external/libcdlg_np_sns_fb.suprx");
	dumpModuleByPath("280000000002801b", "vs0:sys/external/libcdlg_np_trophy_setup.suprx");
	dumpModuleByPath("280000000002801c", "vs0:sys/external/libcdlg_photoimport.suprx");
	dumpModuleByPath("280000000002801d", "vs0:sys/external/libcdlg_photoreview.suprx");
	dumpModuleByPath("2800000000020040", "vs0:sys/external/libcdlg_pocketstation.suprx");
	dumpModuleByPath("2800000000017004", "vs0:sys/external/libcdlg_remote_osk.suprx");
	dumpModuleByPath("280000000002801e", "vs0:sys/external/libcdlg_savedata.suprx");
	dumpModuleByPath("2800000000028096", "vs0:sys/external/libcdlg_twitter.suprx");
	dumpModuleByPath("280000000002809a", "vs0:sys/external/libcdlg_tw_login.suprx");
	dumpModuleByPath("28000000000280b3", "vs0:sys/external/libcdlg_videoimport.suprx");
	dumpModuleByPath("2800000000028020", "vs0:sys/external/libclipboard.suprx");
	dumpModuleByPath("2800000000028023", "vs0:sys/external/libdbg.suprx");
	dumpModuleByPath("2800000000028024", "vs0:sys/external/libfiber.suprx");
	dumpModuleByPath("2800000000028026", "vs0:sys/external/libfios2.suprx");
	dumpModuleByPath("2800000000028027", "vs0:sys/external/libg729.suprx");
	dumpModuleByPath("28000000000280a1", "vs0:sys/external/libgameupdate.suprx");
	dumpModuleByPath("280000000002807a", "vs0:sys/external/libhandwriting.suprx");
	dumpModuleByPath("2800000000028032", "vs0:sys/external/libhttp.suprx");
	dumpModuleByPath("2800000000028033", "vs0:sys/external/libime.suprx");
	dumpModuleByPath("2800000000028088", "vs0:sys/external/libipmi_nongame.suprx");
	dumpModuleByPath("2800000000028036", "vs0:sys/external/liblocation.suprx");
	dumpModuleByPath("280000000002809c", "vs0:sys/external/liblocation_extension.suprx");
	dumpModuleByPath("2800000000028092", "vs0:sys/external/liblocation_factory.suprx");
	dumpModuleByPath("2800000000028091", "vs0:sys/external/liblocation_internal.suprx");
	dumpModuleByPath("280000000002002c", "vs0:sys/external/libmln.suprx");
	dumpModuleByPath("2800000000024003", "vs0:sys/external/libmlnapplib.suprx");
	dumpModuleByPath("2800000000020031", "vs0:sys/external/libmlndownloader.suprx");
	dumpModuleByPath("2800000000028078", "vs0:sys/external/libnaac.suprx");
	dumpModuleByPath("2800000000028037", "vs0:sys/external/libnet.suprx");
	dumpModuleByPath("2800000000028038", "vs0:sys/external/libnetctl.suprx");
	dumpModuleByPath("2800000000028039", "vs0:sys/external/libngs.suprx");
	dumpModuleByPath("280000000002803a", "vs0:sys/external/libpaf.suprx");
	dumpModuleByPath("2800000000020039", "vs0:sys/external/libpaf_web_map_view.suprx");
	dumpModuleByPath("280000000002803c", "vs0:sys/external/libperf.suprx");
	dumpModuleByPath("280000000002803d", "vs0:sys/external/libpgf.suprx");
	dumpModuleByPath("280000000002803e", "vs0:sys/external/libpvf.suprx");
	dumpModuleByPath("2800000000028043", "vs0:sys/external/librudp.suprx");
	dumpModuleByPath("2800000000028044", "vs0:sys/external/libsas.suprx");
	dumpModuleByPath("280000000002809e", "vs0:sys/external/libsceavplayer.suprx");
	dumpModuleByPath("2800000000028093", "vs0:sys/external/libSceBeisobmf.suprx");
	dumpModuleByPath("2800000000028094", "vs0:sys/external/libSceBemp2sys.suprx");
	dumpModuleByPath("2800000000017002", "vs0:sys/external/libSceCompanionUtil.suprx");
	dumpModuleByPath("280000000002808d", "vs0:sys/external/libSceDtcpIp.suprx");
	dumpModuleByPath("2800000000028045", "vs0:sys/external/libSceFt2.suprx");
	dumpModuleByPath("2800000000028046", "vs0:sys/external/libscejpegarm.suprx");
	dumpModuleByPath("2800000000028047", "vs0:sys/external/libscejpegencarm.suprx");
	dumpModuleByPath("28000000000280ad", "vs0:sys/external/libSceJson.suprx");
	dumpModuleByPath("2800000000028076", "vs0:sys/external/libscemp4.suprx");
	dumpModuleByPath("28000000000280a4", "vs0:sys/external/libSceMp4Rec.suprx");
	dumpModuleByPath("2800000000028048", "vs0:sys/external/libSceMusicExport.suprx");
	dumpModuleByPath("280000000002809b", "vs0:sys/external/libSceNearDialogUtil.suprx");
	dumpModuleByPath("2800000000028049", "vs0:sys/external/libSceNearUtil.suprx");
	dumpModuleByPath("280000000002804a", "vs0:sys/external/libScePhotoExport.suprx");
	dumpModuleByPath("2800000000028081", "vs0:sys/external/libScePromoterUtil.suprx");
	dumpModuleByPath("280000000002804b", "vs0:sys/external/libSceScreenShot.suprx");
	dumpModuleByPath("280000000002804c", "vs0:sys/external/libSceShutterSound.suprx");
	dumpModuleByPath("280000000002804d", "vs0:sys/external/libSceSqlite.suprx");
	dumpModuleByPath("280000000002808a", "vs0:sys/external/libSceTelephonyUtil.suprx");
	dumpModuleByPath("28000000000280a3", "vs0:sys/external/libSceTeleportClient.suprx");
	dumpModuleByPath("28000000000280a2", "vs0:sys/external/libSceTeleportServer.suprx");
	dumpModuleByPath("2800000000024001", "vs0:sys/external/libSceVideoExport.suprx");
	dumpModuleByPath("2800000000020034", "vs0:sys/external/libSceVideoSearchEmpr.suprx");
	dumpModuleByPath("280000000002804f", "vs0:sys/external/libSceXml.suprx");
	/*
	dumpModuleByPath("2800000000028050", "vs0:sys/external/libshellsvc.suprx");
	dumpModuleByPath("2800000000028051", "vs0:sys/external/libssl.suprx");
	dumpModuleByPath("2800000000028052", "vs0:sys/external/libsulpha.suprx");
	dumpModuleByPath("2800000000028053", "vs0:sys/external/libsystemgesture.suprx");
	dumpModuleByPath("2800000000028054", "vs0:sys/external/libult.suprx");
	dumpModuleByPath("2800000000028055", "vs0:sys/external/libvoice.suprx");
	dumpModuleByPath("2800000000028056", "vs0:sys/external/libvoiceqos.suprx");
	dumpModuleByPath("2800000000028057", "vs0:sys/external/livearea_util.suprx");
	dumpModuleByPath("28000000000280a0", "vs0:sys/external/mail_api_for_local_libc.suprx");
	dumpModuleByPath("2800000000028058", "vs0:sys/external/near_profile.suprx");
	dumpModuleByPath("2800000000028085", "vs0:sys/external/notification_util.suprx");
	dumpModuleByPath("2800000000028059", "vs0:sys/external/np_activity.suprx");
	dumpModuleByPath("280000000002805a", "vs0:sys/external/np_activity_sdk.suprx");
	dumpModuleByPath("280000000002805b", "vs0:sys/external/np_basic.suprx");
	dumpModuleByPath("280000000002805c", "vs0:sys/external/np_commerce2.suprx");
	dumpModuleByPath("280000000002805d", "vs0:sys/external/np_common.suprx");
	dumpModuleByPath("28000000000280a7", "vs0:sys/external/np_common_ps4.suprx");
	dumpModuleByPath("280000000002805e", "vs0:sys/external/np_friend_privacylevel.suprx");
	dumpModuleByPath("280000000002805f", "vs0:sys/external/np_kdc.suprx");
	dumpModuleByPath("2800000000028060", "vs0:sys/external/np_manager.suprx");
	dumpModuleByPath("2800000000028061", "vs0:sys/external/np_matching2.suprx");
	dumpModuleByPath("2800000000028062", "vs0:sys/external/np_message.suprx");
	dumpModuleByPath("2800000000028074", "vs0:sys/external/np_message_contacts.suprx");
	dumpModuleByPath("2800000000028075", "vs0:sys/external/np_message_dialog_impl.suprx");
	dumpModuleByPath("2800000000028095", "vs0:sys/external/np_message_padding.suprx");
	dumpModuleByPath("2800000000028063", "vs0:sys/external/np_party.suprx");
	dumpModuleByPath("2800000000028064", "vs0:sys/external/np_ranking.suprx");
	dumpModuleByPath("280000000002808f", "vs0:sys/external/np_signaling.suprx");
	dumpModuleByPath("2800000000028084", "vs0:sys/external/np_sns_facebook.suprx");
	dumpModuleByPath("2800000000028065", "vs0:sys/external/np_trophy.suprx");
	dumpModuleByPath("2800000000028066", "vs0:sys/external/np_tus.suprx");
	dumpModuleByPath("2800000000028067", "vs0:sys/external/np_utility.suprx");
	dumpModuleByPath("28000000000280a6", "vs0:sys/external/np_webapi.suprx");
	dumpModuleByPath("280000000002808b", "vs0:sys/external/party_member_list.suprx");
	dumpModuleByPath("280000000002808e", "vs0:sys/external/psmkdc.suprx");
	dumpModuleByPath("280000000002808c", "vs0:sys/external/pspnet_adhoc.suprx");
	dumpModuleByPath("28000000000280ab", "vs0:sys/external/signin_ext.suprx");
	dumpModuleByPath("280000000002806e", "vs0:sys/external/sqlite.suprx");
	dumpModuleByPath("280000000002806f", "vs0:sys/external/store_checkout_plugin.suprx");
	dumpModuleByPath("2800000000024002", "vs0:sys/external/trigger_util.suprx");
	dumpModuleByPath("2800000000028073", "vs0:sys/external/web_ui_plugin.suprx");
	dumpModuleByPath("2800000000020002", "vs0:vsh/common/app_settings.suprx");
	dumpModuleByPath("2800000000020003", "vs0:vsh/common/auth_plugin.suprx");
	dumpModuleByPath("2800000000020004", "vs0:vsh/common/av_content_handler.suprx");
	dumpModuleByPath("280000000002002a", "vs0:vsh/common/backup_restore.suprx");
	dumpModuleByPath("2800000000020006", "vs0:vsh/common/content_operation.suprx");
	dumpModuleByPath("2800000000020007", "vs0:vsh/common/dbrecovery_plugin.suprx");
	dumpModuleByPath("2800000000020008", "vs0:vsh/common/dbsetup.suprx");
	dumpModuleByPath("280000000002003c", "vs0:vsh/common/libBEAVCorePlayer.suprx");
	dumpModuleByPath("280000000002000f", "vs0:vsh/common/libFflMp4.suprx");
	dumpModuleByPath("2800000000020044", "vs0:vsh/common/libical.suprx");
	dumpModuleByPath("2800000000020043", "vs0:vsh/common/libicalss.suprx");
	dumpModuleByPath("280000000002002c", "vs0:vsh/common/libmarlin.suprx");
	dumpModuleByPath("2800000000020013", "vs0:vsh/common/libmarlindownloader.suprx");
	dumpModuleByPath("280000000002002d", "vs0:vsh/common/libmarlin_pb.suprx");
	dumpModuleByPath("2800000000020014", "vs0:vsh/common/libmtp.suprx");
	dumpModuleByPath("280000000002003a", "vs0:vsh/common/libmtphttp.suprx");
	dumpModuleByPath("280000000002003b", "vs0:vsh/common/libmtphttp_wrapper.suprx");
	dumpModuleByPath("2800000000020037", "vs0:vsh/common/libSenvuabsFFsdk.suprx");
	dumpModuleByPath("2800000000020016", "vs0:vsh/common/libvideoprofiler.suprx");
	dumpModuleByPath("2800000000020038", "vs0:vsh/common/mail_api_for_local.suprx");
	dumpModuleByPath("2800000000020001", "vs0:vsh/common/mms/AACPromoter.suprx");
	dumpModuleByPath("2800000000020005", "vs0:vsh/common/mms/bmp_promoter.suprx");
	dumpModuleByPath("280000000002000b", "vs0:vsh/common/mms/gif_promoter.suprx");
	dumpModuleByPath("280000000002000e", "vs0:vsh/common/mms/jpeg_promoter.suprx");
	dumpModuleByPath("280000000002001a", "vs0:vsh/common/mms/meta_gen.suprx");
	dumpModuleByPath("280000000002001b", "vs0:vsh/common/mms/Mp3Promoter.suprx");
	dumpModuleByPath("280000000002001c", "vs0:vsh/common/mms/MsvPromoter.suprx");
	dumpModuleByPath("2800000000020020", "vs0:vsh/common/mms/png_promoter.suprx");
	dumpModuleByPath("2800000000020022", "vs0:vsh/common/mms/RiffPromoter.suprx");
	dumpModuleByPath("2800000000020023", "vs0:vsh/common/mms/SensMe.suprx");
	dumpModuleByPath("2800000000020027", "vs0:vsh/common/mms/tiff_promoter.suprx");
	dumpModuleByPath("280000000002001d", "vs0:vsh/common/mtpr3.suprx");
	dumpModuleByPath("280000000002001e", "vs0:vsh/common/mtp_client.suprx");
	dumpModuleByPath("280000000002002f", "vs0:vsh/common/np_grief_report.suprx");
	dumpModuleByPath("280000000002003f", "vs0:vsh/game/gamecard_installer_plugin.suprx");
	dumpModuleByPath("2800000000020009", "vs0:vsh/game/gamedata_plugin.suprx");
	dumpModuleByPath("2800000000000014", "vs0:vsh/initialsetup/initialsetup.self");
	dumpModuleByPath("280000000002003e", "vs0:vsh/online_storage/online_storage_plugin.suprx");
	dumpModuleByPath("2800000000020029", "vs0:vsh/shell/auth_reset_plugin.suprx");
	dumpModuleByPath("280000000002002e", "vs0:vsh/shell/idu_update_plugin.suprx");
	dumpModuleByPath("280000000002000c", "vs0:vsh/shell/ime_plugin.suprx");
	dumpModuleByPath("280000000002000d", "vs0:vsh/shell/impose_net_plugin.suprx");
	dumpModuleByPath("280000000002003d", "vs0:vsh/shell/liblocation_dolce_provider.suprx");
	dumpModuleByPath("2800000000020010", "vs0:vsh/shell/liblocation_permission.suprx");
	dumpModuleByPath("2800000000020018", "vs0:vsh/shell/livespace_db.suprx");
	dumpModuleByPath("2800000000020019", "vs0:vsh/shell/location_dialog_plugin.suprx");
	dumpModuleByPath("2800000000000001", "vs0:vsh/shell/shell.self");
	dumpModuleByPath("2800000000020025", "vs0:vsh/shell/telephony/initial_check/tel_initial_check_plugin.suprx");
	*/
	psvDebugScreenPrintf("Done\n");

_exit_:
	
	while (1) {}

	sceKernelExitProcess(0);
	return 0;
}
