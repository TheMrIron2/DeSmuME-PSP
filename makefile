TARGET = DsOnPsp
OBJS =\
	./desmume_core/arm_instructions.o \
	./desmume_core/armcpu.o \
	./desmume_core/bios.o \
	./desmume_core/cflash.o \
	./desmume_core/cp15.o \
	./desmume_core/ctrlssdl.o \
	./desmume_core/debug.o \
	./desmume_core/Disassembler.o \
	./desmume_core/decrypt.o \
	./desmume_core/FIFO.o \
	./desmume_core/fs-psp.o \
	./desmume_core/gfx3d.o \
	./desmume_core/GPU.o \
	./desmume_core/matrix.o \
	./desmume_core/mc.o \
	./desmume_core/MMU.o \
	./desmume_core/NDSSystem.o \
	./desmume_core/render3D.o \
	./desmume_core/rtc.o \
	./desmume_core/ROMReader.o \
	./desmume_core/saves.o \
	./desmume_core/sndsdl.o \
	./desmume_core/SPU.o \
	./desmume_core/thumb_instructions.o \
	./desmume_core/wifi.o \
	./PSP/Gudraw.o \
	./PSP/vram.o \
	./PSP/FrontEnd.o \
	./PSP/main.o


#./desmume_core/matrix.o \


ASM      = psp-as
CC       = psp-gcc

PSP_FW_VERSION = 200

INCDIR = ./SDL/include ./PSP ./desmume_core
CFLAGS = -O3 -G0 -Wall


CXXFLAGS = $(CFLAGS) -fno-exceptions -fno-rtti 
#psp-as ./desmume_core/matrix_psp_asm.s -o
ASFLAGS = 
#$(ASM) ./desmume_core/matrix.s -o ./desmume_core/matrix.o \

LIBDIR = ./SDL
LDFLAGS =
LIBS = -lpsppower -lpsppower -lpspwlan -lSDL -lGL -lGLU -glut -lpspvfpu -lpspgum -lpspgu -lpspge -lpspaudio -lpsprtc -lm

EXTRA_TARGETS = EBOOT.PBP
PSP_EBOOT_TITLE = DSONPSP V0.6 by Yoshihiro (kayliah Edition)
PSP_EBOOT_ICON  = ./icon/icon0.png
PSP_EBOOT_PIC1 = ./icon/pic1.png




PSPSDK=$(shell psp-config --pspsdk-path)
include $(PSPSDK)/lib/build.mak
