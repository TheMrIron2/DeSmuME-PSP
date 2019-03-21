#---------------------------------------------------------------------------------
# Build properties
#---------------------------------------------------------------------------------
TARGET			:=	$(notdir $(CURDIR))
EXTRA_TARGETS 	:= 	EBOOT.PBP
OBJS 			:= 	$(CURDIR)/desmume_core/arm_instructions.o \
					$(CURDIR)/desmume_core/armcpu.o \
					$(CURDIR)/desmume_core/bios.o \
					$(CURDIR)/desmume_core/cflash.o \
					$(CURDIR)/desmume_core/cp15.o \
					$(CURDIR)/desmume_core/ctrlssdl.o \
					$(CURDIR)/desmume_core/debug.o \
					$(CURDIR)/desmume_core/Disassembler.o \
					$(CURDIR)/desmume_core/decrypt.o \
					$(CURDIR)/desmume_core/FIFO.o \
					$(CURDIR)/desmume_core/fs-psp.o \
					$(CURDIR)/desmume_core/gfx3d.o \
					$(CURDIR)/desmume_core/GPU.o \
					$(CURDIR)/desmume_core/matrix.o \
					$(CURDIR)/desmume_core/mc.o \
					$(CURDIR)/desmume_core/MMU.o \
					$(CURDIR)/desmume_core/NDSSystem.o \
					$(CURDIR)/desmume_core/render3D.o \
					$(CURDIR)/desmume_core/rtc.o \
					$(CURDIR)/desmume_core/ROMReader.o \
					$(CURDIR)/desmume_core/saves.o \
					$(CURDIR)/desmume_core/sndsdl.o \
					$(CURDIR)/desmume_core/SPU.o \
					$(CURDIR)/desmume_core/thumb_instructions.o \
					$(CURDIR)/desmume_core/wifi.o \
					$(CURDIR)/PSP/callbacks.o \
					$(CURDIR)/PSP/Gudraw.o \
					$(CURDIR)/PSP/vram.o \
					$(CURDIR)/PSP/FrontEnd.o \
					$(CURDIR)/PSP/main.o

#---------------------------------------------------------------------------------
# Metadata properties
#---------------------------------------------------------------------------------
PSP_EBOOT_TITLE := 	DSonPSP v0.8
PSP_EBOOT_ICON	:=	$(CURDIR)/icon/ICON0.PNG
PSP_EBOOT_PIC1	:=	$(CURDIR)/icon/PIC1.PNG

#---------------------------------------------------------------------------------
# Compiler properties
#---------------------------------------------------------------------------------
PSPSDK		=	$(shell psp-config --pspsdk-path)
ASM      	= 	psp-as
ASFLAGS 	=	
CC       	= 	psp-gcc
CFLAGS 		+= 	-O3 -G0 -ffast-math -Wall
CXXFLAGS 	= 	$(CFLAGS) -fno-exceptions -fno-rtti
INCDIR 		=	$(PSPDEV)/psp/include/SDL/ $(CURDIR)/PSP $(CURDIR)/desmume_core
LIBDIR = $(PSPDEV)/psp/include/SDL
LDFLAGS =
LIBS = -lSDL -lGL -lGLU -glut -lpspgum -lpspgu -lpspge -lpspaudio -lm  -lpspirkeyb -lpsppower -lpspwlan -lpspvfpu -lpsprtc -lpsphprm

include $(PSPSDK)/lib/build.mak