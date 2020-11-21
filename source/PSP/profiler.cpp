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

#include <pspkernel.h>
#include <psprtc.h>

#include "profiler.h"

void  pl_perf_init_counter(pl_perf_counter *counter)
{
  counter->fps = 0;
  counter->frame_count = 0;
  counter->ticks_per_second = (float)sceRtcGetTickResolution();
  sceRtcGetCurrentTick(&counter->last_tick);
}

void pl_perf_update_counter(pl_perf_counter *counter)
{
  u64 current_tick;
  sceRtcGetCurrentTick(&current_tick);

  counter->frame_count++;
  if (current_tick - counter->last_tick >= 
      counter->ticks_per_second)
  {
    /* A second elapsed; recompute FPS */
    counter->fps = (float)counter->frame_count 
      / (float)((current_tick - counter->last_tick) / counter->ticks_per_second);
    counter->last_tick = current_tick;
    counter->frame_count = 0;
  }
}
