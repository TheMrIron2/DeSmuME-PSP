/*
	Copyright (C) 2006 yopyop
	Copyright (C) 2008-2015 DeSmuME team

	This file is free software: you can redistribute it and/or modify
	it under the terms of the GNU General Public License as published by
	the Free Software Foundation, either version 2 of the License, or
	(at your option) any later version.

	This file is distributed in the hope that it will be useful,
	but WITHOUT ANY WARRANTY; without even the implied warranty of
	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
	GNU General Public License for more details.

	You should have received a copy of the GNU General Public License
	along with the this software.  If not, see <http://www.gnu.org/licenses/>.
*/

#include "NDSSystem.h"

#include <string.h>
#include <stdlib.h>
#include <algorithm>

#include <math.h>
//#include <zlib.h>

#include "utils/decrypt/decrypt.h"
#include "utils/decrypt/crc.h"
#include "utils/advanscene.h"
//#include "utils/task.h"

#include "common.h"
#include "armcpu.h"
#include "render3D.h"
#include "MMU.h"
#include "ROMReader.h"
#include "gfx3d.h"
#include "GPU.h"
#include "cp15.h"
#include "bios.h"
#include "debug.h"
#include "FIFO.h"
#include "readwrite.h"
#include "registers.h"
#include "debug.h"
#include "driver.h"
#include "firmware.h"
#include "version.h"
#include "path.h"
#include "slot1.h"
#include "slot2.h"
#include "SPU.h"
#include "wifi.h"

#include "PSP/FrontEnd.h"

#include"PSP/pspDmac.h"
#include"PSP/PSPDisplay.h"

#include "melib.h"
#include "me.h"


bool dolog = false;
//===============================================================
FILE *fp_dis7 = NULL;
FILE *fp_dis9 = NULL;

TCommonSettings CommonSettings;
PathInfo path;

static BOOL LidClosed = FALSE;
static u8	countLid = 0;

GameInfo gameInfo;
NDSSystem nds;
CFIRMWARE	*firmware = NULL;

using std::min;
using std::max;

TSCalInfo TSCal;

PSP_SECTION_START(sc_write)
PSP_SECTION_START(me_write)

// 
PSP_SECTION(sc_write)
   static volatile bool Do2dRender;

// 
PSP_SECTION(me_write)
   static volatile bool RenderDone;

PSP_SECTION_END(sc_write)
PSP_SECTION_END(me_write)

namespace DLDI
{
	bool tryPatch(void* data, size_t size, unsigned int device);
}

class FrameSkipper
{
public:
	void RequestSkip()
	{
		nextSkip = true;
	}
	void OmitSkip(bool force, bool forceEvenIfCapturing=false)
	{
		nextSkip = false;
		if((force && consecutiveNonCaptures > 30) || forceEvenIfCapturing)
		{
			SkipCur2DFrame = false;
			SkipCur3DFrame = false;
			SkipNext2DFrame = false;
			if(forceEvenIfCapturing)
				consecutiveNonCaptures = 0;
		}
	}
	void Advance()
	{
		bool capturing = (MainScreen.gpu->dispCapCnt.enabled || (MainScreen.gpu->dispCapCnt.val & 0x80000000));

		if(capturing && consecutiveNonCaptures > 30)
		{
			// the worst-looking graphics corruption problems from frameskip
			// are the result of skipping the capture on first frame it turns on.
			// so we do this to handle the capture immediately,
			// despite the risk of 1 frame of 2d/3d mismatch or wrong screen display.
			SkipNext2DFrame = false;
			nextSkip = false;
		}
		else if(lastOffset != MainScreen.offset && lastSkip && !skipped)
		{
			// if we're switching from not skipping to skipping
			// and the screens are also switching around this frame,
			// go for 1 extra frame without skipping.
			// this avoids the scenario where we only draw one of the two screens
			// when a game is switching screens every frame.
			nextSkip = false;
		}

		if(capturing)
			consecutiveNonCaptures = 0;
		else if(!(consecutiveNonCaptures > 9000)) // arbitrary cap to avoid eventual wrap
			consecutiveNonCaptures++;
		lastLastOffset = lastOffset;
		lastOffset = MainScreen.offset;
		lastSkip = skipped;
		skipped = nextSkip;
		nextSkip = false;

		SkipCur2DFrame = SkipNext2DFrame;
		SkipCur3DFrame = skipped;
		SkipNext2DFrame = skipped;
	}
	FORCEINLINE volatile bool ShouldSkip2D()
	{
		return SkipCur2DFrame;
	}
	FORCEINLINE bool ShouldSkip3D()
	{
		return SkipCur3DFrame;
	}
	FrameSkipper()
	{
		nextSkip = false;
		skipped = false;
		lastSkip = false;
		lastOffset = 0;
		SkipCur2DFrame = false;
		SkipCur3DFrame = false;
		SkipNext2DFrame = false;
		consecutiveNonCaptures = 0;
	}
private:
	bool nextSkip;
	bool skipped;
	bool lastSkip;
	int lastOffset;
	int lastLastOffset;
	int consecutiveNonCaptures;
	volatile bool SkipCur2DFrame;
	bool SkipCur3DFrame;
	bool SkipNext2DFrame;
};
static FrameSkipper frameSkipper;

void Desmume_InitOnce()
{
	static bool initOnce = false;
	if(initOnce) return;
	initOnce = true;
}

int NDS_GetCPUCoreCount()
{
	return 1;
}

void NDS_SetupDefaultFirmware()
{
	NDS_FillDefaultFirmwareConfigData(&CommonSettings.fw_config);
}

void NDS_RunAdvansceneAutoImport()
{
}

void renderScreenFull()
{
	static bool upScreen = true;

	for (int i = 0;i < 192;++i) 
		GPU_RenderLine((upScreen ? &MainScreen : &SubScreen), i, frameSkipper.ShouldSkip2D());

	upScreen = !upScreen;
}

void meUtilityDcacheWritebackInvalidateAll(void)
{
	unsigned int cachesize_bits;
    asm volatile("mfc0 %0, $16; ext %0, %0, 6, 3" : "=r" (cachesize_bits));
    const unsigned int cachesize = 4096 << cachesize_bits;

    unsigned int i;
    for (i = 0; i < cachesize; i += 64) {
        asm volatile("cache 0x14, 0(%0)" : : "r" (i));
    }
}


int renderScreen(JobData data)
{
	PSP_UC(RenderDone) = true;

	bool upScreen = true;
	bool _skip = PSP_UC(frameSkipper).ShouldSkip2D();

	while (true) {

		if (!PSP_UC(Do2dRender)) continue;

		PSP_UC(Do2dRender) = false;
		PSP_UC(RenderDone) = false;

		const bool skip = PSP_UC(frameSkipper).ShouldSkip2D();

		if (!PSP_UC(my_config.FastMERendering)) {
			upScreen = true;

			for (int i = 0;i < 192;++i) {
				GPU_RenderLine(&MainScreen, i, skip);
				GPU_RenderLine(&SubScreen, i, skip);
			}

		}else {
			for (int i = 0;i < 192;++i) 
				GPU_RenderLine((upScreen ? &MainScreen : &SubScreen), i, skip);

			upScreen = !upScreen;
		}

		PSP_UC(RenderDone) = true;
		meUtilityDcacheWritebackInvalidateAll();

		/*if (PSP_UC(my_config.enable_sound))
			SPU_Emulate_core();*/
	}

	return 1;
}

int NDS_Init()
{
	nds.idleFrameCounter = 0;
	memset(nds.runCycleCollector,0,sizeof(nds.runCycleCollector));

	J_Init(false);

	MMU_Init();
	
	if (Screen_Init() != 0)
		return -1;

	{
		char	buf[MAX_PATH];
		memset(buf, 0, MAX_PATH);
		strcpy(buf, path.pathToModule);
		strcat(buf, "desmume.ddb");							// DeSmuME database	:)
		advsc.setDatabase(buf);

		//why is this done here? shitty engineering. not intended.
		NDS_RunAdvansceneAutoImport();
	}

	gfx3d_init();

	armcpu_new(&NDS_ARM9,0);
	NDS_ARM9.SetBaseMemoryInterface(&arm9_base_memory_iface);
	NDS_ARM9.SetBaseMemoryInterfaceData(NULL);
	NDS_ARM9.ResetMemoryInterfaceToBase();
	
	armcpu_new(&NDS_ARM7,1);
	NDS_ARM7.SetBaseMemoryInterface(&arm7_base_memory_iface);
	NDS_ARM7.SetBaseMemoryInterfaceData(NULL);
	NDS_ARM7.ResetMemoryInterfaceToBase();
	
	if (SPU_Init(SNDCORE_DUMMY, 740) != 0)
		return -1;
		
	return 0;
}

void NDS_DeInit(void)
{
	gameInfo.closeROM();
	SPU_DeInit();
	Screen_DeInit();
	MMU_DeInit();
	gpu3D->NDS_3D_Close();

	WIFI_DeInit();
	arm_jit_close();
}

NDS_header* NDS_getROMHeader(void)
{
	NDS_header *newHeader = new NDS_header;
	memcpy(newHeader, &gameInfo.header, sizeof(NDS_header));
	
	return newHeader;
} 




void debug()
{
}

RomBanner::RomBanner(bool defaultInit)
{
	if(!defaultInit) return;
	version = 1; //Version  (0001h)
	crc16 = 0; //CRC16 across entries 020h..83Fh
	memset(reserved,0,sizeof(reserved));
	memset(bitmap,0,sizeof(bitmap));
	memset(palette,0,sizeof(palette));
	memset(titles,0,sizeof(titles));
	memset(end0xFF,0,sizeof(end0xFF));
}

bool GameInfo::hasRomBanner()
{
	if(header.IconOff + sizeof(RomBanner) > romsize)
		return false;
	else return true;
}

const RomBanner& GameInfo::getRomBanner()
{
	return banner;
}

void GameInfo::populate()
{
	const char regions_index[] = "JPFSEODIRKHXVWUC";
	const char *regions[] = {
					"???",
					"JPN",		// J
					"EUR",		// P
					"FRA",		// F
					"ESP",		// S
					"USA",		// E
					"INT",		// O
					"NOE",		// D
					"ITA",		// I
					"RUS",		// R
					"KOR",		// K
					"HOL",		// H
					"EUU",		// X
					"EUU",		// V
					"EUU",		// W
					"AUS",		// U
					"CHN",		// C

	};
	
	memset(ROMserial, 0, sizeof(ROMserial));
	memset(ROMname, 0, sizeof(ROMname));

	if(isHomebrew())
	{
		//we can't really make a serial for a homebrew game that hasnt set a game code
		strcpy(ROMserial, "Homebrew");
	}
	else
	{
		if (isDSiEnhanced())
			strcpy(ROMserial,"TWL-    -");
		else
			strcpy(ROMserial,"NTR-    -");
		memcpy(ROMserial+4, header.gameCode, 4);

		u32 regions_num = ARRAY_SIZE(regions);
		u32 region = (u32)(std::max<s32>(strchr(regions_index,header.gameCode[3]) - regions_index + 1, 0));

		if (region < regions_num)
			strncat(ROMserial, regions[region], sizeof(ROMserial));
		else
			strncat(ROMserial, "???", sizeof(ROMserial));
	}

	//rom name is probably set even in homebrew, so do it regardless
	memset(ROMname, 0, sizeof(ROMname));
	memcpy(ROMname, header.gameTile, 12);
	trim(ROMname,20);
}

