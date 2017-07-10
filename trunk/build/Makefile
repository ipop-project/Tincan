include config.mk

.PHONY : all vars mkdirs clean help

## all : default rule to create ipop-tincan executable
all : mkdirs $(TARGET)

## vars : prints variables
vars :
	@echo HDR_FILES: $(HDR_FILES)
	@echo SRC_FILES: $(SRC_FILES)
	@echo OBJ_FILES: $(OBJ_FILES)

## mkdirs : creates the output directories	if it does not exist
mkdirs :
	if [ ! -d $(ODIR) ]; then  mkdir -p $(ODIR); fi

$(TARGET) : $(OBJ_FILES) $(LOBJ_FILES)
	$(CC) -o $@ $^ -L $(LDIR) $(LIBS)

## ../src/file.obj : comiples the specified object file
$(ODIR)/%.o : $(SRC_DIR)/%.cc $(HDR_FILES)	
	$(CC) -iquote $(IDIR) -isystem $(IDIR_EXT) $(defines) $(cflags_cc) -c $< -o $@


$(LOBJ_FILES) : $(LSRC_FILES) $(IDIR_LNX)/tapdev_lnx.h
	$(CC) -iquote $(IDIR) -isystem $(IDIR_EXT) -iquote $(IDIR_LNX) $(defines) $(cflags_cc) -c $< -o $@

## clean : removes output dir and generated files
clean:
	rm -rf $(OUT)

## help : prints help
help : Makefile
	@sed -n 's/^##//p' $<
