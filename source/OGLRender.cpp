/*
	Copyright (C) 2006 yopyop
	Copyright (C) 2006-2007 shash

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

//problem - alpha-on-alpha texture rendering might work but the dest alpha buffer isnt tracked correctly
//due to zeromus not having any idea how to set dest alpha blending in opengl.
//so, it doesnt composite to 2d correctly.
//(re: new super mario brothers renders the stormclouds at the beginning)

#include <algorithm>
#include <math.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include "debug.h"
#include "MMU.h"

#include "types.h"
#include "debug.h"
#include "MMU.h"
#include "bits.h"
#include "matrix.h"
#include "NDSSystem.h"
#include "OGLRender.h"
#include "gfx3d.h"

#include "PSP/vram.h"
#include "PSP/video.h"

#include <pspdisplay.h>
#include <pspgu.h>
#include <pspge.h>
#include <pspgum.h>

#include"rasterize.h"

//static ALIGN(16) unsigned char  GPU_screen3D		[256*192*4]={0};
//static ALIGN(16) unsigned char  GPU_screenStencil[256*256]={0};

static const int texEnv[4] = { GU_TFX_MODULATE , GU_TFX_DECAL , GU_TFX_MODULATE, GU_TFX_MODULATE };
static const int depthFunc[2] = { GU_LESS , GU_EQUAL };
static bool needRefreshFramebuffer = false;
static unsigned char texMAP[0]; 
static unsigned int textureMode=0;

CACHE_ALIGN const float divide5bitBy31_LUT[32]	= {0.0,             0.0322580645161, 0.0645161290323, 0.0967741935484,
													   0.1290322580645, 0.1612903225806, 0.1935483870968, 0.2258064516129,
													   0.2580645161290, 0.2903225806452, 0.3225806451613, 0.3548387096774,
													   0.3870967741935, 0.4193548387097, 0.4516129032258, 0.4838709677419,
													   0.5161290322581, 0.5483870967742, 0.5806451612903, 0.6129032258065,
													   0.6451612903226, 0.6774193548387, 0.7096774193548, 0.7419354838710,
													   0.7741935483871, 0.8064516129032, 0.8387096774194, 0.8709677419355,
													   0.9032258064516, 0.9354838709677, 0.9677419354839, 1.0};

CACHE_ALIGN float material_8bit_to_float[255] = {0};

float clearAlpha;


//raw ds format poly attributes, installed from the display list
static u32 polyAttr=0,textureFormat=0, texturePalette=0;

//derived values extracted from polyattr etc
static bool wireframe=false, alpha31=false;
static unsigned int polyID=0;
static unsigned int depthFuncMode=0;
static unsigned int envMode=0;
static unsigned int cullingMask=0;
static bool alphaDepthWrite;
static unsigned int lightMask=0;
static bool isTranslucent;

void* framebuffer;

struct Vertex
{
	u16 u, v;
	u16 color;
	s16 x, y, z;
};
//=================================================


static void Reset()
{
	sceKernelDcacheWritebackAll();
}

unsigned int __attribute__((aligned(64))) render_list[256*192*2];

static char Init(void)
{
	/*sceGuInit();
	sceGuStart(GU_DIRECT, render_list);
	printf("Software PSP");
	framebuffer = getStaticVramBuffer(256, 192, GU_PSM_5551);
	sceGuEnable(GU_SCISSOR_TEST);
	sceDisplaySetFrameBuf(framebuffer, 256, GU_PSM_5551, PSP_DISPLAY_SETBUF_IMMEDIATE);
	sceGuFinish();
	sceGuSync(0, 0);*/
	printf("Software PSP");
	return 1;
}

void Close()
{
	sceKernelDcacheWritebackAll();
}


//controls states:
//glStencilFunc
//glStencilOp
//glColorMask
static u32 stencilStateSet = -1;

