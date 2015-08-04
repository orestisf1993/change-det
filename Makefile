# ~ OPTI = -m64 -O0 -g -gdwarf-3 -fno-exceptions
OPTI = -m64 -O3 -fno-exceptions
DEFINES = -DUSE_ACKNOWLEDGEMENT -DUSE_CONDITION_VARIABLES
WARN = -Wextra -Wall -Wpointer-arith -Wformat -Wfloat-equal -Winit-self \
-Wcast-qual -Wwrite-strings -Wshadow -Wstrict-prototypes -Wundef -Wunreachable-code

TARGET = pace

all: $(TARGET)

$(TARGET): $(TARGET).c
	gcc $(OPTI) $(DEFINES) $(WARN) $^ -pthread -o $@ 

clean:
	rm -f $(TARGET)

run: all
	./$(TARGET) 10
