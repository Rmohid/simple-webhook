// Based on server by Nigel Griffiths (nag@uk.ibm.com),

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <fcntl.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#define VERSION 1
#define BUFSIZE 8096
#define ERROR      42
#define LOG        44
#define FORBIDDEN 403
#define NOTFOUND  404
#define LOGFILE "web.log"
#define SETTING_MAX 80
#define LINE_MAX 200
#define TOKEN   "AF2BE4"

// OSX req'd
#ifndef SIGCLD
# define SIGCLD SIGCHLD
#endif

#define DEAMONIZED 

enum APP_KEYS {KEY_TOKEN,KEY_PORT,KEY_LAST};
char *settings [KEY_LAST];
char bufSettings[KEY_LAST * SETTING_MAX];

void logger(int type, char *s1, char *s2, int socket_fd)
{
   int fd ;
   char logbuffer[BUFSIZE*2];

   switch (type) {
      case ERROR: (void)sprintf(logbuffer,"ERROR: %s:%s Errno=%d exiting pid=%d",s1, s2, errno,getpid()); 
                  break;
      case FORBIDDEN: 
                  (void)write(socket_fd, "HTTP/1.1 403 Forbidden\nContent-Length: 185\nConnection: close\nContent-Type: text/html\n\n<html><head>\n<title>403 Forbidden</title>\n</head><body>\n<h1>Forbidden</h1>\nThe requested URL, file type or operation is not allowed on this simple static file webserver.\n</body></html>\n",271);
                  (void)sprintf(logbuffer,"FORBIDDEN: %s:%s",s1, s2); 
                  break;
      case NOTFOUND: 
                  (void)write(socket_fd, "HTTP/1.1 404 Not Found\nContent-Length: 136\nConnection: close\nContent-Type: text/html\n\n<html><head>\n<title>404 Not Found</title>\n</head><body>\n<h1>Not Found</h1>\nThe requested URL was not found on this server.\n</body></html>\n",224);
                  (void)sprintf(logbuffer,"NOT FOUND: %s:%s",s1, s2); 
                  break;
      case LOG: (void)sprintf(logbuffer," INFO: %s:%s:%d",s1, s2,socket_fd); break;
   }	
   /* No checks here, nothing can be done with a failure anyway */
   if((fd = open(LOGFILE, O_CREAT| O_WRONLY | O_APPEND,0644)) >= 0) {
      (void)write(fd,logbuffer,strlen(logbuffer)); 
      (void)write(fd,"\n",1);      
      (void)close(fd);
   }
   if(type == ERROR || type == NOTFOUND || type == FORBIDDEN) exit(3);
}

/* this is a child web server process, so we can exit on errors */
void web(int fd, int hit)
{
   int  file_fd, buflen,method;
   long i, ret, len;
   char *idx, *key, *url = "";
   char *fstr = "text/plain";
   static char buffer[BUFSIZE+1]; 
   static char header[LINE_MAX];
   static char command[BUFSIZE];

   /* read Web request in one go */
   ret =read(fd,buffer,BUFSIZE);
   if(ret == 0 || ret == -1) {	
      logger(FORBIDDEN,"failed to read browser request","",fd);
   }

   /* return code is valid chars */
   if(ret > 0 && ret < BUFSIZE)	
      buffer[ret]=0;		/* terminate the buffer */
   else buffer[0]=0;

   /* remove CF and LF characters */
   for(i=0;i<ret;i++)	
      if(buffer[i] == '\r' || buffer[i] == '\n')
         buffer[i]='*';
   /* make it shell safe */
      if(buffer[i] == '\'')
         buffer[i]='"';

   logger(LOG,"request",buffer,hit);

   idx = buffer;
   if( strncmp(idx,"GET ",4) && strncmp(idx,"get ",4) && strncmp(idx,"POST ",5) && strncmp(idx,"post ",5) ){
      logger(FORBIDDEN,"Only simple POST and GET requests supported",buffer,fd);
   }

   /* check for token .. */
   idx = strstr(buffer,"/webhook/") + 9;
   if( strncmp(idx,settings[KEY_TOKEN],strlen(settings[KEY_TOKEN]))){ 
      logger(FORBIDDEN,"Invalid token",idx,fd);
   }

   idx += strlen(settings[KEY_TOKEN]);
   key = idx + 1; 
   idx = strstr(buffer,"HTTP/");
   *(idx-1) = 0;

   /* Nothing to do if no key */
   if(!strlen(key)){                   
      logger(ERROR,"No key",buffer,fd);
   }

   if( 0 == (strncmp(buffer,"POST ",5) && strncmp(buffer,"post ",5)) ) {
      /* Only support non-multipart post requests */
      idx = strstr(idx,"****");
      if(idx){
         url = idx + 4;
      }else{
         logger(ERROR,"failed to read POST request",buffer,ret);
      }

   }else{
      /* It's a get request and already loaded */
      idx += 5;                   

      /* null terminate slashes and semicolons */
      for(i=key-buffer;i<BUFSIZE;i++) { 
         if((buffer[i] == '/' ) || (buffer[i] == ';')){
            buffer[i] = 0;
         }
         /* Pass GET arguments to script */
         if(buffer[i] == '?' ){ 
            buffer[i] = 0;
            url = &buffer[i + 1];
         }
         /* Ignore rest of url */
         if(buffer[i] == ' ') { 
            buffer[i] = 0;
            break;
         }
      }
   }

   (void)sprintf(command,"./%s '%s'",key, url);
   /* Header + a blank line */
   (void)sprintf(buffer," %s, exit with %d\n\n",command,(short)system(command));
   (void)sprintf(header,
         "HTTP/1.1 200 OK\nServer: server/%d.0\nContent-Length: %ld\nConnection: close\nContent-Type: %s\n\n", 
         VERSION, strlen(buffer), fstr); 
#ifdef DEBUG
    logger(LOG,"Header: ",header,fd);
    logger(LOG,"Buffer: ",buffer,fd);
#endif

   for(ret = strlen(header); ret>0; ret -= write(fd,header,strlen(header)));
   for(ret = strlen(buffer); ret>0; ret -= write(fd,buffer,strlen(buffer)));

   sleep(1);	/* allow socket to drain before signalling the socket is closed */
   close(fd);
   exit(1);
}

