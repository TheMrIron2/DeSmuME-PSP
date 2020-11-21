// Copyright 2007-2015 Akop Karapetyan
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//    http://www.apache.org/licenses/LICENSE-2.0
// 
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "pl_snd.h"

#include <stdio.h>
#include <pspaudio.h>
#include <pspthreadman.h>
#include <string.h>
#include <malloc.h>

#define AUDIO_CHANNELS  1
#define DEFAULT_SAMPLES 512
#define VOLUME_MAX      0x8000

static int sound_ready;
volatile int sound_stop = 0;

volatile bool UseME = true;

pl_snd_channel_info sound_stream[AUDIO_CHANNELS];

static int channel_thread(unsigned int args, void *argp);
static void free_buffers();
static unsigned int get_bytes_per_sample(int channel);

int pl_snd_init(int sample_count,
                int stereo)
{
  int i, j, failed;
  sound_stop = 0;
  sound_ready = 0;

  /* Check sample count */
  if (sample_count <= 0) sample_count = DEFAULT_SAMPLES;
  sample_count = PSP_AUDIO_SAMPLE_ALIGN(sample_count);

  pl_snd_channel_info *ch_info;
  for (i = 0; i < AUDIO_CHANNELS; i++)
  {
    ch_info = &sound_stream[i];
    ch_info->sound_ch_handle = -1;
    ch_info->thread_handle = -1;
    ch_info->left_vol = VOLUME_MAX;
    ch_info->right_vol = VOLUME_MAX;
    ch_info->callback = NULL;
    ch_info->user_data = NULL;
    ch_info->paused = 0;
    ch_info->stereo = stereo;

    for (j = 0; j < 2; j++)
    {
      ch_info->sample_buffer[j] = NULL;
      ch_info->samples[j] = 0;
    }
  }

  /* Initialize buffers */
  for (i = 0; i < AUDIO_CHANNELS; i++)
  {
    ch_info = &sound_stream[i];
    for (j = 0; j < 2; j++)
    {
      if (!(ch_info->sample_buffer[j] = 
              (short*)malloc(sample_count * get_bytes_per_sample(i))))
      {
        free_buffers();
        return 0;
      }

      ch_info->samples[j] = sample_count;
    }
  }

  /* Initialize channels */
  for (i = 0, failed = 0; i < AUDIO_CHANNELS; i++)
  {
    sound_stream[i].sound_ch_handle = 
      sceAudioChReserve(PSP_AUDIO_NEXT_CHANNEL, 
                        sample_count,
                        (stereo) 
                          ? PSP_AUDIO_FORMAT_STEREO
                          : PSP_AUDIO_FORMAT_MONO);

    if (sound_stream[i].sound_ch_handle < 0)
    { 
      failed = 1;
      break;
    }
  }

  if (failed)
  {
    for (i = 0; i < AUDIO_CHANNELS; i++)
    {
      if (sound_stream[i].sound_ch_handle != -1)
      {
        sceAudioChRelease(sound_stream[i].sound_ch_handle);
        sound_stream[i].sound_ch_handle = -1;
      }
    }

    free_buffers();
    return 0;
  }

  sound_ready = 1;

  UseME = false;//InitialiseJobManager();

  if (UseME) return 0;
  
  char label[16];
  strcpy(label, "audiotX");

  for (i = 0; i < AUDIO_CHANNELS; i++)
  {
    label[6] = '0' + i;
    sound_stream[i].thread_handle = 
      sceKernelCreateThread(label, channel_thread, 0x12, 0x10000, 
        0, NULL);

    if (sound_stream[i].thread_handle < 0)
    {
      sound_stream[i].thread_handle = -1;
      failed = 1;
      break;
    }

    if (sceKernelStartThread(sound_stream[i].thread_handle, sizeof(i), &i) != 0)
    {
      failed = 1;
      break;
    }
  }

  if (failed)
  {
    sound_stop = 1;
    for (i = 0; i < AUDIO_CHANNELS; i++)
    {
      if (sound_stream[i].thread_handle != -1)
      {
        //sceKernelWaitThreadEnd(sound_stream[i].threadhandle,NULL);
        sceKernelDeleteThread(sound_stream[i].thread_handle);
      }

      sound_stream[i].thread_handle = -1;
    }

    sound_ready = 0;
    free_buffers();
    return 0;
  }
  

  return sample_count;
}

void pl_snd_shutdown()
{
  int i;
  sound_ready = 0;
  sound_stop = 1;

  for (i = 0; i < AUDIO_CHANNELS; i++)
  {
    if (sound_stream[i].thread_handle != -1)
    {
      //sceKernelWaitThreadEnd(sound_stream[i].threadhandle,NULL);
      sceKernelDeleteThread(sound_stream[i].thread_handle);
    }

    sound_stream[i].thread_handle = -1;
  }

  for (i = 0; i < AUDIO_CHANNELS; i++)
  {
    if (sound_stream[i].sound_ch_handle != -1)
    {
      sceAudioChRelease(sound_stream[i].sound_ch_handle);
      sound_stream[i].sound_ch_handle = -1;
    }
  }

  free_buffers();
}

