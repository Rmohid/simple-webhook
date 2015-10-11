#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

/* YOU WILL HAVE TO CHANGE THESE TWO LINES  TO MATCH YOUR CONFIG */
#define PORT        8181		/* Port number as an integer - web server default is 80 */
#define IP_ADDRESS "127.0.0.1"	/* IP Address as a string */

char *command = "GET /AF2BE4/mykey/callThis HTTP/1.0 \r\n\r\n" ;
/* Note: spaces are delimiters and VERY important */

#define BUFSIZE 8196

pexit(char * msg)
{
   perror(msg);
   exit(1);
}

main(int argc, char **argv)
{
   int i,sockfd;
   char buffer[BUFSIZE];
   char cmd[400];
   char *args[4]={"","","",""};
   static struct sockaddr_in serv_addr;
   if(argc < 3){
      printf("Usage: client token key [value]\n");
      exit(1);
   }
   for(i=1;i<argc;i++){args[i-1] = argv[i];}

   sprintf(cmd,"GET /%s/%s/%s HTTP/1.0 \r\n\r\n",args[0],args[1],args[2]);
   command = cmd;

   printf("client trying to connect to %s and port %d\n",IP_ADDRESS,PORT);
   if((sockfd = socket(AF_INET, SOCK_STREAM,0)) <0) 
      pexit("socket() failed");

   serv_addr.sin_family = AF_INET;
   serv_addr.sin_addr.s_addr = inet_addr(IP_ADDRESS);
   serv_addr.sin_port = htons(PORT);

   /* Connect tot he socket offered by the web server */
   if(connect(sockfd, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) <0) 
      pexit("connect() failed");

   /* Now the sockfd can be used to communicate to the server the GET request */
   printf("Client sent bytes=%d %s\nResponse:\n",(int)strlen(command), command);
   write(sockfd, command, strlen(command));

   /* This displays the raw HTML file (if index.html) as received by the browser */
   while( (i=read(sockfd,buffer,BUFSIZE)) > 0){
      write(1,buffer,i);
   }
   return(0);
}

