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
#ifndef _PSP_Display_H
#define _PSP_Display_H

#include "../types.h"
#include "FrontEnd.h"
#include "video.h"

#define TEXTURE_FLAGS (GU_TEXTURE_16BIT | GU_VERTEX_16BIT | GU_TRANSFORM_2D)
#define TEXTURE_R3D_FLAGS (GU_TEXTURE_16BIT | GU_VERTEX_16BIT | GU_TRANSFORM_3D)
/*struct Vertex
{
unsigned short u, v;
u16 color;
signed short x, y, z;
};*/

extern unsigned int __attribute__((aligned(64))) gulist[256 * 192 * 2];

void Init_PSP_DISPLAY_FRAMEBUFF();


void Set_POSX(int pos);
void Set_PAGE(int pos);

extern void (*UpdateScreen)();
/*
void ShowFPS(int x, int y, pl_perf_counter& fps_counter);
void PrintfXY(const char* text, int x, int y);
*/
void SetupEmuDisplay(bool direct);
void UpdateSingleScreen();
void DrawRom(char* file, f_list* list, int pos,bool reload);
void DrawSettingsMenu(configP* params,int size,int currPos);
void DrawStartUpMenu();
void OSLFINISH();

void SetupDisp_EMU();
void EMU_SCREEN();
void StartGU_RENDER();
void ENDGU_RENDER();
void SEND_DISP();
void SYNC_GUDISP();

/*OSL_IMAGE* GetUpperFrameBuffer();
OSL_IMAGE* GetLowerFrameBuffer();*/

void* GetPointerFrameBuffer(bool upper);
void* GetPointerUpperFrameBuffer();
void* GetPointerLowerFrameBuffer();

#endif  // _PSP_VIDEO_H