static inline int play_blocking(unsigned int channel,
                                unsigned int vol1,
                                unsigned int vol2,
                                void *buf)
{
  if (!sound_ready) return -1;
  //if (channel >= AUDIO_CHANNELS) return -1;

  return sceAudioOutputPannedBlocking(sound_stream[channel].sound_ch_handle,
    vol1, vol2, buf);
}

static int channel_thread(unsigned int args, void *argp)
{
  volatile int bufidx = 0;
  int channel = *(int*)argp;
  int i, j;
  unsigned short *ptr_m;
  unsigned int *ptr_s;
  void *bufptr;
  unsigned int samples;
  pl_snd_callback callback;
  pl_snd_channel_info *ch_info;

  ch_info = &sound_stream[channel];
  for (j = 0; j < 2; j++)
    memset(ch_info->sample_buffer[j], 0,
           ch_info->samples[j] * get_bytes_per_sample(channel));

  while (!sound_stop)
  {
    callback = ch_info->callback;
    bufptr = ch_info->sample_buffer[bufidx];
    samples = ch_info->samples[bufidx];

    if (callback && !ch_info->paused) {
        /* Use callback to fill buffer */
        callback(ch_info->user_data, static_cast<unsigned char*>(bufptr), samples);        
    }
    else
    {
      /* Fill buffer with silence */
    //  if (ch_info->stereo)
        for (i = 0, ptr_s = (unsigned int* )bufptr; i < samples; i++) *(ptr_s++) = 0;
     /* else 
        for (i = 0, ptr_m = (unsigned short*)bufptr; i < samples; i++) *(ptr_m++) = 0;*/
    }

    /* Play sound */
	  play_blocking(channel,
                  ch_info->left_vol,
                  ch_info->right_vol,
                  bufptr);

    /* Switch active buffer */
 //   bufidx = (bufidx ? 0 : 1);
  }

  sceKernelExitThread(0);
  return 0;
}

static int channel_thread(int* arg)
{
    volatile int bufidx = 0;
    int channel = 0;
    int i, j;
    unsigned short* ptr_m;
    unsigned int* ptr_s;
    void* bufptr;
    unsigned int samples;
    pl_snd_callback callback;
    pl_snd_channel_info* ch_info;

    ch_info = &sound_stream[channel];
    for (j = 0; j < 2; j++)
        memset(ch_info->sample_buffer[j], 0,
            ch_info->samples[j] * get_bytes_per_sample(channel));

    while (!sound_stop)
    {
        callback = ch_info->callback;
        bufptr = ch_info->sample_buffer[bufidx];
        samples = ch_info->samples[bufidx];

        if (callback && !ch_info->paused) {
            /* Use callback to fill buffer */
            callback(ch_info->user_data, static_cast<unsigned char*>(bufptr), samples);
        }
        else
        {
            /* Fill buffer with silence */
            if (ch_info->stereo)
                for (i = 0, ptr_s = (unsigned int*)bufptr; i < samples; i++) *(ptr_s++) = 0;
            else
                for (i = 0, ptr_m = (unsigned short*)bufptr; i < samples; i++) *(ptr_m++) = 0;
        }

        /* Play sound */
        play_blocking(channel,
            ch_info->left_vol,
            ch_info->right_vol,
            bufptr);

        /* Switch active buffer */
        bufidx = (bufidx ? 0 : 1);
    }

    sceKernelExitThread(0);
    return 0;
}

static void free_buffers()
{
  int i, j;

  pl_snd_channel_info *ch_info;
  for (i = 0; i < AUDIO_CHANNELS; i++)
  {
    ch_info = &sound_stream[i];
    for (j = 0; j < 2; j++)
    {
      if (ch_info->sample_buffer[j])
      {
        free(ch_info->sample_buffer[j]);
        ch_info->sample_buffer[j] = NULL;
      }
    }
  }
}

int pl_snd_set_callback(int channel,
                        pl_snd_callback callback,
                        void *userdata)
{
  if (channel < 0 || channel > AUDIO_CHANNELS)
    return 0;
  volatile pl_snd_channel_info *pci = &sound_stream[channel];
  pci->callback = NULL;
  pci->user_data = userdata;
  pci->callback = callback;

  if (UseME) {
   /*   SJob * audio = new SJob();
      audio->DoJob = &channel_thread;
      gJobManager.AddJob(audio,sizeof(audio));*/
  }

  return 1;
}

static unsigned int get_bytes_per_sample(int channel)
{
  return (sound_stream[channel].stereo)
    ? sizeof(pl_snd_stereo_sample)
    : sizeof(pl_snd_mono_sample);
}

int pl_snd_pause(int channel)
{
  if (channel < 0 || channel > AUDIO_CHANNELS)
    return 0;
  sound_stream[channel].paused = 1;
  return 1;
}

int pl_snd_resume(int channel)
{
  if (channel < 0 || channel > AUDIO_CHANNELS)
    return 0;
  sound_stream[channel].paused = 0;
  return 1;
}
