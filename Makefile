HEADERS = simple.h
OBJECTS = common.o

default: clean client server run kill

%.o: %.c $(HEADERS)
	gcc -c $< -o $@

client: client.o $(OBJECTS)
	gcc client.o $(OBJECTS) -o bin/$@

server: server.o $(OBJECTS)
	gcc server.o $(OBJECTS) -o bin/$@

run:
	./bin/server 8181 . & sleep 1; ./bin/client 
kill:
	ps a | grep "/bin/server 8181" | grep -v grep | cut -f1 -d' ' | xargs kill
clean:
	-rm -f *.log
	-rm -f *.o
	-rm -f bin/*

