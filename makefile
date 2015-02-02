CC = g++
CFLAGS = -c -Wall
LDFLAGS =
SOURCES = peachshell.cpp
OBJECTS = $(SOURCES:.cpp = .o)
EXECUTABLE = myshell

all: $(SOURCES) $(EXECUTABLE)

$(EXECUTABLE):$(OBJECTS)
	$(CC) $(LDFLAGS) $(OBJECTS) -o $@
.cpp.o:
	$(CC) $(CFLAGS) $< -o $@

clean:
	rm -f myshell peachshell.o
run: myshell
	./myshell