bool GameInfo::loadROM(std::string fname, u32 type)
{
	//printf("ROM %s\n", CommonSettings.loadToMemory?"loaded to RAM":"stream from disk");

	closeROM();

	fROM = fopen(fname.c_str(), "rb");
	if (!fROM) return false;

	headerOffset = (type == ROM_DSGBA)?DSGBA_LOADER_SIZE:0;
   
	fseek(fROM, 0, SEEK_END);
	romsize = ftell(fROM) - headerOffset;
	fseek(fROM, headerOffset, SEEK_SET);
		

	bool res = (fread(&header, 1, sizeof(header), fROM) == sizeof(header));

	
	if (res)
	{
#ifndef LOCAL_LE
		//endian swap necessary fields. It would be better if we made accessors for these. I wonder if you could make a macro for a field accessor that would take the bitsize and do the swap on the fly
		struct FieldSwap {
			const size_t offset;
			const size_t bytes;
		};
		
		static const FieldSwap fieldSwaps[] = {
			{ offsetof(NDS_header,makerCode), 2},
			
			{ offsetof(NDS_header,ARM9src), 4},
			{ offsetof(NDS_header,ARM9exe), 4},
			{ offsetof(NDS_header,ARM9cpy), 4},
			{ offsetof(NDS_header,ARM9binSize), 4},
			{ offsetof(NDS_header,ARM7src), 4},
			{ offsetof(NDS_header,ARM7exe), 4},
			{ offsetof(NDS_header,ARM7cpy), 4},
			{ offsetof(NDS_header,ARM7binSize), 4},
			{ offsetof(NDS_header,FNameTblOff), 4},
			{ offsetof(NDS_header,FNameTblSize), 4},
			{ offsetof(NDS_header,FATOff), 4},
			{ offsetof(NDS_header,FATSize), 4},
			{ offsetof(NDS_header,ARM9OverlayOff), 4},
			{ offsetof(NDS_header,ARM9OverlaySize), 4},
			{ offsetof(NDS_header,ARM7OverlayOff), 4},
			{ offsetof(NDS_header,ARM7OverlaySize), 4},
			{ offsetof(NDS_header,normalCmd), 4},
			{ offsetof(NDS_header,Key1Cmd), 4},
			{ offsetof(NDS_header,IconOff), 4},
			
			{ offsetof(NDS_header,CRC16), 2},
			{ offsetof(NDS_header,ROMtimeout), 2},
			
			{ offsetof(NDS_header,ARM9autoload), 4},
			{ offsetof(NDS_header,ARM7autoload), 4},
			{ offsetof(NDS_header,endROMoffset), 4},
			{ offsetof(NDS_header,HeaderSize), 4},
			
			{ offsetof(NDS_header, ARM9module), 4},
			{ offsetof(NDS_header, ARM7module), 4},
			
			{ offsetof(NDS_header,logoCRC16), 2},
			{ offsetof(NDS_header,headerCRC16), 2},
		};
		
		for(size_t i = 0; i < ARRAY_SIZE(fieldSwaps); i++)
		{
			const u8 *fieldAddr = (u8 *)&header + fieldSwaps[i].offset;
			
			switch(fieldSwaps[i].bytes)
			{
				case 2:
					*(u16 *)fieldAddr = LE_TO_LOCAL_16(*(u16 *)fieldAddr);
					break;
					
				case 4:
					*(u32 *)fieldAddr = LE_TO_LOCAL_32(*(u32 *)fieldAddr);
					break;
			}
		}
#endif
		cardSize = (128 * 1024) << header.cardSize;

		if (cardSize < romsize)
		{
			//x->warn("The ROM header is invalid.\nThe device size has been increased to allow for the provided file size.\n");
			
			for (u32 i = header.cardSize; i < 0xF; i++)
			{
				if (((128 * 1024) << i) >= romsize) 
				{
					header.cardSize = i;
					cardSize = (128 * 1024) << i;
					break;
				}
			}
		}
		
		mask = (cardSize - 1);
		mask |= (mask >>1);
		mask |= (mask >>2);
		mask |= (mask >>4);
		mask |= (mask >>8);
		mask |= (mask >>16);


		if (type == ROM_NDS)
		{
			fseek(fROM, 0x4000 + headerOffset, SEEK_SET);
			fread(&secureArea[0], 1, 0x4000, fROM);
		}

		if (CommonSettings.loadToMemory)
		{
			fseek(fROM, headerOffset, SEEK_SET);
			
			romdata = new u8[romsize + 4];
			if (fread(romdata, 1, romsize, fROM) != romsize)
			{
				delete [] romdata; romdata = NULL;
				romsize = 0;

				return false;
			}

			if(hasRomBanner())
			{
				memcpy(&banner, romdata + header.IconOff, sizeof(RomBanner));
				
				banner.version = LE_TO_LOCAL_16(banner.version);
				banner.crc16 = LE_TO_LOCAL_16(banner.crc16);
				
				for(size_t i = 0; i < ARRAY_SIZE(banner.palette); i++)
				{
					banner.palette[i] = LE_TO_LOCAL_16(banner.palette[i]);
				}
			}

			_isDSiEnhanced = (LE_TO_LOCAL_32(*(u32*)(romdata + 0x180) == 0x8D898581U) && LE_TO_LOCAL_32(*(u32*)(romdata + 0x184) == 0x8C888480U));
			fclose(fROM); fROM = NULL;
			return true;
		}
		_isDSiEnhanced = ((readROM(0x180) == 0x8D898581U) && (readROM(0x184) == 0x8C888480U));
		if (hasRomBanner())
		{
			fseek(fROM, header.IconOff + headerOffset, SEEK_SET);
			fread(&banner, 1, sizeof(RomBanner), fROM);
			
			banner.version = LE_TO_LOCAL_16(banner.version);
			banner.crc16 = LE_TO_LOCAL_16(banner.crc16);
			
			for(size_t i = 0; i < ARRAY_SIZE(banner.palette); i++)
			{
				banner.palette[i] = LE_TO_LOCAL_16(banner.palette[i]);
			}
		}
		fseek(fROM, headerOffset, SEEK_SET);
		lastReadPos = 0;
		return true;
	}

	romsize = 0;
	fclose(fROM); fROM = NULL;
	return false;
}

void GameInfo::closeROM()
{
	if (fROM)
		fclose(fROM);

	if (romdata)
		delete [] romdata;

	fROM = NULL;
	romdata = NULL;
	romsize = 0;
	lastReadPos = 0xFFFFFFFF;
}

u32 GameInfo::readROM(u32 pos)
{
	u32 num;
	u32 data;
	if (!romdata)
	{
		if (lastReadPos != pos)
			fseek(fROM, pos + headerOffset, SEEK_SET);
		num = fread(&data, 1, 4, fROM);
		lastReadPos = (pos + num);
	}
	else
	{
		if(pos + 4 <= romsize)
		{
			//fast path
			data = LE_TO_LOCAL_32(*(u32*)(romdata + pos));
			num = 4;
		}
		else
		{
			data = 0;
			num = 0;
			for(int i=0;i<4;i++)
			{
				if(pos >= romsize)
					break;
				data |= (romdata[pos]<<(i*8));
				pos++;
				num++;
			}
		}
	}


	//in case we didn't read enough data, pad the remainder with 0xFF
	u32 pad = 0;
	while(num<4)
	{
		pad >>= 8;
		pad |= 0xFF000000;
		num++;
	}

	return LE_TO_LOCAL_32(data) & ~pad | pad;
}

bool GameInfo::isDSiEnhanced()
{
	return _isDSiEnhanced;
}

bool GameInfo::isHomebrew()
{
	return ((header.ARM9src < 0x4000) && (T1ReadLong(header.logo, 0) != 0x51AEFF24) && (T1ReadLong(header.logo, 4) != 0x699AA221));
}

static int rom_init_path(const char *filename, const char *physicalName, const char *logicalFilename)
{
	u32	type = ROM_NDS;

	path.init(logicalFilename? logicalFilename : filename);

	/*if ( path.isdsgba(path.path)) {
		type = ROM_DSGBA;
		gameInfo.loadROM(path.path, type);
	}
	else if ( !strcasecmp(path.extension().c_str(), "nds")) {
		type = ROM_NDS;

		gameInfo.loadROM(physicalName ? physicalName : path.path, type); //n.b. this does nothing if the file can't be found (i.e. if it was an extracted tempfile)...
		//...but since the data was extracted to gameInfo then it is ok
	}
	//ds.gba in archives, it's already been loaded into memory at this point
	else if (logicalFilename && path.isdsgba(std::string(logicalFilename))) {
		type = ROM_DSGBA;
	} else {*/
		//well, try to load it as an nds rom anyway
		type = ROM_NDS;
		gameInfo.loadROM(physicalName ? physicalName : path.path, type);
	//}

	
	printf("SU cca\n");

	//check that size is at least the size of the header
	if (gameInfo.romsize < 352) {
		return -1;
	}

	gameInfo.romType = type;

	return 1;
}

int NDS_LoadROM(const char *filename, const char *physicalName, const char *logicalFilename)
{
	int	ret;
	char	buf[MAX_PATH];

	if (filename == NULL)
		return -1;

	printf("qua1?\n");

	ret = rom_init_path(filename, physicalName, logicalFilename);
	if (ret < 1)
		return ret;

	printf("qua2?\n");

	//check whether this rom is any kind of valid
	if(!CheckValidRom((u8*)&gameInfo.header, gameInfo.secureArea))
	{
		//printf("Specified file is not a valid rom\n");
		return -1;
	}

	gameInfo.populate();

	gameInfo.crc = 0;

	gameInfo.chipID  = 0xC2;														// The Manufacturer ID is defined by JEDEC (C2h = Macronix)
	if (!gameInfo.isHomebrew())
	{
		gameInfo.chipID |= ((((128 << gameInfo.header.cardSize) / 1024) - 1) << 8);		// Chip size in megabytes minus 1
																						// (07h = 8MB, 0Fh = 16MB, 1Fh = 32MB, 3Fh = 64MB, 7Fh = 128MB)

		// flags
		// 0: Unknown
		// 1: Unknown
		// 2: Unknown
		// 3: Unknown
		// 4: Unknown
		// 5: DSi? (if set to 1 then DSi Enhanced games send command D6h to Slot1)
		// 6: Unknown
		// 7: ROM speed (Secure Area Block transfer mode (trasfer 8x200h or 1000h bytes)
		// TODO:
		//if (gameInfo.isDSiEnhanced())
		//		gameInfo.chipID |= (0x40 << 24);
		gameInfo.chipID |= (0x00 << 24);
	}

	printf("qua3?\n");


	INFO("\nROM game code: %c%c%c%c\n", gameInfo.header.gameCode[0], gameInfo.header.gameCode[1], gameInfo.header.gameCode[2], gameInfo.header.gameCode[3]);
	if (gameInfo.crc)
		INFO("ROM crc: %08X\n", gameInfo.crc);
	if (!gameInfo.isHomebrew())
	{
		INFO("ROM serial: %s\n", gameInfo.ROMserial);
		INFO("ROM chipID: %08X\n", gameInfo.chipID);
		INFO("ROM internal name: %s\n", gameInfo.ROMname);
		if (gameInfo.isDSiEnhanced()) INFO("ROM DSi Enhanced\n");
	}
	INFO("ROM developer: %s\n", ((gameInfo.header.makerCode == 0) && gameInfo.isHomebrew())?"Homebrew":getDeveloperNameByID(gameInfo.header.makerCode).c_str());

	buf[0] = gameInfo.header.gameCode[0];
	buf[1] = gameInfo.header.gameCode[1];
	buf[2] = gameInfo.header.gameCode[2];
	buf[3] = gameInfo.header.gameCode[3];
	buf[4] = 0;

	printf("qua4?\n");

	if (advsc.checkDB(buf, gameInfo.crc))
	{
		u8 sv = advsc.getSaveType();

		if (!(sv == 0xFF || sv == 0xFE))
		{
			if (CommonSettings.autodetectBackupMethod == 1)
				backup_setManualBackupType(sv + 1);
		}
	}
	NDS_Reset();

	return ret;
}

void NDS_FreeROM(void)
{
	gameInfo.closeROM();
}

void NDS_Sleep() { nds.hw_status.sleeping = TRUE; }

void NDS_TriggerCardEjectIRQ()
{
	NDS_makeIrq(ARMCPU_ARM7, IRQ_BIT_GC_IREQ_MC);
	NDS_makeIrq(ARMCPU_ARM9, IRQ_BIT_GC_IREQ_MC); //zero added on 11-aug-2013 with no proof of accuracy
}



void NDS_SkipNextFrame() {
		frameSkipper.RequestSkip();
}
void NDS_OmitFrameSkip(int force) {
	frameSkipper.OmitSkip(force > 0, force > 1);
}

#define INDEX(i) ((((i)>>16)&0xFF0)|(((i)>>4)&0xF))


enum ESI_DISPCNT
{
	ESI_DISPCNT_HStart, ESI_DISPCNT_HStartIRQ, ESI_DISPCNT_HDraw, ESI_DISPCNT_HBlank
};

u64 nds_timer;
u64 nds_arm9_timer, nds_arm7_timer;

static const u64 kNever = 0xFFFFFFFFFFFFFFFFULL;

struct TSequenceItem
{
	u64 timestamp;
	u32 param;
	bool enabled;

	virtual void save(EMUFILE* os)
	{
		write64le(timestamp,os);
		write32le(param,os);
		writebool(enabled,os);
	}

