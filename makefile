CC = gcc
CFLAGS = -c -Wall
LDFLAGS =
SOURCES = peachshell.c
OBJECTS = $(SOURCES:.c = .o)
EXECUTABLE = my_shell

all: $(SOURCES) $(EXECUTABLE)

$(EXECUTABLE):$(OBJECTS)
	$(CC) $(LDFLAGS) $(OBJECTS) -o $@
.c.o:
	$(CC) $(CFLAGS) $< -o $@

clean:
	rm -f my_shell peachshell.o
run: my_shell
	./my_shell
