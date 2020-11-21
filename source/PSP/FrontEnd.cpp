#include <pspkernel.h>

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdarg.h>
#include <errno.h>
#include <string.h>
#include <time.h>
#include <pspdebug.h>
#include <math.h>
#include <string.h>
#include <pspctrl.h>
#include <pspgu.h>
#include "FrontEnd.h"
#include <psprtc.h>
#include <pspdisplay.h>

#include "PSPDisplay.h"
//#include "Version.h"

configP configparms[30];
int totalconfig=0;
int totalconfigDebug=0;


void InitConfigParms(configured_features * params){
	int c=0;

    strcpy(configparms[c].name,"Screen <--> SWAP");
	params->swap = configparms[c].var;
	c++;
	strcpy(configparms[c].name,"Show Touch Cursor");
	params->cur = configparms[c].var;
	c++;
	strcpy(configparms[c].name,"Show FPS");
	params->showfps = configparms[c].var;
	c++;
#ifndef LOWRAM
	strcpy(configparms[c].name,"Enable Audio");
	params->enable_sound = configparms[c].var;
	c++;
#endif
	strcpy(configparms[c].name,"3D Frameskip");
	params->frameskip = configparms[c].var;
	c++;
    strcpy(configparms[c].name,"Language");
	params->firmware_language = configparms[c].var;
	c++;
	strcpy(configparms[c].name,"Render 3D");
	params->Render3D = configparms[c].var;
	c++;
	strcpy(configparms[c].name, "Emulate ARM on ME");
	params->ARM_ME = configparms[c].var;
	c++;
	strcpy(configparms[c].name, "Hide screen");
	params->hide_screen = configparms[c].var;
	c++;
	strcpy(configparms[c].name, "Perfect VBlank IRQ");
	params->PerFectVTiming = configparms[c].var;
	c++;
	
	totalconfig = c;
	
	/*strcpy(configparms[c].name, "Disable 3d calculation");
	params->Disable_3D_calc = configparms[c].var;
	c++;
	strcpy(configparms[c].name, "Disable ARM7");
	params->D_ARM7 = configparms[c].var;
	c++;*/
	/*strcpy(configparms[c].name,"Max FPS");
	params->fps_cap_num = configparms[c].var;
	c++;*/

	//
	
}

bool changed = false;

void InitDisplayParams(configured_features* params) {

	if (changed) return;

	int c = totalconfig;
	strcpy(configparms[c].name, "BG0 TOP");
	params->gpuLayerEnabled[0][0] = true;
	c++;
	strcpy(configparms[c].name, "BG1 TOP");
	params->gpuLayerEnabled[0][1] = true;
	c++;
	strcpy(configparms[c].name, "BG2 TOP");
	params->gpuLayerEnabled[0][2] = true;
	c++;
	strcpy(configparms[c].name, "BG3 TOP");
	params->gpuLayerEnabled[0][3] = true;
	c++;
	strcpy(configparms[c].name, "OBJ TOP");
	params->gpuLayerEnabled[0][4] = true;
	c++;

	strcpy(configparms[c].name, "BG0 Bottom");
	params->gpuLayerEnabled[1][0] = true;
	c++;
	strcpy(configparms[c].name, "BG1 Bottom");
	params->gpuLayerEnabled[1][1] = true;
	c++;
	strcpy(configparms[c].name, "BG2 Bottom");
	params->gpuLayerEnabled[1][2] = true;
	c++;
	strcpy(configparms[c].name, "BG3 Bottom");
	params->gpuLayerEnabled[1][3] = true;
	c++;
	strcpy(configparms[c].name, "OBJ Bottom");
	params->gpuLayerEnabled[1][4] = true;
	c++;

	strcpy(configparms[c].name, "Disattiva Sprite 3D");
	params->Dspr3D = false;
	c++;

	strcpy(configparms[c].name, "Half 3D");
	params->half3D = false;
	c++;
	totalconfigDebug = c;
}

