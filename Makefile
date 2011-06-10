CODEC_DIR := ./amrnb-7.0.0.2

CFLAGS += -I$(CODEC_DIR)
OBJECTS := echo.o util.o $(CODEC_DIR)/sp_dec.o $(CODEC_DIR)/interf_dec.o 

echo: $(OBJECTS)

clean:
	rm $(OBJECTS) echo
