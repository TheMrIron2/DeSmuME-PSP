/* main.c - this file is part of DeSmuME
 *
 * Copyright (C) 2006-2015 DeSmuME Team
 * Copyright (C) 2007 Pascal Giard (evilynux)
 * Used under fair use by the DSonPSP team, 2019
 *
 * This file is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2, or (at your option)
 * any later version.
 *
 * This file is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; see the file COPYING.  If not, write to
 * the Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

#include <SDL/SDL.h>
#include <SDL/SDL_thread.h>
#include <stdlib.h>
#include <string.h>

//HCF
#include <psppower.h>
#include <pspdebug.h>
#include <pspctrl.h>
#include <pspdisplay.h>
#include <pspsuspend.h>
#include <pspkernel.h>

#include <Profiler/Profiler.h>
Stardust::Profiling::Profiler pf("DeSmuME");

//HCF: To allocate volatile memory
/*
extern void* HCF_RAM_ARRAY;
extern int inVolatileAssigned;
*/

//PROBADO ESTO SIN EXITO ("TESTED THIS WITHOUT SUCCESS")
/*
PSP_HEAP_SIZE_KB(-1024);
PSP_MAIN_THREAD_STACK_SIZE_KB(1024);
*/

/*
PSP_HEAP_SIZE_KB(-256);
PSP_MAIN_THREAD_STACK_SIZE_KB(256);
*/
PSP_MAIN_THREAD_ATTR( PSP_THREAD_ATTR_VFPU );
PSP_HEAP_SIZE_KB(-256);
PSP_MAIN_THREAD_STACK_SIZE_KB(256);



#define SHOW_FPS_VERSION 1
#define FILE_SETTINGS "settings.ini"

#ifndef VERSION
#define VERSION "Unknown version"
#endif

/*
 * FIXME: Not sure how to detect OpenGL in a platform portable way.
 */
#ifdef HAVE_GL_GL_H
#define INCLUDE_OPENGL_2D
#endif

#ifndef CLI_UI
#define CLI_UI
#endif

#include "NDSSystem.h"
#include "GPU.h"
#include "SPU.h"
#include "sndsdl.h"
#include "ctrlssdl.h"
#include "slot2.h"

#include "render3D.h"
#include "rasterize.h"

#include "commandline.h"
#include "GPU_osd.h"
#include "driver.h"

/*
#include "saves.h"
//#include "desmume_config.h"
#include "utils/xstring.h"

*/

//HCF PSP
SDL_Surface *SDLscreen = NULL;
SDL_Surface *surface1;
SDL_Surface *surface2;
TTF_Font *font;


//HCF to store the selected parameters
#define FILE_DATA_INI "rom.ini"
#define MAX_NOMBRE_FICHERO 100 // max_number_files

char rom_filename[256];
int totalframessegundo = 0;

extern int vdp2width;
extern int vdp2height;

extern int iSoundQuality;
extern int iSoundMode;
int frameskip = 0;
int enable_sound = 1;
int iVideoFilterHW;
int inIdiomaFirmware = 1; // in firmware language

//HCF String functions
std::string strright(const std::string& str, int len);
std::string toupper(const std::string& str);

char achMem[10] = " ";
char achFrames[10] = " ";
int timo = 0;
int tima = 0;

#define NUM_FRAMES_TO_TIME 15 // why is this 15 but 60 with DISPLAY_FPS enabled?

#define FPS_LIMITER_FRAME_PERIOD 8
#define MAX_EXPECTED_FRAME_TIME 17
#define MIN_EXPECTED_FRAME_TIME 16
#define JUST_EXPECTED_FRAME_TIME 16

volatile bool execute = false;

int soundskipa = 0;

//HCF
intptr_t errno;

static float nds_screen_size_ratio = 1.0f;

#define DISPLAY_FPS

#ifdef DISPLAY_FPS
#define NUM_FRAMES_TO_TIME 60
#endif

#define FPS_LIMITER_FPS 60

