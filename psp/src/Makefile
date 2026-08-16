TARGET = operazione_ponte_spezzato
OBJS = main.o game.o render.o audio.o
BUILD_PRX = 1
EXTRA_TARGETS = EBOOT.PBP
PSP_EBOOT_TITLE = Operazione Ponte Spezzato
PSPSDK = $(shell psp-config --pspsdk-path)
LIBS = -lpspgu -lpspaudio -lpspctrl -lpspdisplay -lpspdebug -lm
include $(PSPSDK)/lib/build.mak