void ChangeValueDebug(configured_features* params) {
	int c = totalconfig;
	strcpy(configparms[c].name, "BG0 TOP");
	params->gpuLayerEnabled[0][0] = configparms[c].var;
	c++;
	strcpy(configparms[c].name, "BG1 TOP");
	params->gpuLayerEnabled[0][1] = configparms[c].var;
	c++;
	strcpy(configparms[c].name, "BG2 TOP");
	params->gpuLayerEnabled[0][2] = configparms[c].var;
	c++;
	strcpy(configparms[c].name, "BG3 TOP");
	params->gpuLayerEnabled[0][3] = configparms[c].var;
	c++;
	strcpy(configparms[c].name, "OBJ TOP");
	params->gpuLayerEnabled[0][4] = configparms[c].var;
	c++;

	strcpy(configparms[c].name, "BG0 Bottom");
	params->gpuLayerEnabled[1][0] = configparms[c].var;
	c++;
	strcpy(configparms[c].name, "BG1 Bottom");
	params->gpuLayerEnabled[1][1] = configparms[c].var;
	c++;
	strcpy(configparms[c].name, "BG2 Bottom");
	params->gpuLayerEnabled[1][2] = configparms[c].var;
	c++;
	strcpy(configparms[c].name, "BG3 Bottom");
	params->gpuLayerEnabled[1][3] = configparms[c].var;
	c++;
	strcpy(configparms[c].name, "OBJ Bottom");
	params->gpuLayerEnabled[1][4] = configparms[c].var;
	c++;

	strcpy(configparms[c].name, "Disattiva Sprite 3D");
	params->Dspr3D = configparms[c].var;
	c++;
	
	strcpy(configparms[c].name, "Half 3D");
	params->half3D = configparms[c].var;
	c++;
	totalconfigDebug = c;
}

int selposconfig=0;
int selposDebug=0;

void DisplayConfigParms(){
	int c;

	for (c=0;c<totalconfig;c++){
	 	
	    if(selposconfig == c)
		{
			pspDebugScreenSetTextColor(0x0000ffff); // Yellow
		}else {
			pspDebugScreenSetTextColor(0xffffffff); // red
		}
		pspDebugScreenPrintf("  %s :  %d\n",configparms[c].name,configparms[c].var);
		
	}

}

void DisplayDebugParms() {
	int c;

	for (c = totalconfig;c < totalconfigDebug;c++) {

		if (selposDebug == c)
		{
			pspDebugScreenSetTextColor(0x0f00ffff); // Yellow
		}
		else {
			pspDebugScreenSetTextColor(0xff00f55f); // red
		}
		pspDebugScreenPrintf("  %s :  %d\n", configparms[c].name, configparms[c].var);

	}
}

int frameposconfig = 1;
int frameDMAconfig = 10;
int langposconfig = 0;
int screenpos = 0;

void Debug(configured_features* params)
{
	int done = 0;
	int debug_sel = 0; 
	SceCtrlData pad, oldPad;

	pspDebugScreenSetXY(0, 0);
	selposDebug = totalconfig;

	//for (cnt=0;cnt<100;cnt++)//pspDebugScreenPrintf("\n");
	while (!done) {
		sceDisplayWaitVblankStart();
		pspDebugScreenSetTextColor(0xffffffff);
		pspDebugScreenSetXY(0, 3);
		pspDebugScreenPrintf("\n");
		pspDebugScreenPrintf("\n");
		pspDebugScreenPrintf("  Debug:\n\n");
		pspDebugScreenPrintf("  Disable Rendering: BG0 - BG1 - BG2 - BG3 - OBJ\n");
		pspDebugScreenPrintf("\n");
		pspDebugScreenPrintf("\n");

		ChangeValueDebug(params);
		DisplayDebugParms();
	
		if (sceCtrlPeekBufferPositive(&pad, 1))
		{
			if (pad.Buttons != oldPad.Buttons)
			{
				if (pad.Buttons & PSP_CTRL_LEFT)
				{
					configparms[selposDebug].var = 0;
				}
				else if (pad.Buttons & PSP_CTRL_RIGHT) {
					configparms[selposDebug].var = 1;
					changed = true;
				}
				if (pad.Buttons & PSP_CTRL_START) {
					pspDebugScreenSetTextColor(0xffffffff);
					done = 1;
					return;
				}
				if (pad.Buttons & PSP_CTRL_UP) {
					selposDebug--;
					if (selposDebug < totalconfig)selposDebug = totalconfig;
				}
				if (pad.Buttons & PSP_CTRL_DOWN) {
					selposDebug++;
					if (selposDebug >= totalconfigDebug - 1)selposDebug = totalconfigDebug - 1;
				}

			}
			oldPad = pad;
		}

	}

	pspDebugScreenSetTextColor(0xffffffff);

}

