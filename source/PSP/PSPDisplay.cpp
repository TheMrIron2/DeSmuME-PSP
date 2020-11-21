#include <pspkernel.h>
#include <pspdebug.h>
#include <pspdisplay.h>
#include <pspctrl.h>
#include <pspvfpu.h>
#include <stdio.h>
#include <pspgu.h>
#include <pspgum.h>
#include <psprtc.h>
#include <psppower.h>

#include <string.h>
#include <malloc.h>

#include "../common.h"

#include "../utils/decrypt/header.h"

#include "vram.h"
#include "pspvfpu.h"
#include "PSPDisplay.h"
#include "pspDmac.h"
#include "../GPU.h"
#include "intraFont.h"

#define SLICE_SIZE 16

#define MAX_COL 3

#define ICON_SZ 32

#define RGB(r,v,b)	((r) | ((v)<<8) | ((b)<<16) | (0xff<<24))

#define Bianco 0xFFFF

#define GU_SCR_WIDTH       480
#define GU_SCR_HEIGHT      272
#define GU_SCR_ASPECT      ((float)GU_SCR_WIDTH / (float)GU_SCR_HEIGHT)

#define GU_VRAM_TOP        0x00000000
#define GU_VRAM_WIDTH      512

#define GU_VRAM_BUFSIZE    (GU_VRAM_WIDTH*GU_SCR_HEIGHT*2)
#define GU_VRAM_BP_0       (void *)(GU_VRAM_TOP)
#define GU_VRAM_BP_1       (void *)(GU_VRAM_TOP+GU_VRAM_BUFSIZE)
#define GU_VRAM_BP_2       (void *)(GU_VRAM_TOP+(GU_VRAM_BUFSIZE*2))

void* list = memalign(16, 2048);
unsigned int __attribute__((aligned(64))) gulist[256 * 192 * 2];

intraFont* Font;
intraFont* RomFont;

struct DispVertex {
	unsigned short u, v;
	signed short x, y, z;
};

static void blit_sliced(int sx, int sy, int sw, int sh, int dx, int dy /*, int SLICE_SIZE*/) {
	int start, end;
	// blit maximizing the use of the texture-cache
	for (start = sx, end = sx + sw; start < end; start += SLICE_SIZE, dx += SLICE_SIZE) {
		struct DispVertex* vertices = (struct DispVertex*)sceGuGetMemory(2 * sizeof(struct DispVertex));
		int width = (start + SLICE_SIZE) < end ? SLICE_SIZE : end - start;

		vertices[0].u = start;
		vertices[0].v = sy;
		vertices[0].x = dx;
		vertices[0].y = dy;
		vertices[0].z = 0;

		vertices[1].u = start + width;
		vertices[1].v = sy + sh;
		vertices[1].x = dx + width;
		vertices[1].y = dy + sh;
		vertices[1].z = 0;

		sceGuDrawArray(GU_SPRITES, TEXTURE_FLAGS, 2, NULL, vertices);
	}
}

static void blit_sliced3D(int sx, int sy, int sw, int sh, int dx, int dy /*, int SLICE_SIZE*/) {
	int start, end;
	// blit maximizing the use of the texture-cache
	for (start = sx, end = sx + sw; start < end; start += SLICE_SIZE, dx += SLICE_SIZE) {
		struct DispVertex* vertices = (struct DispVertex*)sceGuGetMemory(2 * sizeof(struct DispVertex));
		int width = (start + SLICE_SIZE) < end ? SLICE_SIZE : end - start;

		vertices[0].u = start;
		vertices[0].v = sy;
		vertices[0].x = dx;
		vertices[0].y = dy;
		vertices[0].z = 0;

		vertices[1].u = start + width;
		vertices[1].v = sy + sh;
		vertices[1].x = dx + width;
		vertices[1].y = dy + sh;
		vertices[1].z = 0;

		sceGuDrawArray(GU_SPRITES, TEXTURE_R3D_FLAGS, 2, NULL, vertices);
	}
}

class Icon {

public:

	u16* GetIconData() {
		return data;
	}

	char* GetIconName() {
		return RomName;
	}

	char* GetDevName() {
		return Developer;
	}

	char* GetFileName() {
		return Filename;
	}

