/*  Copyright (C) 2006 yopyop
    yopyop156@ifrance.com
    yopyop156.ifrance.com

	Copyright (C) 2007 shash

    This file is part of DeSmuME

    DeSmuME is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    DeSmuME is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with DeSmuME; if not, write to the Free Software
    Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
*/

#ifndef FIFO_H
#define FIFO_H

#include "types.h"

#ifdef __cplusplus
extern "C" {
#endif

	//=================================================== IPC FIFO
typedef struct
{
	u32		sendBuf[2][16];
	u32		recvBuf[2][16];
	
	u8		sendTail[2];
	u8		recvTail[2];
} IPC_FIFO;

IPC_FIFO ipc_fifo;
void IPC_FIFOclear();
void IPC_FIFOsend(u8 proc, u32 val);
u32 IPC_FIFOrecv(u8 proc);
void IPC_FIFOcnt(u8 proc, u16 val);



typedef struct
{
       u32 data[0x8000];
       u32 begin;
       u32 end;
       BOOL full;
       BOOL empty;
       BOOL error;
} FIFO;

void FIFOInit(FIFO * fifo);
void FIFOAdd(FIFO * fifo, u32 v);
u32 FIFOValue(FIFO * fifo);

//=================================================== GFX FIFO
typedef struct
{
	u32		cmd[261];
	u32		param[261];

	u32		tail;		// tail
} GFX_FIFO;

extern void GFX_FIFOcnt(u32 val);

GFX_FIFO gxFIFO;

//=================================================== Display memory FIFO
typedef struct
{
	u32		buf[0x6000];			// 256x192 32K color
	u32		head;					// head
	u32		tail;					// tail
} DISP_FIFO;

DISP_FIFO disp_fifo;
void DISP_FIFOinit();
void DISP_FIFOsend(u32 val);
u32 DISP_FIFOrecv();


#ifdef __cplusplus
}
#endif

#endif