void DoConfig(configured_features * params)
{
	int done = 0;
	SceCtrlData pad, oldPad;

	pspDebugScreenSetXY(0, 0);

#ifdef LOWRAM
	configparms[5].var = 1;
#else
	configparms[6].var = 1;
#endif // LOWRAM

	

	int cnt;
	//for (cnt=0;cnt<100;cnt++)//pspDebugScreenPrintf("\n");
	while (!done) {
		sceDisplayWaitVblankStart();
		pspDebugScreenSetTextColor(0xffffffff);
		pspDebugScreenSetXY(0, 3);
		pspDebugScreenPrintf("\n");
		pspDebugScreenPrintf("\n");
		pspDebugScreenPrintf("  CONFIG:\n\n");
		//pspDebugScreenPrintf("  Disabling gfx checks, you will get some frames \n  at the cost of some gliches \n\n");
		pspDebugScreenPrintf("  Lang config: 0 = JAP, 1 = ENG, 2 = FRE, 3 = GER,\n");
		pspDebugScreenPrintf("  4 = ITA, 5 = SPA, 6 = CHI, 7 = RES\n\n");
		pspDebugScreenPrintf("  Hide Screen: 1 = Bottom, 2 = Top   \n\n");
		pspDebugScreenPrintf("  Perfect VBlank IRQ: Enable to fix some vertical moving glitches\n");
		//pspDebugScreenPrintf("  So enable it if you really don't need it\n");
		pspDebugScreenPrintf("\n");
		pspDebugScreenPrintf("\n");
		InitConfigParms(params);
		DisplayConfigParms();
		if (sceCtrlPeekBufferPositive(&pad, 1))
		{
			if (pad.Buttons != oldPad.Buttons)
			{
				if (pad.Buttons & PSP_CTRL_LEFT)
				{
					if (strcmp(configparms[selposconfig].name, "Language") == 0)
					{
						configparms[selposconfig].var = langposconfig--;
						if (langposconfig == -1)langposconfig = 0;
					}
					else
						if (strcmp(configparms[selposconfig].name, "3D Frameskip") == 0)
						{
							configparms[selposconfig].var = frameposconfig--;
							if (frameposconfig == -1)frameposconfig = 0;
						}
						else
						if (strcmp(configparms[selposconfig].name, "Hide screen") == 0)
						{
							configparms[selposconfig].var = (screenpos--);
							if (screenpos < 0) screenpos = 0;
						}
						else
						{
							configparms[selposconfig].var = 0;
						}
				}
				else
					if (pad.Buttons & PSP_CTRL_RIGHT) {
						if (strcmp(configparms[selposconfig].name, "Language") == 0)
						{
							configparms[selposconfig].var = langposconfig++;
							if (langposconfig == 8)langposconfig = 7;
						}
						else
							if (strcmp(configparms[selposconfig].name, "3D Frameskip") == 0)
							{
								configparms[selposconfig].var = frameposconfig++;
								if (frameposconfig == 10)frameposconfig = 9;
							}
							else
							if (strcmp(configparms[selposconfig].name, "Hide screen") == 0)
							{
								configparms[selposconfig].var = (screenpos++);
								if (screenpos > 2) screenpos = 2;
							}
							else
							{
								configparms[selposconfig].var = 1;
							}
					}
				if (pad.Buttons & PSP_CTRL_START) {
					done = 1;//delay
					break;
				}
				if (pad.Buttons & PSP_CTRL_SELECT) {
				//#ifdef DEB
					Debug(params);
				//#endif
				}
				
				if (pad.Buttons & PSP_CTRL_UP) {
					selposconfig--;
					if (selposconfig < 0)selposconfig = 0;
				}
				if (pad.Buttons & PSP_CTRL_DOWN) {
					selposconfig++;
					if (selposconfig >= totalconfig - 1)selposconfig = totalconfig - 1;
				}

			}
			oldPad = pad;
		}

	}

	pspDebugScreenSetTextColor(0xffffffff);

}

//5SM2SF




f_list filelist;

void ClearFileList(){
	filelist.cnt =0;
}


int HasExtension(char *filename){
	if(filename[strlen(filename)-4] == '.'){
		return 1;
	}
	return 0;
}


void GetExtension(const char *srcfile,char *outext){
	if(HasExtension((char *)srcfile)){
		strcpy(outext,srcfile + strlen(srcfile) - 3);
	}else{
		strcpy(outext,"");
	}
}

enum {
	EXT_NDS = 1,
	EXT_GZ = 2,
	EXT_ZIP = 4,
	EXT_UNKNOWN = 8,
};

const struct {
	char *szExt;
	int nExtId;
} stExtentions[] = {
	{"nds",EXT_NDS},
//	{"gz",EXT_GZ},
//	{"zip",EXT_ZIP},
	{NULL, EXT_UNKNOWN}
};

int getExtId(const char *szFilePath) {
	char *pszExt;

	if ((pszExt = strrchr(szFilePath, '.'))) {
		pszExt++;
		int i;
		for (i = 0; stExtentions[i].nExtId != EXT_UNKNOWN; i++) {
			if (!strcasecmp(stExtentions[i].szExt,pszExt)) {
				return stExtentions[i].nExtId;
			}
		}
	}

	return EXT_UNKNOWN;
}

