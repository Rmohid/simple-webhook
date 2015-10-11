HEADERS = 
OBJECTS = 
SPATH = ./bin/script

default: clean client server run test kill

%.o: %.c $(HEADERS)
	gcc -c $< -o $@

client: client.o $(OBJECTS)
	gcc client.o $(OBJECTS) -o bin/$@

server: server.o $(OBJECTS)
	gcc -g server.o $(OBJECTS) -o bin/$@

run: kill
	./bin/server 8181 $(SPATH) &  sleep 1
test:
	./bin/client BF2BE4 script1 
	./bin/client AF2BE4 script1
	./bin/client AF2BE4 ""
	./bin/client AF2BE4 script2
	./bin/client AF2BE4 script2?one=1
	./bin/client AF2BE4 script3
	./bin/client AF2BE4 "ls -l"
	cat $(SPATH)/web.log
kill:
	ps a | grep "/bin/server 8181" | grep -v grep | cut -f1 -d' ' | xargs -t kill
clean:
	-rm -f $(SPATH)/*.log
	-rm -f *.o
	-rm -f bin/*

