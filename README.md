# simple-webhook 

A daemon, in C, to allow simple callback functionality for github's webhooks, based on [nweb](http://www.ibm.com/developerworks/systems/library/es-nweb/).
Run webhook on a target host and provide a path to executable target scripts. Any get or post request to the hosts ip and port with the specified token and scriptname will trigger execution of that script with the request data passed as arguments on the command line. JSON data is passed unparsed as a single string.

## Usage
   webhook PORT PATH [token]
   
   and curl 'host:PORT/webhook/token/script?arg1&arg2' to trigger on the command line