void GetFileList(const char *root)
{
	int dfd;
	dfd = sceIoDopen(root);
	if(dfd > 0){
		SceIoDirent dir;
		while(sceIoDread(dfd, &dir) > 0)
		{
			if(dir.d_stat.st_attr & FIO_SO_IFDIR)
			{
				//directories
			}
			else
			{				
				if(getExtId(dir.d_name)!= EXT_UNKNOWN){
				strcpy(filelist.fname[filelist.cnt].name,dir.d_name);
				filelist.cnt++;
				}
			
			}
		}
		sceIoDclose(dfd);
	}
}
//#include "RomIcon.h"

int selpos=0, oldpos = -1, oldpage = 0;
void DisplayFileList(char* root)
{

	/*DrawRom(root, &filelist, selpos, true);
	return;*/

	static const int MAX_ROM = filelist.cnt;
	static const int ROM_SHOWN = 20;

	const int CURR_PAGE = (selpos / ROM_SHOWN);

	//const int HOW_MANY = (MAX_ROM - (CURR_PAGE * 15));

	const int index = CURR_PAGE * ROM_SHOWN;

	if (selpos == oldpos) return;

	//printf("Curr page: %d, index: %d \n", CURR_PAGE,index);

	//pspDebugScreenClear();
	//DrawRom(root, &filelist, selpos, true);

	bool max_reached = false;

	oldpos = selpos;
	
	/*if (oldpage != CURR_PAGE)
		pspDebugScreenClear();

	oldpage = CURR_PAGE;*/


	if (CURR_PAGE > 0)
		pspDebugScreenPrintf("\nBack\n");
	else 
		pspDebugScreenPrintf("\n\n");


	for (int c = index;c < index + ROM_SHOWN;c++) {

		if (c >= MAX_ROM) {
			pspDebugScreenPrintf("\n");
			max_reached = true;
			continue;
		}

		if (selpos == c) {
			pspDebugScreenSetTextColor(0x0000ffff);
		}
		else {
			pspDebugScreenSetTextColor(0xffffffff);
		}

		pspDebugScreenPrintf("\n%s", filelist.fname[c].name);
	}

	pspDebugScreenSetTextColor(0xffffffff);

	if(!max_reached)
		pspDebugScreenPrintf("\n\nNext Page");
	else
		pspDebugScreenPrintf("\n\n\n");
}

int X = 0, Page = 0,OldX = -1, OldPage = -1;

void DisplayFileListGui(char* root) {

	if (X == OldX) return;
	if (X < 0) X = 0;

	OldPage = Page;
	OldX = X;

	pspDebugScreenClear();
	DrawRom(root, &filelist, X, true);
}

void DSEmuGui(char *path,char *out)
{
	char tmp[256];
	char app_path[128];
	char romname[256];

	SceCtrlData pad,oldPad;
	
	ClearFileList();

	getcwd(app_path,128);

	sprintf(tmp,"%s/ROMS/",app_path);

	GetFileList(tmp);

	sceKernelDelayThread(100);

	while(1){
		sceDisplayWaitVblankStart();
		DisplayFileListGui(tmp);
		if(sceCtrlPeekBufferPositive(&pad, 1))
		{
			if (pad.Buttons != oldPad.Buttons)
			{
				if(pad.Buttons & PSP_CTRL_SQUARE){
			      sceKernelExitGame();
				}

				if(pad.Buttons & PSP_CTRL_CROSS)
				{
					if (X < 0) X = 0;
					SetupDisp_EMU();
					sprintf(out,"%s/%s",tmp,filelist.fname[X].name);	
					X = -1;
					break;
				}
			
				if(pad.Buttons & PSP_CTRL_UP){
					/*selpos--;
					if(selpos < 0)selpos=0;*/
					X -= 3;
					if (X < 0) X = 0;
				}else 
				if(pad.Buttons & PSP_CTRL_DOWN){
					/*selpos++;
					if(selpos >= filelist.cnt -1)selpos=filelist.cnt-1;*/
					X += 3;
					if (X >= filelist.cnt - 1) X = filelist.cnt - 1;
				}else
				if (pad.Buttons & PSP_CTRL_LEFT) {
					/*selpos -= 10;
					if(selpos < 0)selpos=0;*/
					--X;
					if (X < 0) X = 0;
				}else
				if (pad.Buttons & PSP_CTRL_RIGHT) {
					/*selpos+=10;
					if(selpos >= filelist.cnt -1)selpos=filelist.cnt-1;*/
					++X;
					if (X >= filelist.cnt - 1)X = filelist.cnt - 1;
				}

			}
			sceKernelDelayThread(100);
			oldPad = pad;
		}

	}
}