	virtual bool load(EMUFILE* is)
	{
		if(read64le(&timestamp,is) != 1) return false;
		if(read32le(&param,is) != 1) return false;
		if(readbool(&enabled,is) != 1) return false;
		return true;
	}

	FORCEINLINE bool isTriggered()
	{
		return enabled && nds_timer >= timestamp;
	}

	FORCEINLINE u64 next()
	{
		return timestamp;
	}
};

struct TSequenceItem_GXFIFO : public TSequenceItem
{
	FORCEINLINE bool isTriggered()
	{
		return enabled && nds_timer >= MMU.gfx3dCycles;
	}

	FORCEINLINE void exec()
	{
//		IF_DEVELOPER(DEBUG_statistics.sequencerExecutionCounters[4]++);
		while(isTriggered()) {
			enabled = false;
			gfx3d_execute3D();
		}
	}

	FORCEINLINE u64 next()
	{
		if(enabled) return MMU.gfx3dCycles;
		else return kNever;
	}
};

template<int procnum, int num> struct TSequenceItem_Timer : public TSequenceItem
{
	FORCEINLINE bool isTriggered()
	{
		return enabled && nds_timer >= nds.timerCycle[procnum][num];
	}

	FORCEINLINE void schedule()
	{
		enabled = MMU.timerON[procnum][num] && MMU.timerMODE[procnum][num] != 0xFFFF;
	}

	FORCEINLINE u64 next()
	{
		return nds.timerCycle[procnum][num];
	}

	FORCEINLINE void exec()
	{
//		IF_DEVELOPER(DEBUG_statistics.sequencerExecutionCounters[13+procnum*4+num]++);
		u8* regs = procnum==0?MMU.ARM9_REG:MMU.ARM7_REG;
		bool first = true;
		//we'll need to check chained timers..
		for(int i=num;i<4;i++)
		{
			bool over = false;
			//maybe too many checks if this is here, but we need it here for now
			if(!MMU.timerON[procnum][i]) return;

			if(MMU.timerMODE[procnum][i] == 0xFFFF)
			{
				++(MMU.timer[procnum][i]);
				over = !MMU.timer[procnum][i];
			}
			else
			{
				if(!first) break; //this timer isn't chained. break the chain
				first = false;

				over = true;
				int remain = 65536 - MMU.timerReload[procnum][i];
				int ctr=0;
				while(nds.timerCycle[procnum][i] <= nds_timer) {
					nds.timerCycle[procnum][i] += (remain << MMU.timerMODE[procnum][i]);
					ctr++;
				}
			}

			if(over)
			{
				MMU.timer[procnum][i] = MMU.timerReload[procnum][i];
				if(T1ReadWord(regs, 0x102 + i*4) & 0x40) 
				{
					NDS_makeIrq(procnum, IRQ_BIT_TIMER_0 + i);
				}
			}
			else
				break; //no more chained timers to trigger. we're done here
		}
	}
};

template<int procnum, int chan> struct TSequenceItem_DMA : public TSequenceItem
{
	DmaController* controller;

	FORCEINLINE bool isTriggered()
	{
		return (controller->dmaCheck && nds_timer>= controller->nextEvent);
	}

	FORCEINLINE bool isEnabled() { 
		return controller->dmaCheck?TRUE:FALSE;
	}

	FORCEINLINE u64 next()
	{
		return controller->nextEvent;
	}

	FORCEINLINE void exec()
	{
//		IF_DEVELOPER(DEBUG_statistics.sequencerExecutionCounters[5+procnum*4+chan]++);

		//if (nds.freezeBus) return;
	
		//printf("exec from TSequenceItem_DMA: %d %d\n",procnum,chan);
		/*if (controller->startmode == EDMAMode_GXFifo) {
			SDE_PROFILE(pf, controller->exec(), "DMA GFX");
			pf.outputStats("dma.txt");
		}
		else*/
			controller->exec();

//		//give gxfifo dmas a chance to re-trigger
//		if(MMU.DMAStartTime[procnum][chan] == EDMAMode_GXFifo) {
//			MMU.DMAing[procnum][chan] = FALSE;
//			if (gxFIFO.size <= 127) 
//			{
//				execHardware_doDma(procnum,chan,EDMAMode_GXFifo);
//				if (MMU.DMACompleted[procnum][chan])
//					goto docomplete;
//				else return;
//			}
//		}
//
//docomplete:
//		if (MMU.DMACompleted[procnum][chan])	
//		{
//			u8* regs = procnum==0?MMU.ARM9_REG:MMU.ARM7_REG;
//
//			//disable the channel
//			if(MMU.DMAStartTime[procnum][chan] != EDMAMode_GXFifo) {
//				T1WriteLong(regs, 0xB8 + (0xC*chan), T1ReadLong(regs, 0xB8 + (0xC*chan)) & 0x7FFFFFFF);
//				MMU.DMACrt[procnum][chan] &= 0x7FFFFFFF; //blehhh i hate this shit being mirrored in memory
//			}
//
//			if((MMU.DMACrt[procnum][chan])&(1<<30)) {
//				if(procnum==0) NDS_makeARM9Int(8+chan);
//				else NDS_makeARM7Int(8+chan);
//			}
//
//			MMU.DMAing[procnum][chan] = FALSE;
//		}

	}
};

struct TSequenceItem_divider : public TSequenceItem
{
	FORCEINLINE bool isTriggered()
	{
		return MMU.divRunning && nds_timer >= MMU.divCycles;
	}

	bool isEnabled() { return MMU.divRunning!=0; }

	FORCEINLINE u64 next()
	{
		return MMU.divCycles;
	}

	void exec()
	{
//		IF_DEVELOPER(DEBUG_statistics.sequencerExecutionCounters[2]++);
		MMU_new.div.busy = 0;
#ifdef HOST_64 
		T1WriteQuad(MMU.MMU_MEM[ARMCPU_ARM9][0x40], 0x2A0, MMU.divResult);
		T1WriteQuad(MMU.MMU_MEM[ARMCPU_ARM9][0x40], 0x2A8, MMU.divMod);
#else
		T1WriteLong(MMU.MMU_MEM[ARMCPU_ARM9][0x40], 0x2A0, (u32)MMU.divResult);
		T1WriteLong(MMU.MMU_MEM[ARMCPU_ARM9][0x40], 0x2A4, (u32)(MMU.divResult >> 32));
		T1WriteLong(MMU.MMU_MEM[ARMCPU_ARM9][0x40], 0x2A8, (u32)MMU.divMod);
		T1WriteLong(MMU.MMU_MEM[ARMCPU_ARM9][0x40], 0x2AC, (u32)(MMU.divMod >> 32));
#endif
		MMU.divRunning = FALSE;
	}

};

struct TSequenceItem_sqrtunit : public TSequenceItem
{
	FORCEINLINE bool isTriggered()
	{
		return MMU.sqrtRunning && nds_timer >= MMU.sqrtCycles;
	}

	bool isEnabled() { return MMU.sqrtRunning!=0; }

	FORCEINLINE u64 next()
	{
		return MMU.sqrtCycles;
	}

	FORCEINLINE void exec()
	{
//		IF_DEVELOPER(DEBUG_statistics.sequencerExecutionCounters[3]++);
		MMU_new.sqrt.busy = 0;
		T1WriteLong(MMU.MMU_MEM[ARMCPU_ARM9][0x40], 0x2B4, MMU.sqrtResult);
		MMU.sqrtRunning = FALSE;
	}

};

struct TSequenceItem_ReadSlot1 : public TSequenceItem
{
	FORCEINLINE bool isTriggered()
	{
		return enabled && nds_timer >= timestamp;
	}

	bool isEnabled() { return this->enabled; }

	FORCEINLINE u64 next()
	{
		return timestamp;
	}

	void exec()
	{
		u32 procnum = param;
		enabled = false;
		u32 val = T1ReadLong(MMU.MMU_MEM[procnum][0x40], 0x1A4);
		val |= 0x00800000;
		T1WriteLong(MMU.MMU_MEM[procnum][0x40], 0x1A4, val);
		triggerDma(EDMAMode_Card);
	}

};

struct Sequencer
{
	bool nds_vblankEnded;
	bool reschedule;
	TSequenceItem dispcnt;
	TSequenceItem wifi;
	TSequenceItem_divider divider;
	TSequenceItem_sqrtunit sqrtunit;
	TSequenceItem_GXFIFO gxfifo;
	//TSequenceItem_ReadSlot1 readslot1;
	TSequenceItem_DMA<0,0> dma_0_0; TSequenceItem_DMA<0,1> dma_0_1; 
	TSequenceItem_DMA<0,2> dma_0_2; TSequenceItem_DMA<0,3> dma_0_3; 
	TSequenceItem_DMA<1,0> dma_1_0; TSequenceItem_DMA<1,1> dma_1_1; 
	TSequenceItem_DMA<1,2> dma_1_2; TSequenceItem_DMA<1,3> dma_1_3; 
	TSequenceItem_Timer<0,0> timer_0_0; TSequenceItem_Timer<0,1> timer_0_1;
	TSequenceItem_Timer<0,2> timer_0_2; TSequenceItem_Timer<0,3> timer_0_3;
	TSequenceItem_Timer<1,0> timer_1_0; TSequenceItem_Timer<1,1> timer_1_1;
	TSequenceItem_Timer<1,2> timer_1_2; TSequenceItem_Timer<1,3> timer_1_3;

	void init();

	void execHardware();
	u64 findNext();

	void save(EMUFILE* os)
	{
		write64le(nds_timer,os);
		write64le(nds_arm9_timer,os);
		write64le(nds_arm7_timer,os);
		dispcnt.save(os);
		divider.save(os);
		sqrtunit.save(os);
		gxfifo.save(os);
		wifi.save(os);
#define SAVE(I,X,Y) I##_##X##_##Y .save(os);
		SAVE(timer,0,0); SAVE(timer,0,1); SAVE(timer,0,2); SAVE(timer,0,3); 
		SAVE(timer,1,0); SAVE(timer,1,1); SAVE(timer,1,2); SAVE(timer,1,3); 
		SAVE(dma,0,0); SAVE(dma,0,1); SAVE(dma,0,2); SAVE(dma,0,3); 
		SAVE(dma,1,0); SAVE(dma,1,1); SAVE(dma,1,2); SAVE(dma,1,3); 
#undef SAVE
	}

	bool load(EMUFILE* is, int version)
	{
		if(read64le(&nds_timer,is) != 1) return false;
		if(read64le(&nds_arm9_timer,is) != 1) return false;
		if(read64le(&nds_arm7_timer,is) != 1) return false;
		if(!dispcnt.load(is)) return false;
		if(!divider.load(is)) return false;
		if(!sqrtunit.load(is)) return false;
		if(!gxfifo.load(is)) return false;
		if(version >= 1) if(!wifi.load(is)) return false;
#define LOAD(I,X,Y) if(!I##_##X##_##Y .load(is)) return false;
		LOAD(timer,0,0); LOAD(timer,0,1); LOAD(timer,0,2); LOAD(timer,0,3); 
		LOAD(timer,1,0); LOAD(timer,1,1); LOAD(timer,1,2); LOAD(timer,1,3); 
		LOAD(dma,0,0); LOAD(dma,0,1); LOAD(dma,0,2); LOAD(dma,0,3); 
		LOAD(dma,1,0); LOAD(dma,1,1); LOAD(dma,1,2); LOAD(dma,1,3); 
#undef LOAD

		return true;
	}

} sequencer;

void NDS_RescheduleReadSlot1(int procnum, int size)
{
	/*u32 gcromctrl = T1ReadLong(MMU.MMU_MEM[procnum][0x40], 0x1A4);
	
	u32 clocks = (gcromctrl & (1<<27)) ? 8 : 5;
	u32 gap = gcromctrl & 0x1FFF;
	
	//time to send 8 command bytes, and then wait for the gap
	u32 delay = (8+gap)*clocks;

	//if data is to be returned, the first word is read before it's available and irqs and dmas fire
	if(size != 0) delay += 4;

	//timings are basically 33mhz but internal tracking is 66mhz
	delay *= 2;

	sequencer.readslot1.param = procnum;
	sequencer.readslot1.timestamp = nds_timer + delay;
	sequencer.readslot1.enabled = true;

	NDS_Reschedule();*/
}

void NDS_RescheduleGXFIFO(u32 cost)
{
	if(!sequencer.gxfifo.enabled) {
		MMU.gfx3dCycles = nds_timer;
		sequencer.gxfifo.enabled = true;
	}
	MMU.gfx3dCycles += cost;
	NDS_Reschedule();
}

