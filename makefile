CC = g++
CFLAGS = -c -Wall
LDFLAGS =
SOURCES = peachshell.cpp
OBJECTS = $(SOURCES:.cpp = .o)
EXECUTABLE = my_shell

all: $(SOURCES) $(EXECUTABLE)

$(EXECUTABLE):$(OBJECTS)
	$(CC) $(LDFLAGS) $(OBJECTS) -o $@
.cpp.o:
	$(CC) $(CFLAGS) $< -o $@

clean:
	rm -f my_shell peachshell.o
run: my_shell
	./my_shell