static void BeginRenderPoly(POLY *thePoly)
{

	if (gfx3d.polylist->count <=  0) return;

	bool enableDepthWrite = true;
	u32 tmp=0;

	sceGuDepthFunc((thePoly->getAttributeEnableDepthTest()) ? GU_EQUAL : GU_LESS);

	static const u8 oglCullingMode[4] = {0, GU_CW , GU_CCW , 0};
	u8 cullingMode = oglCullingMode[thePoly->getAttributeEnableFaceCullingFlags()];

	// Cull face
	if (cullingMode == 0)
	{
		sceGuDisable(GU_CULL_FACE);
	}
	else
	{
		sceGuEnable(GU_CULL_FACE);
		//glCullFace(cullingMode);
	}
		

	/*if (!thePoly->isWireframe())
	{
		xglPolygonMode (GL_FRONT_AND_BACK, GL_FILL);
	}
	else
	{
		xglPolygonMode (GL_FRONT_AND_BACK, GL_LINE);
	}*/

	//setTexture(textureFormat, texturePalette);

	if(thePoly->isTranslucent())
		enableDepthWrite = alphaDepthWrite;

	//handle shadow polys
	if(thePoly->getAttributePolygonMode() == 3)
	{
		sceGuEnable(GU_STENCIL_TEST);
		if(thePoly->getAttributePolygonID() == 0) {
			enableDepthWrite = false;
			if(stencilStateSet!=0) {
				stencilStateSet = 0;
				//when the polyID is zero, we are writing the shadow mask.
				//set stencilbuf = 1 where the shadow volume is obstructed by geometry.
				//do not write color or depth information.
				sceGuStencilFunc(GU_ALWAYS,65,255);
				sceGuStencilOp(GU_KEEP, GU_REPLACE, GU_KEEP);
			//	glColorMask(GL_FALSE,GL_FALSE,GL_FALSE,GL_FALSE);
			}
		} else {
			enableDepthWrite = true;
			if(stencilStateSet!=1) {
				stencilStateSet = 1;
				//when the polyid is nonzero, we are drawing the shadow poly.
				//only draw the shadow poly where the stencilbuf==1.
				//I am not sure whether to update the depth buffer here--so I chose not to.
				sceGuStencilFunc(GU_EQUAL,65,255);
				sceGuStencilOp(GU_KEEP, GU_KEEP, GU_KEEP);
				//glColorMask(GL_TRUE,GL_TRUE,GL_TRUE,GL_TRUE);
			}
		}
	} else {
		sceGuEnable(GU_STENCIL_TEST);
		if(thePoly->isTranslucent())
		{
			stencilStateSet = 3;
			sceGuStencilFunc(GU_NOTEQUAL,thePoly->getAttributePolygonID(),255);
			sceGuStencilOp(GU_KEEP, GU_KEEP, GU_REPLACE);
			//glColorMask(GL_TRUE,GL_TRUE,GL_TRUE,GL_TRUE);
		}
		else
		if(stencilStateSet!=2) {
			stencilStateSet=2;
			sceGuStencilFunc(GU_ALWAYS,1,255);
			sceGuStencilOp(GU_REPLACE, GU_REPLACE, GU_REPLACE);
			//glColorMask(GL_TRUE,GL_TRUE,GL_TRUE,GL_TRUE);
		}
	}

	//glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, texEnv[envMode]);


	//xglDepthMask(enableDepthWrite?GL_TRUE:GL_FALSE);
}

static void InstallPolygonAttrib(unsigned long val)
{
	// Light enable/disable
	lightMask = (val&0xF);

	// texture environment
	envMode = (val&0x30)>>4;

	// overwrite depth on alpha pass
	alphaDepthWrite = BIT11(val)!=0;

	// depth test function
	depthFuncMode = depthFunc[BIT14(val)];

	// back face culling
	cullingMask = (val&0xC0);

	alpha31 = ((val>>16)&0x1F)==31;
	
	// Alpha value, actually not well handled, 0 should be wireframe
	wireframe = ((val>>16)&0x1F)==0;

	// polyID
	polyID = (val>>24)&0x1F;
}

static void Control()
{
	/*if(gfx3d.state.enableTexturing) glEnable (GU_TEXTURE_2D);
	else*/ sceGuDisable(GU_TEXTURE_2D);

	if(gfx3d.state.enableAlphaTest)
		sceGuAlphaFunc(GU_GREATER, gfx3d.state.alphaTestRef,255);
	else
		sceGuAlphaFunc(GU_GREATER, 0,0);

	/*if(gfx3d.state.enableAlphaBlending)
	{
		glEnable		(GU_BLEND);
	}
	else*/
	{
		sceGuDisable (GU_BLEND);
	}
}