	void SetIconPixel(u8 X, u8 Y, u16 pixel) {
		data[X + (Y * ICON_SZ)] = pixel;
	}

	void SetIconName(const char* Name) {
		
		if (*Name == '.')
			strcpy(RomName, "Homebrew");
		else
			strcpy(RomName, Name);
		
		RomName[11] = 0;
	}
	void SetDevName(const char* Name) {
		strcpy(Developer, Name);
		Developer[63] = 0;
	}
	void SetFileName(const char* Name) {
		strcpy(Filename, Name);
		Filename[127] = 0;
	}

	void ClearIcon(u16 color) {
		memset(data, color, ICON_SZ * ICON_SZ);
	}

	void MEMSetIcon(u16* buff) {
		sceKernelDcacheWritebackInvalidateAll();
		sceDmacMemcpy(data, buff, ICON_SZ * ICON_SZ * 2);
		sceKernelDcacheWritebackInvalidateAll();
	}

private:
	char RomName[12];
	char Developer[64];
	char Filename[128];
	__attribute__((aligned(16))) u16 data[ICON_SZ * ICON_SZ];
};

Icon menu [MAX_COL];

void DrawBackground(u16 x, u16 y) {
	/*sceGuTexMode(GU_PSM_5551, 0, 0, 0);
	sceGuTexImage(0, 480, 256, BUF_WIDTH / 2, );
	sceGuTexFunc(GU_TFX_REPLACE, GU_TCC_RGB);
	sceGuTexFilter(GU_LINEAR, GU_LINEAR);
	sceGuAmbientColor(0xffffffff);

	// render sprite
	sceGuColor(0xffffffff);

	//Render Screen 1 AKA TOP
	blit_sliced(0, 0, ICON_SZ, ICON_SZ, x, y);*/
}

void DrawIcon(u16 x, u16 y, u8 sprX) {

	sceGuColor(0xffffffff);

	struct DispVertex* vertices = (struct DispVertex*)sceGuGetMemory(2 * sizeof(struct DispVertex));

	sceGuTexMode(GU_PSM_5551, 0, 0, 0);
	sceGuTexImage(0, ICON_SZ, ICON_SZ, ICON_SZ, menu[sprX].GetIconData());
	sceGuTexFunc(GU_TFX_REPLACE, GU_TCC_RGB);
	sceGuTexFilter(GU_LINEAR, GU_LINEAR);
	sceGuTexWrap(GU_CLAMP,GU_CLAMP);

	vertices[0].u = 0;
	vertices[0].v = 0;
	vertices[0].x = x;
	vertices[0].y = y;
	vertices[0].z = 0;

	vertices[1].u = ICON_SZ;
	vertices[1].v = ICON_SZ;
	vertices[1].x = x + ICON_SZ + 15;
	vertices[1].y = y + ICON_SZ + 15;
	vertices[1].z = 0;

	sceKernelDcacheWritebackInvalidateAll();
	sceGuDrawArray(GU_SPRITES, TEXTURE_FLAGS, 2, NULL, vertices);

}

void DrawInfoMenu() {
	sceGuTexEnvColor(0xFF9F3F3F);
	sceGuColor(0xFFAC6F3F);

	struct DispVertex* vertices = (struct DispVertex*)sceGuGetMemory(2 * sizeof(struct DispVertex));

	sceGuDisable(GU_TEXTURE_2D);

	vertices[0].u = 0;
	vertices[0].v = 0;
	vertices[0].x = 0;
	vertices[0].y = 180;
	vertices[0].z = 0;

	vertices[1].u = 0;
	vertices[1].v = 0;
	vertices[1].x = 480;
	vertices[1].y = 272;
	vertices[1].z = 0;

	sceKernelDcacheWritebackInvalidateAll();
	sceGuDrawArray(GU_SPRITES, TEXTURE_FLAGS, 2, NULL, vertices);

	sceGuEnable(GU_TEXTURE_2D);
}

void DrawSprite()
{
}


int curr_posX = -1;
int curr_page = 0;
int howManyRom = 0;
 
void Set_POSX(int pos) {
	curr_posX = pos;
}

void Set_PAGE(int pos) {
	curr_page = pos;
}

