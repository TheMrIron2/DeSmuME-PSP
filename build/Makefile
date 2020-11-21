TARGET = DesmumePSP

MESS=1
LSB_FIRST=1
PSP_LARGE_MEMORY = 1
SRCDIR = ../source

OBJS = $(SRCDIR)/arm_instructions.o \
$(SRCDIR)/arm_jit.o \
$(SRCDIR)/armcpu.o \
$(SRCDIR)/bios.o \
$(SRCDIR)/common.o \
$(SRCDIR)/cp15.o \
$(SRCDIR)/debug.o \
$(SRCDIR)/driver.o \
$(SRCDIR)/encrypt.o \
$(SRCDIR)/FIFO.o \
$(SRCDIR)/firmware.o \
$(SRCDIR)/gfx3d.o \
$(SRCDIR)/GPU.o \
$(SRCDIR)/matrix.o \
$(SRCDIR)/mc.o \
$(SRCDIR)/mic.o \
$(SRCDIR)/MMU.o \
$(SRCDIR)/NDSSystem.o \
$(SRCDIR)/path.o \
$(SRCDIR)/readwrite.o \
$(SRCDIR)/render3D.o \
$(SRCDIR)/ROMReader.o \
$(SRCDIR)/rtc.o \
$(SRCDIR)/saves.o \
$(SRCDIR)/slot1.o \
$(SRCDIR)/slot2.o \
$(SRCDIR)/SPU.o \
$(SRCDIR)/texcache.o \
$(SRCDIR)/thumb_instructions.o \
$(SRCDIR)/wifi.o \
$(SRCDIR)/utils/advanscene.o \
$(SRCDIR)/utils/xstring.o \
$(SRCDIR)/utils/tinyxml/tinystr.o \
$(SRCDIR)/utils/tinyxml/tinyxml.o \
$(SRCDIR)/utils/tinyxml/tinyxmlerror.o \
$(SRCDIR)/utils/tinyxml/tinyxmlparser.o  \
$(SRCDIR)/utils/decrypt/crc.o \
$(SRCDIR)/utils/decrypt/decrypt.o \
$(SRCDIR)/utils/decrypt/header.o \
$(SRCDIR)/metaspu/metaspu.o \
$(SRCDIR)/metaspu/SndOut.o \
$(SRCDIR)/metaspu/Timestretcher.o \
$(SRCDIR)/emufile.o \
$(SRCDIR)/rasterize.o \
$(SRCDIR)/metaspu/SoundTouch/SoundTouch.o \
$(SRCDIR)/metaspu/SoundTouch/FIFOSampleBuffer.o \
$(SRCDIR)/metaspu/SoundTouch/RateTransposer.o \
$(SRCDIR)/metaspu/SoundTouch/TDStretch.o \
$(SRCDIR)/metaspu/SoundTouch/FIRFilter.o \
$(SRCDIR)/metaspu/SoundTouch/AAFilter.o \
$(SRCDIR)/addons/slot1_none.o \
$(SRCDIR)/addons/slot1_r4.o \
$(SRCDIR)/addons/slot1_retail_auto.o \
$(SRCDIR)/addons/slot1_retail_mcrom.o \
$(SRCDIR)/addons/slot1_retail_nand.o \
$(SRCDIR)/addons/slot1comp_mc.o \
$(SRCDIR)/addons/slot1comp_protocol.o \
$(SRCDIR)/addons/slot1comp_rom.o \
$(SRCDIR)/addons/slot2_auto.o \
$(SRCDIR)/addons/slot2_expMemory.o  \
$(SRCDIR)/addons/slot2_none.o \
$(SRCDIR)/addons/slot2_paddle.o \
$(SRCDIR)/addons/slot2_passme.o \
$(SRCDIR)/addons/slot2_piano.o \
$(SRCDIR)/addons/slot2_rumblepak.o \
$(SRCDIR)/addons/slot2_gbagame.o \
$(SRCDIR)/addons/slot2_guitarGrip.o \
$(SRCDIR)/PSP/FrontEnd.o \
$(SRCDIR)/PSP/PSPDisplay.o \
$(SRCDIR)/PSP/vram.o \
$(SRCDIR)/PSP/psp_sound.o \
$(SRCDIR)/PSP/FastMemcpyPSP.o \
$(SRCDIR)/PSP/DisableFPUExceptions.o \
$(SRCDIR)/sndsdl.o \
$(SRCDIR)/sndpsp.o \
$(SRCDIR)/ctrlssdl.o \
$(SRCDIR)/main.o

CC=psp-g++
CXX=psp-g++


PSPSDK = $(shell psp-config --pspsdk-path)
PSPDEV = $(shell psp-config -d)

CFLAGS = -O2 -finline-functions -fno-rtti -ffast-math -fweb -fomit-frame-pointer  -G0 -Wall -D PSP -D NOSSE2 -D HAVE_LIBZ -funsigned-char -Wall \
                  -fstrict-aliasing -falign-functions=32 \
                  -falign-loops -falign-labels -falign-jumps -faligned-new


CXXFLAGS = $(CFLAGS) -fno-exceptions

ASFLAGS = $(CFLAGS) 

INCDIR =  ./libs/include

LIBDIR = ./libs/lib
LIBS  =  -lme -lintrafont -lpspdmac  -lpspmath -lpthread-psp -lpsppower -lglut -lpspgum  -lfreetype -lpng -lz  -lm -lpspgu -lpsphprm -lpspaudio -lstdc++ -lpspvfpu -lpsprtc -lvorbisidec
LIBS += -lpspaudiolib -lpspvram  -lpspsdk

EXTRA_TARGETS = EBOOT.PBP
PSP_EBOOT_TITLE = Desmume

include $(PSPSDK)/lib/build.mak

PSP_EBOOT_ICON = ICON0.png
#PSP_EBOOT_PIC1 = PIC1.png
#PSP_EBOOT_SND0 = SND0.at3
