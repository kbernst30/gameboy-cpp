CC = g++
CFLAGS = -std=c++0x
DEPS = gameboy.o

install: gameboy

.PHONY: clean

clean:
	$(RM) *.o

%.o: %.cpp
	$(CC) $(CFLAGS) -c -o $@ $<

gameboy: $(DEPS)
	$(CC) $(CFLAGS) main.cpp -o gameboy -lSDL2