void drawmenu() {

	sceGuStart(GU_DIRECT, gulist);
	
	sceGuClearColor(0x228A8F8F);
	sceGuClear(GU_COLOR_BUFFER_BIT);
	sceGuClearDepth(0);
	sceGuClearStencil(0);

	int romX = 0;
	
	DrawInfoMenu();
	
	for (int x = 70; x < 470; x += 150, romX++) {

		if (howManyRom <= romX) break;

		if (romX == curr_posX) {
			DrawIcon(x, 70, romX);
			intraFontPrint(RomFont, 5, 200, "Game:");
			intraFontPrint(RomFont, 5, 215, menu[romX].GetIconName());
			intraFontPrint(RomFont, 5, 235, "Developer:");
			intraFontPrint(RomFont, 5, 250, menu[romX].GetDevName());
			intraFontPrint(RomFont, 180, 200, "File name:");
			intraFontPrint(RomFont, 180, 215, menu[romX].GetFileName());
			intraFontPrint(RomFont, 180, 240, "Press X to start the game");
			intraFontPrint(RomFont, 180, 255, "Press [] to exit");
		}
		else
			DrawIcon(x, 85, romX);	
	}

	
	
	char buff[12];
	char buffBattery[16];
	sprintf(buff, "Pag:%d", curr_page+1);
	sprintf(buffBattery, "Battery:%d%%", scePowerGetBatteryLifePercent());
	intraFontPrint(Font, 25, 15, buff);
	intraFontPrint(Font, 410, 15, buffBattery);

	sceGuFinish();
	sceGuSync(GU_SYNC_FINISH, GU_SYNC_WHAT_DONE);
	//sceDisplayWaitVblankStart();
	//sceGuSwapBuffers();
}

const int sz_SCR = 256 * 192 * 4;

void StartGU_RENDER() {
	sceGuStart(GU_DIRECT, gulist);
}

void ENDGU_RENDER() {
	sceGuFinish();
	sceGuSync(GU_SYNC_FINISH, GU_SYNC_WHAT_DONE);
}

int top_index = 0;
int bottom_index = sz_SCR;

#include "pspdisplay.h"

const int padding_top = (1024 * 48);
#define VRAM_START 0x4000000

void* DISP_POINTER = (void*)VRAM_START + padding_top;

void* fbp0 = 0;

void SetupDisp_EMU() {
	sceGuStart(GU_DIRECT, gulist);
	sceGuDrawBuffer(GU_PSM_5551, (void*)DISP_POINTER, 512);
	sceGuFinish();
	sceGuSync(GU_SYNC_FINISH, GU_SYNC_WHAT_DONE);
}

#include "../rasterize.h"


void EMU_SCREEN() {
	sceDmacMemcpy(DISP_POINTER, (const void*)&GPU_Screen, sz_SCR);
//	sceDmacMemcpy(DISP_POINTER, (const void*)&_screen, sz_SCR);
}

void SEND_DISP() { return; sceGuSendList(GU_TAIL, gulist, 0); }

void SYNC_GUDISP(){ return; sceGuSync(GU_SYNC_FINISH, GU_SYNC_WHAT_DONE); }

//static u8* dbp0 = fbp0 + 512 * 272 * sizeof(u32);

void* frameBuffer = (void*)0;
const void* doubleBuffer = (void*)0x44000;
const void* depthBuffer = (void*)0x110000;

