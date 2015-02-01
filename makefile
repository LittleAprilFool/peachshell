CC = g++
CFLAGS = -c -Wall
LDFLAGS =
SOURCES = assignment1.cpp
OBJECTS = $(SOURCES:.cpp = .o)
EXECUTABLE = proc_parse

all: $(SOURCES) $(EXECUTABLE)

$(EXECUTABLE):$(OBJECTS)
	$(CC) $(LDFLAGS) $(OBJECTS) -o $@
.cpp.o:
	$(CC) $(CFLAGS) $< -o $@

clean:
	rm -f proc_parse assignment1.o
run: proc_parse
	./proc_parse
