/*
	Copyright (C) 2009-2013 DeSmuME team

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

//windows note: make sure this file gets compiled with _cdecl

//#include <glib.h>
#include <algorithm>
#include <stdio.h>
#include "commandline.h"
#include "types.h"
//#include "movie.h"
#include "slot1.h"
#include "slot2.h"
#include "NDSSystem.h"
#include "utils/xstring.h"

int _scanline_filter_a = 0, _scanline_filter_b = 2, _scanline_filter_c = 2, _scanline_filter_d = 4;
int _commandline_linux_nojoy = 0;

CommandLine::CommandLine()
: is_cflash_configured(false)
, _load_to_memory(-1)
, error(NULL)
, ctx()   //g_option_context_new (""))
, _play_movie_file(0)
, _record_movie_file(0)
, _cflash_image(0)
, _cflash_path(0)
, _gbaslot_rom(0)
, _bios_arm9(NULL)
, _bios_arm7(NULL)
, _bios_swi(0)
, _spu_sync_mode(-1)
, _spu_sync_method(-1)
, _spu_advanced(0)
, _num_cores(-1)
, _rigorous_timing(0)
, _advanced_timing(-1)
, _slot1(NULL)
, _slot1_fat_dir(NULL)
, _slot1_fat_dir_type(false)
#ifdef HAVE_JIT
, _cpu_mode(-1)
, _jit_size(-1)
#endif
, _console_type(NULL)
, _advanscene_import(NULL)
, depth_threshold(-1)
, load_slot(-1)
, arm9_gdb_port(0)
, arm7_gdb_port(0)
, start_paused(FALSE)
, autodetect_method(-1)
{
#ifndef HOST_WINDOWS 
	disable_sound = 0;
	disable_limiter = 0;
#endif
}

CommandLine::~CommandLine()
{
	/*
	if(error) g_error_free (error);
	g_option_context_free (ctx);
	*/
}
/*
void CommandLine::loadCommonOptions()
{
	//these options should be available in every port.
	//my advice is, do not be afraid of using #ifdef here if it makes sense.
	//but also see the gtk port for an example of how to combine this with other options
	//(you may need to use ifdefs to cause options to be entered in the desired order)
	static const GOptionEntry options[] = {
		{ "load-type", 0, 0, G_OPTION_ARG_INT, &_load_to_memory, "ROM loading method, 0 - stream from disk (like an iso), 1 - load entirely to RAM (default 0)", "LOAD_TYPE"},
		{ "load-slot", 0, 0, G_OPTION_ARG_INT, &load_slot, "Loads savestate from slot NUM", "NUM"},
		{ "play-movie", 0, 0, G_OPTION_ARG_FILENAME, &_play_movie_file, "Specifies a dsm format movie to play", "PATH_TO_PLAY_MOVIE"},
		{ "record-movie", 0, 0, G_OPTION_ARG_FILENAME, &_record_movie_file, "Specifies a path to a new dsm format movie", "PATH_TO_RECORD_MOVIE"},
		{ "start-paused", 0, 0, G_OPTION_ARG_NONE, &start_paused, "Indicates that emulation should start paused", "START_PAUSED"},
		{ "cflash-image", 0, 0, G_OPTION_ARG_FILENAME, &_cflash_image, "Requests cflash in gbaslot with fat image at this path", "CFLASH_IMAGE"},
		{ "cflash-path", 0, 0, G_OPTION_ARG_FILENAME, &_cflash_path, "Requests cflash in gbaslot with filesystem rooted at this path", "CFLASH_PATH"},
		{ "gbaslot-rom", 0, 0, G_OPTION_ARG_FILENAME, &_gbaslot_rom, "Requests this GBA rom in gbaslot", "GBASLOT_ROM"},
		{ "bios-arm9", 0, 0, G_OPTION_ARG_FILENAME, &_bios_arm9, "Uses the arm9 bios provided at the specified path", "BIOS_ARM9_PATH"},
		{ "bios-arm7", 0, 0, G_OPTION_ARG_FILENAME, &_bios_arm7, "Uses the arm7 bios provided at the specified path", "BIOS_ARM7_PATH"},
		{ "bios-swi", 0, 0, G_OPTION_ARG_INT, &_bios_swi, "Uses SWI from the provided bios files", "BIOS_SWI"},
		{ "spu-mode", 0, 0, G_OPTION_ARG_INT, &_spu_sync_mode, "Select SPU Synchronization Mode. 0 - Dual SPU Synch/Asynch (traditional), 1 - Synchronous (sometimes needed for streams) (default 0)", "SPU_MODE"},
		{ "spu-method", 0, 0, G_OPTION_ARG_INT, &_spu_sync_method, "Select SPU Synchronizer Method. 0 - N, 1 - Z, 2 - P (default 0)", "SPU_SYNC_METHOD"},
		{ "spu-advanced", 0, 0, G_OPTION_ARG_INT, &_spu_advanced, "Uses advanced SPU capture functions", "SPU_ADVANCED"},
		{ "num-cores", 0, 0, G_OPTION_ARG_INT, &_num_cores, "Override numcores detection and use this many", "NUM_CORES"},
		{ "scanline-filter-a", 0, 0, G_OPTION_ARG_INT, &_scanline_filter_a, "Intensity of fadeout for scanlines filter (topleft) (default 0)", "SCANLINE_FILTER_A"},
		{ "scanline-filter-b", 0, 0, G_OPTION_ARG_INT, &_scanline_filter_b, "Intensity of fadeout for scanlines filter (topright) (default 2)", "SCANLINE_FILTER_B"},
		{ "scanline-filter-c", 0, 0, G_OPTION_ARG_INT, &_scanline_filter_c, "Intensity of fadeout for scanlines filter (bottomleft) (default 2)", "SCANLINE_FILTER_C"},
		{ "scanline-filter-d", 0, 0, G_OPTION_ARG_INT, &_scanline_filter_d, "Intensity of fadeout for scanlines filter (bottomright) (default 4)", "SCANLINE_FILTER_D"},
		{ "rigorous-timing", 0, 0, G_OPTION_ARG_INT, &_rigorous_timing, "Use some rigorous timings instead of unrealistically generous (default 0)", "RIGOROUS_TIMING"},
		{ "advanced-timing", 0, 0, G_OPTION_ARG_INT, &_advanced_timing, "Use advanced BUS-level timing (default 1)", "ADVANCED_TIMING"},
		{ "slot1", 0, 0, G_OPTION_ARG_STRING, &_slot1, "Device to mount in slot 1 (default retail)", "SLOT1"},
		{ "slot1-fat-dir", 0, 0, G_OPTION_ARG_STRING, &_slot1_fat_dir, "Directory to scan for slot 1", "SLOT1_DIR"},
		{ "depth-threshold", 0, 0, G_OPTION_ARG_INT, &depth_threshold, "Depth comparison threshold (default 0)", "DEPTHTHRESHOLD"},
		{ "console-type", 0, 0, G_OPTION_ARG_STRING, &_console_type, "Select console type: {fat,lite,ique,debug,dsi}", "CONSOLETYPE" },
		{ "advanscene-import", 0, 0, G_OPTION_ARG_STRING, &_advanscene_import, "Import advanscene, dump .ddb, and exit", "ADVANSCENE_IMPORT" },
#ifdef HAVE_JIT
		{ "cpu-mode", 0, 0, G_OPTION_ARG_INT, &_cpu_mode, "ARM CPU emulation mode: 0 - interpreter, 1 - dynarec (default 1)", NULL},
		{ "jit-size", 0, 0, G_OPTION_ARG_INT, &_jit_size, "ARM JIT block size: 1..100 (1 - accuracy, 100 - faster) (default 100)", NULL},
#endif
#ifndef HOST_WINDOWS 
		{ "disable-sound", 0, 0, G_OPTION_ARG_NONE, &disable_sound, "Disables the sound emulation", NULL},
		{ "disable-limiter", 0, 0, G_OPTION_ARG_NONE, &disable_limiter, "Disables the 60fps limiter", NULL},
		{ "nojoy", 0, 0, G_OPTION_ARG_INT, &_commandline_linux_nojoy, "Disables joystick support", "NOJOY"},
#endif
#ifdef GDB_STUB
		{ "arm9gdb", 0, 0, G_OPTION_ARG_INT, &arm9_gdb_port, "Enable the ARM9 GDB stub on the given port", "PORT_NUM"},
		{ "arm7gdb", 0, 0, G_OPTION_ARG_INT, &arm7_gdb_port, "Enable the ARM7 GDB stub on the given port", "PORT_NUM"},
#endif
		{ "autodetect_method", 0, 0, G_OPTION_ARG_INT, &autodetect_method, "Autodetect backup method (0 - internal, 1 - from database)", "AUTODETECT_METHOD"},
		{ NULL }
	};

	g_option_context_add_main_entries (ctx, options, "options");
}

static char mytoupper(char c) { return ::toupper(c); }

static std::string strtoupper(const std::string& str)
{
	std::string ret = str;
	std::transform(ret.begin(), ret.end(), ret.begin(), ::mytoupper);
	return ret;
}
*/