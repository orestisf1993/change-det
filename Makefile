# ~ OPTI = -m64 -O0 -g -gdwarf-3 -fno-exceptions
OPTI = -m64 -O3 -fdiagnostics-color=auto
DEFINES = -DUSE_ACKNOWLEDGEMENT -DUSE_CONDITION_VARIABLES
WARN = -pedantic -Wextra -Wall -Wpointer-arith -Wformat -Wfloat-equal -Winit-self \
-Wcast-qual -Wwrite-strings -Wshadow -Wstrict-prototypes -Wmissing-prototypes -Wundef \
-Wunreachable-code -Wbad-function-cast -Wstrict-overflow=5 -Winline -Wundef -Wnested-externs \
-Wlogical-op -Wformat=2 -Wredundant-decls

TARGET = pace

all: $(TARGET)

$(TARGET): $(TARGET).c
	gcc $(OPTI) $(DEFINES) $(WARN) $^ -pthread -o $@ 

clean:
	rm -f $(TARGET)

run: all
	./$(TARGET) 10
