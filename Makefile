CC = gcc
CFLAGS = -Wall -Wextra -O2
LDFLAGS = -lpcreposix -lpcre

SRC = lexer.c
OUT = lexer.exe
TEST = Demo.java

all: $(OUT)

$(OUT): $(SRC)
	$(CC) $(CFLAGS) $(SRC) -o $(OUT) $(LDFLAGS)

run: $(OUT)
	./$(OUT) $(TEST)

clean:
	rm -f $(OUT)