//static SDL_Surface * surface;

/* Flags to pass to SDL_SetVideoMode */
static int sdl_videoFlags;

SoundInterface_struct *SNDCoreList[] = {
  &SNDDummy,
  &SNDDummy,
  &SNDSDL,
  NULL
};

GPU3DInterface *core3DList[] = {
  &gpu3DNull,
  &gpu3DRasterize,
  NULL
};

const char * save_type_names[] = {
  "Autodetect",
  "EEPROM 4kbit",
  "EEPROM 64kbit",
  "EEPROM 512kbit",
  "FRAM 256kbit",
  "FLASH 2mbit",
  "FLASH 4mbit",
  NULL
};


/* Our keyboard config is different because of the directional keys */
/* Please note that m is used for fake microphone */
const u16 cli_kb_cfg[NB_KEYS] =
  {
    SDLK_x,         // A
    SDLK_z,         // B
    SDLK_RSHIFT,    // select
    SDLK_RETURN,    // start
    SDLK_RIGHT,     // Right
    SDLK_LEFT,      // Left
    SDLK_UP,        // Up
    SDLK_DOWN,      // Down
    SDLK_w,         // R
    SDLK_q,         // L
    SDLK_s,         // X
    SDLK_a,         // Y
    SDLK_p,         // DEBUG
    SDLK_o,         // BOOST
    SDLK_BACKSPACE, // Lid
  };

void vdDejaLog(char *msg);

class configured_features : public CommandLine
{
public:
  int auto_pause;
  int frameskip;

  int engine_3d;
  int savetype;

#ifdef INCLUDE_OPENGL_2D
  int opengl_2d;
  int soft_colour_convert;
#endif

  int firmware_language;
};

void DrawText(short * screen, unsigned int x, unsigned int y, bool invert, const char * text);
void DrawColorText(short * screen, unsigned int x, unsigned int y, bool invert, const char * text);

int iGetFreeMemory()
{
    int iStep = 1000000;
    int iAmount = 0;
    short shSalir = 0;

    char *pchAux = NULL;

    while(!shSalir)
    {
        iAmount += iStep;
        pchAux = (char*) malloc( iAmount );
        if(pchAux == NULL)
        {
            //No hay memoria libre!!! = There is no free memory!
            iAmount -= iStep;
            shSalir = 1;

        }
        else
        {
            free(pchAux);
            pchAux = NULL;
        }
    }

    return iAmount;
}

void vdResetEmulator()
{
    exit(0);
}

void vdDrawStylus()
{
	int x = mouse.x;
	int y = 192 + mouse.y;

	//+ drawing
	if( x < 3 )
		x = 0;
	else if (x > 249)
		x = 249;
	else
		x -= 3;

	if( y < 3 )
		y = 0;
	else if (y > 377)
		y = 377;
	else
		y -= 3;

	//DrawCursor((short*)GPU_screen, x, y);
	//DrawText((short*)GPU_screen, x, y, false, "+");
}

#include <pspgu.h>

