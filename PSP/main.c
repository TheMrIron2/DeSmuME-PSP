/* main.c - this file is part of DeSmuME
 *
 * Copyright (C) 2006,2007 DeSmuME Team
 * Copyright (C) 2007 Pascal Giard (evilynux)
 * Copyright (C) 2009 Yoshihiro (DsonPSP)
 * This file is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2, or (at your option)
 * any later version.
 *
 * This file is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; see the file COPYING.  If not, write to
 * the Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */
#include <pspkernel.h>
#include <pspdebug.h>
#include <pspdisplay.h>
#include <pspctrl.h>
#include <pspvfpu.h>
#include <stdio.h>
#include "video.h"
#include "MMU.h"
#include "NDSSystem.h"
#include "cflash.h"
#include "debug.h"
#include "sndsdl.h"
#include "ctrlssdl.h"
#include "render3D.h"
#include "gdbstub.h"
#include "FrontEnd.h"
#include "Version.h"
#include <GL/gl.h>
#include <GL/glu.h>


PSP_MODULE_INFO("DSOnPSP", 0, 1, 1);
//PSP_MAIN_THREAD_ATTR(PSP_THREAD_ATTR_VFPU | PSP_THREAD_ATTR_USER);

NDS_header * header;

volatile BOOL execute = FALSE;

static float nds_screen_size_ratio = 1.0f;

#define NUM_FRAMES_TO_TIME 15

#define FPS_LIMITER_FRAME_PERIOD 8

static SDL_Surface * surface;

/* Flags to pass to SDL_SetVideoMode */
static int sdl_videoFlags = 0;
static int sdl_quit = 0;
static u16 keypad;
  
u8 *GPU_vram[512*192*4];
u8 *GPU_mergeA[256*192*4];
u8 *GPU_mergeB[256*192*4];

u32 fps_timing = 0;
u32 fps_frame_counter = 0;
u32 fps_previous_time = 0;
u32 fps_temp_time;
u32 opengl_2d = 0;


#include <pspgu.h>
#include <pspgum.h>
#include <psprtc.h>

SoundInterface_struct *SNDCoreList[] = {
  &SNDDummy,
  &SNDFile,
  &SNDSDL,
  NULL
};

GPU3DInterface *core3DList[] = {
&gpu3DNull
};

int savetype=MC_TYPE_AUTODETECT;
u32 savesize=1;


const char * save_type_names[] = {
    "Autodetect",
    "EEPROM 4kbit",
    "EEPROM 64kbit",
    "EEPROM 512kbit",
    "FRAM 256kbit",
    "FLASH 2mbit",
    "FLASH 4mbit",
    NULL
};


void Gu_draw()
{
	 int i;
	 PspImage *image;
	 u16 *src, *dst,*dstA,*dstB;
     src = (u16*)GPU_screen;
	 dstA = (u16*)GPU_mergeA;
	 dstB = (u16*)GPU_mergeB;
     dst = (u16*)GPU_vram;
   
	 for(i=0; i < 256*192; i++)
	 { 
	 dstA[i] = src[i];           // MainScreen Hack
	 dstB[i] = src[(256*192)+i]; // SubScreen Hack
	 }
    if(swap)
    GuImageDirect(dstB,dstA,0,0,256,192, 0,40, 256, 192);
	else
    GuImageDirect(dstA,dstB,0,0,256,192, 0,40, 256, 192);
}

/* Exit callback */
int exit_callback(int arg1, int arg2, void *common)
{
	sceKernelExitGame();

	return 0;
}

/* Callback thread */
int CallbackThread(SceSize args, void *argp)
{
	int cbid;

	cbid = sceKernelCreateCallback("Exit Callback", exit_callback, NULL);
	sceKernelRegisterExitCallback(cbid);

	sceKernelSleepThreadCB();

	return 0;
}

/* Sets up the callback thread and returns its thread id */
int SetupCallbacks(void)
{
	int thid = 0;

	thid = sceKernelCreateThread("update_thread", CallbackThread, 0x11, 0xFA0, 0, 0);
	if(thid >= 0)
	{
		sceKernelStartThread(thid, 0, 0);
	}

	return thid;
}

  int cycles;

void DSonpspExec()
{  

	sdl_quit = process_ctrls_events( &keypad, NULL, nds_screen_size_ratio);
    
	
    // Update mouse position and click
    if(mouse.down) {
		NDS_setTouchPos(mouse.x, mouse.y);
	}
	
    if(mouse.click)
      { 
        NDS_releasTouch();
        mouse.click = FALSE;
      }

	update_keypad(keypad);     /* Update keypad */

    //cycles = NDS_exec((560190<<1)-cycles,FALSE);
	NDS_exec(FALSE);

	if ( enable_sound) {
		SPU_Emulate();
	}

}


int main(SceSize args, void *argp)
{
  int f;
  struct armcpu_memory_iface *arm9_memio = &arm9_base_memory_iface;
  struct armcpu_memory_iface *arm7_memio = &arm7_base_memory_iface;
  struct armcpu_ctrl_iface *arm9_ctrl_iface;
  struct armcpu_ctrl_iface *arm7_ctrl_iface;
  char rom_filename[256];

  pspDebugScreenInitEx((void*)(0x44000000),PSP_DISPLAY_PIXEL_FORMAT_5551, 1);
  //Overclock 
  scePowerSetClockFrequency(333, 333, 166);
  
  SetupCallbacks();
 
  /* this holds some info about our display */
  const SDL_VideoInfo *videoInfo;

  SceCtrlData pad;
 
  DSEmuGui(argp,rom_filename);
    
  pspDebugScreenClear(); 
  
  DoConfig();

  pspDebugScreenClear();

  cflash_disk_image_file = NULL;


  GuInit();

  /* the firmware settings */
  struct NDS_fw_config_data fw_config;

  NDS_FillDefaultFirmwareConfigData(&fw_config); 


  NDS_Init( arm9_memio, &arm9_ctrl_iface,
            arm7_memio, &arm7_ctrl_iface);
  

    /* Create the dummy firmware */
  NDS_CreateDummyFirmware( &fw_config);

  if ( enable_sound) {
    SPU_ChangeSoundCore(SNDCORE_SDL, 735 * 4);
  }
  
  //rom_filename = "rom.nds";
 mmu_select_savetype(savetype, &savetype, &savesize);


if (NDS_LoadROM( rom_filename  , MC_TYPE_AUTODETECT, 1, cflash_disk_image_file) < 0) {
    SceCtrlData pad;
	printf("Error loading %s\n", rom_filename);
	sceKernelDelayThread(100000);
	do { sceCtrlReadBufferPositive(&pad, 1);
	} while (pad.Buttons == 0);
	sceKernelExitGame();
  }

  execute = TRUE;
  

//    SDL_ShowCursor(SDL_DISABLE);

 
  
  while(!sdl_quit) {
  	
	 // Look for queued events and update keypad status
	if(frameskip > 0)
	for(f= 0; f<frameskip; f++) DSonpspExec();
	else DSonpspExec();

    Gu_draw();

    if(showfps)
     FPS();


	if ( enable_sound) {
		SPU_Emulate();
	}

}

  SDL_Quit();
  NDS_DeInit();
  return 0;
}

/*
int module_start(SceSize args, void *argp)
{
	SceUID uid;

	uid = sceKernelCreateThread("DsOnPSP", _main, 32, 0x10000, 0, 0);
	if(uid < 0)
	{
		return 1;
	}
	sceKernelStartThread(uid, args, argp);

	return 0;
}
*/