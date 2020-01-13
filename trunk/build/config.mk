CC = g++

DEBUG ?= 0
ifeq ($(DEBUG), 1)
    OPT = debug
    cflags_cc = -O0
else
	OPT = release
    cflags_cc = -O3
endif

ARCH = $(shell uname -m)

WOLF_DIR ?= /usr/local

INC_DIR = ../include
INC_DIR_LNX = $(INC_DIR)/linux
EXT_INC_DIR = -isystem ../../external/include
EXT_INC_DIR += -I $(WOLF_DIR)/include/wolfssl

SRC_DIR = ../src
SRC_DIR_LNX = $(SRC_DIR)/linux

EXT_LIB_DIR = -L ../../external/3rd-Party-Libs/$(OPT)
EXT_LIB_DIR += -L $(WOLF_DIR)/lib

OUT = ../out
OBJ_DIR = $(OUT)/$(OPT)/$(ARCH)/obj
BIN_DIR = $(OUT)/$(OPT)/$(ARCH)

BINARY = ipop-tincan
TARGET = $(patsubst %,$(BIN_DIR)/%,$(BINARY))

defines = -DLINUX -D_IPOP_LINUX -DWEBRTC_POSIX -DWEBRTC_LINUX -D_GLIBCXX_USE_CXX11_ABI=0

cflags_cc += -std=c++14 -pthread -g2 -gsplit-dwarf -fno-strict-aliasing --param=ssp-buffer-size=4 -fstack-protector -funwind-tables -fPIC -pipe -Wall -fno-rtti

LIBS = -ljsoncpp -lrtc_p2p -lrtc_base -lrtc_base_approved -lfield_trial_default -lwolfssl -lprotobuf_lite -lpthread -lnetlink -lutil

HDR_FILES = $(wildcard $(INC_DIR)/*.h)
SRC_FILES = $(wildcard $(SRC_DIR)/*.cc)
OBJ_FILES = $(patsubst $(SRC_DIR)/%.cc, $(OBJ_DIR)/%.o, $(SRC_FILES))

LSRC_FILES = $(wildcard $(SRC_DIR_LNX)/*.cc)
LOBJ_FILES = $(patsubst $(SRC_DIR_LNX)/%.cc, $(OBJ_DIR)/%.o, $(LSRC_FILES))