void Init_PSP_DISPLAY_FRAMEBUFF() {
	static bool inited = false;

	sceGuInit();

	sceGuStart(GU_DIRECT, list);

	ScePspFMatrix4 _default = {
		{ 1, 0, 0, 0},
		{ 0, 1, 0, 0},
		{ 0, 0, 1, 0},
		{ 0, 0, 0, 1}
	};

	// Init draw an disp buffers from the base of the vram

	//Reset 3D buffer
	//sceGuDrawBuffer(GU_PSM_5551, (void*)doubleBuffer, GU_VRAM_WIDTH);

	sceGuDrawBuffer(GU_PSM_5551, frameBuffer, GU_VRAM_WIDTH);
	sceGuDispBuffer(SCR_WIDTH, SCR_HEIGHT, (void*)frameBuffer, GU_VRAM_WIDTH);
	sceGuDepthBuffer((void*)depthBuffer, GU_VRAM_WIDTH);

	//sceGuDrawBufferList(GU_PSM_5551, (void*)depthBuffer, 512);

	//sceGuDepthRange(65535, 0);

	// Background color and disable scissor test
	// because it is enabled by default with no size sets
	sceGuClearColor(0xFF404040);
	sceGuDisable(GU_SCISSOR_TEST);

	sceGuDepthFunc(GU_GEQUAL);
	sceGuEnable(GU_DEPTH_TEST);
	//sceGuDepthBuffer(dbp0, 512);

	// Enable clamped rgba texture mode
	sceGuTexWrap(GU_CLAMP, GU_CLAMP);
	sceGuTexMode(GU_PSM_5551, 0, 1, 0);
	sceGuEnable(GU_TEXTURE_2D);

	// Enable modulate blend mode 
	sceGuEnable(GU_BLEND);
	sceGuTexFunc(GU_TFX_MODULATE, GU_TCC_RGBA);
	sceGuBlendFunc(GU_ADD, GU_SRC_ALPHA, GU_ONE_MINUS_SRC_ALPHA, 0, 0);

	sceGuSetMatrix(GU_PROJECTION, &_default);
	sceGuSetMatrix(GU_TEXTURE, &_default);
	sceGuSetMatrix(GU_MODEL, &_default);
	sceGuSetMatrix(GU_VIEW, &_default);

	//sceGuOffset(2048 - (480 / 2), 2048 - (272 / 2));
	sceGuViewport(0, 0, 480, 272);

	// Turn the display on, and finish the current list
	sceGuFinish();
	sceGuSync(GU_SYNC_FINISH, GU_SYNC_WHAT_DONE);

	sceGuDisplay(GU_TRUE);

	if (inited) return;
	inited = true;

	static const char* font = "flash0:/font/ltn1.pgf"; //small font
	static const char* font2 = "flash0:/font/ltn0.pgf"; //small font

	intraFontInit();
	Font = intraFontLoad(font, INTRAFONT_CACHE_MED);
	intraFontActivate(Font);
	intraFontSetStyle(Font, 0.6f, 0xFFFFFFFF, 0, 0, INTRAFONT_ALIGN_CENTER);

	RomFont = intraFontLoad(font2, INTRAFONT_CACHE_MED);
	intraFontActivate(RomFont);
	intraFontSetStyle(RomFont, 0.6f, 0xFF000000, 0, 0, 0);

	/*for (int i = 0; i < 3;++i) {
		menu[i].Init();
	}
	*/
	//sceDisplaySetFrameBuf((void*)VRAM_START, 512, PSP_DISPLAY_PIXEL_FORMAT_5551, PSP_DISPLAY_SETBUF_NEXTFRAME);

}

//From: https://github.com/CTurt/IconExtractor/blob/master/source/main.c

Header header;

int readBanner(char* filename, tNDSBanner* banner) {
	FILE* romF = fopen(filename, "rb");
	if (!romF) return 1;

	fread(&header, sizeof(header), 1, romF);
	fseek(romF, header.banner_offset, SEEK_SET);
	fread(banner, sizeof(*banner), 1, romF);
	fclose(romF);

	return 0;
}


bool CreateRomIcon(char* file, f_list* list) {

	tNDSBanner banner;

	howManyRom = 0;

	for (int c = 0; c < MAX_COL;c++) {

		if (list->cnt <= c) break;

		char rompath[256];

		int index = c + (curr_page * 3);

		if (list->cnt < index) break;

		strcpy(rompath, file);
		strcat(rompath, list->fname[index].name);

		if (readBanner(rompath, &banner)) {
			return false;
		}

		u16 image[32][32];

		int tile, pixel;
		for (tile = 0; tile < 16; tile++) {
			for (pixel = 0; pixel < 32; pixel++) {
				unsigned short a = banner.icon[(tile << 5) + pixel];

				int px = ((tile & 3) << 3) + ((pixel << 1) & 7);
				int py = ((tile >> 2) << 3) + (pixel >> 2);

				unsigned short upper = (a & 0xf0) >> 4;
				unsigned short lower = (a & 0x0f);

				if (upper != 0) image[py][px + 1] = banner.palette[upper];
				else image[py][px + 1] = 0;

				if (lower != 0) image[py][px] = banner.palette[lower];
				else image[py][px] = 0;
			}
		}

		menu[c].MEMSetIcon((u16*)image);
		menu[c].SetIconName(header.title);
		menu[c].SetDevName(getDeveloperNameByID(atoi(header.makercode)).c_str());
		menu[c].SetFileName(list->fname[index].name);
		howManyRom++;
	}

return true;
}

