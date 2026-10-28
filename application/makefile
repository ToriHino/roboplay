TARGET := roboplay

include ../config.env

CODE := 0x0180
DATA := 0x0000
ifeq ($(OS),Windows_NT)
	LIBS := -l..\support\build\support.lib -l..\drivers\build\drivers.lib
else
	LIBS := -l../support/build/support.lib -l../drivers/build/drivers.lib
endif


all: build $(OUTPUT)/$(TARGET).com
ifeq ($(OS),Windows_NT)
	@copy $(OUTPUT)\$(TARGET).com $(DISK) > NUL
else
	@cp $(OUTPUT)/$(TARGET).com $(DISK)
endif

$(OUTPUT)/%.rel: src/%.z80
	$(AS) -o $@ $<

$(OUTPUT)/%.rel: src/%.c
	$(CC) $(CFLAGS) $(LDFLAGS) -c $< -o $@

$(OUTPUT)/$(TARGET).com: $(OBJS_C) $(OBJS_Z80)
	$(CC) $(CFLAGS) $(LDFLAGS) $(LIBS) --code-loc $(CODE) --data-loc $(DATA) $(OBJS_C) $(OBJS_Z80) -o $(OUTPUT)/$(TARGET).ihx
	../bin/hex2bin -e com $(OUTPUT)/$(TARGET).ihx

build:
	@mkdir $(OUTPUT)

.PHONY: clean
clean:
ifeq ($(OS),Windows_NT)
	@if exist $(OUTPUT) del /s/q $(OUTPUT)
	@if exist $(OUTPUT) rmdir $(OUTPUT)
else
	rm -rf $(OUTPUT)
	rm -f $(DISKFILE)
endif