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

#define ADD_KEY(keypad,key) ( (keypad) |= (key) )
#define RM_KEY(keypad,key) ( (keypad) &= ~(key) )
#define KEYMASK_(k)	(1 << (k))


struct mouse_status
{
	BOOL click;
	BOOL down;

	u8 x;
	u8 y;
};

extern mouse_status mouse;


void load_default_config(const u16 kbCfg[]);
BOOL init_joy( void);
void uninit_joy( void);
u16 get_joy_key(int index);
u16 get_set_joy_key(int index);
void update_keypad(u16 keys);
u16 get_keypad( void);
u16 lookup_key (u16 keyval);
u16 lookup_joy_key (u16 keyval);
void process_ctrls_event(u16 &keypad);

void process_joystick_events( u16 *keypad);

void DrawCursor(unsigned int x, unsigned int y);

#endif /* CTRLSSDL_H */
