# Makefile hex2bin/mot2bin
SRCDIR  = src
BINDIR  = bin
OBJDIR  = obj
TGTDIR  = $(BINDIR)
B_SRCFILES= $(foreach F, hex2bin.c common.c libcrc.c binary.c, $(SRCDIR)/$(F))
B_OBJFILES= $(foreach F, hex2bin.o common.o libcrc.o binary.o, $(OBJDIR)/$(F))
M_SRCFILES= $(foreach F, mot2bin.c common.c libcrc.c binary.c, $(SRCDIR)/$(F))
M_OBJFILES= $(foreach F, mot2bin.o common.o libcrc.o binary.o, $(OBJDIR)/$(F))

# For generating documentation (hex2bin.1, select the second line)
#  -- You will require pod2man installed for this to work
TGT_FILES = $(foreach F, hex2bin mot2bin, $(TGTDIR)/$(F))
#TGT_FILES = $(foreach F, hex2bin mot2bin hex2bin.1, $(TGTDIR)/$(F))

CPFLAGS = -std=gnu99 -O3 -fsigned-char -Wall -pedantic -fcommon
# Compile
all: objectdir $(TGT_FILES)

ifeq ($(OS),Windows_NT)
    TARGET = hex2bin.exe
else
    TARGET = hex2bin
endif

$(OBJDIR)/%.o: $(SRCDIR)/%.c
	gcc -c $(CPFLAGS) $< -o $@

objectdir:
	@echo "Creating directory $(OBJDIR)..."
ifeq ($(OS),Windows_NT)
	if not exist $(OBJDIR)\ mkdir $(OBJDIR)
else
	mkdir -p $(OBJDIR)
endif

$(TGTDIR)/hex2bin.1: $(SRCDIR)/hex2bin.pod
	pod2man $(SRCDIR)/hex2bin.pod > $(TGTDIR)/hex2bin.1

$(TGTDIR)/hex2bin: $(B_OBJFILES)
	gcc $(CPFLAGS) -o $(TGTDIR)/hex2bin $(B_OBJFILES)
ifeq ($(OS),Windows_NT)
	copy bin\$(TARGET) ..\bin
else
	cp bin/$(TARGET) ../bin
endif

$(TGTDIR)/mot2bin: $(M_OBJFILES)
	gcc $(CPFLAGS) -o $(TGTDIR)/mot2bin $(M_OBJFILES)

clean:
	@echo "Removing objects directory $(OBJDIR)/ ..."
ifeq ($(OS),Windows_NT)
	if exist $(OBJDIR) rmdir /s/q $(OBJDIR)
else
	@rm -rf $(OBJDIR)
endif

cleanall: clean
	@echo "Removing binary files in $(BINDIR)/ ..."
ifeq ($(OS),Windows_NT)
	if exist $(BINDIR) del /q $(BINDIR)\*.*
else
	@rm -f $(BINDIR)/*
endif
