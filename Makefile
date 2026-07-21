CC = gcc
CFLAGS = -Wall -Wextra -O2
LDFLAGS = -lX11 -lXrandr -lm
TARGET = whxwm
SRC = whxwm.c

.PHONY: clean install

clean:
	rm -f $(TARGET)
	rm -f ~/bin/$(TARGET)

install:
	@echo "Compiling $(TARGET)..."
	$(CC) -o $(TARGET) $(SRC) $(CFLAGS) $(LDFLAGS)
	cp $(TARGET) /usr/local/bin
	@echo "Installed to /usr/local/bin/$(TARGET)"
