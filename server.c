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
// For shared memory
#include <sys/ipc.h>
#include <sys/shm.h>

#define VERSION 1
#define BUFSIZE 8096
#define ERROR      42
#define LOG        44
#define FORBIDDEN 403
#define NOTFOUND  404
#define TOKEN     AF2BE4
#define PORT      12345
#define MAX_TRIGGER 32
#define MAX_CALLBACK 256
#define NUM_TRIGGERS 5
#define SHMSIZE (sizeof(Trigger) * NUM_TRIGGERS + 1023 ) % 1024

// OSX req'd
#ifndef SIGCLD
# define SIGCLD SIGCHLD
#endif

#define xstr(a) str(a)
#define str(a) #a

enum APP_KEYS {KEY_TOKEN,KEY_PORT,KEY_LAST};

char *settings [] = {
   [KEY_TOKEN]=xstr(TOKEN),  
   [KEY_PORT]=xstr(PORT),  
   [KEY_LAST]=NULL };

typedef struct{
   char trigger[MAX_TRIGGER];
   char callback[MAX_CALLBACK];
}Trigger;

void dumpTriggers(Trigger *t){
   int i;
   for(i = 0; i < NUM_TRIGGERS ; i++){
      printf( "%d: %s -> %s, ",i, t[i].trigger,t[i].callback);
   }
   printf("\n");
}
char *lookup(char *key, char *value, Trigger *t){
   int i,idx = 0;
   /* Find if it's already present or first empty */
   for(i = 0; i < NUM_TRIGGERS ; i++){
      if(!strcmp(key, t[i].trigger) || (t[i].trigger[0] == 0)){
         printf("Match %s with %s at %d\n", key, t[i].trigger, i);
         idx = i;
         break;
      }
   }
   strncpy(t[idx].trigger,key,MAX_TRIGGER);
   if(strlen(value)){ /* registering a new trigger, save and echo it back */
      strncpy(t[idx].callback,value,MAX_CALLBACK);
   }
   dumpTriggers(t);

   return t[idx].callback;
}

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
   if((fd = open("web.log", O_CREAT| O_WRONLY | O_APPEND,0644)) >= 0) {
      (void)write(fd,logbuffer,strlen(logbuffer)); 
      (void)write(fd,"\n",1);      
      (void)close(fd);
   }
   if(type == ERROR || type == NOTFOUND || type == FORBIDDEN) exit(3);
}

/* this is a child web server process, so we can exit on errors */
void web(int fd, int hit, char *shmp)
{
   int j, file_fd, buflen;
   long i, ret, len;
   char * fstr = "text/plain";
   char *idx, *key, *value = NULL;
   char log[200];
   static char buffer[BUFSIZE+1]; /* static so zero filled */

   ret =read(fd,buffer,BUFSIZE); 	/* read Web request in one go */
   if(ret == 0 || ret == -1) {	/* read failure stop now */
      logger(FORBIDDEN,"failed to read browser request","",fd);
   }

   if(ret > 0 && ret < BUFSIZE)	/* return code is valid chars */
      buffer[ret]=0;		/* terminate the buffer */
   else buffer[0]=0;

   for(i=0;i<ret;i++)	        /* remove CF and LF characters */
      if(buffer[i] == '\r' || buffer[i] == '\n')
         buffer[i]='*';

   logger(LOG,"request",buffer,hit);

   idx = buffer;
   if( strncmp(idx,"GET ",4) && strncmp(idx,"get ",4) ) {
      logger(FORBIDDEN,"Only simple GET operation supported",buffer,fd);
   }

   /* check for token .. */
   idx += 5;
   if( strncmp(idx,settings[KEY_TOKEN],strlen(settings[KEY_TOKEN]))){ 
      logger(FORBIDDEN,"Invalid token",idx,fd);
   }

   /* Get key and null previous slash */
   idx += strlen(settings[KEY_TOKEN]);
   key = idx + 1; 

   if(!key){
      logger(ERROR,"No key",buffer,fd);
   }

   for(i=key-buffer;i<BUFSIZE;i++) { /* null terminate after next slash */
      if(buffer[i] == '/' ) {
         buffer[i] = 0;
         value = &buffer[i + 1];
      }
      if(buffer[i] == ' ') { /* Ignore rest of url, string is "GET URL " +lots of other stuff */
         buffer[i] = 0;
         break;
      }
   }
   logger(LOG,"key:",key,hit);
   value = lookup(key, value, (Trigger *)shmp);
   len = strlen(value);

   (void)sprintf(buffer,"HTTP/1.1 200 OK\nServer: server/%d.0\nContent-Length: %ld\nConnection: close\nContent-Type: %s\n\n", 
         VERSION, len, fstr); /* Header + a blank line */

   logger(LOG,"Header",buffer,hit);
   (void)write(fd,buffer,strlen(buffer));
   bzero(buffer,BUFSIZE);
   (void)sprintf(buffer," + Key %s value %s length %ld\n",key,value,len);
   logger(LOG,"Payload",buffer,hit);
   (void)write(fd,buffer,strlen(buffer));

   sleep(1);	/* allow socket to drain before signalling the socket is closed */
   close(fd);
   exit(1);
}

int main(int argc, char **argv)
{
   int i, shmid, port, pid, listenfd, socketfd, hit;
   key_t key;
   char *data;
   socklen_t length;
   static struct sockaddr_in cli_addr; /* static = initialised to zeros */
   static struct sockaddr_in serv_addr; /* static = initialised to zeros */

   if( argc < 3  || argc > 3 || !strcmp(argv[1], "-?") ) {
      (void)printf("hint: server Port-Number Top-Directory\t\tversion %d\n\n"
            "\tserver is a small and very safe mini web server\n"
            "\tThere is no fancy features = safe and secure.\n\n"
            "\tExample: server 8181 /home/serverdir &\n\n", VERSION);
      exit(0);
   }
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
   /////////* Become deamon + unstopable and no zombies children (= no wait()) */
   ////////if(fork() != 0)
   ////////	return 0; /* parent returns OK to shell */
   ////////(void)signal(SIGCLD, SIG_IGN); /* ignore child death */
   ////////(void)signal(SIGHUP, SIG_IGN); /* ignore terminal hangups */
   ////////for(i=0;i<32;i++)
   ////////	(void)close(i);		/* close open files */
   ////////(void)setpgrp();		/* break away from process group */
      if ((key = ftok(argv[0], 'R')) == -1) /*Here the file must exist */ 
    {
        perror("ftok");
        exit(1);
    }

    /*  create the segment: */
    if ((shmid = shmget(key, SHMSIZE, 0644 | IPC_CREAT)) == -1) {
        perror("shmget");
        exit(1);
    }

    /* attach to the segment to get a pointer to it: */
    data = shmat(shmid, (void *)0, 0);
    if (data == (char *)(-1)) {
        perror("shmat");
        exit(1);
    }

   memset(data, 0, SHMSIZE);

   logger(LOG,"server starting",argv[1],getpid());
   /* setup the network socket */
   if((listenfd = socket(AF_INET, SOCK_STREAM,0)) <0)
      logger(ERROR, "system call","socket",0);
   port = atoi(argv[1]);
   if(port < 0 || port >60000)
      logger(ERROR,"Invalid port number (try 1->60000)",argv[1],0);
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
         if(pid == 0) { 	/* child */
            (void)close(listenfd);
            web(socketfd,hit,data); /* never returns */
         } else { 	/* parent */
            (void)close(socketfd);
         }
      }
   }
       /* detach from the segment: */
    if (shmdt(data) == -1) {
        perror("shmdt");
        exit(1);
    }
}
