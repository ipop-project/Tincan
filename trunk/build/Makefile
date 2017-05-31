# all: library.cpp main.cpp
# $@ evaluates to all, $< evaluates to library.cpp, $^ evaluates to library.cpp main.cpp

CC = g++

IDIR = ../include
IDIR_LNX = ../include/linux
IDIR_EXT = ../../external/include

SRCDIR = ../src
LNX_SRC = ../src/linux

LDIR = ../../external/lib/release/x64/ubuntu

ODIR = ../out/release/x64/obj
BDIR = ../out/release/x64

BINARY = ipop-tincan
TARGET = $(patsubst %,$(BDIR)/%,$(BINARY))

defines = -DLINUX -D_IPOP_LINUX -DWEBRTC_POSIX -DWEBRTC_LINUX

cflags_cc = -std=c++14 -O3 -m64 -march=x86-64 -pthread -g2 -gsplit-dwarf -fno-strict-aliasing --param=ssp-buffer-size=4 -fstack-protector -funwind-tables -fPIC -pipe -Wall -fno-rtti

LIBS = -ljsoncpp -lrtc_p2p -lrtc_base -lrtc_base_approved -lfield_trial_default -lboringssl -lboringssl_asm -lprotobuf_lite -lpthread

_OBJ = control_dispatch.o control_listener.o peer_network.o tincan.o tincan_control.o tincan_main.o tunnel.o virtual_link.o virtual_network.o tap_frame.o
LIN_OBJ_=tapdev_lnx.o
OBJ = $(patsubst %,$(ODIR)/%,$(_OBJ))
LIN_OBJ = $(patsubst %,$(ODIR)/%,$(LIN_OBJ_))
HDRS = $(IDIR_I)/%.h $(IDIR_L)/%.h


$(ODIR)/%.o: $(SRCDIR)/%.cc
	if [ ! -d $(ODIR) ]; then  mkdir -p $(ODIR); fi
	$(CC) -iquote $(IDIR) -isystem $(IDIR_EXT) $(defines) $(cflags_cc) -c $< -o $@

$(TARGET): $(OBJ) $(LIN_OBJ)
	$(CC) -o $@ $^ -L $(LDIR) $(LIBS)

$(LIN_OBJ): $(LNX_SRC)/tapdev_lnx.cc
	$(CC) -iquote $(IDIR) -isystem $(IDIR_EXT) -iquote $(IDIR_LNX) $(defines) $(cflags_cc) -c $< -o $@

clean:
	rm -rf $(TARGET) $(ODIR) *~

