#include <pspkernel.h>
#include <pspdebug.h>
#include <pspdisplay.h>
#include <pspctrl.h>
#include <pspvfpu.h>
#include <stdio.h>
#include <pspgu.h>
#include <pspgum.h>
#include <psprtc.h>

#include "video.h"
#include "FrontEnd.h"

#define SLICE_SIZE 64
#define	FRAMESIZE	0x44000			//in byte
#define VRAM_START 0x4000000
#define VRAM_SIZE  0x00200000

float curr_ms = 1.0f;
struct timeval time_slices[16];
int val = 0;

static int   TexFilter;
static void *DisplayBuffer;
static void *DrawBuffer;
static int   PixelFormat;
static int   TexFormat;
static int   TexColor;
static void *VramOffset;
static void *VramChunkOffset;

static unsigned short __attribute__((aligned(16))) ScratchBuffer[BUF_WIDTH * SCR_HEIGHT];
static unsigned int VramBufferOffset;
static unsigned int __attribute__((aligned(16))) List[512*192*4]; /* TODO: ? */

void FPS() {
    float curr_fps = 1.0f / curr_ms;

    pspDebugScreenSetXY(0,5);
    pspDebugScreenPrintf("FPS %d.%03d V0.6",(int)curr_fps,(int)((curr_fps-(int)curr_fps) * 1000.0f));
    gettimeofday(&time_slices[val & 15],0);

    val++;

    if (!(val & 15)) {
        struct timeval last_time = time_slices[0];
        unsigned int i;

        curr_ms = 0;
        for (i = 1; i < 16; ++i) {
            struct timeval curr_time = time_slices[i];

            if (last_time.tv_usec > curr_time.tv_usec) {
                curr_time.tv_sec++;
                curr_time.tv_usec-=1000000;
            }

            curr_ms += ((curr_time.tv_usec-last_time.tv_usec) + (curr_time.tv_sec-last_time.tv_sec) * 1000000) * (1.0f/1000000.0f);

            last_time = time_slices[i];
        }
        curr_ms /= 15.0f;
    }
    sceDisplayWaitVblankStart();
}

void* fbp0;
void* fbp1;
void* zbp;
void* framebuffer;

void GuInit() {
    PixelFormat = GU_PSM_5551;
    TexFormat = GU_PSM_5551;
    TexColor = GU_COLOR_5551;
    TexFilter = GU_NEAREST;

    int size;

    fbp0 = getStaticVramBuffer(BUF_WIDTH,SCR_HEIGHT,GU_PSM_8888);
    fbp1 = getStaticVramBuffer(BUF_WIDTH,SCR_HEIGHT,GU_PSM_8888);
    zbp = getStaticVramBuffer(BUF_WIDTH,SCR_HEIGHT,GU_PSM_4444);

    sceGuInit();

    sceGuStart(GU_DIRECT,List);
    sceGuDrawBuffer(GU_PSM_5551,fbp0,BUF_WIDTH);
    sceGuDispBuffer(SCR_WIDTH,SCR_HEIGHT,fbp1,BUF_WIDTH);
    sceGuDepthBuffer(zbp,BUF_WIDTH);
    sceGuDisable(GU_TEXTURE_2D);
    sceGuOffset(0, 0);
    sceGuViewport(SCR_WIDTH/2, SCR_HEIGHT/2, SCR_WIDTH, SCR_HEIGHT);
    sceGuDepthRange(0xc350, 0x2710);
    sceGuDisable(GU_ALPHA_TEST);
    sceGuBlendFunc(GU_ADD, GU_SRC_ALPHA, GU_ONE_MINUS_SRC_ALPHA, 0, 0);
    sceGuEnable(GU_BLEND); // todo: hmm
    sceGuDisable(GU_DEPTH_TEST);
    sceGuEnable(GU_CULL_FACE);
    sceGuDisable(GU_LIGHTING);
    sceGuFrontFace(GU_CW);
    sceGuScissor(0, 0, SCR_WIDTH, SCR_HEIGHT);
    sceGuEnable(GU_SCISSOR_TEST);
    sceGuClear(GU_COLOR_BUFFER_BIT|GU_DEPTH_BUFFER_BIT);
    //sceGuAmbientColor(0xffffffff);
    sceGuFinish();
    sceGuSync(0,0);

    sceKernelDcacheWritebackAll();

    sceGuDisplay(1);
}

void pspVideoSwapBuffers() {
    fbp1 = sceGuSwapBuffers();
}

void pspVideoShutdown() {
    sceGuTerm();
}

void pspVideoWaitVSync() {
    sceDisplayWaitVblankStart();
}


void GuImageDirect(u16* imageA,u16* imageB,int VX,int VY,int Width,int Height, int dx, int dy, int dw, int dh) {
    sceKernelDcacheWritebackAll();

    sceGuStart(GU_DIRECT, List);

    sceGuScissor(dx, dy, dx + dw, dy + dh);

    if (dw == Width && dh == Height) {
        if(vertical) {
            sceGuCopyImage(PixelFormat,
                VX, VY,
                Width, Height,
                Width, imageA, dx, dy,
                BUF_WIDTH, (void *)(VRAM_START + (u32)fbp1));
                sceGuCopyImage(PixelFormat,
                    VX, VY,
                    Width, Height,
                    Width, imageB, Width, dy,
                    BUF_WIDTH, (void *)(VRAM_START + (u32)fbp1));
                    sceGuTexSync();
                } else {
                    sceGuCopyImage(PixelFormat,
                        VX, VY,
                        240, Height,
                        Width, imageA, dx, dy,
                        BUF_WIDTH, (void *)(VRAM_START + (u32)fbp1));
                        sceGuCopyImage(PixelFormat,
                            VX, VY,
                            240, Height,
                            Width, imageB, 240, dy,
                            BUF_WIDTH, (void *)(VRAM_START + (u32)fbp1));
                            sceGuTexSync();
                        }
                    } else {
                        sceGuEnable(GU_TEXTURE_2D);
                        sceGuTexMode(TexFormat, 0, 0, 0);
                        sceGuTexImage(0, Width, Width, Width, imageA);
                        sceGuTexFunc(GU_TFX_REPLACE, GU_TCC_RGBA);
                        sceGuTexFilter(TexFilter, TexFilter);

                        struct Vertex* vertices;
                        int start, end, sc_end, slsz_scaled;
                        slsz_scaled = ceil((float)dw * (float)SLICE_SIZE) / (float)Width;

                        for (start = VX, end = VX + Width, sc_end = dx + dw; start < end; start += SLICE_SIZE, dx += slsz_scaled) {
                            vertices = (struct Vertex*)sceGuGetMemory(2 * sizeof(struct Vertex));

                            vertices[0].u = start;
                            vertices[0].v = VY;
                            vertices[1].u = start + SLICE_SIZE;
                            vertices[1].v = Height + VY;

                            vertices[0].x = dx; vertices[0].y = dy;
                            vertices[1].x = dx + slsz_scaled; vertices[1].y = dy + dh;

                            vertices[0].color
                            = vertices[1].color
                            = vertices[0].z
                            = vertices[1].z = 0;

                            sceGuDrawArray(GU_SPRITES,GU_TEXTURE_16BIT|GU_COLOR_5551|GU_VERTEX_16BIT|GU_TRANSFORM_2D,2,0,vertices);
                        }

                        sceGuDisable(GU_TEXTURE_2D);

                    }

                    sceGuScissor(0, 0, SCR_WIDTH, SCR_HEIGHT);
                    sceGuFinish();
                    sceGuSync(0,0);
                    pspVideoSwapBuffers();
                    sceDisplayWaitVblankStart();
                }
