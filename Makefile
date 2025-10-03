CC = gcc
CFLAGS = -Wall -O2

TARGET = lexer
SRC = lexer.c

all: $(TARGET)

$(TARGET): $(SRC)
	$(CC) $(CFLAGS) $(SRC) -o $(TARGET)

run: $(TARGET)
	./$(TARGET) Demo.java

clean:
	rm -f $(TARGET)
