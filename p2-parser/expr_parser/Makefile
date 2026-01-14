CFLAGS=-g --std=c11 -Wall
LDFLAGS=

default: expr_parser

test: expr_parser
	./expr_tests.sh

expr_parser: expr_parser.c expr_lexer.c
	$(CC) $(CFLAGS) -o $@ $<

clean:
	rm -f expr_parser