void Gu_draw()
{
	int i, j;
	u16* runSrc;
	u16* runDst;

    SDL_Rect rectPant1, rectPant2;

	//PARA IMPRIMIR FPS - to print FPS
	char achTextoFPS[20];
	SDL_Rect rectTextoFPS;
	SDL_Rect rectTextoFrameskip;
	SDL_Surface *picTextoFPS;
	SDL_Color textColor = { 240, 0, 200 };

	rectTextoFPS.x = 10;
	rectTextoFPS.y = 10;
	rectTextoFrameskip.y = 10;
	rectPant1.x = 0;
	rectPant1.y = 40;
    
	rectPant2.x = 240;
	rectPant2.y = 40;


    //Screen 1 - No stretch
    if (SDL_MUSTLOCK(surface1))
    while (SDL_LockSurface(surface1) < 0)
        SDL_Delay(10);    //HCF: QUITABLE

    memcpy(surface1->pixels, GPU_screen, 256 * 192 * 2);

    if (SDL_MUSTLOCK(surface1))
        SDL_UnlockSurface(surface1);

    SDL_BlitSurface(surface1, NULL, SDLscreen, &rectPant1);
    
    //Screen 2 - No stretch
    if (SDL_MUSTLOCK(surface2))
    while (SDL_LockSurface(surface2) < 0)
        SDL_Delay(10);    //HCF: QUITABLE


    memcpy(surface2->pixels, &GPU_screen[256*192*2], 256 * 192 * 2);

    if (SDL_MUSTLOCK(surface2))
        SDL_UnlockSurface(surface2);


    pf.beginProfileMethod();
    SDL_BlitSurface(surface2, NULL, SDLscreen, &rectPant2);

//Para imprimir FPS
    #if( SHOW_FPS_VERSION  == 1 )
			sprintf(achTextoFPS, "%d", totalframessegundo);
			picTextoFPS = TTF_RenderText_Solid( font, achTextoFPS, textColor );
			rectTextoFPS.x = 260;
			SDL_BlitSurface(picTextoFPS, NULL, SDLscreen, &rectTextoFPS);
			SDL_FreeSurface(picTextoFPS);

			//Para imprimir frameskip
			//Para imprimir frameskip
			sprintf(achTextoFPS, "%d", frameskip);
			picTextoFPS = TTF_RenderText_Solid( font, achTextoFPS, textColor );
			rectTextoFrameskip.x = 330;
			SDL_BlitSurface(picTextoFPS, NULL, SDLscreen, &rectTextoFrameskip);
			SDL_FreeSurface(picTextoFPS);
	#endif

            SDL_Flip(SDLscreen);

}


int compara(char a, char b, char c)
{
	if( a == b || a == c )
	{
		return 1;
	}
	else
	{
		return 0;
	}
}

static void
init_config( class configured_features *config) {

  config->auto_pause = 0;
  config->frameskip = 0;

  config->engine_3d = 1;
  config->savetype = 0;

#ifdef INCLUDE_OPENGL_2D
  config->opengl_2d = 0;
  config->soft_colour_convert = 0;
#endif

  /* use the default language */
  config->firmware_language = -1;
}

/* this is a stub for resizeWindow_stub in the case of no gl headers or no opengl 2d */

#ifdef INCLUDE_OPENGL_2D
static void
resizeWindow_stub (u16 width, u16 height, GLuint *screen_texture) {
}
#else
static void
resizeWindow_stub (u16 width, u16 height, void *screen_texture) {
}
#endif


static void desmume_cycle(struct ctrls_event_config * cfg)
{
    SDL_Event event;

    cfg->nds_screen_size_ratio = nds_screen_size_ratio;

    /* Look for queued events and update keypad status */
    /* IMPORTANT: Reenable joystick events iif needed. */
	/***
    if(SDL_JoystickEventState(SDL_QUERY) == SDL_IGNORE)
      SDL_JoystickEventState(SDL_ENABLE);

    // There's an event waiting to be processed?
    while ( !cfg->sdl_quit &&
        (SDL_PollEvent(&event) || (!cfg->focused && SDL_WaitEvent(&event))))
      {
        process_ctrls_event( event, cfg);
    }
	*/
	process_ctrls_event(cfg->keypad);

    /* Update mouse position and click */

  if (mouse.click)
	  {
		  NDS_setTouchPos(mouse.x, mouse.y);
	  }
	  else {
		  NDS_releaseTouch();
	  }

	  //vdDejaLog("update keypad  ");

    update_keypad(cfg->keypad);     /* Update keypad */

	//vdDejaLog("NDS_EXEC  ");

    pf.beginProfileMethod();
    NDS_exec<false>();
    pf.endProfileMethod("NDS_EXEC");

	//vdDejaLog("A SOUND  ");

    pf.beginProfileMethod();
	if ( enable_sound)
	{
		if(soundskipa == 0)
		{
			SPU_Emulate_user();
		}
		if(iEnableSound > 1)
		{
			soundskipa = (soundskipa + 1) % iEnableSound;
		}
	}
    pf.endProfileMethod("spu_emulate_user");


}

