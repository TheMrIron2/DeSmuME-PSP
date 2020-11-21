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

#ifndef _PL_PERF_H
#define _PL_PERF_H

#include <psptypes.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct pl_perf_counter_t
{
  float ticks_per_second;
  int frame_count;
  u64 last_tick;
  float fps;
} pl_perf_counter;

void  pl_perf_init_counter(pl_perf_counter *counter);
void pl_perf_update_counter(pl_perf_counter *counter);

#ifdef __cplusplus
}
#endif

#endif // _PL_PERF_H
