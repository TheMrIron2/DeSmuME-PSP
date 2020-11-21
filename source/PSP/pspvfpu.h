#ifndef FPU_PSP_H
#define FPU_PSP_H

#include "../types.h"

static float sceFpuSqrt(float x) {
	float result;
	__asm__(
		"sqrt.s			%0, %1\n"
		: "=f"(result)
		: "f"(x)
	);
	return result;
}

void memcpy_vfpu(void* dst, const void* src, u32 size);
void memcpy_vfpu_byteswap(void* dst, const void* src, u32 size);

void sceVfpuMemset(void* dst, int c, unsigned int n);

#define fast_memset 		sceVfpuMemset

#define fast_memcpy 		memcpy_vfpu
#define fast_memcpy_swizzle memcpy_vfpu_byteswap

#endif