void NDS_RescheduleTimers()
{
#define check(X,Y) sequencer.timer_##X##_##Y .schedule();
	check(0,0); check(0,1); check(0,2); check(0,3);
	check(1,0); check(1,1); check(1,2); check(1,3);
#undef check

	NDS_Reschedule();
}

void NDS_RescheduleDMA()
{
	//TBD
	NDS_Reschedule();

}


static void initSchedule()
{
	sequencer.init();

	//begin at the very end of the last scanline
	//so that at t=0 we can increment to scanline=0
	nds.hw_status.VCount = 262; 

	sequencer.nds_vblankEnded = false;
}


// 2196372 ~= (ARM7_CLOCK << 16) / 1000000
// This value makes more sense to me, because:
// ARM7_CLOCK   = 33.51 mhz
//				= 33513982 cycles per second
// 				= 33.513982 cycles per microsecond
const u64 kWifiCycles = 67;//34*2;
//(this isn't very precise. I don't think it needs to be)

void Sequencer::init()
{
	NDS_RescheduleTimers();
	NDS_RescheduleDMA();

	reschedule = false;
	nds_timer = 0;
	nds_arm9_timer = 0;
	nds_arm7_timer = 0;

	dispcnt.enabled = true;
	dispcnt.param = ESI_DISPCNT_HStart;
	dispcnt.timestamp = 0;

	gxfifo.enabled = false;

	dma_0_0.controller = &MMU_new.dma[0][0];
	dma_0_1.controller = &MMU_new.dma[0][1];
	dma_0_2.controller = &MMU_new.dma[0][2];
	dma_0_3.controller = &MMU_new.dma[0][3];
	dma_1_0.controller = &MMU_new.dma[1][0];
	dma_1_1.controller = &MMU_new.dma[1][1];
	dma_1_2.controller = &MMU_new.dma[1][2];
	dma_1_3.controller = &MMU_new.dma[1][3];


	#ifdef EXPERIMENTAL_WIFI_COMM
	wifi.enabled = true;
	wifi.timestamp = kWifiCycles;
	#else
	wifi.enabled = false;
	#endif
}

bool ARM7_SKIP_HACK = false;
bool ARM9_SKIP_HACK = false;
bool math_underclock = true;

static void execHardware_hblank()
{
	//this logic keeps moving around.
	//now, we try and give the game as much time as possible to finish doing its work for the scanline,
	//by drawing scanline N at the end of drawing time (but before subsequent interrupt or hdma-driven events happen)
	//don't try to do this at the end of the scanline, because some games (sonic classics) may use hblank IRQ to set
	//scroll regs for the next scanline

	if (nds.hw_status.VCount<192) {		
		triggerDma(EDMAMode_HBlank);
	}

	//turn on hblank status bit
	T1WriteWord(MMU.ARM9_REG, 4, T1ReadWord(MMU.ARM9_REG, 4) | 2);
	T1WriteWord(MMU.ARM7_REG, 4, T1ReadWord(MMU.ARM7_REG, 4) | 2);

	//fire hblank interrupts if necessary
	if(T1ReadWord(MMU.ARM9_REG, 4) & 0x10) NDS_makeIrq(ARMCPU_ARM9,IRQ_BIT_LCD_HBLANK);
	if(T1ReadWord(MMU.ARM7_REG, 4) & 0x10) NDS_makeIrq(ARMCPU_ARM7,IRQ_BIT_LCD_HBLANK);
}

static void execHardware_hstart_vblankEnd()
{
	sequencer.nds_vblankEnded = true;
	sequencer.reschedule = true;

	//turn off vblank status bit
	T1WriteWord(MMU.ARM9_REG, 4, T1ReadWord(MMU.ARM9_REG, 4) & ~1);
	T1WriteWord(MMU.ARM7_REG, 4, T1ReadWord(MMU.ARM7_REG, 4) & ~1);

	//some emulation housekeeping
	frameSkipper.Advance();
}


static void execHardware_hstart_vblankStart()
{
	//printf("--------VBLANK!!!--------\n");
	//fire vblank interrupts if necessary
	for(int i=0;i<2;i++)
		if(MMU.reg_IF_pending[i] & (1<<IRQ_BIT_LCD_VBLANK))
		{
			MMU.reg_IF_pending[i] &= ~(1<<IRQ_BIT_LCD_VBLANK);
			NDS_makeIrq(i,IRQ_BIT_LCD_VBLANK);
		}

	//trigger vblank dmas
	triggerDma(EDMAMode_VBlank);


	//tracking for arm9 load average
	nds.runCycleCollector[0][nds.idleFrameCounter] = 1120380-nds.idleCycles[0];
	nds.runCycleCollector[1][nds.idleFrameCounter] = 1120380-nds.idleCycles[1];
	nds.idleFrameCounter++;
	nds.idleFrameCounter &= 15;
	nds.idleCycles[0] = 0;
	nds.idleCycles[1] = 0;
}

static u16 execHardware_gen_vmatch_goal()
{
	u16 vmatch = T1ReadWord(PSP_UC(MMU.ARM9_REG), 4);
	vmatch = ((vmatch>>8)|((vmatch<<1)&(1<<8)));
	return vmatch;
}

static void execHardware_hstart_vcount_irq()
{
	//trigger pending VMATCH irqs
	if(MMU.reg_IF_pending[ARMCPU_ARM9] & (1<<IRQ_BIT_LCD_VMATCH))
	{
		MMU.reg_IF_pending[ARMCPU_ARM9] &= ~(1<<IRQ_BIT_LCD_VMATCH);
		NDS_makeIrq(ARMCPU_ARM9,IRQ_BIT_LCD_VMATCH);
	}
	if(MMU.reg_IF_pending[ARMCPU_ARM7] & (1<<IRQ_BIT_LCD_VMATCH))
	{
		MMU.reg_IF_pending[ARMCPU_ARM7] &= ~(1<<IRQ_BIT_LCD_VMATCH);
		NDS_makeIrq(ARMCPU_ARM7,IRQ_BIT_LCD_VMATCH);
	}
}

static void execHardware_hstart_vcount()
{
	u16 vmatch = execHardware_gen_vmatch_goal();
	if(nds.hw_status.VCount==vmatch)
	{
		//arm9 vmatch
		T1WriteWord(MMU.ARM9_REG, 4, T1ReadWord(MMU.ARM9_REG, 4) | 4);
		if(T1ReadWord(MMU.ARM9_REG, 4) & 32) {
			MMU.reg_IF_pending[ARMCPU_ARM9] |= (1<<IRQ_BIT_LCD_VMATCH);
		}
	}
	else
		T1WriteWord(MMU.ARM9_REG, 4, T1ReadWord(MMU.ARM9_REG, 4) & 0xFFFB);

	vmatch = T1ReadWord(MMU.ARM7_REG, 4);
	vmatch = ((vmatch>>8)|((vmatch<<1)&(1<<8)));
	if(nds.hw_status.VCount==vmatch)
	{
		//arm7 vmatch
		T1WriteWord(MMU.ARM7_REG, 4, T1ReadWord(MMU.ARM7_REG, 4) | 4);
		if(T1ReadWord(MMU.ARM7_REG, 4) & 32)
			MMU.reg_IF_pending[ARMCPU_ARM7] |= (1<<IRQ_BIT_LCD_VMATCH);
	}
	else
		T1WriteWord(MMU.ARM7_REG, 4, T1ReadWord(MMU.ARM7_REG, 4) & 0xFFFB);
}

static void execHardware_hdraw()
{
	//due to hacks in our selection of rendering time, we do not actually render here as intended.
	//consider changing this if there is some problem with raster fx timing but check the documentation near the gpu rendering calls
	//to make sure you check for regressions (nsmb, sonic classics, et al)
}

static void execHardware_hstart_irq()
{
	//this function very soon after the registers get updated to trigger IRQs
	//this is necessary to fix "egokoro kyoushitsu" which idles waiting for vcount=192, which never happens due to a long vblank irq
	//100% accurate emulation would require the read of VCOUNT to be in the pipeline already with the irq coming in behind it, thus 
	//allowing the vcount to register as 192 occasionally (maybe about 1 out of 28 frames)
	//the actual length of the delay is in execHardware() where the events are scheduled
	sequencer.reschedule = true;
	if(nds.hw_status.VCount==192)
	{
		//when the vcount hits 192, vblank begins
		execHardware_hstart_vblankStart();
	}

	execHardware_hstart_vcount_irq();
}

static void execHardware_hstart()
{
	nds.hw_status.VCount++;

	if(nds.hw_status.VCount==263)
	{
		//when the vcount hits 263 it rolls over to 0
		nds.hw_status.VCount=0;
	}
	else if(nds.hw_status.VCount==262)
	{
		gfx3d_VBlankEndSignal(frameSkipper.ShouldSkip3D());
		
		if (!NDS_ARM9.freeze && nds.hw_status.overclock < 2)
		{
			//suspend arm7 during overclocking so much doesn't run out of control
			//actually, this isn't needed yet.
			NDS_ARM7.freeze |= CPU_FREEZE_OVERCLOCK_HACK;

			nds.hw_status.overclock++;
			nds.hw_status.VCount = 261;
		}
		else
		{
			//overclock arm7 lock is always released here; if it wasn't actiev, this benign
			NDS_ARM7.freeze &= ~CPU_FREEZE_OVERCLOCK_HACK;

			//when the vcount hits 262, vblank ends (oam pre-renders by one scanline)
			execHardware_hstart_vblankEnd();
		}
	}
	else if (nds.hw_status.VCount == 261)
	{
		nds.hw_status.overclock = 0;
	}
	else if(nds.hw_status.VCount==192)
	{
		//turn on vblank status bit
		
		T1WriteWord(MMU.ARM9_REG, 4, T1ReadWord(MMU.ARM9_REG, 4) | 1);
		T1WriteWord(MMU.ARM7_REG, 4, T1ReadWord(MMU.ARM7_REG, 4) | 1);
	
		//check whether we'll need to fire vblank irqs

		if (T1ReadWord(MMU.ARM9_REG, 4) & 0x8) MMU.reg_IF_pending[ARMCPU_ARM9] |= (1 << IRQ_BIT_LCD_VBLANK);
		if (T1ReadWord(MMU.ARM7_REG, 4) & 0x8) MMU.reg_IF_pending[ARMCPU_ARM7] |= (1 << IRQ_BIT_LCD_VBLANK);
		

		//this is important for the character select in Dragon Ball Kai - Ultimate Butouden
		//it seems if you allow the 3d to begin before the vblank, then it will get interrupted and not complete.
		//the game needs to pick up the gxstat reg busy as clear after it finishes processing vblank.
		//therefore, this can't happen until sometime after vblank.
		//devil survivor 2 will have screens get stuck if this is on any other scanline.
		//obviously 192 is the right choice.

		gfx3d_VBlankSignal();
		//this isnt important for any known game, but it would be nice to prove it. 
		NDS_RescheduleGXFIFO(392 * 2);

		//Render 3D here
		if (!frameSkipper.ShouldSkip3D() && my_config.Render3D) {
			gfx3d_VBlankEndSignal(false);
		}

	}
	
	//write the new vcount
	T1WriteWord(MMU.ARM9_REG, 6, nds.hw_status.VCount);
	T1WriteWord(MMU.ARM9_REG, 0x1006, nds.hw_status.VCount);
	T1WriteWord(MMU.ARM7_REG, 6, nds.hw_status.VCount);
	T1WriteWord(MMU.ARM7_REG, 0x1006, nds.hw_status.VCount);

	//turn off hblank status bit
	T1WriteWord(MMU.ARM9_REG, 4, T1ReadWord(MMU.ARM9_REG, 4) & 0xFFFD);
	T1WriteWord(MMU.ARM7_REG, 4, T1ReadWord(MMU.ARM7_REG, 4) & 0xFFFD);

	//handle vcount status
	execHardware_hstart_vcount();	
	
	//trigger hstart dmas
	
	triggerDma(EDMAMode_HStart);

	if(nds.hw_status.VCount<192)
	{
		//this is hacky.
		//there is a corresponding hack in doDMA.
		//it should be driven by a fifo (and generate just in time as the scanline is displayed)
		//but that isnt even possible until we have some sort of sub-scanline timing.
		//it may not be necessary.
		triggerDma(EDMAMode_MemDisplay);
	}
}

void NDS_Reschedule()
{
	sequencer.reschedule = true;
}

FORCEINLINE u32 _fast_min32(u32 a, u32 b, u32 c, u32 d)
{
	return ((( ((s32)(a-b)) >> (32-1)) & (c^d)) ^ d);
}

