HEADERS = 
OBJECTS = 
SPATH = ./bin/script

default: clean client server run test kill

%.o: %.c $(HEADERS)
	gcc -c $< -o $@

client: client.o $(OBJECTS)
	gcc client.o $(OBJECTS) -o bin/$@

server: server.o $(OBJECTS)
	gcc -g server.o $(OBJECTS) -o bin/webhook

run: kill server
	./bin/webhook 8181 $(SPATH) &  sleep 1
test:
	./bin/client localhost 8181 BF2BE4 script1 
	./bin/client localhost 8181 AF2BE4 script1
	curl 'localhost:8181/AF2BE4/script1'
	curl 'localhost:8181/AF2BE4/script2?echo&this&is&curl'
	curl --data 'postdata1=valid&param2=value2' localhost:8181/AF2BE4/script2
	curl -H "Content-Type: application/json" -X POST -d '{"username":"xyz","password":"xyz"}' http://localhost:8181/AF2BE4/script2

	cat $(SPATH)/web.log
kill:
	ps a | grep "/bin/webhook 8181" | grep -v grep | cut -f1 -ds | xargs -t kill

clean: kill
	rm -f $(SPATH)/*.log
	rm -f *.o
	find bin/ -type f| grep -v script |xargs rm

