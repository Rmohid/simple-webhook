HEADERS = 
OBJECTS = 
SPATH = ./bin/script
TOKEN = AC2BE5

default: clean client server run test kill

%.o: %.c $(HEADERS)
	gcc -c $< -o $@

client: client.o $(OBJECTS)
	gcc client.o $(OBJECTS) -o bin/$@

server: server.o $(OBJECTS)
	gcc -g server.o $(OBJECTS) -o bin/webhook

run: kill server
	./bin/webhook 8181 $(SPATH) $(TOKEN) &  sleep 1
	ps -ef | grep 'AC2BE5$$'

test: client run
	./bin/client localhost 8181 webhook/BF2BE4 script1 
	./bin/client localhost 8181 webhook/$(TOKEN) script1
	curl 'localhost:8181/webhook/$(TOKEN)/script3'
	curl 'localhost:8181/webhook/$(TOKEN)/script2?echo&this&is&curl'
	curl --data 'postdata1=valid&param2=value2' localhost:8181/webhook/$(TOKEN)/script2
	curl -H "Content-Type: application/json" -X POST -d '{"pusher":{"name":"Jimmy"}}' http://localhost:8181/webhook/AC2BE5/script4

kill:
	`ps -aeo pid,command | awk '/$(TOKEN)$$/{system("kill -9 " $$1)}' ` || true

clean: kill
	rm -f $(SPATH)/*.log
	rm -f *.o
	find bin/ -type f| grep -v script |xargs rm || true

