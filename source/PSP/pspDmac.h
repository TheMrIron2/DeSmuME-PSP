#ifndef DMAC_PSP_H
#define DMAC_PSP_H

#include "../types.h"

extern "C" {
	/*Transfer memory data using DMAC */
	int sceDmacMemcpy(void* pDst, const void* pSrc, unsigned int uiSize);
}

#endif