int old_page = -1;


void DrawRom(char* file, f_list* list, int pos, bool reload) {

	char rompath[256];
	char RomFileName[128];
	//Get rom file path 
	strcpy(rompath, file);

	curr_page = pos / 3;
	curr_posX = pos % 3;	

	if (old_page != curr_page) {
		CreateRomIcon(rompath, list);
		old_page = curr_page;
	}
	drawmenu();

}





#if 0
#include <oslib/oslib.h>

OSL_IMAGE* displayUpper;
OSL_IMAGE* displayLower;
OSL_IMAGE* rom;
OSL_IMAGE* background;


void Init_PSP_DISPLAY_FRAMEBUFF() {
	oslInit(0);
	oslInitGfx(OSL_PF_5551, 0);

	oslIntraFontInit(INTRAFONT_CACHE_MED);

	oslIntraFontSetStyle(osl_sceFont, 1.0, RGBA(255, 255, 255, 255), RGBA(0, 0, 0, 0), INTRAFONT_ALIGN_LEFT);
	oslSetFont(osl_sceFont);

	/*displayUpper = oslCreateImage(256, 192, OSL_IN_VRAM, OSL_PF_5551);
	oslClearImage(displayUpper, RGB15(0, 0, 0));

	displayLower = oslCreateImage(256, 192, OSL_IN_VRAM, OSL_PF_5551);
	oslClearImage(displayLower, RGB15(0, 0, 0));

	rom = oslCreateImage(32, 32, OSL_IN_VRAM, OSL_PF_5551);
	oslClearImage(rom, RGB15(0, 0, 0));*/

	background = oslLoadImageFileJPG("background.jpg", OSL_IN_VRAM | OSL_SWIZZLED, OSL_PF_5551);
}



void ShowFPS(int x, int y, pl_perf_counter& fps_counter) {
	/*pspDebugScreenSetXY(x, y);
	pspDebugScreenPrintf("FPS: %.2f     ", fps_counter.fps);
	sceDisplayWaitVblankStart();*/

	char fullText[50];
	sprintf(fullText, "FPS: %.2f    ", fps_counter.fps);
	oslDrawString(x, y, "ciaone");
}


void PrintfXY(const char* text, int x, int y) {
	/*pspDebugScreenSetXY(x, y);
	pspDebugScreenPrintf(text);
	sceDisplayWaitVblankStart();*/
	oslDrawString(x, y, text);
}*/


void DrawStartUpMenu() {
	oslStartDrawing();

	oslDrawImageXY(background, 115, 40);

	//Draw Rom Icon
	{
		OSL_IMAGE* rom = oslLoadImageFileJPG("rom.jpg", OSL_IN_RAM | OSL_SWIZZLED, OSL_PF_5551);
		oslDrawImageXY(rom, 145, 110);
	}

	oslEndDrawing();
}

void DrawSettingsMenu(configP* params, int size, int currPos) {
	oslStartDrawing();
	oslClearScreen(0);

	oslDrawImageXY(background, 115, 40);

	for (u8 i = size; i--;) {

		char fullText[512];
		sprintf(fullText, "%s: %d", params[i].name, params[i].var);

		if (i != currPos)
			oslDrawString(120, 45 + (i * 20), fullText);
		else
			oslDrawTextBox(110, 40 + (i * 20), 400, 45 + (i * 20), fullText, 0);
	}

	oslEndDrawing();
}


//From: https://github.com/CTurt/IconExtractor/blob/master/source/main.c

//char * gameTitle = "test\0";
Header header;

int readBanner(char* filename, tNDSBanner* banner) {


	FILE* romF = fopen(filename, "rb");
	if (!romF) return 1;

	fread(&header, sizeof(header), 1, romF);
	fseek(romF, header.banner_offset, SEEK_SET);
	fread(banner, sizeof(*banner), 1, romF);
	fclose(romF);

	return 0;
}

