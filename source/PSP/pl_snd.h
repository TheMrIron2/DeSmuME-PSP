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

#ifndef _PL_SND_H
#define _PL_SND_H

#ifdef __cplusplus
extern "C" {
#endif

#define PL_SND_ALIGN_SAMPLE(s) (((s) + 63) & ~63)
#define PL_SND_TRUNCATE_SAMPLE(s) ((s) & ~63)

typedef struct pl_snd_stereo_sample_t
{
  short l;
  short r;
} pl_snd_stereo_sample;

typedef struct pl_snd_mono_sample_t
{
  short ch;
} pl_snd_mono_sample;

typedef union pl_snd_sample_t
{
  pl_snd_stereo_sample stereo;
  pl_snd_mono_sample mono;
} pl_snd_sample;

typedef void (*pl_snd_callback)(void* user_data,
                                unsigned char* buffer,
                                unsigned int samples
                               );

typedef struct {
    int thread_handle;
    int sound_ch_handle;
    int left_vol;
    int right_vol;
    pl_snd_callback callback;
    void* user_data;
    short* sample_buffer[2];
    unsigned int samples[2];
    unsigned char paused;
    unsigned char stereo;
} pl_snd_channel_info;

extern pl_snd_channel_info sound_stream[1];
extern volatile int sound_stop;

int  pl_snd_init(int samples, 
                 int stereo);
int  pl_snd_set_callback(int channel,
                         pl_snd_callback callback,
                         void *userdata);
int  pl_snd_pause(int channel);
int  pl_snd_resume(int channel);
void pl_snd_shutdown();

void Play_sound(int channel);

#ifdef __cplusplus
}
#endif

#endif // _PL_SND_H

