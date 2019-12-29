CC = g++
CFLAGS = -std=c++14 -Wall -Wextra -pedantic-errors
LDFLAGS = -lm -lSDL2
DEPS = gameboy.o display.o cpu.o mmu.o

install: gameboy

.PHONY: clean

clean:
	$(RM) *.o

%.o: %.cpp
	$(CC) $(CFLAGS) -c -o $@ $<

gameboy: $(DEPS)
	$(CC) $(LDFLAGS) $(CFLAGS) $(DEPS) main.cpp -o gameboy

