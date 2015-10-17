#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>

#define BUFSIZE 8196

pexit(char * msg)
{
   perror(msg);
   exit(1);
}

typedef struct{
   char hostname[INET6_ADDRSTRLEN];
   int  port;
   struct sockaddr_in serv_addr;
   char command[400];
}AppData;

int main(int argc, char **argv)
{
   int i,sockfd;
   char buffer[BUFSIZE];
   char *args[4]={"","","",""};

   static AppData my;

   if(argc < 4)
      pexit("Usage: host port token key [value]\n");

   for(i=1;i<argc;i++){args[i-1] = argv[i];}

   sprintf(my.command,"GET /%s/%s HTTP/1.0 \r\n\r\n",args[2],args[3]);
   my.port = atoi(args[1]);
   strncpy(my.hostname,args[0], sizeof my.hostname);

   /* Do hostname lookup on destination */
   int status;
   struct addrinfo hints,*res,*p;

   memset(&hints, 0, sizeof hints); // make sure the struct is empty
   hints.ai_family = AF_INET;       // IPV4
   hints.ai_socktype = SOCK_STREAM; // TCP stream sockets
   hints.ai_flags = AI_PASSIVE;     // fill in my IP for me

   if ((status = getaddrinfo(args[0], "80", &hints, &res)) != 0) {
      pexit((char *) gai_strerror(status));
   }

   // res now points to a linked list of 1 or more struct addrinfos

   for(p = res;p != NULL; p = p->ai_next) {
      void *addr;
      char *ipver;

      // get the pointer to the address itself,
      // different fields in IPv4 and IPv6:
      if (p->ai_family == AF_INET) { // IPv4
         struct sockaddr_in *ipv4 = (struct sockaddr_in *)p->ai_addr;
         addr = &(ipv4->sin_addr);
         ipver = "IPv4";
         // convert the IP to a string and print it:
         inet_ntop(p->ai_family, addr, my.hostname, sizeof my.hostname);
         break;
      } 
   }

   freeaddrinfo(res); // free the linked-list

   printf("client trying to connect to %s and port %d\n",my.hostname,my.port);
   if((sockfd = socket(AF_INET, SOCK_STREAM,0)) <0) 
      pexit("socket() failed");

   my.serv_addr.sin_family = AF_INET;
   my.serv_addr.sin_addr.s_addr = inet_addr(my.hostname);
   my.serv_addr.sin_port = htons(my.port);

   /* Connect to the socket offered by the web server */
   if(connect(sockfd, (struct sockaddr *)&my.serv_addr, sizeof(my.serv_addr)) <0) 
      pexit("connect() failed");

   /* Now the sockfd can be used to communicate to the server the GET request */
   printf("Client sent bytes=%d %s\nResponse:\n",(int)strlen(my.command), my.command);
   write(sockfd, my.command, strlen(my.command));

   /* This displays the raw HTML file (if index.html) as received by the browser */
   while( (i=read(sockfd,buffer,BUFSIZE)) > 0){
      write(1,buffer,i);
   }
   return(0);
}

