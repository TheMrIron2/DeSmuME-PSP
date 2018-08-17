/** PSP helper library ***************************************/
/**                                                         **/
/**                          video.h                        **/
/**                                                         **/
/** This file contains declarations for the video rendering **/
/** library                                                 **/
/**                                                         **/
/** Copyright (C) Akop Karapetyan 2007                      **/
/**     You are not allowed to distribute this software     **/
/**     commercially. Please, notify me, if you make any    **/
/**     changes to this file.                               **/
/*************************************************************/
#ifndef _PSP_VIDEO_H
#define _PSP_VIDEO_H

#include "types.h"
#include "vram.h"

struct Vertex
{
	u16 u, v;
	u16 color;
	s16 x, y, z;
};


typedef struct
{
  unsigned int color;
  short x, y, z;
} PspVertex;


typedef struct
{
  int X;
  int Y;
  int Width;
  int Height;
} PspViewport;

typedef struct
{
  int Width;
  int Height;
  unsigned short* Pixels;
  PspViewport Viewport;
  int FreeBuffer;
} PspImage;

#define PSP_VIDEO_WHITE	   (u32)0xffffffff
#define PSP_VIDEO_BLACK	   (u32)0xff000000
#define PSP_VIDEO_GRAY	   (u32)0xffcccccc
#define PSP_VIDEO_DARKGRAY (u32)0xff777777
#define PSP_VIDEO_RED	     (u32)0xff0000ff
#define PSP_VIDEO_GREEN	   (u32)0xff00ff00
#define PSP_VIDEO_BLUE	   (u32)0xffff0000
#define PSP_VIDEO_YELLOW   (u32)0xff00ffff
#define PSP_VIDEO_MAGENTA  (u32)0xffff00ff

#define PSP_VIDEO_FC_RESTORE 020
#define PSP_VIDEO_FC_BLACK   021
#define PSP_VIDEO_FC_RED     022
#define PSP_VIDEO_FC_GREEN   023
#define PSP_VIDEO_FC_BLUE    024
#define PSP_VIDEO_FC_GRAY    025
#define PSP_VIDEO_FC_YELLOW  026
#define PSP_VIDEO_FC_MAGENTA 027
#define PSP_VIDEO_FC_WHITE   030

#define PSP_VIDEO_UNSCALED    0
#define PSP_VIDEO_FIT_HEIGHT  1
#define PSP_VIDEO_FILL_SCREEN 2

#define BUF_WIDTH 512
#define SCR_WIDTH 480
#define SCR_HEIGHT 272

void fps_init();
void GuInit();
void pspVideoShutdown();
void pspVideoClearScreen();
void pspVideoWaitVSync();
void pspVideoSwapBuffers();

void pspVideoBegin();
void pspVideoEnd();

void pspVideoDrawLine(int sx, int sy, int dx, int dy, u32 color);
void pspVideoDrawRect(int sx, int sy, int dx, int dy, u32 color);
void pspVideoFillRect(int sx, int sy, int dx, int dy, u32 color);

void GuImageDirect(u16* imageA,u16* imageB,int VX,int VY,int Width,int Height, int dx, int dy, int dw, int dh);

void pspVideoGlowRect(int sx, int sy, int dx, int dy, u32 color, int radius);
void pspVideoShadowRect(int sx, int sy, int dx, int dy, u32 color, int depth);

PspImage* pspVideoGetVramBufferCopy();

void pspVideoCallList(const void *list);

void* pspVideoAllocateVramChunk(unsigned int bytes);

#endif  // _PSP_VIDEO_H