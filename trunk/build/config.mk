CC = g++
OS = ubuntu
IDIR = ../include
IDIR_LNX = ../include/linux
IDIR_EXT = ../../external/include

SRC_DIR = ../src
LNX_SRC_DIR = ../src/linux

LDIR = ../../external/lib/release/x64/$(OS)
OUT = ../out
ODIR = $(OUT)/release/x64/obj
BDIR = $(OUT)/release/x64

BINARY = ipop-tincan
TARGET = $(patsubst %,$(BDIR)/%,$(BINARY))

defines = -DLINUX -D_IPOP_LINUX -DWEBRTC_POSIX -DWEBRTC_LINUX

cflags_cc = -std=c++14 -O3 -m64 -march=x86-64 -pthread -g2 -gsplit-dwarf -fno-strict-aliasing --param=ssp-buffer-size=4 -fstack-protector -funwind-tables -fPIC -pipe -Wall -fno-rtti

LIBS = -ljsoncpp -lrtc_p2p -lrtc_base -lrtc_base_approved -lfield_trial_default -lboringssl -lboringssl_asm -lprotobuf_lite -lpthread

HDR_FILES = $(wildcard $(IDIR)/*.h)
SRC_FILES = $(wildcard $(SRC_DIR)/*.cc)
OBJ_FILES = $(patsubst $(SRC_DIR)/%.cc, $(ODIR)/%.o, $(SRC_FILES))

LSRC_FILES = $(wildcard $(LNX_SRC_DIR)/*.cc)
LOBJ_FILES = $(patsubst $(LNX_SRC_DIR)/%.cc, $(ODIR)/%.o, $(LSRC_FILES))