#define RAW_RED(colour) (((colour)) & 0x1f)
#define RAW_BLUE(colour) (((colour) >> 5) & 0x1f)
#define RAW_GREEN(colour) (((colour) >> 10) & 0x1f)

#define RED(colour) (((RAW_RED(colour) << 3) + (RAW_RED(colour) >> 2)))
#define BLUE(colour) (((RAW_BLUE(colour) << 3) + (RAW_BLUE(colour) >> 2)))
#define GREEN(colour) (((RAW_GREEN(colour) << 3) + (RAW_GREEN(colour) >> 2)))

void DStoRGBA(unsigned short(*ds)[32], unsigned char(*rgba)[32][4]) {
	int x, y;
	for (x = 0; x < 32; x++) {
		for (y = 0; y < 32; y++) {
			unsigned short c = ds[y][x];

			rgba[x][y][0] = RAW_RED(c);
			rgba[x][y][1] = RAW_BLUE(c);
			rgba[x][y][2] = RAW_GREEN(c);
			rgba[x][y][3] = 0;
		}
	}
}

void CreateRomIcon(char* file, OSL_IMAGE* romimg) {
	
	tNDSBanner banner;
	

	if (readBanner(file, &banner)) {
		return;
	}

	u16 image[32][32];
	

	union {
		unsigned char rgbaImage[32][32][4];
		u16 u16rgbaImage[64][64];
	};

	int tile, pixel;
	for (tile = 0; tile < 16; tile++) {
		for (pixel = 0; pixel < 32; pixel++) {
			unsigned short a = banner.icon[(tile << 5) + pixel];

			int px = ((tile & 3) << 3) + ((pixel << 1) & 7);
			int py = ((tile >> 2) << 3) + (pixel >> 2);

			unsigned short upper = (a & 0xf0) >> 4;
			unsigned short lower = (a & 0x0f);

			if (upper != 0) image[px + 1][py] = banner.palette[upper];
			else image[px + 1][py] = 0;

			if (lower != 0) image[px][py] = banner.palette[lower];
			else image[px][py] = 0;
		}
	}

	DStoRGBA(image, rgbaImage);

	for (int y = 0; y < 32; y++) {
		for (int x = 0; x < 32 ; x++) {
			//Set the pixel into the image buffer  	 //Generate lighter pixels
			oslSetImagePixel(romimg, x, y, RGB15((int)(rgbaImage[y][x][0] * 7), (int)(rgbaImage[y][x][1] * 7), (int)(rgbaImage[y][x][2]  * 7))); 
		}
	}
}

int curr_pos = 0;

void DrawRom(char* file, f_list* list, int pos, bool reload) {

	oslSetDrawBuffer(OSL_DEFAULT_BUFFER);
	oslStartDrawing();

	//Draw Background
	/*oslDrawImageXY(background, 115, 40);
	oslDrawFillRect(140, 75, 300, 120, RGBA15(255, 255, 255, 255));*/

	//Draw Rom Icon

	char rompath[256];
	int y_pos = 40;

	bool first = false, second = false, third = false;

	curr_pos = (pos/3)*3;

	switch (pos % 3)
	{
	case 0:
		first = true;
		break;
	case 1:
		second = true;
		break;
	case 2:
		third = true;
		break;
	default:
		break;
	}

	char RomFileName[128];

	{
	
		memset(RomFileName, 0, sizeof(RomFileName));

		//Get rom file path 
		strcpy(rompath, file);

		//Add rom filename
		strcat(rompath, list->fname[curr_pos].name);

		//Create rom icon
		CreateRomIcon(rompath, rom);

		//Get rom name
		memcpy(RomFileName, header.gamecode, 4);
		strcat(RomFileName, "_");
		strcat(RomFileName, header.title);

		//Draw the icon + rom name
		if (first) {
			oslDrawImageXY(rom, 145, 70);
			oslDrawStringf(190, 80, RomFileName);
		}
		else {
			oslDrawImageXY(rom, 155, 80);
			oslDrawStringf(200, 90, RomFileName);
		}

		//Repeat the steps for second & third rom

		memset(RomFileName, 0, sizeof(RomFileName));

		strcpy(rompath, file);
		strcat(rompath, list->fname[curr_pos + 1].name);

		CreateRomIcon(rompath, rom);
		memcpy(RomFileName, header.gamecode, 4);
		strcat(RomFileName, "_");
		strcat(RomFileName, header.title);
		if (second) {
			oslDrawImageXY(rom, 145, 70 + y_pos);
			oslDrawStringf(190, 80 + y_pos, RomFileName);
		}
		else {
			oslDrawImageXY(rom, 155, 80 + y_pos);
			oslDrawStringf(200, 90 + y_pos, RomFileName);
		}


		strcpy(rompath, file);
		strcat(rompath, list->fname[curr_pos + 2].name);

		memset(RomFileName, 0, sizeof(RomFileName));

		y_pos *= 2;

		CreateRomIcon(rompath, rom);
		memcpy(RomFileName, header.gamecode, 4);
		strcat(RomFileName, "_");
		strcat(RomFileName, header.title);
		
		
		if (third) {
			oslDrawImageXY(rom, 145, 70 + y_pos);
			oslDrawStringf(190, 80 + y_pos, RomFileName);
		}
		else {
			oslDrawImageXY(rom, 155, 80 + y_pos);
			oslDrawStringf(200, 90 + y_pos, RomFileName);
		}
	}

	oslEndDrawing();

}