FORCEINLINE u64 _fast_min(u64 a, u64 b)
{
	return a<b?a:b;
}



u64 Sequencer::findNext()
{
	//this one is always enabled so dont bother to check it
	u64 next = dispcnt.next();
	
	if (!math_underclock){
		if(divider.isEnabled()) next = _fast_min(next,divider.next());
		if(sqrtunit.isEnabled()) next = _fast_min(next,sqrtunit.next());
	}

	if(gxfifo.enabled) next = _fast_min(next,gxfifo.next());

	//if(readslot1.isEnabled()) next = _fast_min(next,readslot1.next());

#ifdef EXPERIMENTAL_WIFI_COMM
	next = _fast_min(next,wifi.next());
#endif

#define test(X,Y) if(dma_##X##_##Y .isEnabled()) next = _fast_min(next,dma_##X##_##Y .next());
	if (!ARM9_SKIP_HACK) {	
		test(0,0); test(0,1); test(0,2); test(0,3);
	}
	if (!ARM7_SKIP_HACK) {
		test(1, 0); test(1, 1); test(1, 2); test(1, 3);
	}
#undef test
#define test(X,Y) if(timer_##X##_##Y .enabled) next = _fast_min(next,timer_##X##_##Y .next());
	if (!ARM9_SKIP_HACK) {
		test(0,0); test(0,1); test(0,2); test(0,3);
	}
	if (!ARM7_SKIP_HACK) {
		test(1, 0); test(1, 1); test(1, 2); test(1, 3);
	}
#undef test

	return next;
}

void Sequencer::execHardware()
{
	if(dispcnt.isTriggered())
	{

//		IF_DEVELOPER(DEBUG_statistics.sequencerExecutionCounters[1]++);

		switch(dispcnt.param)
		{
		case ESI_DISPCNT_HStart:
			execHardware_hstart();
			//(used to be 3168)
			//hstart is actually 8 dots before the visible drawing begins
			//we're going to run 1 here and then run 7 in the next case
			dispcnt.timestamp += 12;
			dispcnt.param = ESI_DISPCNT_HStartIRQ;
			break;
		case ESI_DISPCNT_HStartIRQ:
			execHardware_hstart_irq();
			dispcnt.timestamp += 84;
			dispcnt.param = ESI_DISPCNT_HDraw;
			break;
			
		case ESI_DISPCNT_HDraw:
			execHardware_hdraw();
			//duration of non-blanking period is ~1606 clocks (gbatek agrees) [but says its different on arm7]
			//im gonna call this 267 dots = 267*6=1602
			//so, this event lasts 267 dots minus the 8 dot preroll
			dispcnt.timestamp += 3108;
			dispcnt.param = ESI_DISPCNT_HBlank;
			break;

		case ESI_DISPCNT_HBlank:
			execHardware_hblank();
			//(once this was 1092 or 1092/12=91 dots.)
			//there are surely 355 dots per scanline, less 267 for non-blanking period. the rest is hblank and then after that is hstart
			dispcnt.timestamp += 1056;
			dispcnt.param = ESI_DISPCNT_HStart;
			break;
		}
	}
	//if (readslot1.isTriggered()) readslot1.exec();

	if (gxfifo.isTriggered()) gxfifo.exec();

	if (!math_underclock){
		if (sqrtunit.isTriggered()) sqrtunit.exec();
		if (divider.isTriggered()) divider.exec();
	}	
		
#define test(X,Y) if(dma_##X##_##Y .isTriggered()) dma_##X##_##Y .exec();
	if (!ARM9_SKIP_HACK) {
		test(0,0); test(0,1); test(0,2); test(0,3);
	}
	if (!ARM7_SKIP_HACK) {
		test(1, 0) else test(1, 1) else test(1, 2) else test(1, 3);
	}
#undef test
#define test(X,Y) if(timer_##X##_##Y .enabled) if(timer_##X##_##Y .isTriggered()) timer_##X##_##Y .exec();
	if (!ARM9_SKIP_HACK) {
		test(0,0); test(0,1); test(0,2); test(0,3);
	}
	if (!ARM7_SKIP_HACK) {
		test(1, 0); test(1, 1); test(1, 2); test(1, 3);
	}
#undef test

}

void execHardware_interrupts();

static void saveUserInput(EMUFILE* os);
static bool loadUserInput(EMUFILE* is, int version);

void nds_savestate(EMUFILE* os)
{
	//version
	write32le(3,os);

	sequencer.save(os);

	saveUserInput(os);

	write32le(LidClosed,os);
	write8le(countLid,os);
}

bool nds_loadstate(EMUFILE* is, int size)
{
	// this isn't part of the savestate loading logic, but
	// don't skip the next frame after loading a savestate
	frameSkipper.OmitSkip(true, true);

	//read version
	u32 version;
	if(read32le(&version,is) != 1) return false;

	if(version > 3) return false;

	bool temp = true;
	temp &= sequencer.load(is, version);
	if(version <= 1 || !temp) return temp;
	temp &= loadUserInput(is, version);

	if(version < 3) return temp;

	read32le((s32*)(&LidClosed),is);
	read8le(&countLid,is);

	return temp;
}

//these have not been tuned very well yet.
static const int kMaxWork = 4000;
static const int kIrqWait = 4000;

template<bool doarm9, bool doarm7>
static FORCEINLINE s32 minarmtime(s32 arm9, s32 arm7)
{
	if(doarm9)
		if(doarm7)
			return min(arm9,arm7);
		else
			return arm9;
	else
		return arm7;
}

template<bool doarm9, bool doarm7, bool jit>
static /*donotinline*/ std::pair<s32,s32> armInnerLoop(
	const u64 nds_timer_base, const s32 s32next, s32 arm9, s32 arm7)
{

	s32 timer = minarmtime<doarm9,doarm7>(arm9,arm7);

	while(timer < s32next && !sequencer.reschedule)
	{

		if(doarm9 && (!doarm7 || arm9 <= timer))
		{
			if(!(NDS_ARM9.freeze & CPU_FREEZE_WAIT_IRQ) && !nds.freezeBus && !ARM9_SKIP_HACK)
			{
				arm9 += armcpu_exec<ARMCPU_ARM9,jit>();

				timer = minarmtime<doarm9,doarm7>(arm9,arm7);
				nds_timer = nds_timer_base + timer;
				continue;
			}

			s32 temp = arm9;
			arm9 = min(s32next, arm9 + kIrqWait);
			nds.idleCycles[0] += arm9-temp;
			if (gxFIFO.size < 255) nds.freezeBus &= ~1;
		}
		if(doarm7 && (!doarm9 || arm7 <= timer))
		{
			bool cpufreeze = !!(NDS_ARM7.freeze & (CPU_FREEZE_WAIT_IRQ|CPU_FREEZE_OVERCLOCK_HACK));

			if(!cpufreeze && !nds.freezeBus && !ARM7_SKIP_HACK)
			{
				arm7 += (armcpu_exec<ARMCPU_ARM7,jit>()<<1);

				timer = minarmtime<doarm9,doarm7>(arm9,arm7);
				nds_timer = nds_timer_base + timer;
				continue;
			}
			
			s32 temp = arm7;
			arm7 = min(s32next, arm7 + kIrqWait);
			nds.idleCycles[1] += arm7-temp;
			if(arm7 == s32next)
			{
				nds_timer = nds_timer_base + minarmtime<doarm9,false>(arm9,arm7);
				return armInnerLoop<doarm9,false,jit>(nds_timer_base, s32next, arm9, arm7);
			}
		}

		timer = minarmtime<doarm9,doarm7>(arm9,arm7);

		nds_timer = nds_timer_base + timer;
	}

	return std::make_pair(arm9, arm7);
}

template<bool FORCE>
void NDS_exec(s32 nb)
{
	sequencer.nds_vblankEnded = false;
	ARM9_SKIP_HACK = false;

	if(nds.hw_status.sleeping)
	{
		//speculative code: if ANY irq happens, wake up the arm7.
		//I think the arm7 program analyzes the system and may decide not to wake up
		//if it is dissatisfied with the conditions
		if((MMU.reg_IE[1] & MMU.gen_IF<1>()))
		{
			nds.hw_status.sleeping = FALSE;
		}

		return;
	}

	for(;;)
	{

		sequencer.execHardware();

		//break out once per frame
		if(unlikely(sequencer.nds_vblankEnded)) break;
		//it should be benign to execute execHardware in the next frame,
		//since there won't be anything for it to do (everything should be scheduled in the future)

		execHardware_interrupts();

		//find next work unit:
		u64 next = sequencer.findNext();
		next = min(next,nds_timer+kMaxWork); //lets set an upper limit for now


		sequencer.reschedule = false;

		//cast these down to 32bits so that things run faster on 32bit procs
		u64 nds_timer_base = nds_timer;
		s32 arm9 = (s32)(nds_arm9_timer-nds_timer);
		s32 arm7 = (s32)(nds_arm7_timer-nds_timer);
		s32 s32next = (s32)(next-nds_timer);

		std::pair<s32,s32> arm9arm7;
		
		arm9arm7 = armInnerLoop<true,true,true>(nds_timer_base,s32next,arm9,arm7);

		ARM7_SKIP_HACK = nds.hw_status.VCount < 185 || (my_config.extrmarm7down && nds.hw_status.VCount > 230);

		ARM9_SKIP_HACK = nds.hw_status.VCount > 250;

		math_underclock = nds.hw_status.VCount > 250;

		arm9 = arm9arm7.first;
		arm7 = arm9arm7.second;
		nds_arm9_timer = nds_timer_base+arm9;
		nds_arm7_timer = nds_timer_base+arm7;

		//if we were waiting for an irq, don't wait too long:
		//let's re-analyze it after this hardware event (this rolls back a big burst of irq waiting which may have been interrupted by a resynch)
		if(NDS_ARM9.freeze & CPU_FREEZE_WAIT_IRQ)
		{
			nds.idleCycles[0] -= (s32)(nds_arm9_timer-nds_timer);
			nds_arm9_timer = nds_timer;
		}

		if (NDS_ARM7.freeze & CPU_FREEZE_WAIT_IRQ)
		{
			nds.idleCycles[1] -= (s32)nds_arm7_timer-nds_timer;
			nds_arm7_timer = nds_timer;
		}
	}

	if (PSP_UC(RenderDone)){
		PSP_UC(Do2dRender) = true;
		EMU_SCREEN();
		return;
	}

	if (IsEmu()){
		renderScreenFull();
		EMU_SCREEN();
	}
}

template<int PROCNUM> static void execHardware_interrupts_core()
{
	u32 IF = MMU.gen_IF<PROCNUM>();
	u32 IE = MMU.reg_IE[PROCNUM];
	u32 masked = IF & IE;
	if((ARMPROC.freeze & CPU_FREEZE_IRQ_IE_IF) && masked)
	{
		ARMPROC.freeze &= ~CPU_FREEZE_IRQ_IE_IF;
	}

	if(masked && MMU.reg_IME[PROCNUM] && !ARMPROC.CPSR.bits.I)
		armcpu_irqException(&ARMPROC);
}
//027FFC3C

void execHardware_interrupts()
{
	if (!ARM9_SKIP_HACK) execHardware_interrupts_core<ARMCPU_ARM9>();
	if (!ARM7_SKIP_HACK) execHardware_interrupts_core<ARMCPU_ARM7>();
}

static void resetUserInput();

