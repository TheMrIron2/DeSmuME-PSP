/*
	Copyright (C) 2005-2006 Theo Berkau
	Copyright (C) 2006-2010 DeSmuME team

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

#include <stdlib.h>
#include <string.h>

#include "types.h"
#include "SPU.h"
#include "sndsdl.h"
#include "debug.h"

int SNDSDLInit(int buffersize);
void SNDSDLDeInit();
void SNDSDLUpdateAudio(s16 *buffer, u32 num_samples);
u32 SNDSDLGetAudioSpace();
void SNDSDLMuteAudio();
void SNDSDLUnMuteAudio();
void SNDSDLSetVolume(int volume);

int SNDSDL;

static u16 *stereodata16;
static u32 soundoffset;
static volatile u32 soundpos;
static u32 soundlen;
static u32 soundbufsize;


//////////////////////////////////////////////////////////////////////////////
static void MixAudio(void *userdata, u8 *stream, int len) {
   int i;
   u8 *soundbuf=(u8 *)stereodata16;

   for (i = 0; i < len; i++)
   {
      if (soundpos >= soundbufsize)
         soundpos = 0;

      stream[i] = soundbuf[soundpos];
      soundpos++;
   }
}

//////////////////////////////////////////////////////////////////////////////

int SNDSDLInit(int buffersize)
{
   return 0;
}

//////////////////////////////////////////////////////////////////////////////

void SNDSDLDeInit()
{
}

//////////////////////////////////////////////////////////////////////////////

void SNDSDLUpdateAudio(s16 *buffer, u32 num_samples)
{
   
}

//////////////////////////////////////////////////////////////////////////////

u32 SNDSDLGetAudioSpace()
{
   u32 freespace=0;

   if (soundoffset > soundpos)
      freespace = soundbufsize - soundoffset + soundpos;
   else
      freespace = soundpos - soundoffset;

   return (freespace / sizeof(s16) / 2);
}

//////////////////////////////////////////////////////////////////////////////

void SNDSDLMuteAudio()
{

}

//////////////////////////////////////////////////////////////////////////////

void SNDSDLUnMuteAudio()
{

}

//////////////////////////////////////////////////////////////////////////////

void SNDSDLSetVolume(int volume)
{
}

//////////////////////////////////////////////////////////////////////////////
