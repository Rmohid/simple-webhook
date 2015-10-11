# simple-webhook 

A daemon, in C, to allow simple callback functionality over http, based on [nweb](http://www.ibm.com/developerworks/systems/library/es-nweb/).
Run webhook on a target host and provide a path with executable target scripts. Any get request to the hosts ip and port with the specified token and scriptname will trigger execution.

## Usage
   webhook PORT PATH [token]
   
   and curl http://hostIp:PORT/token/script to trigger
