CC      = clang
  CFLAGS  = -O3 -std=c11 -Wall -Wextra
  SRCS    = ibedrockfinder.c cubiomes/util.c cubiomes/noise.c
  TARGET  = ibedrockfinder

  all: $(TARGET)

  $(TARGET): $(SRCS)
  	$(CC) $(CFLAGS) -o $@ $(SRCS) -lm

  clean:
  	rm -f $(TARGET)

  .PHONY: all clean
  