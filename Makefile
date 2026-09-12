# Package metadata.
TITLE       := GoldHEN Plugin Manager
VERSION     := 01.10
TITLE_ID    := PLGM00001
CONTENT_ID  := IV0000-PLGM00001_00-GOLDHENPLUGINMGR

# Libraries linked into the ELF.
LIBS        := -lc -lkernel -lSceUserService -lScePigletv2VSH -lSceSysmodule -lSceFreeType \
               -lScePad -lSceSystemService -lSceCommonDialog -lSceMsgDialog -lSceAudioOut -lSceVideoOut \
               -lSDL2 -ldbglogger -ljbc -lSQLite

# Host compiler and binary for `make test`.
HOSTCC      ?= cc
TESTBIN     := build/test_ini_model
TESTBIN_GP  := build/test_gamepad_model

# Additional compile flags.
EXTRAFLAGS  := -fcolor-diagnostics -Wall -D__PS4__

ifeq ($(DEBUGLOG),1)
    EXTRAFLAGS += -DDEBUG_ENABLE_LOG
    ifneq ($(DEBUGLOG_DEST),)
        EXTRAFLAGS += -DDEBUG_LOG_DEST=\"$(DEBUGLOG_DEST)\"
    endif
endif

# Asset and module directories.
ASSETS 		:= $(wildcard assets/**/*)
LIBMODULES  := $(wildcard sce_module/*)

# You likely won't need to touch anything below this point.

# Root vars
TOOLCHAIN   := $(OO_PS4_TOOLCHAIN)
PROJDIR     := source
#COMMONDIR   := $(TOOLCHAIN)/samples/_common
INTDIR      := build/x64/Debug

# Define objects to build
CFILES      := $(wildcard $(PROJDIR)/*.c)
CPPFILES    := $(wildcard $(PROJDIR)/*.cpp)
#COMMONFILES := $(wildcard $(COMMONDIR)/*.cpp)
OBJS        := $(patsubst $(PROJDIR)/%.c, $(INTDIR)/%.o, $(CFILES)) $(patsubst $(PROJDIR)/%.cpp, $(INTDIR)/%.o, $(CPPFILES))

# Define final C/C++ flags
CFLAGS      := --target=x86_64-pc-freebsd12-elf -fPIC -funwind-tables -c $(EXTRAFLAGS) -isysroot $(TOOLCHAIN) -isystem $(TOOLCHAIN)/include -Iinclude
CXXFLAGS    := $(CFLAGS) -isystem $(TOOLCHAIN)/include/c++/v1
LDFLAGS     := -m elf_x86_64 -pie --script $(TOOLCHAIN)/link.x --eh-frame-hdr -L$(TOOLCHAIN)/lib $(LIBS) $(TOOLCHAIN)/lib/crt1.o

# Create the intermediate directory incase it doesn't already exist.
_unused     := $(shell mkdir -p $(INTDIR))

# Check for linux vs macOS and account for clang/ld path
UNAME_S     := $(shell uname -s)

ifeq ($(UNAME_S),Linux)
		CC      := clang
		CCX     := clang++
		LD      := ld.lld
		CDIR    := linux
endif
ifeq ($(UNAME_S),Darwin)
		CC      := /usr/local/opt/llvm/bin/clang
		CCX     := /usr/local/opt/llvm/bin/clang++
		LD      := /usr/local/opt/llvm/bin/ld.lld
		CDIR    := macos
endif

.PHONY: all clean test

# PkgTool.Core is a .NET Core 3.0 binary: it needs invariant globalization (no ICU)
# and OpenSSL 1.1. On distros that only ship OpenSSL 3, point PKGTOOL_LIBSSL_DIR at a
# directory containing libssl.so.1.1 / libcrypto.so.1.1 (e.g. extracted from the
# Ubuntu focal libssl1.1 .deb).
PKGTOOL_ENV := DOTNET_SYSTEM_GLOBALIZATION_INVARIANT=1
ifneq ($(PKGTOOL_LIBSSL_DIR),)
    PKGTOOL_ENV += LD_LIBRARY_PATH=$(PKGTOOL_LIBSSL_DIR)
endif
PKGTOOL     := $(PKGTOOL_ENV) $(TOOLCHAIN)/bin/$(CDIR)/PkgTool.Core

all: $(CONTENT_ID).pkg

$(CONTENT_ID).pkg: pkg.gp4
	$(PKGTOOL) pkg_build $< .

pkg.gp4: eboot.bin sce_sys/about/right.sprx sce_sys/param.sfo sce_sys/icon0.png sce_sys/save_data.png $(LIBMODULES) $(ASSETS)
	$(TOOLCHAIN)/bin/$(CDIR)/create-gp4 -out $@ --content-id=$(CONTENT_ID) --files "$^"

sce_sys/param.sfo: Makefile
	$(PKGTOOL) sfo_new $@
	$(PKGTOOL) sfo_setentry $@ APP_TYPE --type Integer --maxsize 4 --value 1 
	$(PKGTOOL) sfo_setentry $@ APP_VER --type Utf8 --maxsize 8 --value '$(VERSION)'
	$(PKGTOOL) sfo_setentry $@ ATTRIBUTE --type Integer --maxsize 4 --value 32  
	$(PKGTOOL) sfo_setentry $@ CATEGORY --type Utf8 --maxsize 4 --value 'gde'  
	$(PKGTOOL) sfo_setentry $@ CONTENT_ID --type Utf8 --maxsize 48 --value '$(CONTENT_ID)'
	$(PKGTOOL) sfo_setentry $@ DOWNLOAD_DATA_SIZE --type Integer --maxsize 4 --value 0 
	$(PKGTOOL) sfo_setentry $@ SYSTEM_VER --type Integer --maxsize 4 --value 0  
	$(PKGTOOL) sfo_setentry $@ TITLE --type Utf8 --maxsize 128 --value '$(TITLE)'
	$(PKGTOOL) sfo_setentry $@ TITLE_ID --type Utf8 --maxsize 12 --value '$(TITLE_ID)'
	$(PKGTOOL) sfo_setentry $@ VERSION --type Utf8 --maxsize 8 --value '$(VERSION)'

eboot.bin: $(INTDIR) $(OBJS)
	$(LD) $(INTDIR)/*.o -o $(INTDIR)/$(PROJDIR).elf $(LDFLAGS)
	$(TOOLCHAIN)/bin/$(CDIR)/create-fself -in=$(INTDIR)/$(PROJDIR).elf -out=$(INTDIR)/$(PROJDIR).oelf --eboot "eboot.bin" --paid 0x3800000000000011 --authinfo 000000000000000000000000001C004000FF000000000080000000000000000000000000000000000000008000400040000000000000008000000000000000080040FFFF000000F000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000

$(INTDIR)/%.o: $(PROJDIR)/%.c
	$(CC) $(CFLAGS) -o $@ $<

$(INTDIR)/%.o: $(PROJDIR)/%.cpp
	$(CCX) $(CXXFLAGS) -o $@ $<

clean:
	rm -f $(CONTENT_ID).pkg pkg.gp4 sce_sys/param.sfo eboot.bin \
		$(INTDIR)/$(PROJDIR).elf $(INTDIR)/$(PROJDIR).oelf $(OBJS) $(TESTBIN) $(TESTBIN_GP)

#---------------------------------------------------------------------------------
# Host-side unit tests for the pure ini/json model (no PS4 toolchain required).
test:
	@mkdir -p build
	$(HOSTCC) -std=c99 -Wall -Wextra -Werror -pedantic -Iinclude -o $(TESTBIN) source/ini_model.c tests/test_ini_model.c
	$(HOSTCC) -std=c99 -Wall -Wextra -Werror -pedantic -Iinclude -o $(TESTBIN_GP) source/gamepad_model.c tests/test_gamepad_model.c
	./$(TESTBIN)
	./$(TESTBIN_GP)
