/*
	Copyright (C) 2007 Pascal Giard
	Copyright (C) 2007-2011 DeSmuME team

	This file is free software: you can redistribute it and/or modify
	it under the terms of the GNU General Public License as published by
	the Free Software Foundation, either version 2 of the License, or
	(at your option) any later version.

	This file is distributed in the hope that it will be useful,
	but WITHOUT ANY WARRANTY; without even the implied warranty of
	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
	GNU General Public License for more details.

	You should have received a copy of the GNU General Public License
	along with the this software.  If not, see <http://www.gnu.org/licenses/>.
*/

#include "ctrlssdl.h"
#include "saves.h"
#include "SPU.h"
#include "NDSSystem.h"
#include "melib.h"
#include "PSP/FrontEnd.h"
#include "PSP/video.h"
#ifdef FAKE_MIC
#include "mic.h"
#endif
#include <pspctrl.h>
#include <pspdisplay.h>
#include <pspkernel.h>

#include "GPU.h"

#define NB_KEYS 12

const u16 default_psp_cfg_h[NB_KEYS] =
  { PSP_CTRL_CIRCLE,    //A
	PSP_CTRL_CROSS,     //B
	PSP_CTRL_SELECT,	//Select
	PSP_CTRL_START,		//Start
	PSP_CTRL_RIGHT,		//Right
	PSP_CTRL_LEFT,		//Left
	PSP_CTRL_UP,		//Up
	PSP_CTRL_DOWN,		//Down
	PSP_CTRL_RTRIGGER,	//R
	PSP_CTRL_LTRIGGER,	//L
	PSP_CTRL_TRIANGLE,  //X
	PSP_CTRL_SQUARE     //Y
  };

mouse_status mouse;

#define MAX_CURSOR_COLORS 4
int ashCursorColors[MAX_CURSOR_COLORS] = { 0x1C34, 0xD2E3, 0x1A54, 0xFFFF };

char achCursor[] =
{
	
	0, 1, 1, 1, 0, 
	1, 1, 1, 1, 1, 
	0, 1, 0, 1, 0, 
	0, 1, 1, 1, 1, 
	1, 0, 0, 0, 1 
};

const int bottom_index = 256*192;

#define VRAM_START 0x4000000
const int top_padding = 48 * 1024;

u8* GetFrameBuffer() {
	return (u8*)(VRAM_START + top_padding);
}

void DrawCursor(unsigned int x, unsigned int y)
{
	return;
	unsigned int fontAddr = 0;
	//short shCurrentCursorColor = 0;

	for (unsigned int yy = 0; yy < 4; yy++)
	{
		u8* framebuf = (GetFrameBuffer() + ((yy + y) * 1024)) + 512;

		for (unsigned int xx = 0; xx < 8; xx++)
		{
			if (achCursor[fontAddr])
				*(framebuf + (x + xx)) = ashCursorColors[0];
			else
				*(framebuf + (x + xx)) = ashCursorColors[2];

			fontAddr++;
		}
	}

	//*(GetFrameBuffer() + (5 +(3 + y) * 1024)) = ashCursorColors[(shCurrentCursorColor+1)%MAX_CURSOR_COLORS];
}

/* Load default joystick and keyboard configurations */
void load_default_config(const u16 kbCfg[]){}

/* Set all buttons at once */
static void set_joy_keys(const u16 joyCfg[]){}

/* Initialize joysticks */
BOOL init_joy( void) {
  return TRUE;
}

/* Unload joysticks */
void uninit_joy( void)
{
 
}

/* Return keypad vector with given key set to 1 */
u16 lookup_joy_key (u16 keyval) { return 0; }

/* Return keypad vector with given key set to 1 */
u16 lookup_key (u16 keyval) { return 0; }

/* Get pressed joystick key */
u16 get_joy_key(int index) { return 0; }

/* Get and set a new joystick key */
u16 get_set_joy_key(int index) { return 0; }

static signed long
screen_to_touch_range( signed long scr, float size_ratio) {
  return (signed long)((float)scr * size_ratio);
}

/* Set mouse coordinates */
static void set_mouse_coord(signed long x,signed long y)
{
  if(x<0) x = 0; else if(x>255) x = 255;
  if(y<0) y = 0; else if(y>192) y = 192;
  mouse.x = x;
  mouse.y = y;
  //mouse.psp_x = mouse.x;
 // mouse.psp_y = mouse.y;
}

// Adapted from Windows port
bool allowUpAndDown = false;
static buttonstruct<int> cardinalHeldTime = {0};

static void RunAntipodalRestriction(const buttonstruct<bool>& pad)
{
	if(allowUpAndDown)
		return;

	pad.U ? (cardinalHeldTime.U++) : (cardinalHeldTime.U=0);
	pad.D ? (cardinalHeldTime.D++) : (cardinalHeldTime.D=0);
	pad.L ? (cardinalHeldTime.L++) : (cardinalHeldTime.L=0);
	pad.R ? (cardinalHeldTime.R++) : (cardinalHeldTime.R=0);
}
static void ApplyAntipodalRestriction(buttonstruct<bool>& pad)
{
	if(allowUpAndDown)
		return;

	// give preference to whichever direction was most recently pressed
	if(pad.U && pad.D)
		if(cardinalHeldTime.U < cardinalHeldTime.D)
			pad.D = false;
		else
			pad.U = false;
	if(pad.L && pad.R)
		if(cardinalHeldTime.L < cardinalHeldTime.R)
			pad.R = false;
		else
			pad.L = false;
}

