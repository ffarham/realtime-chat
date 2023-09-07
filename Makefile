all: error_handler.o server.o client.o client server clean

error_handler.o: error_handler.cpp
	g++ -c error_handler.cpp

server.o: server.cpp
	g++ -c server.cpp

client.o: client.cpp
	g++ -c client.cpp

client: client.o error_handler.o
	g++ client.o error_handler.o -o client

server: server.o error_handler.o
	g++ server.o error_handler.o -o server

clean:
	rm *.o 