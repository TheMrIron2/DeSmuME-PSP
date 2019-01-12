#Cambiar TARGET, OBJS y PSP_EBOOT_TITLE
TARGET = DesmumePSP

MESS=1
LSB_FIRST=1

	
OBJS = arm_instructions.o armcpu.o bios.o common.o commandline.o cp15.o debug.o \
driver.o  encrypt.o FIFO.o firmware.o gfx3d.o GPU.o GPU_osd_stub.o \
matrix.o mc.o mic.o MMU.o NDSSystem.o path.o  readwrite.o \
render3D.o ROMReader.o rtc.o slot1.o slot2.o SPU.o texcache.o thumb_instructions.o \
 wifi.o utils/advanscene.o utils/xstring.o utils/tinyxml/tinystr.o utils/tinyxml/tinyxml.o utils/tinyxml/tinyxmlerror.o utils/tinyxml/tinyxmlparser.o  \
utils/decrypt/crc.o utils/decrypt/decrypt.o utils/decrypt/header.o \
metaspu/metaspu.o metaspu/SndOut.o metaspu/Timestretcher.o emufile.o rasterize.o \
metaspu/SoundTouch/SoundTouch.o metaspu/SoundTouch/FIFOSampleBuffer.o metaspu/SoundTouch/RateTransposer.o \
metaspu/SoundTouch/TDStretch.o metaspu/SoundTouch/FIRFilter.o metaspu/SoundTouch/AAFilter.o \
addons/slot1_none.o addons/slot1_r4.o addons/slot1_retail_auto.o addons/slot1_retail_mcrom.o \
addons/slot1_retail_nand.o addons/slot1comp_mc.o addons/slot1comp_protocol.o addons/slot1comp_rom.o \
addons/slot2_auto.o addons/slot2_expMemory.o  \
addons/slot2_none.o addons/slot2_paddle.o addons/slot2_passme.o \
addons/slot2_piano.o addons/slot2_rumblepak.o addons/slot2_gbagame.o addons/slot2_guitarGrip.o \
sndsdl.o ctrlssdl.o main.o


CC=psp-g++
CXX=psp-g++

#ESTO NO HACE FALTA: SOLO RENOMBRAR A CPP!!!
#ESTO NO HACE FALTA: SOLO RENOMBRAR A CPP!!!
GCC/%.o: %.cpp
			$(CXX) $< -o $@

mess/drivers/%.o: %.c
			$(CXX) $< -o $@

emu/drivers/%.o: %.c
			$(CXX) $< -o $@

emu/machine/%.o: %.c
			$(CXX) $< -o $@

emu/%.o: %.c
			$(CXX) $< -o $@
            
#ESTO NO HACE FALTA: SOLO RENOMBRAR A CPP!!!
#ESTO NO HACE FALTA: SOLO RENOMBRAR A CPP!!!
           
PSPSDK = $(shell psp-config --pspsdk-path)
PSPDEV = $(shell psp-config -d)
PSPBIN = $(PSPDEV)/bin
SDL_CONFIG = $(PSPBIN)/sdl-config


###CFLAGS = -O2 -fweb -fomit-frame-pointer -G0 -Wall -D PSP
CFLAGS = -Os -ffast-math -fweb -fomit-frame-pointer  -G0 -Wall -D PSP


CFLAGS += $(shell $(SDL_CONFIG) --cflags)

CXXFLAGS = $(CFLAGS) -fno-exceptions

ASFLAGS = $(CFLAGS)

LIBS= -lpthread-psp -lSDLmain  -lSMPEG -lSDL -lSDL_ttf -lSDL_image -lpspirkeyb -lpspwlan -lpsppower -lGL -lfreetype -ljpeg -lpng -lz -lm -lSDL -lpspgu -lpsphprm -lpspaudio -lstdc++ -lpspvfpu -lpsprtc -lSMPEG -lvorbisidec $( shell $( SDL_CONFIG ) --libs )
LIBS += -lpspaudiolib -lpspaudio -lpsppower 
LIBS += -lSDL_Mixer -lSMPEG -lSDL -lSDL_ttf -lvorbisidec -logg -lstdc++ 

EXTRA_TARGETS = EBOOT.PBP
PSP_EBOOT_TITLE = DesmumePSP

include $(PSPSDK)/lib/build.mak

PSP_EBOOT_ICON = ICON0.png
#PSP_EBOOT_PIC1 = PIC1.png
#PSP_EBOOT_SND0 = SND0.at3




    