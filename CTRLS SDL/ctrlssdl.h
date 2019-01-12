/* joysdl.h - this file is part of DeSmuME
 *
 * Copyright (C) 2007 Pascal Giard
 *
 * Author: Pascal Giard <evilynux@gmail.com>
 *
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

#ifndef CTRLSSDL_H
#define CTRLSSDL_H

#include <stdio.h>
#include <stdlib.h>
//HCF
//#include <unistd.h>

//#include <pthread.h>
#include <SDL.h>
#include <SDL_image.h>
#include <SDL_ttf.h>
#include "FrontEnd.h"
#include "MMU.h"

//HCF Last
#include "SPU.h"

#include "types.h"

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
extern SDL_Joystick *GAMEPAD; //Gamepad
extern short ashBotones[NUM_BOTONES];

extern TTF_Font *font;
extern SDL_Surface *SDLscreen;

//HCF 0-doble screen, 1 solo arriba, 2 solo abajo
#define STRETCH_MODE_NONE 0
#define STRETCH_MODE_HALF 1
#define STRETCH_MODE_FULL 2
extern int iModoStretch;
extern int iModoStretchNuevo;
extern int iModoGrafico;
extern int iModoGraficoNuevo;
extern int iMouseSpeed;
extern bool bAutoFrameskip;
extern int nFrameskip;  //This will store the frameskip configured by the user 
				        //On the other hand, "frameskip" stores the actual frameskip

extern int iUsarDynarec; //iGlobalSpeed;
extern int emula3D;

extern BOOL bBlitAll;

//HCF: Sound overclocking
extern int iEnableSound;

#define ADD_KEY(keypad,key) ( (keypad) |= (key) )
#define RM_KEY(keypad,key) ( (keypad) &= ~(key) )
#define KEYMASK_(k)	(1 << (k))
#define JOY_AXIS_(k)    (((k)+1) << 8)

#define NB_KEYS		14
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
  signed long x;
  signed long y;
  BOOL click;
  BOOL down;
};

extern struct mouse_status mouse;

void set_mouse_coord(signed long x,signed long y);
#endif // !GTK_UI

void load_default_config( void);
BOOL init_joy( void);
void uninit_joy( void);
void set_joy_keys(const u16 joyCfg[]);
void set_kb_keys(u16 kbCfg[]);
u16 get_set_joy_key(int index);
void get_set_joy_axis(int index, int index_opp);
void update_keypad(u16 keys);
u16 get_keypad( void);
u16 lookup_key (u16 keyval);
u16 lookup_joy_key (u16 keyval);
int
process_ctrls_events( u16 *keypad,
                      void (*external_videoResizeFn)( u16 width, u16 height),
                      float nds_screen_size_ratio);

void
process_joystick_events( u16 *keypad);

#endif /* CTRLSSDL_H */
