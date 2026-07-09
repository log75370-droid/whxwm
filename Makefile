CC = gcc
CFAGS = -lX11 -lm -Wall -Wextra -O2
TAGRET = whxwm whxwm.c

clean:
  rm -f whxwm
  rm -f ~/bin/whxwm
install:
  echo "compiling"
  CC -o $(TARGET) $(CFAGS)
  mkdir ~/bin
  cp whxwm ~/bin