void UpdateSingleScreen() {
	oslStartDrawing();
	oslSetDrawBuffer(OSL_DEFAULT_BUFFER);
	oslDrawImageXY(displayUpper, 128, 40);
	oslEndDrawing();
}

OSL_COLOR backColor = 0;
OSL_COLOR color_render = RGB15(255, 255, 255);


#define VRAM_START 0x4000000
static unsigned int __attribute__((aligned(16))) List[16]; /* TODO: ? */

void SetupVramDirectMode()
{
	int size;

	/*fbp1 = getStaticVramBuffer(256, 384, GU_PSM_5551);

	sceGuInit();

	sceGuStart(GU_DIRECT, List);
	sceGuDispBuffer(256, 192, (void*)(VRAM_START + (u8*)fbp1), 256);
	sceGuClear(GU_COLOR_BUFFER_BIT);
	sceGuFinish();

	sceGuSync(0, 0);*/
	sceKernelDcacheWritebackAll();
}

void (*UpdateScreen)();

const u16 screen_gap_top = 1 << 13;
const u16 screen_gap = 1 << 11;

const u8 size_x = 1 << 7;
const u8 padding_x1 = 50;
const u8 padding_x2 = 78;
const u8 size_y = 96;

void OSLFINISH() {
	oslEndGfx();
}

void UpdateScreenDirect() {
	/*u32* disp = (u32*)(VRAM_START + fbp1);
	u32* data = (u32*)displayUpper->data;

	disp += screen_gap_top;

	for (int y = size_y; y--;) {
		disp += padding_x1;
		for (int x = size_x;x--;) {
			*disp = *data;
			++disp;
			++data;
		}
		disp += padding_x2;
		data += size_x;
	}

	disp += screen_gap;

	for (int y = size_y; y--;) {
		disp += padding_x1;
		for (int x = size_x;x--;) {
			*disp = *data;
			++disp;
			++data;
		}
		disp += padding_x2;
		data += size_x;
	}*/
}

void UpdateScreenOGL() {

	oslStartDrawing();

	/*if (my_config.RenderBack) color_render = backColor;
	oslClearScreen(color_render);*/

	oslDrawImageXY(displayUpper, 0, 40);
	oslDrawImageXY(displayLower, 256, 40);

	oslEndDrawing();
}

void SetupEmuDisplay(bool direct) {

	if (direct) {
		SetupVramDirectMode();
		UpdateScreen = UpdateScreenDirect;
	}
	else {
		oslStartDrawing();
		oslSetDrawBuffer(OSL_DEFAULT_BUFFER);
		oslClearScreen(0);
		oslEndDrawing();
		UpdateScreen = UpdateScreenOGL;
	}
}

OSL_IMAGE* GetUpperFrameBuffer() {
	return displayUpper;
}

OSL_IMAGE* GetLowerFrameBuffer() {
	return displayLower;
}

void* GetPointerFrameBuffer(bool upper) {
	return upper ? displayUpper->data : displayLower->data;
}

void* GetPointerUpperFrameBuffer() {
	return displayUpper->data;
}

void* GetPointerLowerFrameBuffer() {
	return displayLower->data;
}
#endif