int main(int argc, char **argv)
{
   int i, port, pid, listenfd, socketfd, hit;
   socklen_t length;
   static struct sockaddr_in cli_addr; /* static = initialised to zeros */
   static struct sockaddr_in serv_addr; /* static = initialised to zeros */

   if( argc < 3  || argc > 4 || !strcmp(argv[1], "-?") ) {
      (void)printf("hint: server Port-Number Top-Directory [Token]\t\tversion %d\n\n"
            "\tserver is a small and very safe mini web server\n"
            "\tThere is no fancy features = safe and secure.\n\n"
            "\tExample: server 8181 /home/servescripts ABC123 &\n\n", VERSION);
      exit(0);
   }

   for(i=0;i<KEY_LAST;i++){
      settings[i] = &bufSettings[i* SETTING_MAX];
   }
   strncpy(settings[KEY_PORT],argv[1],SETTING_MAX);
   strncpy(settings[KEY_TOKEN],TOKEN,SETTING_MAX);
   if(argc == 4)
      strncpy(settings[KEY_TOKEN],argv[3],SETTING_MAX);
   if( !strncmp(argv[2],"/"   ,2 ) || !strncmp(argv[2],"/etc", 5 ) ||
         !strncmp(argv[2],"/bin",5 ) || !strncmp(argv[2],"/lib", 5 ) ||
         !strncmp(argv[2],"/tmp",5 ) || !strncmp(argv[2],"/usr", 5 ) ||
         !strncmp(argv[2],"/dev",5 ) || !strncmp(argv[2],"/sbin",6) ){
      (void)printf("ERROR: Bad top directory %s, see server -?\n",argv[2]);
      exit(3);
   }
   if(chdir(argv[2]) == -1){ 
      (void)printf("ERROR: Can't Change to directory %s\n",argv[2]);
      exit(4);
   }
#ifdef DEAMONIZED
   /* Become deamon + unstopable and no zombies children (= no wait()) */
   if(fork() != 0)
      return 0;                         /* parent returns OK to shell */
   (void)signal(SIGCLD, SIG_IGN);         /* ignore child death */
   (void)signal(SIGHUP, SIG_IGN);         /* ignore terminal hangups */
   for(i=0;i<32;i++)
      (void)close(i);		          /* close open files */
   (void)setpgrp();	                  /* break away from process group */
#endif
   logger(LOG,"server starting",settings[KEY_TOKEN],getpid());

   /* setup the network socket */
   if((listenfd = socket(AF_INET, SOCK_STREAM,0)) <0)
      logger(ERROR, "system call","socket",0);
   port = atoi(settings[KEY_PORT]);
   if(port < 0 || port >60000)
      logger(ERROR,"Invalid port number (try 1->60000)",settings[KEY_PORT],0);

   serv_addr.sin_family = AF_INET;
   serv_addr.sin_addr.s_addr = htonl(INADDR_ANY);
   serv_addr.sin_port = htons(port);

   if(bind(listenfd, (struct sockaddr *)&serv_addr,sizeof(serv_addr)) <0)
      logger(ERROR,"system call","bind",0);
   if( listen(listenfd,64) <0)
      logger(ERROR,"system call","listen",0);

   for(hit=1; ;hit++) {
      length = sizeof(cli_addr);
      if((socketfd = accept(listenfd, (struct sockaddr *)&cli_addr, &length)) < 0)
         logger(ERROR,"system call","accept",0);
      if((pid = fork()) < 0) {
         logger(ERROR,"system call","fork",0);
      }
      else {
         if(pid == 0) {             /* child */
            (void)close(listenfd);
            web(socketfd,hit);      /* never returns */
         } else { 	            /* parent */
            (void)close(socketfd);
         }
      }
   }
}
