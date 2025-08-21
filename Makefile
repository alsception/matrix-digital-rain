# Compiler and flags
CC = gcc
CFLAGS = -Wall -g

# Target executable
TARGET = bin/matrix

# Object files directory
OBJDIR = obj
OBJS = $(OBJDIR)/matrix.o

# Default target
all: $(TARGET)

# Link object files to create executable
$(TARGET): $(OBJS)
	@mkdir -p $(dir $@)
	$(CC) -o $@ $(OBJS) -lm

# Compile source files into object files
$(OBJDIR)/matrix.o: src/matrix.c src/lib/types/Colors.h src/lib/types/Position.h src/lib/types/TailSegment.h
	@mkdir -p $(OBJDIR)
	$(CC) $(CFLAGS) -c $< -o $@

# Clean rule to remove compiled files
clean:
	rm -f $(OBJS) $(TARGET)