static void PrepareBiosARM7()
{
	//begin with the bios unloaded
	NDS_ARM7.BIOS_loaded = false;
	memset(MMU.ARM7_BIOS, 0, sizeof(MMU.ARM7_BIOS));

	if(CommonSettings.UseExtBIOS == true)
	{
		//read arm7 bios from inputfile and flag it if it succeeds
		FILE *arm7inf = fopen(CommonSettings.ARM7BIOS,"rb");
		if (arm7inf) 
		{
			if (fread(MMU.ARM7_BIOS,1,16384,arm7inf) == 16384)
				NDS_ARM7.BIOS_loaded = true;
			fclose(arm7inf);
		}
	}

	//choose to use SWI emulation or routines from bios
	if((CommonSettings.SWIFromBIOS) && (NDS_ARM7.BIOS_loaded))
	{
		NDS_ARM7.swi_tab = 0;

		//if we used routines from bios, apply patches
		if (CommonSettings.PatchSWI3)
		{
			//[3801] SUB R0, #1 -> [4770] BX LR
			T1WriteWord(MMU.ARM7_BIOS,0x2F08, 0x4770);
		}
	} 
	else 
		NDS_ARM7.swi_tab = ARM_swi_tab[ARMCPU_ARM7];

	if(NDS_ARM7.BIOS_loaded)
	{
		INFO("ARM7 BIOS load: %s.\n", NDS_ARM7.BIOS_loaded?"OK":"FAILED");
	}
	else
	{
		//fake bios content, critical to normal operations, since we dont have a real bios.

		T1WriteLong(MMU.ARM7_BIOS, 0x0000, 0xEAFFFFFE); //B 00000000 (reset: infinite loop) (originally: 0xE25EF002 - SUBS PC, LR, #2  
		T1WriteLong(MMU.ARM7_BIOS, 0x0004, 0xEAFFFFFE); //B 00000004 (undefined instruction: infinite loop)
		T1WriteLong(MMU.ARM7_BIOS, 0x0008, 0xEAFFFFFE); //B 00000280 (SWI: infinite loop [since we will be HLEing the SWI routines])
		T1WriteLong(MMU.ARM7_BIOS, 0x000C, 0xEAFFFFFE); //B 0000000C (prefetch abort: infinite loop)
		T1WriteLong(MMU.ARM7_BIOS, 0x0010, 0xEAFFFFFE); //B 00000010 (data abort: infinite loop)
		T1WriteLong(MMU.ARM7_BIOS, 0x0018, 0xEA000000); //B 00000020 (IRQ: branch to handler)
		T1WriteLong(MMU.ARM7_BIOS, 0x001C, 0xEAFFFFFE); //B 0000001C (FIQ vector: infinite loop)
		//IRQ handler
		T1WriteLong(MMU.ARM7_BIOS, 0x0020, 0xE92D500F); //STMDB SP!, {R0-R3,R12,LR}
		T1WriteLong(MMU.ARM7_BIOS, 0x0024, 0xE3A00301); //MOV R0, #4000000
		T1WriteLong(MMU.ARM7_BIOS, 0x0028, 0xE28FE000); //ADD LR, PC, #0
		T1WriteLong(MMU.ARM7_BIOS, 0x002C, 0xE510F004); //LDR PC, [R0, -#4]
		T1WriteLong(MMU.ARM7_BIOS, 0x0030, 0xE8BD500F); //LDMIA SP!, {R0-R3,R12,LR}
		T1WriteLong(MMU.ARM7_BIOS, 0x0034, 0xE25EF004); //SUBS PC, LR, #4
	}
}

static void PrepareBiosARM9()
{
	//begin with the bios unloaded
	memset(MMU.ARM9_BIOS, 0, sizeof(MMU.ARM9_BIOS));
	NDS_ARM9.BIOS_loaded = false;

	if(CommonSettings.UseExtBIOS == true)
	{
		//read arm9 bios from inputfile and flag it if it succeeds
		FILE* arm9inf = fopen(CommonSettings.ARM9BIOS,"rb");
		if (arm9inf) 
		{
			if (fread(MMU.ARM9_BIOS,1,4096,arm9inf) == 4096) 
				NDS_ARM9.BIOS_loaded = true;
			fclose(arm9inf);
		}
	}

	//choose to use SWI emulation or routines from bios
	if((CommonSettings.SWIFromBIOS) && (NDS_ARM9.BIOS_loaded))
	{
		NDS_ARM9.swi_tab = 0;
		
		//if we used routines from bios, apply patches
		//[3801] SUB R0, #1 -> [4770] BX LR
		if (CommonSettings.PatchSWI3)
			T1WriteWord(MMU.ARM9_BIOS, 0x07CC, 0x4770);
	}
	else NDS_ARM9.swi_tab = ARM_swi_tab[ARMCPU_ARM9];

	if(NDS_ARM9.BIOS_loaded) 
	{
		INFO("ARM9 BIOS load: %s.\n", NDS_ARM9.BIOS_loaded?"OK":"FAILED");
	} 
	else 
	{
		//fake bios content, critical to normal operations, since we dont have a real bios.
		//it'd be cool if we could write this in some kind of assembly language, inline or otherwise, without some bulky dependencies
		//perhaps we could build it with devkitarm? but thats bulky (offline) dependencies, to be sure..

		//reminder: bios chains data abort to fast irq

		//exception vectors:
		T1WriteLong(MMU.ARM9_BIOS, 0x0000, 0xEAFFFFFE);		// (infinite loop for) Reset !!!
		//T1WriteLong(MMU.ARM9_BIOS, 0x0004, 0xEAFFFFFE);		// (infinite loop for) Undefined instruction
		T1WriteLong(MMU.ARM9_BIOS, 0x0004, 0xEA000004);		// Undefined instruction -> Fast IRQ (just guessing)
		T1WriteLong(MMU.ARM9_BIOS, 0x0008, 0xEA00009C);		// SWI -> ?????
		T1WriteLong(MMU.ARM9_BIOS, 0x000C, 0xEAFFFFFE);		// (infinite loop for) Prefetch Abort
		T1WriteLong(MMU.ARM9_BIOS, 0x0010, 0xEA000001);		// Data Abort -> Fast IRQ
		T1WriteLong(MMU.ARM9_BIOS, 0x0014, 0x00000000);		// Reserved
		T1WriteLong(MMU.ARM9_BIOS, 0x0018, 0xEA000095);		// Normal IRQ -> 0x0274
		T1WriteLong(MMU.ARM9_BIOS, 0x001C, 0xEA00009D);		// Fast IRQ -> 0x0298

		//copy the logo content into the bios - Pokemon Platinum uses this in Pal Park trade
		//it compares the logo from the arm9 bios to the logo in the GBA header.
		//NOTE: in the unlikely event that the valid header is missing from the gameInfo, we'd be doing wrong here.
		//      however, its nice not to have the logo embedded here. 
		//      TODO - take a CRC of the logo, check vs logoCRC16, and a hardcoded value, to make sure all is in order--report error if not
		memcpy(&MMU.ARM9_BIOS[0x20], &gameInfo.header.logo[0], 0x9C);
		T1WriteWord(MMU.ARM9_BIOS, 0x20 + 0x9C,  gameInfo.header.logoCRC16);
		//... and with that we are at 0xBC:

		//(now what goes in this gap?? nothing we need, i guess)

		//IRQ handler: get dtcm address and jump to a vector in it
		T1WriteLong(MMU.ARM9_BIOS, 0x0274, 0xE92D500F); //STMDB SP!, {R0-R3,R12,LR} 
		T1WriteLong(MMU.ARM9_BIOS, 0x0278, 0xEE190F11); //MRC CP15, 0, R0, CR9, CR1, 0
		T1WriteLong(MMU.ARM9_BIOS, 0x027C, 0xE1A00620); //MOV R0, R0, LSR #C
		T1WriteLong(MMU.ARM9_BIOS, 0x0280, 0xE1A00600); //MOV R0, R0, LSL #C 
		T1WriteLong(MMU.ARM9_BIOS, 0x0284, 0xE2800C40); //ADD R0, R0, #4000
		T1WriteLong(MMU.ARM9_BIOS, 0x0288, 0xE28FE000); //ADD LR, PC, #0   
		T1WriteLong(MMU.ARM9_BIOS, 0x028C, 0xE510F004); //LDR PC, [R0, -#4] 

		//????
		T1WriteLong(MMU.ARM9_BIOS, 0x0290, 0xE8BD500F); //LDMIA SP!, {R0-R3,R12,LR}
		T1WriteLong(MMU.ARM9_BIOS, 0x0294, 0xE25EF004); //SUBS PC, LR, #4

		//-------
		//FIQ and abort exception handler
		//TODO - this code is copied from the bios. refactor it
		//friendly reminder: to calculate an immediate offset: encoded = (desired_address-cur_address-8)

		T1WriteLong(MMU.ARM9_BIOS, 0x0298, 0xE10FD000); //MRS SP, CPSR  
		T1WriteLong(MMU.ARM9_BIOS, 0x029C, 0xE38DD0C0); //ORR SP, SP, #C0
		
		T1WriteLong(MMU.ARM9_BIOS, 0x02A0, 0xE12FF00D); //MSR CPSR_fsxc, SP
		T1WriteLong(MMU.ARM9_BIOS, 0x02A4, 0xE59FD000 | (0x2D4-0x2A4-8)); //LDR SP, [FFFF02D4]
		T1WriteLong(MMU.ARM9_BIOS, 0x02A8, 0xE28DD001); //ADD SP, SP, #1   
		T1WriteLong(MMU.ARM9_BIOS, 0x02AC, 0xE92D5000); //STMDB SP!, {R12,LR}
		
		T1WriteLong(MMU.ARM9_BIOS, 0x02B0, 0xE14FE000); //MRS LR, SPSR
		T1WriteLong(MMU.ARM9_BIOS, 0x02B4, 0xEE11CF10); //MRC CP15, 0, R12, CR1, CR0, 0
		T1WriteLong(MMU.ARM9_BIOS, 0x02B8, 0xE92D5000); //STMDB SP!, {R12,LR}
		T1WriteLong(MMU.ARM9_BIOS, 0x02BC, 0xE3CCC001); //BIC R12, R12, #1

		T1WriteLong(MMU.ARM9_BIOS, 0x02C0, 0xEE01CF10); //MCR CP15, 0, R12, CR1, CR0, 0
		T1WriteLong(MMU.ARM9_BIOS, 0x02C4, 0xE3CDC001); //BIC R12, SP, #1    
		T1WriteLong(MMU.ARM9_BIOS, 0x02C8, 0xE59CC010); //LDR R12, [R12, #10] 
		T1WriteLong(MMU.ARM9_BIOS, 0x02CC, 0xE35C0000); //CMP R12, #0  

		T1WriteLong(MMU.ARM9_BIOS, 0x02D0, 0x112FFF3C); //BLXNE R12    
		T1WriteLong(MMU.ARM9_BIOS, 0x02D4, 0x027FFD9C); //0x027FFD9C  
		//---------
	}
}

static void JumbleMemory()
{
	//put random garbage in vram for homebrew games, to help mimic the situation where libnds does not clear out junk
	//which the card's launcher may or may not have left behind
	//analysis:
  //1. retail games dont clear TCM, so why should we jumble it and expect homebrew to clear it?
  //2. some retail games _dont boot_ if main memory is jumbled. wha...?
  //3. clearing this is not as useful as tracking uninitialized reads in dev+ builds
  //4. the vram clearing causes lots of graphical corruptions in badly coded homebrews. this reduces compatibility substantially
  //conclusion: disable it for now and bring it back as an option
}

static void PrepareLogfiles()
{
}

bool NDS_LegitBoot()
{

	//hack for firmware boot in JIT mode.
	//we know that it takes certain jit parameters to successfully boot the firmware.
	//CRAZYMAX: is it safe to accept anything smaller than 12?
	//CommonSettings.jit_max_block_size = std::min(CommonSettings.jit_max_block_size,12U);
	
	//partially clobber the loaded firmware with the user settings from DFC
	firmware->loadSettings();

	//since firmware only boots encrypted roms, we have to make sure it's encrypted first
	//this has not been validated on big endian systems. it almost positively doesn't work.
	if (gameInfo.header.CRC16 != 0)
		EncryptSecureArea((u8*)&gameInfo.header, (u8*)gameInfo.secureArea);

	//boot processors from their bios entrypoints
	armcpu_init(&NDS_ARM7, 0x00000000);
	armcpu_init(&NDS_ARM9, 0xFFFF0000);

	return true;
}

