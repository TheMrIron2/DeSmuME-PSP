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

#ifndef CTRLSSDL_H
#define CTRLSSDL_H

#ifdef HAVE_GL_GL_H
#include <GL/gl.h>
#endif

#include <stdio.h>
#include <stdlib.h>
//#include <unistd.h>

#include "MMU.h"

#include "types.h"

//HCF D3D
#define ANCHURA_PANTALLA 256
#define ALTURA_PANTALLA_SIMPLE 192
#define ALTURA_PANTALLA_DOBLE 384
//HCF D3D

//HCF Second screen
#define PANTALLA2_X 128
#define PANTALLA2_Y 192
#define PANTALLA2_UNICA_X 32
#define PANTALLA2_UNICA_Y 7

//HCF SDL CONTROLS
#define MOUSE_DEFAULT_SPEED 3
#define NUM_BOTONES 20
#define BOTON_ARRIBA 0
#define BOTON_ABAJO 1
#define BOTON_IZQUIERDA 2
#define BOTON_DERECHA 3
#define BOTON_AA 4
#define BOTON_BB 5
#define BOTON_XX 6
#define BOTON_YY 7
#define BOTON_BLANCO 8
#define BOTON_NEGRO 9
#define BOTON_START 10
#define BOTON_BACK 11
#define BOTON_LTRIGGER 12
#define BOTON_RTRIGGER 13
#define BOTON_LTHUMBSTICK 14
#define BOTON_RTHUMBSTICK 15

//HCF FRAMESKIP
#define FRAMESKIP_FIXED      0
#define FRAMESKIP_AUTO_ODD   1
#define FRAMESKIP_AUTO_EVEN  2
#define FRAMESKIP_AUTO_BOTH  3

//HCF LIMIT FRAMERATE MODES
#define LIMIT_FRAMERATE_NO       0
#define LIMIT_FRAMERATE_HCF      1
#define LIMIT_FRAMERATE_DESMUME  2

//HCF SOUND MODE
#define SOUND_MODE_ASYNC               0
#define SOUND_MODE_SYNC                1
#define SOUND_MODE_SYNC_INTERPOLATED   2

//HCF 0-doble screen, 1 solo arriba, 2 solo abajo
#define STRETCH_MODE_NONE 0
#define STRETCH_MODE_HALF 1
#define STRETCH_MODE_FULL 2
extern int iModoStretch;
extern int iModoStretchNuevo;
extern int iModoGrafico;
extern int iModoGraficoNuevo;
extern int iShowFramesAndMemory;
extern int iMouseSpeed;
extern int iAutoFrameskip;
extern int nFrameskip;  //This will store the frameskip configured by the user 
				        //On the other hand, "frameskip" stores the actual frameskip

extern int iUsarDynarec; //iGlobalSpeed;
//extern int emula3D;

extern BOOL bBlitAll;
extern int iLimitFramerate;

//HCF: Sound overclocking
extern int iEnableSound;

#define ADD_KEY(keypad,key) ( (keypad) |= (key) )
#define RM_KEY(keypad,key) ( (keypad) &= ~(key) )
#define KEYMASK_(k)	(1 << (k))

#define JOY_AXIS  0
#define JOY_HAT  1
#define JOY_BUTTON  2

#define JOY_HAT_RIGHT 0
#define JOY_HAT_LEFT 1
#define JOY_HAT_UP 2
#define JOY_HAT_DOWN 3

#define NB_KEYS		15
#define KEY_NONE		0
#define KEY_A			1
#define KEY_B			2
#define KEY_SELECT		3
#define KEY_START		4
#define KEY_RIGHT		5
#define KEY_LEFT		6
#define KEY_UP			7
#define KEY_DOWN		8
#define KEY_R			9
#define KEY_L			10
#define KEY_X			11
#define KEY_Y			12
#define KEY_DEBUG		13
#define KEY_BOOST		14
#define KEY_LID			15

/* Keypad key names */
extern const char *key_names[NB_KEYS];
/* Current keyboard configuration */
extern u16 keyboard_cfg[NB_KEYS];
/* Current joypad configuration */
extern u16 joypad_cfg[NB_KEYS];
/* Number of detected joypads */
extern u16 nbr_joy;

#ifndef GTK_UI
struct mouse_status
{
	BOOL click;
	BOOL down;

	u8 x;
	u8 y;
/*
	u16 psp_x;
	u16 psp_y;*/
};

extern mouse_status mouse;
#endif // !GTK_UI

struct ctrls_event_config {
  unsigned short keypad;
  float nds_screen_size_ratio;
  int auto_pause;
  int focused;
  int sdl_quit;
  int boost;
  int fake_mic;
#ifdef HAVE_GL_GL_H
  GLuint *screen_texture;
  void (*resize_cb)(u16 width, u16 height, GLuint *screen_texture);
#else
  void *screen_texture;
  void (*resize_cb)(u16 width, u16 height, void *screen_texture);
#endif
};

void load_default_config(const u16 kbCfg[]);
BOOL init_joy( void);
void uninit_joy( void);
u16 get_joy_key(int index);
u16 get_set_joy_key(int index);
void update_keypad(u16 keys);
u16 get_keypad( void);
u16 lookup_key (u16 keyval);
u16 lookup_joy_key (u16 keyval);
void
//process_ctrls_event( SDL_Event& event,
 process_ctrls_event(u16 &keypad);

void
process_joystick_events( u16 *keypad);

void DrawCursor(unsigned int x, unsigned int y);

#endif /* CTRLSSDL_H */
