all: bank

bank: bank.o string_parser.o
	gcc -pthread -o bank bank.o string_parser.o -lpthread

bank.o: bank.c account.h
	gcc -c bank.c


string_parser.o: string_parser.c string_parser.h
	gcc -c string_parser.c

clean:
	rm -f core *.o bank