//the fake firmware boot-up process
bool NDS_FakeBoot()
{

	NDS_header * header = NDS_getROMHeader();

	if (!header) return false;

	nds.hw_status.fakeBoot = true;

	//since we're bypassing the code to decrypt the secure area, we need to make sure its decrypted first
	//this has not been validated on big endian systems. it almost positively doesn't work.
	if (gameInfo.header.CRC16 != 0)
	{
		bool okRom = DecryptSecureArea((u8*)&gameInfo.header, (u8*)gameInfo.secureArea);

		if(!okRom) {
			return false;
		}
	}

	//bios (or firmware) sets this default, which is generally not important for retail games but some homebrews are depending on
	_MMU_write08<ARMCPU_ARM9>(REG_WRAMCNT,3);

	//EDIT - whats this firmware and how is it relating to the dummy firmware below
	//how do these even get used? what is the purpose of unpack and why is it not used by the firmware boot process?
	if (CommonSettings.UseExtFirmware && firmware->loaded())
	{
		firmware->unpack();
		firmware->loadSettings();
	}

	// Create the dummy firmware
	//EDIT - whats dummy firmware and how is relating to the above?
	//it seems to be emplacing basic firmware data into MMU.fw.data
	NDS_CreateDummyFirmware(&CommonSettings.fw_config);

	//firmware loads the game card arm9 and arm7 programs as specified in rom header
	{
		bool hasSecureArea = ((gameInfo.romType == ROM_NDS) && (gameInfo.header.CRC16 != 0));
		//copy the arm9 program to the address specified by rom header
		u32 src = header->ARM9src;
		u32 dst = header->ARM9cpy;
		for(u32 i = 0; i < header->ARM9binSize; i+=4)
		{
			u32 tmp = (hasSecureArea && ((src >= 0x4000) && (src < 0x8000)))?LE_TO_LOCAL_32(*(u32*)(gameInfo.secureArea + (src - 0x4000))):gameInfo.readROM(src);

			_MMU_write32<ARMCPU_ARM9>(dst, tmp);

			dst += 4;
			src += 4;
		}

		//copy the arm7 program to the address specified by rom header
		src = header->ARM7src;
		dst = header->ARM7cpy;
		for(u32 i = 0; i < header->ARM7binSize; i+=4)
		{
			_MMU_write32<ARMCPU_ARM7>(dst, gameInfo.readROM(src));

			dst += 4;
			src += 4;
		}
	}

	//bios does this (thats weird, though. shouldnt it get changed when the card is swapped in the firmware menu?
	//right now our firmware menu isnt detecting any change to the card.
	//are some games depending on it being written here? please document.
	//_MMU_write16<ARMCPU_ARM9>(0x027FF808, T1ReadWord(MMU.CART_ROM, 0x15E));

	//firmware sets up a copy of the firmware user settings in memory.
	//TBD - this code is really clunky
	//it seems to be copying the MMU.fw.data data into RAM in the user memory stash locations
	u8 temp_buffer[NDS_FW_USER_SETTINGS_MEM_BYTE_COUNT];
	if ( copy_firmware_user_data( temp_buffer, MMU.fw.data)) {
		for ( int fw_index = 0; fw_index < NDS_FW_USER_SETTINGS_MEM_BYTE_COUNT; fw_index++)
			_MMU_write08<ARMCPU_ARM9>(0x027FFC80 + fw_index, temp_buffer[fw_index]);
	}

	//something copies the whole header to Main RAM 0x27FFE00 on startup. (http://nocash.emubase.de/gbatek.htm#dscartridgeheader)
	//once upon a time this copied 0x90 more. this was thought to be wrong, and changed.
	if(nds.Is_DSI())
	{
		//dsi needs this copied later in memory. there are probably a number of things that  get copied to a later location in memory.. thats where the NDS consoles tend to stash stuff.
		for (int i = 0; i < (0x170); i+=4)
			_MMU_write32<ARMCPU_ARM9>(0x027FFE00 + i, gameInfo.readROM(i));
	}
	else
	{
		for (int i = 0; i < (0x170); i+=4)
			_MMU_write32<ARMCPU_ARM9>(0x027FFE00 + i, gameInfo.readROM(i));
	}

	//the firmware will be booting to these entrypoint addresses via BX (well, the arm9 at least; is unverified for the arm7)
	armcpu_init(&NDS_ARM7, header->ARM7exe);
	armcpu_init(&NDS_ARM9, header->ARM9exe);

	//firmware sets REG_POSTFLG to the value indicating post-firmware status
	MMU.ARM9_REG[0x300] = 1;
	MMU.ARM7_REG[0x300] = 1;

	//firmware makes system think it's booted from card -- EXTREMELY IMPORTANT!!! This is actually checked by some things. (which things?) Thanks to cReDiAr
	_MMU_write08<ARMCPU_ARM7>(0x02FFFC40,0x1); //<zero> removed redundant write to ARM9, this is going to shared main memory. But one has to wonder why r3478 was made which corrected a typo resulting in only ARMCPU_ARM7 getting used.

	//the chipId is read several times
	//for some reason, each of those reads get stored here.
	_MMU_write32<ARMCPU_ARM7>(0x027FF800, gameInfo.chipID);		//1st chipId
	_MMU_write32<ARMCPU_ARM7>(0x027FF804, gameInfo.chipID);		//2nd (secure) chipId
	_MMU_write32<ARMCPU_ARM7>(0x027FFC00, gameInfo.chipID);		//3rd (secure) chipId
	
	// Write the header checksum to memory
	_MMU_write16<ARMCPU_ARM9>(0x027FF808, gameInfo.header.headerCRC16);

	//bitbox 4k demo is so stripped down it relies on default stack values
	//otherwise the arm7 will crash before making a sound
	//(these according to gbatek softreset bios docs)
	NDS_ARM7.R13_svc = 0x0380FFDC;
	NDS_ARM7.R13_irq = 0x0380FFB0;
	NDS_ARM7.R13_usr = 0x0380FF00;
	NDS_ARM7.R[13] = NDS_ARM7.R13_usr;
	//and let's set these for the arm9 while we're at it, though we have no proof
	NDS_ARM9.R13_svc = 0x00803FC0;
	NDS_ARM9.R13_irq = 0x00803FA0;
	NDS_ARM9.R13_usr = 0x00803EC0;
	NDS_ARM9.R13_abt = NDS_ARM9.R13_usr; //????? 
	//I think it is wrong to take gbatek's "SYS" and put it in USR--maybe USR doesnt matter. 
	//i think SYS is all the misc modes. please verify by setting nonsensical stack values for USR here
	NDS_ARM9.R[13] = NDS_ARM9.R13_usr;
	//n.b.: im not sure about all these, I dont know enough about arm9 svc/irq/etc modes
	//and how theyre named in desmume to match them up correctly. i just guessed.

	//--------------------------------
	//setup the homebrew argv
	//this is useful for nitrofs apps which are emulating themselves via cflash
	//struct __argv {
	//	int argvMagic;		//!< argv magic number, set to 0x5f617267 ('_arg') if valid 
	//	char *commandLine;	//!< base address of command line, set of null terminated strings
	//	int length;			//!< total length of command line
	//	int argc;			//!< internal use, number of arguments
	//	char **argv;		//!< internal use, argv pointer
	//};
	std::string rompath = "fat:/" + path.RomName;
	const u32 kCommandline = 0x027E0000;
	//const u32 kCommandline = 0x027FFF84;

	//homebrew-related stuff.
	//its safe to put things in this position.. apparently nothing important is here.
	//however, some games could be checking them as an anti-desmume measure, so we might have to control it with slot-1 settings to suggest booting a homebrew app
	//perhaps we could automatically boot homebrew to an R4-like device.
	_MMU_write32<ARMCPU_ARM9>(0x02FFFE70, 0x5f617267);
	_MMU_write32<ARMCPU_ARM9>(0x02FFFE74, kCommandline); //(commandline starts here)
	_MMU_write32<ARMCPU_ARM9>(0x02FFFE78, rompath.size()+1);
	//0x027FFF7C (argc)
	//0x027FFF80 (argv)
	for(size_t i=0;i<rompath.size();i++)
		_MMU_write08<ARMCPU_ARM9>(kCommandline+i, rompath[i]);
	_MMU_write08<ARMCPU_ARM9>(kCommandline+rompath.size(), 0);
	//--------------------------------

	//Call the card post_fakeboot hook to perform additional initialization
	slot1_device->post_fakeboot(ARMCPU_ARM9);
	slot1_device->post_fakeboot(ARMCPU_ARM7);

	delete header;
	return true;
}

bool _HACK_DONT_STOPMOVIE = false;
void NDS_Reset()
{
	if(!IsEmu()) J_EXECUTE_ME_ONCE(&renderScreen, (int)frameSkipper.ShouldSkip2D());

	resetUserInput();
	
	nds.hw_status.val = 0;
	nds.freezeBus = 0;
	nds.power1.lcd = nds.power1.gpuMain = nds.power1.gfx3d_render = nds.power1.gfx3d_geometry = nds.power1.gpuSub = nds.power1.dispswap = 1;
	nds.power2.speakers = 1;
	nds.power2.wifi = 0;
	memset(nds.timerCycle, 0, sizeof(u64) * 2 * 4);
	nds.scr_touchX = nds.scr_touchY = nds.adc_touchX = nds.adc_touchY = 0;
	nds.hw_status.fakeBoot = false;
	nds.paddle = 0;
	nds.ensataEmulation = CommonSettings.EnsataEmulation;
	nds.ensataHandshake = ENSATA_HANDSHAKE_none;
	nds.ensataIpcSyncCounter = 0;
	nds_timer = 0;
	nds_arm9_timer = 0;
	nds_arm7_timer = 0;
	LidClosed = FALSE;
	countLid = 0;

	MMU_Reset();

	SetupMMU(false,false);

	JumbleMemory();

	arm_jit_reset(true);

	//initialize CP15 specially for this platform
	//TODO - how much of this is necessary for firmware boot?
	//(only ARM9 has CP15)
	reconstruct(&cp15);
	MMU.ARM9_RW_MODE = BIT7(cp15.ctrl);
	NDS_ARM9.intVector = 0xFFFF0000 * (BIT13(cp15.ctrl));
	NDS_ARM9.LDTBit = !BIT15(cp15.ctrl); //TBit

	PrepareBiosARM7();
	PrepareBiosARM9();

	if (firmware)
	{
		delete firmware;
		firmware = NULL;
	}

	firmware = new CFIRMWARE();
	firmware->load();

	NDS_FakeBoot();

	// Init calibration info
	memcpy(&TSCal, firmware->getTouchCalibrate(), sizeof(TSCalInfo));

	Screen_Reset();

	gfx3d_reset();

	gpu3D->NDS_3D_Reset();

	WIFI_Reset();
	memcpy(FW_Mac, (MMU.fw.data + 0x36), 6);

	//this needs to happen last, pretty much, since it establishes the correct scheduling state based on all of the above initialization
	initSchedule();
}

static std::string MakeInputDisplayString(u16 pad, const std::string* Buttons, int count) {
    std::string s;
    for (int x = 0; x < count; x++) {
        if (pad & (1 << x))
            s.append(Buttons[x].size(), ' '); 
        else
            s += Buttons[x];
    }
    return s;
}

static std::string MakeInputDisplayString(u16 pad, u16 padExt) {
    const std::string Buttons[] = {"A", "B", "Sl", "St", "R", "L", "U", "D", "Rs", "Ls"};
    const std::string Ext[] = {"X", "Y"};

    std::string s = MakeInputDisplayString(pad, Ext, ARRAY_SIZE(Ext));
    s += MakeInputDisplayString(padExt, Buttons, ARRAY_SIZE(Buttons));

    return s;
}


buttonstruct<bool> Turbo;
buttonstruct<int> TurboTime;
buttonstruct<bool> AutoHold;

void ClearAutoHold(void) {
	
	for (u32 i=0; i < ARRAY_SIZE(AutoHold.array); i++) {
		AutoHold.array[i]=false;
	}
}

//convert a 12.4 screen coordinate to an ADC value.
//the desmume host system will track the screen coordinate, but the hardware should be receiving the raw ADC values.
//so we'll need to use this to simulate the ADC values corresponding to the desired screen coords, based on the current TSC calibrations
u16 NDS_getADCTouchPosX(int scrX_lsl4)
{
	scrX_lsl4 >>= 4;
	int rv = ((scrX_lsl4 - TSCal.scr.x1 + 1) * TSCal.adc.width) / TSCal.scr.width + TSCal.adc.x1;
	rv = min(0xFFF, max(0, rv));
	return (u16)(rv);
}
u16 NDS_getADCTouchPosY(int scrY_lsl4)
{
	scrY_lsl4 >>= 4;
	int rv = ((scrY_lsl4 - TSCal.scr.y1 + 1) * TSCal.adc.height) / TSCal.scr.height + TSCal.adc.y1;
	rv = min(0xFFF, max(0, rv));
	return (u16)(rv);
}

static UserInput rawUserInput = {}; // requested input, generally what the user is physically pressing
static UserInput intermediateUserInput = {}; // intermediate buffer for modifications (seperated from finalUserInput for safety reasons)
static UserInput finalUserInput = {}; // what gets sent to the game and possibly recorded
bool validToProcessInput = false;

const UserInput& NDS_getRawUserInput()
{
	return rawUserInput;
}
UserInput& NDS_getProcessingUserInput()
{
	//assert(validToProcessInput);
	return intermediateUserInput;
}
bool NDS_isProcessingUserInput()
{
	return validToProcessInput;
}
const UserInput& NDS_getFinalUserInput()
{
	return finalUserInput;
}