/* Update NDS keypad */
void update_keypad(u16 keys)
{
	// Set raw inputs
	{
		buttonstruct<bool> input = {};
		input.G = (keys>>12)&1;
		input.E = (keys>>8)&1;
		input.W = (keys>>9)&1;
		input.X = (keys>>10)&1;
		input.Y = (keys>>11)&1;
		input.A = (keys>>0)&1;
		input.B = (keys>>1)&1;
		input.S = (keys>>3)&1;
		input.T = (keys>>2)&1;
		input.U = (keys>>6)&1;
		input.D = (keys>>7)&1;
		input.L = (keys>>5)&1;
		input.R = (keys>>4)&1;
		input.F = (keys>>14)&1;
		RunAntipodalRestriction(input);
		NDS_setPad(
			input.R, input.L, input.D, input.U,
			input.T, input.S, input.B, input.A,
			input.Y, input.X, input.W, input.E,
			input.G, input.F);
	}

	// Set real input
	NDS_beginProcessingInput();
	{
		UserButtons& input = NDS_getProcessingUserInput().buttons;
		ApplyAntipodalRestriction(input);
	}
	NDS_endProcessingInput();
}

/* Retrieve current NDS keypad */
u16 get_keypad( void)
{
  u16 keypad;
  keypad = ~MMU.ARM7_REG[0x136];
  keypad = (keypad & 0x3) << 10;
#ifdef WORDS_BIGENDIAN
  keypad |= ~(MMU.ARM9_REG[0x130] | (MMU.ARM9_REG[0x131] << 8)) & 0x3FF;
#else
  keypad |= ~((u16 *)MMU.ARM9_REG)[0x130>>1] & 0x3FF;
#endif
  return keypad;
}

typedef struct{
	char name[32];
	int var;
}option;

option Options[] = {{"Resume",-1},{"Change Rom",-1},{"Reset Rom",-1},{"Save State",-1},{"Load State",-1},{"Emu Config",-1},{"Exit",-1}};

u8 curr_index = 0;
u8 N_options = 7;
bool menu_quit = false;

void MenuAction(){
	switch(curr_index){
		case 1:
			ChangeRom(true); 
		break;
		
		case 2:
			ResetRom(); 
		break;

		case 3:
			savestate_slot(0);
		break;

		case 4:
			loadstate_slot(0);
		break;

		case 5:
			EMU_Conf();
		break;

		case 6:
			sceKernelExitGame();
		return;
	}

	menu_quit = true;
}

void MenuOption(){
	pspDebugScreenSetXY(0, 3);

	for(u8 i = 0;i < N_options;i++){
		pspDebugScreenSetTextColor(0xffffffff);

		if (i == curr_index)
			pspDebugScreenSetTextColor(0x0000ffff);

	    if (Options[i].var == -1)
			pspDebugScreenPrintf("  %s \n",Options[i].name);
		else
			pspDebugScreenPrintf("  %s :  %d\n",Options[i].name,Options[i].var);
	}

	pspDebugScreenSetTextColor(0xffffffff);
	
}

void Menu(){	
	SceCtrlData pad,oldPad;
	menu_quit = false;
	curr_index = 0;

	pspDebugScreenClear();

	while(!menu_quit){
	
	sceDisplayWaitVblankStart();
	MenuOption();

	if(sceCtrlPeekBufferPositive(&pad, 1))
		{
			if (pad.Buttons != oldPad.Buttons)
			{
				if (pad.Buttons & PSP_CTRL_UP){
					--curr_index;
					if (curr_index < 0) curr_index = 0;
				}
				if (pad.Buttons & PSP_CTRL_DOWN){
					++curr_index;
					if (curr_index > N_options-1) curr_index = 0;
				}
				if (pad.Buttons & PSP_CTRL_CROSS){
					MenuAction();
				}
				 if (pad.Buttons & PSP_CTRL_CIRCLE){
					 pspDebugScreenClear();

					 if (my_config.frameskip == 0)
						 my_config.frameskip++; //Add one more Frameskip if it's set to 0. We need at least one to process input.
					return;
	  			}
			}

			oldPad = pad;
		}
	}

	pspDebugScreenClear();
}

extern bool ARM7_SKIP_HACK;

void
//process_ctrls_event( SDL_Event& event,
process_ctrls_event(u16 &keypad)
{
	  SceCtrlData pad;
	  sceCtrlSetSamplingCycle(0);
	  sceCtrlSetSamplingMode(PSP_CTRL_MODE_ANALOG);
	  sceCtrlPeekBufferPositive(&pad, 1); 

	  if (pad.Lx < 10) {
		  --mouse.x; --mouse.x;
		  --mouse.x; --mouse.x;
	  }

	  if (pad.Lx > 245) {
		  ++mouse.x; ++mouse.x;
		  ++mouse.x; ++mouse.x;
	  }

	  if (pad.Ly < 10) {
		  --mouse.y; --mouse.y;
		  --mouse.y; --mouse.y;
	  }

	  if (pad.Ly > 245) {
		  ++mouse.y; ++mouse.y;
		  ++mouse.y; ++mouse.y;
	  }

	  set_mouse_coord(mouse.x, mouse.y);

	  if (pad.Buttons & PSP_CTRL_RTRIGGER && pad.Buttons & PSP_CTRL_TRIANGLE) {
	  	ARM7_SKIP_HACK = !ARM7_SKIP_HACK;
		return;
	  }

	  if (pad.Buttons & PSP_CTRL_RTRIGGER && pad.Buttons & PSP_CTRL_CIRCLE) {
	  	mouse.click = TRUE;
		return;
	  }else{
		mouse.click = FALSE;
	  }

	  if (pad.Buttons & PSP_CTRL_HOME) {
		  Menu();
	  }
	  

	  for(int i=0;i<12;i++) {

		if (pad.Buttons & default_psp_cfg_h[i])
			ADD_KEY(keypad, KEYMASK_(i));
		else
			RM_KEY(keypad, KEYMASK_(i));
	  }
}
