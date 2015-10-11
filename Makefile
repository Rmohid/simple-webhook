HEADERS = simple.h
OBJECTS = common.o

default: clean client server run test kill

%.o: %.c $(HEADERS)
	gcc -c $< -o $@

client: client.o $(OBJECTS)
	gcc client.o $(OBJECTS) -o bin/$@

server: server.o $(OBJECTS)
	gcc -g server.o $(OBJECTS) -o bin/$@

run: kill
	./bin/server 8181 . & sleep 1
test:
	./bin/client BF2BE4 mykey 
	./bin/client AF2BE4 mykey callThis 
	./bin/client AF2BE4 mykey 
	./bin/client AF2BE4 newkey 
	./bin/client AF2BE4 newkey callThat
	./bin/client AF2BE4 newkey changeIt
kill:
	ps a | grep "/bin/server 8181" | grep -v grep | cut -f1 -d' ' | xargs kill
clean:
	-rm -f *.log
	-rm -f *.o
	-rm -f bin/*