static void Render()
{
	//printf("OGL YES");
	//if(!BEGINGL()) return;
	//printf("Software PSP");

	sceGuStart(GU_DIRECT, render_list);

	Control();

	float alpha = 1.0f;

	/*if(hasShaders)
	{
		//TODO - maybe this should only happen if the toon table is stale (for a slight speedup)

		glActiveTexture(GL_TEXTURE1);
		glBindTexture(GL_TEXTURE_1D, oglToonTableTextureID);
		
		//generate a 8888 toon table from the ds format one and store it in a texture
		u32 rgbToonTable[32];
		for(int i=0;i<32;i++) rgbToonTable[i] = RGB15TO32(gfx3d.state.u16ToonTable[i], 255);
		glTexImage1D(GL_TEXTURE_1D, 0, GL_RGB, 32, 0, GL_RGBA, GL_UNSIGNED_BYTE, rgbToonTable);
	}*/

	//xglDepthMask(GL_TRUE);

	//sceGumPushMatrix();
	

	sceGuClearStencil((gfx3d.renderState.clearColor >> 24) & 0x3F);
	u32 clearFlag = GU_STENCIL_BUFFER_BIT;

	sceGuClearColor(gfx3d.renderState.clearColor);
	sceGuClearDepth((float)gfx3d.renderState.clearDepth / (float)0x00FFFFFF);
	clearFlag |= GU_COLOR_BUFFER_BIT | GU_DEPTH_BUFFER_BIT;

	sceGuClear(clearFlag);

	/*glMatrixMode(GL_PROJECTION);
	glLoadIdentity();*/

	sceGumMatrixMode(GU_PROJECTION);
	sceGumLoadIdentity();
	//sceGumTranslate(0);
	//glTranslatef(0,0,0);


	//render display list
	//TODO - properly doublebuffer the display lists
	sceGuEnable(GU_TEXTURE_2D);
	//sceGuDisable(GU_TEXTURE_2D);

	//sceGuScissor(0, 0, 480, 272);

	{
		u32 lastTextureFormat = 0, lastTexturePalette = 0, lastPolyAttr = 0, lastViewport = 0xFFFFFFFF;

		//sceKernelDcacheWritebackAll();

		for(int i=gfx3d.polylist->count;i--;) {
			POLY *poly = &gfx3d.polylist->list[gfx3d.indexlist.list[i]];
			int type = poly->type;
			//sceGumPushMatrix();

			//a very macro-level state caching approach:
			//these are the only things which control the GPU rendering state.
			if(i==0 /*|| lastTextureFormat != poly->texParam || lastTexturePalette != poly->texPalette */|| lastPolyAttr != poly->polyAttr)
			{
				isTranslucent = poly->isTranslucent();
				lastTextureFormat = textureFormat = poly->texParam;
				lastTexturePalette = texturePalette = poly->texPalette;
				BeginRenderPoly(poly);
			}

			//glViewport(0, 0, 480, 272);
			
			if(lastViewport != poly->viewport)
			{
				VIEWPORT viewport;
				viewport.decode(poly->viewport);
				//glViewport(viewport.x,viewport.y,viewport.width,viewport.height);
				sceGumPerspective(viewport.x,viewport.y,viewport.width,viewport.height);
				lastViewport = poly->viewport;
			}


		if(gfx3d.renderState.enableAlphaTest && (gfx3d.renderState.alphaTestRef > 0))
		{
			alpha = divide5bitBy31_LUT[gfx3d.renderState.alphaTestRef];// glAlphaFunc(GL_GEQUAL, divide5bitBy31_LUT[engine.renderState.alphaTestRef]);
		}
		else
		{
			alpha = 1.0f;
		}

		const static u8 types[] = { GU_TRIANGLES , GU_SPRITES , GU_TRIANGLE_STRIP, GU_SPRITES ,	//TODO: GL_QUAD_STRIP
							GU_TRIANGLE_FAN , GU_TRIANGLE_FAN , GU_LINE_STRIP, GU_LINE_STRIP };


		struct Vertex* vertices = (struct Vertex*)sceGuGetMemory(3*sizeof(struct Vertex));

		

		for(int j=0;j<3;j++) 
		{
			VERT* vert = &gfx3d.vertlist->list[poly->vertIndexes[j]];

			vertices[j].u = vert->u;
			vertices[j].v = vert->v;

			vertices[j].color = material_8bit_to_float[vert->color[1]];

			vertices[j].x = (int)vert->x;
			vertices[j].y = (int)vert->y;
			vertices[j].z = (int)vert->z;
		}

		/*sceGuDisable(GU_TEXTURE_2D);
		sceGuDisable(GU_DEPTH_TEST);
		sceGuDepthMask(GU_TRUE);*/

		sceGuDrawArray(GU_TRIANGLES, GU_TEXTURE_16BIT | GU_COLOR_5551 |
			GU_VERTEX_16BIT | GU_TRANSFORM_2D, 9, 0, vertices);
		//sceGuDrawArray(types[type], GU_TRANSFORM_3D, 4, nullptr, vertices);
		//sceGuBeginObject(GU_VERTEX_16BIT, type, 0, vertices);
		/*sceGuDisable(GU_TEXTURE_2D);
		sceGuShadeModel(GU_FLAT);
		sceGuDrawArray(GU_LINE_STRIP, GU_COLOR_5551 | GU_TRANSFORM_2D | GU_VERTEX_16BIT, 3*type, 0, vertices);
		sceGuShadeModel(GU_SMOOTH);
		sceGuEnable(GU_TEXTURE_2D);*/
		//sceGumDrawArray(GU_TRIANGLES, GU_TRANSFORM_2D, 1, 0, vertices);
		//sceGuEndObject();
		//sceGumPopMatrix();
		}
	}

	//since we just redrew, we need to refresh the framebuffers
	needRefreshFramebuffer = true;
	sceGuScissor(0, 0, 256, 192);
	//sceGuSync(0, 0);

	sceGuDrawBuffer(GU_PSM_5551,(void*)0, 256);

	sceGuFinish();
	
	//ENDGL();
}