static void saveUserInput(EMUFILE* os, UserInput& input)
{
	os->fwrite((const char*)input.buttons.array, 14);
	writebool(input.touch.isTouch, os);
	write16le(input.touch.touchX, os);
	write16le(input.touch.touchY, os);
	write32le(input.mic.micButtonPressed, os);
}
static bool loadUserInput(EMUFILE* is, UserInput& input, int version)
{
	is->fread((char*)input.buttons.array, 14);
	readbool(&input.touch.isTouch, is);
	read16le(&input.touch.touchX, is);
	read16le(&input.touch.touchY, is);
	read32le(&input.mic.micButtonPressed, is);
	return true;
}
static void resetUserInput(UserInput& input)
{
	memset(&input, 0, sizeof(UserInput));
}
// (userinput is kind of a misnomer, e.g. finalUserInput has to mirror nds.pad, nds.touchX, etc.)
static void saveUserInput(EMUFILE* os)
{
	saveUserInput(os, finalUserInput);
	saveUserInput(os, intermediateUserInput); // saved in case a savestate is made during input processing (which Lua could do if nothing else)
	writebool(validToProcessInput, os);
	for(int i = 0; i < 14; i++)
		write32le(TurboTime.array[i], os); // saved to make autofire more tolerable to use with re-recording
}
static bool loadUserInput(EMUFILE* is, int version)
{
	bool rv = true;
	rv &= loadUserInput(is, finalUserInput, version);
	rv &= loadUserInput(is, intermediateUserInput, version);
	readbool(&validToProcessInput, is);
	for(int i = 0; i < 14; i++)
		read32le((u32*)&TurboTime.array[i], is);
	return rv;
}
static void resetUserInput()
{
	resetUserInput(finalUserInput);
	resetUserInput(intermediateUserInput);
}

static inline void gotInputRequest(){}

void NDS_setPad(bool R,bool L,bool D,bool U,bool T,bool S,bool B,bool A,bool Y,bool X,bool W,bool E,bool G, bool F)
{
	gotInputRequest();
	UserButtons& rawButtons = rawUserInput.buttons;
	rawButtons.R = R;
	rawButtons.L = L;
	rawButtons.D = D;
	rawButtons.U = U;
	rawButtons.T = T;
	rawButtons.S = S;
	rawButtons.B = B;
	rawButtons.A = A;
	rawButtons.Y = Y;
	rawButtons.X = X;
	rawButtons.W = W;
	rawButtons.E = E;
	rawButtons.G = G;
	rawButtons.F = F;
}
void NDS_setTouchPos(u16 x, u16 y)
{
	gotInputRequest();
	rawUserInput.touch.touchX = x<<4;
	rawUserInput.touch.touchY = y<<4;
	rawUserInput.touch.isTouch = true;
}
void NDS_releaseTouch(void)
{ 
	gotInputRequest();
	rawUserInput.touch.touchX = 0;
	rawUserInput.touch.touchY = 0;
	rawUserInput.touch.isTouch = false;
}
void NDS_setMic(bool pressed)
{
	gotInputRequest();
	rawUserInput.mic.micButtonPressed = (pressed ? TRUE : FALSE);
}


static void NDS_applyFinalInput();


void NDS_beginProcessingInput()
{
	// start off from the raw input
	intermediateUserInput = rawUserInput;

	// processing is valid now
	validToProcessInput = true;
}

void NDS_endProcessingInput()
{
	// transfer the processed input
	finalUserInput = intermediateUserInput;

	// processing is invalid now
	validToProcessInput = false;

	// use the final input for a few things right away
	NDS_applyFinalInput();
}








static void NDS_applyFinalInput()
{
	const UserInput& input = NDS_getFinalUserInput();

	u16	pad	= (0 |
		((input.buttons.A ? 0 : 0x80) >> 7) |
		((input.buttons.B ? 0 : 0x80) >> 6) |
		((input.buttons.T ? 0 : 0x80) >> 5) |
		((input.buttons.S ? 0 : 0x80) >> 4) |
		((input.buttons.R ? 0 : 0x80) >> 3) |
		((input.buttons.L ? 0 : 0x80) >> 2) |
		((input.buttons.U ? 0 : 0x80) >> 1) |
		((input.buttons.D ? 0 : 0x80)     ) |
		((input.buttons.E ? 0 : 0x80) << 1) |
		((input.buttons.W ? 0 : 0x80) << 2)) ;

	pad = LOCAL_TO_LE_16(pad);
	((u16 *)MMU.ARM9_REG)[0x130>>1] = (u16)pad;
	((u16 *)MMU.ARM7_REG)[0x130>>1] = (u16)pad;

	u16 k_cnt = ((u16 *)MMU.ARM9_REG)[0x132>>1];
	if ( k_cnt & (1<<14))
	{
		//INFO("ARM9: KeyPad IRQ (pad 0x%04X, cnt 0x%04X (condition %s))\n", pad, k_cnt, k_cnt&(1<<15)?"AND":"OR");
		u16 k_cnt_selected = (k_cnt & 0x3F);
		if (k_cnt&(1<<15))	// AND
		{
			if ((~pad & k_cnt_selected) == k_cnt_selected) NDS_makeIrq(ARMCPU_ARM9,IRQ_BIT_KEYPAD);
		}
		else				// OR
		{
			if (~pad & k_cnt_selected) NDS_makeIrq(ARMCPU_ARM9,IRQ_BIT_KEYPAD);
		}
	}

	k_cnt = ((u16 *)MMU.ARM7_REG)[0x132>>1];
	if ( k_cnt & (1<<14))
	{
		//INFO("ARM7: KeyPad IRQ (pad 0x%04X, cnt 0x%04X (condition %s))\n", pad, k_cnt, k_cnt&(1<<15)?"AND":"OR");
		u16 k_cnt_selected = (k_cnt & 0x3F);
		if (k_cnt&(1<<15))	// AND
		{
			if ((~pad & k_cnt_selected) == k_cnt_selected) NDS_makeIrq(ARMCPU_ARM7,IRQ_BIT_KEYPAD);
		}
		else				// OR
		{
			if (~pad & k_cnt_selected) NDS_makeIrq(ARMCPU_ARM7,IRQ_BIT_KEYPAD);
		}
	}


	if(input.touch.isTouch)
	{
		u16 adc_x = NDS_getADCTouchPosX(input.touch.touchX);
		u16 adc_y = NDS_getADCTouchPosY(input.touch.touchY);
		nds.adc_touchX = adc_x;
		nds.adc_touchY = adc_y;

		nds.scr_touchX = input.touch.touchX;
		nds.scr_touchY = input.touch.touchY;
		nds.hw_status.Touching  = 1;
	}
	else
	{
		nds.adc_touchX = 0;
		nds.adc_touchY = 0;
		nds.scr_touchX = 0;
		nds.scr_touchY = 0;
		nds.hw_status.Touching = 0;
	}

	if (input.buttons.F && !countLid) 
	{
		LidClosed = (!LidClosed) & 0x01;
		if (!LidClosed)
		{
			NDS_makeIrq(ARMCPU_ARM7,IRQ_BIT_ARM7_FOLD);
		}

		countLid = 30;
	}
	else 
	{
		if (countLid > 0)
			countLid--;
	}

	u16 padExt = ((input.buttons.X ? 0 : 0x80) >> 7) |
		((input.buttons.Y ? 0 : 0x80) >> 6) |
		((input.buttons.G ? 0 : 0x80) >> 4) |
		((LidClosed) << 7) |
		0x0034;

	padExt = LOCAL_TO_LE_16(padExt);
	padExt |= (((u16 *)MMU.ARM7_REG)[0x136>>1] & 0x0070);
	
	((u16 *)MMU.ARM7_REG)[0x136>>1] = (u16)padExt;

	//InputDisplayString=MakeInputDisplayString(padExt, pad);

	//put into the format we want for the movie system
	//fRLDUTSBAYXWEg
	//we don't really need nds.pad anymore, but removing it would be a pain

 	nds.pad =
		((input.buttons.R ? 1 : 0) << 12)|
		((input.buttons.L ? 1 : 0) << 11)|
		((input.buttons.D ? 1 : 0) << 10)|
		((input.buttons.U ? 1 : 0) << 9)|
		((input.buttons.T ? 1 : 0) << 8)|
		((input.buttons.S ? 1 : 0) << 7)|
		((input.buttons.B ? 1 : 0) << 6)|
		((input.buttons.A ? 1 : 0) << 5)|
		((input.buttons.Y ? 1 : 0) << 4)|
		((input.buttons.X ? 1 : 0) << 3)|
		((input.buttons.W ? 1 : 0) << 2)|
		((input.buttons.E ? 1 : 0) << 1);
}


void NDS_suspendProcessingInput(bool suspend)
{
	static int suspendCount = 0;
	if(suspend)
	{
		// enter non-processing block
		//assert(validToProcessInput);
		validToProcessInput = false;
		suspendCount++;
	}
	else if(suspendCount)
	{
		// exit non-processing block
		validToProcessInput = true;
		suspendCount--;
	}
	else
	{
		// unwound past first time -> not processing
		validToProcessInput = false;
	}
}

void NDS_swapScreen()
{
	u16 tmp = MainScreen.offset;
	MainScreen.offset = SubScreen.offset;
	SubScreen.offset = tmp;
}


void emu_halt() {
	//printf("halting emu: ARM9 PC=%08X/%08X, ARM7 PC=%08X/%08X\n", NDS_ARM9.R[15], NDS_ARM9.instruct_adr, NDS_ARM7.R[15], NDS_ARM7.instruct_adr);
	execute = false;
#ifdef LOG_ARM9
	if (fp_dis9)
	{
		char buf[256] = { 0 };
		sprintf(buf, "halting emu: ARM9 PC=%08X/%08X\n", NDS_ARM9.R[15], NDS_ARM9.instruct_adr);
		fwrite(buf, 1, strlen(buf), fp_dis9);
		INFO("ARM9 halted\n");
	}
#endif

#ifdef LOG_ARM7
	if (fp_dis7)
	{
		char buf[256] = { 0 };
		sprintf(buf, "halting emu: ARM7 PC=%08X/%08X\n", NDS_ARM7.R[15], NDS_ARM7.instruct_adr);
		fwrite(buf, 1, strlen(buf), fp_dis7);
		INFO("ARM7 halted\n");
	}
#endif
}

//returns true if exmemcnt specifies satisfactory parameters for the device, which calls this function
bool ValidateSlot2Access(u32 procnum, u32 demandSRAMSpeed, u32 demand1stROMSpeed, u32 demand2ndROMSpeed, int clockbits)
{
	static const u32 _sramSpeeds[] = {10,8,6,18};
	static const u32 _rom1Speeds[] = {10,8,6,18};
	static const u32 _rom2Speeds[] = {6,4};
	u16 exmemcnt = T1ReadWord(MMU.MMU_MEM[procnum][0x40], 0x204);
	u16 exmemcnt9 = T1ReadWord(MMU.MMU_MEM[ARMCPU_ARM9][0x40], 0x204);
	u32 arm7access = (exmemcnt9 & EXMEMCNT_MASK_SLOT2_ARM7);
	u32 sramSpeed = _sramSpeeds[(exmemcnt & EXMEMCNT_MASK_SLOT2_SRAM_TIME)];
	u32 romSpeed1 = _rom1Speeds[(exmemcnt & EXMEMCNT_MASK_SLOT2_ROM_1ST_TIME)>>2];
	u32 romSpeed2 = _rom2Speeds[(exmemcnt & EXMEMCNT_MASK_SLOT2_ROM_2ND_TIME)>>4];
	u32 curclockbits = (exmemcnt & EXMEMCNT_MASK_SLOT2_CLOCKRATE)>>5;
	
	if(procnum==ARMCPU_ARM9 && arm7access) return false;
	if(procnum==ARMCPU_ARM7 && !arm7access) return false;

	//what we're interested in here is whether the rom/ram are too low -> too fast. then accesses won't have enough time to work.
	//i'm not sure if this gives us enough flexibility, but it is good enough for now.
	//should make the arguments to this function bitmasks later if we need better.
	if(sramSpeed < demandSRAMSpeed) return false;
	if(romSpeed1 < demand1stROMSpeed) return false;
	if(romSpeed2 < demand2ndROMSpeed) return false;

	if(clockbits != -1 && clockbits != (int)curclockbits) return false;

	return true;
}

//these templates needed to be instantiated manually
template void NDS_exec<FALSE>(s32 nb);
template void NDS_exec<TRUE>(s32 nb);

