
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
#include <pspgu.h>
#include <pspgum.h>
#include <psprtc.h>

#include <GL/gl.h>
#include <GL/glu.h>
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
#include "callbacks.h"
#include "intraFont/libraries/graphics.h"
#include "intraFont/intraFont.h"

PSP_MODULE_INFO("DSOnPSP", 0, 1, 1);
PSP_MAIN_THREAD_ATTR(PSP_THREAD_ATTR_VFPU | PSP_THREAD_ATTR_USER);

const char * save_types[] = {
    "Autodetect",
    "EEPROM 4kbit", // unchanged for compatibility
    "EEPROM 64kbit",
    "EEPROM 512kbit",
    "FRAM 256kbit",
    "FLASH 2mbit",
    "FLASH 4mbit",
    NULL
};

enum colors {
  RED =  0xFF0000FF,
  GREEN =  0xFF00FF00,
  BLUE =  0xFFFF0000,
  WHITE =  0xFFFFFFFF,
  LITEGRAY = 0xFFBFBFBF,
  GRAY =  0xFF7F7F7F,
  DARKGRAY = 0xFF3F3F3F,
  BLACK = 0xFF000000,
};

// Video flags
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


int main() {
    scePowerSetClockFrequency(333, 333, 166);
    pspDebugScreenInit();
    setupCallbacks();

    intraFontInit();

    intraFont* ltn[16];
    intraFontLoad("flash0:/font/ltn4.pgf", 0);
    intraFontSetStyle(ltn4, 1.0f, WHITE, DARKGRAY, 0.f, 0);
    initGraphics();
    clearScreen(GRAY);
    guStart();
}