#ifdef HAVE_LIBAGG
T_AGG_RGB555 agg_targetScreen_cli(GPU_screen, 256, 384, 512);
#endif

/*
std::string strsub(const std::string& str, int pos, int len) {
	int strlen = str.size();

	if(strlen==0) return str; //empty strings always return empty strings
	if(pos>=strlen) return str; //if you start past the end of the string, return the entire string. this is unusual, but there you have it

	//clipping
	if(pos<0) {
		len += pos;
		pos = 0;
	}

	if (pos+len>=strlen)
		len=strlen-pos+1;

	//return str.str().substr(pos,len);
	return str.substr(pos,len);
}
std::string strright(const std::string& str, int len)
{
	return len ? strsub(str,str.size()-len,len) : "";
}
std::string toupper(const std::string& str)
{
	std::string ret = str;
	for(u32 i=0;i<str.size();i++)
		ret[i] = toupper(ret[i]);
	return ret;
}
*/
// Why commented out?

void vdDejaLog(char *msg)
{
	return;

    FILE *fd;
    fd = fopen("ZZLOG.TXT", "a");
    fprintf(fd, "%s\n", msg);
    fclose(fd);
}

extern "C" int SDL_main(int argc, char **argv) {
//int main(int argc, char ** argv) {
  class configured_features my_config;
  struct ctrls_event_config ctrls_cfg;

  int limiter_frame_counter = 0;
  int limiter_tick0 = 0;
  int error;


  FILE *fdini;
  char chCar;
  int jjj, kk, kkk, salir, indice;
  int t1, t2, t3, tsegundo1, framessegundo;
  char dataParams[MAX_NOMBRE_FICHERO];
  char achFicheroElegido[MAX_NOMBRE_FICHERO];

  //LOGS
  char CAPIMBE[128];

  //  GKeyFile *keyfile;

  int now;

#ifdef DISPLAY_FPS
  u32 fps_timing = 0;
  u32 fps_frame_counter = 0;
  u32 fps_previous_time = 0;
#endif

  scePowerSetClockFrequency(333, 333, 166);

    vdDejaLog("INICIO");

    //LOGS
    memset(CAPIMBE,0x00, 128);
    sprintf(CAPIMBE, "%d", iGetFreeMemory());
    vdDejaLog(CAPIMBE);
    //LOGS

  //HCF: Less accuracy, more speed in float operations
  //HCF: Speed improvement was not noticeable, so we comment it
  //_controlfp(_PC_24, _MCW_PC);

  /* this holds some info about our display */
  //const SDL_VideoInfo *videoInfo;

  //HCF Settings (firmware language... and RFU)
  inIdiomaFirmware = 1;  //English by default


  //HCF configuration parameters

  //DATAPARAMS

  /* the firmware settings */
  struct NDS_fw_config_data fw_config;

  NDS_Init();

  /* default the firmware settings, they may get changed later */
  NDS_FillDefaultFirmwareConfigData( &fw_config);

  init_config( &my_config);
  /*
  if ( !fill_config( &my_config, argc, argv)) {
    exit(1);
  }
	*/

  if ( my_config.firmware_language != -1) {
    fw_config.language = my_config.firmware_language;
  }

  //HCF
    //my_config.process_addonCommands();

    int slot2_device_type = NDS_SLOT2_AUTO;

    if (my_config.is_cflash_configured)
        slot2_device_type = NDS_SLOT2_CFLASH;

    if(my_config.gbaslot_rom != "") {

        //set the GBA rom and sav paths
        GBACartridge_RomPath = my_config.gbaslot_rom.c_str();
        if(toupper(strright(GBACartridge_RomPath,4)) == ".GBA")
          GBACartridge_SRAMPath = strright(GBACartridge_RomPath,4) + ".sav";
        else
          //what to do? lets just do the same thing for now
          GBACartridge_SRAMPath = strright(GBACartridge_RomPath,4) + ".sav";

        // Check if the file exists and can be opened
        FILE * test = fopen(GBACartridge_RomPath.c_str(), "rb");
        if (test) {
            slot2_device_type = NDS_SLOT2_GBACART;
            fclose(test);
        }
    }

	switch (slot2_device_type)
	{
		case NDS_SLOT2_NONE:
			break;
		case NDS_SLOT2_AUTO:
			break;
		case NDS_SLOT2_CFLASH:
			break;
		case NDS_SLOT2_RUMBLEPAK:
			break;
		case NDS_SLOT2_GBACART:
			break;
		case NDS_SLOT2_GUITARGRIP:
			break;
		case NDS_SLOT2_EXPMEMORY:
			break;
		case NDS_SLOT2_EASYPIANO:
			break;
		case NDS_SLOT2_PADDLE:
			break;
		case NDS_SLOT2_PASSME:
			break;
		default:
			slot2_device_type = NDS_SLOT2_NONE;
			break;
	}

    slot2_Init();

    slot2_Change((NDS_SLOT2_TYPE)slot2_device_type);

  /*
  if ( !g_thread_supported()) {
    g_thread_init( NULL);
  }
  */

  driver = new BaseDriver();

  /* Create the dummy firmware */
  NDS_CreateDummyFirmware( &fw_config);

  if ( !my_config.disable_sound) {
    SPU_ChangeSoundCore(SNDCORE_SDL, 735 * 4);
  }

  NDS_3D_ChangeCore(my_config.engine_3d);

  backup_setManualBackupType(my_config.savetype);

	//HCF Parameters
	nFrameskip = 5;  //Frameskip 3
	iEnableSound = 5;  //Sound underclocked x 4
	iMouseSpeed = 3;

	//USE JIT/DYNAREC (old motherboard speed)
	iUsarDynarec = 0;


	//Sound Quality (channels playing)
	iSoundQuality = 1;  //16 channels

//HCF LIMIT FRAMERATE MODES
#define LIMIT_FRAMERATE_NO       0
#define LIMIT_FRAMERATE_HCF      1
#define LIMIT_FRAMERATE_DESMUME  2

	//Limit Framerate
	iLimitFramerate = LIMIT_FRAMERATE_NO;

	iAutoFrameskip = 0;
	/*
	if( dataParams[6] == '1' )
	{
		bAutoFrameskip = true;
	}
	else
	{
		bAutoFrameskip = false;
	}
	*/

	bBlitAll = true;

	emula3D = false; //true;


	//Sound Mode (Async / Sync / Interpolated)
    //HCF SOUND MODE
    #define SOUND_MODE_ASYNC               0
    #define SOUND_MODE_SYNC                1
    #define SOUND_MODE_SYNC_INTERPOLATED   2
	iSoundMode = SOUND_MODE_ASYNC;


    //Selected Rom

	//Final character
	strcpy(achFicheroElegido, "test.nds");

	//HCF This avoids the stupid XB error
	memset(rom_filename, 0, 256);

	strcpy(rom_filename, achFicheroElegido);

	//HCF Fix the name of the selected file!
	salir = 0;
	for( kk = 0; kk < 256 && !salir; kk++ )
	{
		if( rom_filename[kk] == '.' )
		{
			if( ( compara(rom_filename[kk + 1],'n','N') && compara(rom_filename[kk + 2],'d','D') && compara(rom_filename[kk + 3],'s','S')  )
			|| ( compara(rom_filename[kk + 1],'z','Z') && compara(rom_filename[kk + 2],'i','I') && compara(rom_filename[kk + 3],'p','P')  ))
			{
				for( indice = kk + 4; indice < 256; indice++ )
				{
					rom_filename[indice] = '\0';
				}
				salir = 1;
			}

		}
	}
	//HCF Fix the name of the selected file!

  //HCF Aqui se sobreescribe la configuracion por defecto
  //HCF con la que eligio el usuario = default config overwritten here with user preferences
  ////frameskip = nFrameskip;
  /*
  if(iAutoFrameskip == FRAMESKIP_AUTO_EVEN)
      frameskip = 0;
  else if(iAutoFrameskip == FRAMESKIP_AUTO_ODD)
	  frameskip = 1;
  else //Auto_both or Fixed
  */
  frameskip = nFrameskip;


  enable_sound = iEnableSound;

  //HCF Adjusting sound mode (SYNC)
  if(iSoundMode != SOUND_MODE_ASYNC)
	SPU_SetSynchMode(1, CommonSettings.SPU_sync_method);
  else
	SPU_SetSynchMode(0, CommonSettings.SPU_sync_method);

  //HCF Adjusting sound mode (INTERPOLATION)
  if(iSoundMode == SOUND_MODE_SYNC_INTERPOLATED)
	  CommonSettings.spuInterpolationMode = 1;
  else
	  CommonSettings.spuInterpolationMode = 0;


  vdDejaLog("LOAD ROM");

    //LOGS
    memset(CAPIMBE,0x00, 128);
    sprintf(CAPIMBE, "%d", iGetFreeMemory());
    vdDejaLog(CAPIMBE);
    //LOGS

	//Alloc the Volatile memory
	/*
	if( !inVolatileAssigned )
	{
		int HCF_RAM_SIZE;  // = 3670016; //3,5 MB de momento
		int reta = sceKernelVolatileMemLock(0,	&HCF_RAM_ARRAY, &HCF_RAM_SIZE);
		MMU.MAIN_MEM = (u8*)HCF_RAM_ARRAY;

		memset(HCF_RAM_ARRAY, 0, HCF_RAM_SIZE);
		//memset(&(MMU.MAIN_MEM), 0, 4*1024*1024);
		inVolatileAssigned = 1;
	}
	*/


  error = NDS_LoadROM( rom_filename ) ;   //my_config.nds_file.c_str() );
  if (error < 0) {
      vdDejaLog("ERROR LEYENDO ROM:");
      vdDejaLog(rom_filename);
    //fprintf(stderr, "error while loading %s\n", my_config.nds_file.c_str());
    exit(-1);
  }

  execute = true;

  vdDejaLog("INIT SDL");

  //LOGS
    memset(CAPIMBE,0x00, 128);
    sprintf(CAPIMBE, "%d", iGetFreeMemory());
    vdDejaLog(CAPIMBE);
  //LOGS

  if(SDL_Init(SDL_INIT_TIMER | SDL_INIT_VIDEO) == -1)
    {
      fprintf(stderr, "Error trying to initialize SDL: %s\n",
              SDL_GetError());
      return 1;
    }
  //SDL_WM_SetCaption("Desmume SDL", NULL);

  vdDejaLog("SET VIDEO MODE");

    //HCF PSP
    //SDLscreen = SDL_SetVideoMode(480, 272, 16, SDL_HWPALETTE | SDL_RESIZABLE); // hardware code commented out?
	SDLscreen = SDL_SetVideoMode(480, 272, 16, SDL_SWSURFACE | SDL_FULLSCREEN);


  vdDejaLog("FONT");

    TTF_Init();
    font = TTF_OpenFont( "OpenSans-Regular.ttf", 12 );

    vdDejaLog("CREATE SURFACES 1 y 2");

    //REVISAR FLAGS!!!
	/*
    surface1 = SDL_CreateRGBSurface(SDL_SWSURFACE, 256, 192, 16, 0x001F, 0x03E0, 0x7C00, 0);
    surface2 = SDL_CreateRGBSurface(SDL_SWSURFACE, 256, 192, 16, 0x001F, 0x03E0, 0x7C00, 0);
	*/

    //BLUE 0b111110000000000
    //GREEN 0b1111100000
    //RED 0b11111
    surface2 = SDL_CreateRGBSurface(SDL_SWSURFACE, 256, 192, 16, 0b11110, 0b1111100000, 0b111110000000000, 0);
    surface1 = SDL_CreateRGBSurface(SDL_SWSURFACE, 256, 192, 16, 0b11110, 0b1111100000, 0b111110000000000, 0);


    vdDejaLog("CREADAS SURFACES 1 y 2");
    //LOGS
    memset(CAPIMBE,0x00, 128);
    sprintf(CAPIMBE, "%d", iGetFreeMemory());
    vdDejaLog(CAPIMBE);
    //LOGS


  /* Fetch the video info */
  /*
	videoInfo = SDL_GetVideoInfo( );
  if ( !videoInfo ) {
    //fprintf( stderr, "Video query failed: %s\n", SDL_GetError( ) );
    exit( -1);
  }
  */

 /* This checks if hardware blits can be done */
  /*
	if ( videoInfo->blit_hw )
    sdl_videoFlags |= SDL_HWACCEL;

    sdl_videoFlags |= SDL_SWSURFACE;
    surface = SDL_SetVideoMode(256, 384, 32, sdl_videoFlags);

    if ( !surface ) {
      fprintf( stderr, "Video mode set failed: %s\n", SDL_GetError( ) );
      exit( -1);
    }
	*/

    vdDejaLog("INIT JOY");


  /* Initialize joysticks */
  if(!init_joy()) return 1;
  /* Load keyboard and joystick configuration */
  //keyfile = desmume_config_read_file(cli_kb_cfg);
  //desmume_config_dispose(keyfile);
  /* Since gtk has a different mapping the keys stop to work with the saved configuration :| */

  load_default_config(cli_kb_cfg);

  //HCF Savestates
  /*
  if(my_config.load_slot != -1){
    loadstate_slot(my_config.load_slot);
  }
  */


#ifdef HAVE_LIBAGG
  Desmume_InitOnce();
  Hud.reset();
  // Now that gtk port draws to RGBA buffer directly, the other one
  // has to use ugly ways to make HUD rendering work again.
  // desmume gtk: Sorry desmume-cli :(
  aggDraw.hud = &agg_targetScreen_cli;
  aggDraw.hud->setFont("verdana18_bold");
#endif

  //HCF De momento NO se hace Delay, pero DEBE HACERSE
  //HCF OJO OJO OJO
  ctrls_cfg.boost = 1;    //0;
  ctrls_cfg.sdl_quit = 0;
  ctrls_cfg.auto_pause = my_config.auto_pause;
  ctrls_cfg.focused = 1;
  ctrls_cfg.fake_mic = 0;
  ctrls_cfg.keypad = 0;
  ctrls_cfg.screen_texture = NULL;
  ctrls_cfg.resize_cb = &resizeWindow_stub;


  //Para imprimir frameskip y FPS
  framessegundo = 0;
  totalframessegundo = 0;
  tsegundo1 = 0;
  //Para imprimir frameskip y FPS

  vdDejaLog("A BUCLE PPAL");

  int profileSamples = 0;

  //while(!ctrls_cfg.sdl_quit) {
  while(1)
  {
	  vdDejaLog("EN BUCLE PPAL");

	  t1 = SDL_GetTicks();

	  	//Para imprimir FPS
		if(t1 - tsegundo1 > 1000)
		{
			//Ha pasado un segundo
			tsegundo1 = t1;
			totalframessegundo = framessegundo;
			framessegundo = 0;
		}
		desmume_cycle(&ctrls_cfg);
		framessegundo++;

		osd->update();

//vdDejaLog("DRAW HUD");


		DrawHUD();

//vdDejaLog("GU DRAW");


        //HCF PSP

        Gu_draw();

//vdDejaLog("OSD CLEAR");

	osd->clear();

		//vdDejaLog("EJEC");

    if (profileSamples % 100 == 0)
    {
        pf.outputStats();
    }
    profileSamples++;

		//for ( int i = 0; i < my_config.frameskip; i++ ) {
		for ( int i = 0; i < frameskip; i++ ) {

			//vdDejaLog("A SKIP");

			NDS_SkipNextFrame();

			//vdDejaLog("A CYCLE");

			desmume_cycle(&ctrls_cfg);

			//vdDejaLog("A FPS++");


			framessegundo++;
		}

		//vdDejaLog("A SEGUNDO TIMING");


		/*
	#ifdef DISPLAY_FPS
		now = SDL_GetTicks();
	#endif
		*/
		if( iAutoFrameskip > 0 )
		{
			t2 = SDL_GetTicks();
			t3 = t2 - t1;

			//if( (t3 > ((frameskip+1) * MAX_EXPECTED_FRAME_TIME)) && (frameskip < nFrameskip) )
			if( (t3 > ((frameskip+1) * MAX_EXPECTED_FRAME_TIME))  )
			{
				if( iAutoFrameskip == FRAMESKIP_AUTO_BOTH )
				{
					if(frameskip < nFrameskip)
						frameskip++;
				}
				else
				{
					if(frameskip + 2 <= nFrameskip)
						frameskip += 2;
				}
			}
			//else if( (t3 < ((frameskip+1) * MIN_EXPECTED_FRAME_TIME)) && (frameskip > 0) )
			else if( (t3 < ((frameskip+1) * MIN_EXPECTED_FRAME_TIME)) )
			{
				if( iAutoFrameskip == FRAMESKIP_AUTO_BOTH )
				{
					if(frameskip > 0)
						frameskip--;
				}
				else
				{
					if(frameskip - 2 >= 0)
						frameskip -= 2;
				}
			}
		}

		if( iLimitFramerate == LIMIT_FRAMERATE_HCF)
        {
            t2 = SDL_GetTicks();
			t3 = t2 - t1;

			 if( t3 < JUST_EXPECTED_FRAME_TIME )
				SDL_Delay(JUST_EXPECTED_FRAME_TIME - t3);
            /*
			if( t3 < ( (frameskip + 1) * JUST_EXPECTED_FRAME_TIME )  )
                SDL_Delay(( (frameskip + 1) * JUST_EXPECTED_FRAME_TIME ) - t3);
			*/
        }
		else if (iLimitFramerate == LIMIT_FRAMERATE_DESMUME)
		{
			//if ( !my_config.disable_limiter && !ctrls_cfg.boost) {
			/*
		#ifndef DISPLAY_FPS
			now = SDL_GetTicks();
		#endif
			*/
			//HCF Temporal
			now = SDL_GetTicks();

			int delay =  (limiter_tick0 + limiter_frame_counter*1000/FPS_LIMITER_FPS) - now;
			if (delay < -500 || delay > 100)
			{
				// reset if we fall too far behind don't want to run super fast until we catch up
				limiter_tick0 = now;
				limiter_frame_counter = 0;
			}
			else if (delay > 0)
			{
				SDL_Delay(delay);
			}
		}
		// always count frames, we'll mess up if the limiter gets turned on later otherwise
		//limiter_frame_counter += 1 + my_config.frameskip;
		limiter_frame_counter += 1 + frameskip;




#ifdef DISPLAY_FPS
    fps_frame_counter += 1;
    fps_timing += now - fps_previous_time;
    fps_previous_time = now;

    if ( fps_frame_counter == NUM_FRAMES_TO_TIME) {
      char win_title[20];
      float fps = NUM_FRAMES_TO_TIME * 1000.f / fps_timing;

      fps_frame_counter = 0;
      fps_timing = 0;

      //snprintf( win_title, sizeof(win_title), "Desmume %f", fps);

      //SDL_WM_SetCaption( win_title, NULL);
    }
#endif
  }

  vdDejaLog("SALE DE BUCLE PPAL");
  pf.outputStats();
  /* Unload joystick */
  uninit_joy();

  SDL_Quit();
  NDS_DeInit();


  return 0;
}
