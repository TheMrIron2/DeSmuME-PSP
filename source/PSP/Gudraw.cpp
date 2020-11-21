#include <pspkernel.h>
#include <pspdebug.h>
#include <pspdisplay.h>
#include <pspctrl.h>
#include <pspvfpu.h>
#include <stdio.h>
#include <pspgu.h>
#include <pspgum.h>
#include <psprtc.h>

#include "../utils/decrypt/header.h"

#include "vram.h"
#include "video.h"
#include <oslib/oslib.h>

OSL_IMAGE* displayUpper;
OSL_IMAGE* displayLower;
OSL_IMAGE* rom;
OSL_IMAGE* background;


void Init_PSP_DISPLAY_FRAMEBUFF() {
	oslInit(0);
	oslInitGfx(OSL_PF_5551, 0);
	oslSetQuitOnLoadFailure(1);

	oslIntraFontInit(INTRAFONT_CACHE_MED);

	oslIntraFontSetStyle(osl_sceFont, 1.0, RGBA(255, 255, 255, 255), RGBA(0, 0, 0, 0), INTRAFONT_ALIGN_LEFT);
	oslSetFont(osl_sceFont);

	displayUpper = oslCreateImage(256, 192, OSL_IN_VRAM, OSL_PF_5551);
	oslClearImage(displayUpper, RGB15(0, 0, 0));

	displayLower = oslCreateImage(256, 192, OSL_IN_VRAM, OSL_PF_5551);
	oslClearImage(displayLower, RGB15(0, 0, 0));

	rom = oslCreateImage(32, 32, OSL_IN_VRAM, OSL_PF_5551);
	oslClearImage(rom, RGB15(0, 0, 0));

	background = oslLoadImageFileJPG("background.jpg", OSL_IN_VRAM | OSL_SWIZZLED, OSL_PF_5551);
}

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
	oslDrawImageXY(background, 115, 40);
	oslDrawFillRect(140, 75, 300, 120, RGBA15(255, 255, 255, 255));

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

void* fbp1;
#define VRAM_START 0x4000000
static unsigned int __attribute__((aligned(16))) List[16]; /* TODO: ? */

void SetupVramDirectMode()
{
	int size;

	fbp1 = getStaticVramBuffer(256, 384, GU_PSM_5551);

	sceGuInit();

	sceGuStart(GU_DIRECT, List);
	sceGuDispBuffer(256, 192, (void*)(VRAM_START + (u8*)fbp1), 256);
	sceGuClear(GU_COLOR_BUFFER_BIT);
	sceGuFinish();

	sceGuSync(0, 0);
	sceKernelDcacheWritebackAll();
}

void (*UpdateScreen)();

const u16 screen_gap_top = 1 << 13;
const u16 screen_gap = 1 << 11;

const u8 size_x = 1 << 7;
const u8 padding_x1 = 50;
const u8 padding_x2 = 78;
const u8 size_y = 96;

void UpdateScreenDirect() {
	u32* disp = (u32*)(VRAM_START + fbp1);
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
	}
}

void UpdateScreenOGL() {

	oslStartDrawing();

	if (my_config.RenderBack) color_render = backColor;
	oslClearScreen(color_render);

	oslDrawImageXY(displayUpper, 0, 40);
	oslDrawImageXY(displayLower, 256, 40);

	oslEndDrawing();
}

void SetupEmuDisplay(bool direct) {
	oslStartDrawing();
	oslSetDrawBuffer(OSL_DEFAULT_BUFFER);
	oslClearScreen(0);
	oslEndDrawing();

	if (direct) {
		SetupVramDirectMode();
		UpdateScreen = UpdateScreenDirect;
	}
	else {
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
