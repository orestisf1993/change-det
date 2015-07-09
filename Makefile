OPTI = -m64 -O3 -fno-exceptions
WARN = -Wextra -Wall -Wpointer-arith -Wformat -Wfloat-equal -Winit-self \
-Wcast-qual -Wwrite-strings -Wshadow -Wstrict-prototypes -Wundef -Wunreachable-code

TARGET = pace

all: $(TARGET)

$(TARGET): $(TARGET).c
	gcc $(OPTI) $(WARN) $^ -lpthread -o $@ 

clean:
	rm -f $(TARGET)

run: all
	./$(TARGET) 10
