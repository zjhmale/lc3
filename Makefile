CC=c++
FLAG=-Wall -std=c++14 -I./include/range-v3/include
MAIN=main
TEST=test

.PHONY: test clean all

all: token.o lexer.o ast.o repl.o parser.o main.o test main

token.o:
	$(CC) $(FLAG) -c ./src/token.hpp

lexer.o: token.o
	$(CC) $(FLAG) -c ./src/lexer.hpp

ast.o: token.o
	$(CC) $(FLAG) -c ./src/ast.hpp

repl.o: lexer.o
	$(CC) $(FLAG) -c ./src/repl.hpp

parser.o: lexer.o
	$(CC) $(FLAG) -c ./src/parser.hpp

main.o: repl.o
	$(CC) $(FLAG) -c ./src/main.cc

main: main.o
	$(CC) $(FLAG) -ledit main.o -o $(MAIN)

test:
	$(CC) $(FLAG) ./test/token_test.cc && ./a.out
	$(CC) $(FLAG) ./test/lexer_test.cc && ./a.out
	$(CC) $(FLAG) ./test/ast_test.cc && ./a.out

clean:
	rm *.o *.out $(MAIN)