static void VramReconfigureSignal()
{

}

#define VRAM_START 0x4000000

void GL_ReadFramebuffer()
{
	u16* disp = (u16*)framebuffer;

	sceGuStart(GU_DIRECT, render_list);

	//sceGuDrawBufferList(GU_PSM_5551, (void*)0, 512);
	sceGuFinish();


	/*for(int y = 385;y--;t_y++)
		for(int x = 0;x < 513;x++)
					gfx3d_convertedScreen[((t_y * 512)+ x)] =  GPU_screen3D[(y*512)+x+1];*/
	/*for(int i=0,y=191;y>=0;y--)
	{
		u8* dst = gfx3d_convertedScreen + (y<<(8+2));

		for(int x=0;x<256;x++,i++)
		{
			u32 &u32screen3D = ((u32*)GPU_screen3D)[i];
			u32screen3D>>=2;
			u32screen3D &= 0x3F3F3F3F;
			
			const int t = i<<2;
			const u8 a = GPU_screen3D[t+3] >> 1;
			const u8 r = GPU_screen3D[t+2];
			const u8 g = GPU_screen3D[t+1];
			const u8 b = GPU_screen3D[t+0];
			*dst++ = r;
			*dst++ = g;
			*dst++ = b;
			*dst++ = a;
		}
	}*/

	//glReadPixels(0,0,256,192,GL_STENCIL_INDEX,		GL_UNSIGNED_BYTE,	GPU_screenStencil);
	//ENDGL();

	//memcpy(gfx3d_convertedScreen, GPU_screen3D, GFX3D_FRAMEBUFFER_WIDTH*GFX3D_FRAMEBUFFER_HEIGHT*4);

//debug: view depth buffer via color buffer for debugging
	//int ctr=0;
	//for(ctr=0;ctr<256*192;ctr++) {
	//	float zval = GPU_screen3Ddepth[ctr];
	//	u8* colorPtr = GPU_screen3D+ctr*3;
	//	if(zval<0) {
	//		colorPtr[0] = 255;
	//		colorPtr[1] = 0;
	//		colorPtr[2] = 0;
	//	} else if(zval>1) {
	//		colorPtr[0] = 0;
	//		colorPtr[1] = 0;
	//		colorPtr[2] = 255;
	//	} else {
	//		colorPtr[0] = colorPtr[1] = colorPtr[2] = zval*255;
	//		//printlog("%f %f %d\n",zval, zval*255,colorPtr[0]);
	//	}

	//}
}


GPU3DInterface gpu3Dgl = {	
	"PSP 3DRender",
	Init,
	Reset,
	Close,
	Render,
	GL_ReadFramebuffer,
	VramReconfigureSignal
};



