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
#include "sndpsp.h"
#include "debug.h"
#include <pspaudio.h>
#include <pspaudiolib.h>

#include"PSP/pl_snd.h"
#include"PSP/pspvfpu.h"
#include"PSP/pspDmac.h"

int SNDPSPInit(int buffersize);
void SNDPSPDeInit();
void SNDPSPUpdateAudio(s16 *buffer, u32 num_samples);
u32 SNDPSPGetAudioSpace();
void SNDPSPMuteAudio();
void SNDPSPUnMuteAudio();
void SNDPSPSetVolume(int volume);

#define AUDIO_CHANNELS  1
#define DEFAULT_SAMPLES 512
#define VOLUME_MAX      0x8000

static u32 soundoffset;
static u32 soundpos;
static u32 soundlen;
static u32 soundbufsize;
static u16* stereodata16;

SoundInterface_struct SNDPSP = {
SNDCORE_PSP,
"PSP Sound Interface",
SNDPSPInit,
SNDPSPDeInit,
SNDPSPUpdateAudio,
SNDPSPGetAudioSpace,
SNDPSPMuteAudio,
SNDPSPUnMuteAudio,
SNDPSPSetVolume
};

static void MixAudio(void* userdata, u8* stream, u32 len) {
    int i;
    u8* soundbuf = (u8*)stereodata16;

    for (i = 0; i < len; i++)
    {
        if (soundpos >= soundbufsize)
            soundpos = 0;

        stream[i] = soundbuf[soundpos];
        soundpos++;
    }
}

int SNDPSPInit(int buffersize)
{
    int i, j, failed;

    const int freq = 44100;
    const int samples = (freq / 60) * 2;

    u32 normSamples = 512;
    while (normSamples < samples)
        normSamples <<= 1;

    soundlen = freq / 60; // 60 for NTSC
    soundbufsize = buffersize * sizeof(s16) * 2;
    soundpos = 0;

    int ret = pl_snd_init(normSamples, 1);
    pl_snd_set_callback(0, MixAudio, (void*)0);

    if ((stereodata16 = (u16*)malloc(soundbufsize)) == NULL)
        return -1;

    memset(stereodata16, 0, soundbufsize);
    
    printf("\n\nAUDIO PSP Inited: %d\n",ret);

   return ret;
}

//////////////////////////////////////////////////////////////////////////////

void SNDPSPDeInit()
{
/*
#ifdef _XBOX
	doterminate = true;
	while(!terminated) {
		Sleep(1);
	}
#endif
*/

/*   if (stereodata16)
      free(stereodata16);*/

    pl_snd_shutdown();
  
}

//////////////////////////////////////////////////////////////////////////////

void SNDPSPUpdateAudio(s16 *buffer, u32 num_samples)
{
    u32 copy1size = 0, copy2size = 0;

   pl_snd_pause(0);
   if ((soundbufsize - soundoffset) < (num_samples * sizeof(s16) * 2))
   {
       copy1size = (soundbufsize - soundoffset);
       copy2size = (num_samples * sizeof(s16) * 2) - copy1size;
   }
   else
   {
       copy1size = (num_samples * sizeof(s16) * 2);
       copy2size = 0;
   }

   sceKernelDcacheWritebackInvalidateAll();
   sceDmacMemcpy((((u8*)stereodata16) + soundoffset), buffer, copy1size);
   sceKernelDcacheWritebackInvalidateAll();

   if (copy2size) {
       sceDmacMemcpy(stereodata16, ((u8*)buffer) + copy1size, copy2size);
       sceKernelDcacheWritebackInvalidateAll();
   }

   soundoffset += copy1size + copy2size;
   soundoffset %= soundbufsize;

   pl_snd_resume(0);

}

//////////////////////////////////////////////////////////////////////////////

u32 SNDPSPGetAudioSpace()
{
   u32 freespace= 0;

   if (soundoffset > soundpos)
      freespace = soundbufsize - soundoffset + soundpos;
   else
      freespace = soundpos - soundoffset;

   return  freespace / sizeof(s16) / 2;
}

//////////////////////////////////////////////////////////////////////////////

void SNDPSPMuteAudio()
{
    pl_snd_pause(0);
}

//////////////////////////////////////////////////////////////////////////////

void SNDPSPUnMuteAudio()
{
    pl_snd_resume(0);
}

//////////////////////////////////////////////////////////////////////////////

void SNDPSPSetVolume(int volume)
{
}

//////////////////////////////////////////////////////////////////////////////