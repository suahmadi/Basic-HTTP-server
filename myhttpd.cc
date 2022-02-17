#include <stdlib.h>
#include <stdio.h>
#include <string>
#include <unistd.h>
#include <dirent.h>
#include <iostream>
#include <time.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <dlfcn.h>
#include <iomanip>
#include <cstring>
#include <pthread.h>
#include <thread>
#include <fstream>
#include <netdb.h>
#include <string.h>
#include <sys/wait.h>
#include <limits.h>
#include <bits/stdc++.h>
#include <link.h>
#include <vector>
#include <errno.h>


struct arg_struct {
   int masterSocket;
   sockaddr_in *sockInfo;
   int *alen;
}pool;

struct file_struct{
        std::string type;
        std::string path;
        std::string name;
        std::string time;
        std::string size;

};

void ThreadForEach(int master, sockaddr_in *socket_info, int *len, int slaveSocket);
void PoolOfThreads(int master, sockaddr_in socket_info, int len);
void processHTTP(int fd);
void processHTTPthread(int socket_ptr);
void module(int fd, char* cwd, char* cgi);


pthread_mutex_t mutex;
pthread_mutexattr_t mattr;
int concurrency_type = -1;
FILE * stats;
int port = 0;
FILE * logs;
int QueueLength = 5;
void poolSlave(int masterSocket, sockaddr_in *sockInfo, int *alen);
void pool_loop(arg_struct *pool);
void list_dir(int m, char* cwd, int socket);
typedef void (*httprunfunc)(int ssock, const char* querystring);
int request_count = 0;
struct tm* time_info;
long double max_time = 0;
char max_url[512] = {0};
char min_url[512] = {0};
long double min_time = 2000000000;


extern "C" void zombie_Kill(int sig);

extern "C" void zombie_Kill(int sig)
{
        while(waitpid(-1, NULL, WNOHANG) > 0) {
          continue;
        };
}

const char *realm = "Sultan Server";
const char *password = "U3VsdGFuOkFsYWhtYWRpCg==";
const char *correct_format = "\nthe correct fromat of usage is the following: myhttpd [-f|-t|p] [<port>]\n";

int main(int argc, char **argv)
{

  //check if the input is format is correct, if it's not then return error.
  if (argc < 2 || argc > 3) {
     fprintf(stderr, "%s", correct_format);
     exit(-1);
   }

  if (argc == 3) {
    if (strcmp(argv[1], "-f") == 0) {
        concurrency_type = 0;
    } else if (strcmp(argv[1], "-t") == 0) {
        concurrency_type = 1;

    } else if (strcmp(argv[1], "-p") == 0) {
        concurrency_type = 2;
    } else {
      fprintf(stderr, "%s", correct_format);
      exit(-1);
    }
    port = atoi(argv[2]);

  } else {
    port = atoi(argv[1]);
  }

  printf("the mode is %s and the mode is %d\n", argv[1], port);

  //Setting the IP address and port
  struct sockaddr_in serverIPAddress;
  memset(&serverIPAddress, 0, sizeof(serverIPAddress));
  serverIPAddress.sin_family = AF_INET;
  serverIPAddress.sin_addr.s_addr = INADDR_ANY;
  serverIPAddress.sin_port = htons((u_short) port);

  // Allocate a socket
  int masterSocket = socket(PF_INET, SOCK_STREAM, 0);
  if ( masterSocket < 0) {
     perror("socket");
     exit( -1 );
  }


  int optval = 1;
  int err = setsockopt(masterSocket, SOL_SOCKET
      , SO_REUSEADDR, (char *) &optval, sizeof( int ));
  int error = bind( masterSocket,(struct sockaddr *) &serverIPAddress, sizeof(serverIPAddress) );

  if ( error ) {
    perror("bind");
    exit(-1);
  }

  error = listen( masterSocket, QueueLength);
  if ( error ) {
      perror("listen");
      exit(-1);
  }

  time_t rawtime;
  time ( &rawtime );
  time_info = localtime ( &rawtime );


  struct sigaction zombie;
  zombie.sa_handler = zombie_Kill;
  sigemptyset(&zombie.sa_mask);
  zombie.sa_flags = SA_RESTART;

  if(sigaction(SIGCHLD, &zombie, NULL)) {
    perror("sigaaction child");
    exit(2);
  }

  while ( 1 ) {
   
    struct sockaddr_in clientIPAddress;
    int alen = sizeof( clientIPAddress );
    int slaveSocket = accept ( masterSocket, (struct sockaddr * )&clientIPAddress,
        (socklen_t *) &alen);

    if (slaveSocket < 0) {
       perror("accept");
       exit(-1);
    }


     int res = 0;
    if (concurrency_type == 0) {

        if (slaveSocket == -1 && errno == EINTR) {
            continue;
        }

        int ret = fork();
        if (ret == 0) {
            processHTTP(slaveSocket);
            shutdown(slaveSocket, SHUT_WR);
            exit(0);
        } 

     close(slaveSocket);
     

    } else if (concurrency_type == -1) {
       processHTTP(slaveSocket);

       shutdown(slaveSocket, SHUT_WR);

       close(slaveSocket);
    }
    else if (concurrency_type == 1) {
       ThreadForEach(masterSocket, &clientIPAddress, &alen, slaveSocket);

    }
    else if (concurrency_type == 2) {
       poolSlave(masterSocket, &clientIPAddress, &alen);
    }
  }

}



bool compare_names(file_struct a, file_struct b){
        return(a.name < b.name);
}
bool compare_times(file_struct a, file_struct b){
        return(a.time < b.time);
}
bool compare_sizes(file_struct a, file_struct b){
        return(a.size < b.size);
}
bool compare_namesT(file_struct a, file_struct b){
        return(a.name > b.name);
}
bool compare_timesT(file_struct a, file_struct b){
        return(a.time < b.time);
}
bool compare_sizesT(file_struct a, file_struct b){
        return(a.size > b.size);
}


void list_dir(int m, char* cwd, int fd ) {


    DIR *dir;
    int mode = m;
    struct dirent *ent;
 
    std::string n_s = "C=N;O=A",m_s = "C=M;O=A",s_s = "C=S;O=A",d_s = "C=D;O=A";

    if(mode == 0){
        n_s= "C=N;O=D";
    }
    if(mode == 2){
        m_s= "C=M;O=D";
    }
    if(mode == 4){
        s_s= "C=S;O=D";
    }
    if(mode == 6){
        d_s ="C=D;O=D";
    }

    if ((dir = opendir (cwd)) != NULL) {
        const char *protocol = "HTTP/1.0 200 Document follows";
        const char *crlf = "\r\n";
        const char *server = "Server: CS 252 lab5/1.0";
        const char *content_type = "Content-type: ";

        write(fd, protocol, strlen(protocol));
        write(fd, crlf, strlen(crlf));
        write(fd, server, strlen(server));
        write(fd, crlf, strlen(crlf));
        write(fd, content_type, strlen(content_type));
        write(fd, crlf, strlen(crlf));
        write(fd, crlf, strlen(crlf));
        
        char blank[512] = {0};
        strcpy(blank,"/icons/blank.gif");
        //strcpy(blank, cwd);
        printf("the blank is the following %s\n",blank);
        char htmltext[1024];
        sprintf(htmltext, "<!DOCTYPE HTML PUBLIC \"-//W3C//DTD HTML 3.2 Final//EN\">\n<html>\n<head>\n<title>Index of /homes/cs252/lab5-http-server/lab5-src/http-root-dir/htdocs/dir1</title>\n</head>\n<body>\n<h1>Index of /homes/cs252/lab5-http-server/lab5-src/http-root-dir/htdocs/dir1</h1>\n<table>\n");
        write(fd,htmltext,strlen(htmltext));
        sprintf(htmltext, "<tr><th valign=\"top\"> </th><th><a href=\"?%s\">Name</a></th><th><a href=\"?%s\">Last modified</a></th><th><a href=\"?%s\">Size</a></th><th><a href=\"?%s\">Description</a></th></tr>\n<tr><th colspan=\"5\"><hr></th></tr>\n",n_s.c_str(),m_s.c_str(),s_s.c_str(),d_s.c_str());
        write(fd,htmltext,strlen(htmltext));
        char * temp = strstr(cwd, "/subdir");
        if(temp){
                sprintf(htmltext, "<tr><td valign=\"top\"><img src=\"/icons/back.gif\" alt=\"[PARENTDIR]\"></td><td><a href=\"./\">Parent Directory</a></td><td>&nbsp;</td><td align=\"right\">  - </td><td>&nbsp;</td></tr>");
        }else{
                sprintf(htmltext, "<tr><td valign=\"top\"><img src=\"/icons/back.gif\" alt=\"[PARENTDIR]\"></td><td><a href=\"../\">Parent Directory</a></td><td>&nbsp;</td><td align=\"right\">  - </td><td>&nbsp;</td></tr>");
        }
        write(fd,htmltext,strlen(htmltext));
        std::vector<file_struct> file_vector;
        std::vector<file_struct> folder_vector;
        while ((ent = readdir (dir)) != NULL) {
                char file[512] = {0};
                strcpy(file, cwd);
                char * temp = strstr(file, "/dir");
                if(temp[strlen(temp)-1] != '/'){
                        strcat(temp,"/");
                }
                strcpy(file, "/htdocs/");
                strcat(file,temp);
                strcat(file, ent->d_name);
                if(strncmp(ent->d_name, ".", 1) != 0 ){

                        char abs[512] = {0};
                        strcpy(abs, cwd);
                        strcat(abs, ent->d_name);
                        struct stat attr;
                        stat(abs, &attr);
                        float fileSize = (float)attr.st_size/1000.0;
                        struct tm *tm;
                        tm = localtime(&attr.st_mtime);
                        char timeinc[512]= {0};
                        sprintf(timeinc,"%d-%d-%d %d:%d", tm->tm_year + 1900, tm->tm_mon + 1, tm->tm_mday, tm->tm_hour, tm->tm_min);


                        char type[512] = {0};
                        int isdir = 0;
                        strcpy(type,"/icons/");
                        if(strstr(ent->d_name, ".gif") != 0){
                                strcat(type, "image.gif");
                        }else if(strstr(ent->d_name, "dir") != 0){
                                isdir = 1;
                                strcat(abs, "/");
                                strcat(type, "menu.gif");
                                temp = strstr(file, "/dir");
                                strcpy(file,temp);
                                fileSize = -1;
                        }else{
                                strcat(type, "unknown.gif");
                        }
                        char filesi[512];
                        if(fileSize == -1){
                                sprintf(filesi, "-");
                        }else{
                                sprintf(filesi, "%.2f K", fileSize);
                        }
                        file_struct tbp;
                        std::string inter(type);
                        std::string inter1(file);
                        std::string inter2(ent->d_name);
                        std::string inter3(timeinc);
                        std::string inter4(filesi);
                        if(isdir){
                                folder_vector.push_back({inter,inter1,inter2,inter3,inter4});
                        }else{
                                file_vector.push_back({inter,inter1,inter2,inter3,inter4});
                        }
                }
        }

        //sorting based on mode
   
        if(mode == 0){
                std::sort(file_vector.begin(), file_vector.end(), compare_names);
                std::sort(folder_vector.begin(), folder_vector.end(), compare_names);
        }
        if(mode == 1){
                std::sort(file_vector.begin(), file_vector.end(), compare_namesT);
                std::sort(folder_vector.begin(), folder_vector.end(), compare_namesT);
        }
        if(mode == 2){
                std::sort(file_vector.begin(), file_vector.end(), compare_times);
                std::sort(folder_vector.begin(), folder_vector.end(), compare_times);
        }
        if(mode == 3){
                std::sort(file_vector.begin(), file_vector.end(), compare_timesT);
                std::sort(folder_vector.begin(), folder_vector.end(), compare_timesT);
        }
        if(mode == 4){
                std::sort(file_vector.begin(), file_vector.end(), compare_sizes);
                std::sort(folder_vector.begin(), folder_vector.end(), compare_sizes);
        }
        if(mode == 5){
                std::sort(file_vector.begin(), file_vector.end(), compare_sizesT);
                std::sort(folder_vector.begin(), folder_vector.end(), compare_sizesT);
        }
        if(mode == 6){
                std::sort(file_vector.begin(), file_vector.end(), compare_names);
                std::sort(folder_vector.begin(), folder_vector.end(), compare_names);
        }
        if(mode == 7){
                std::sort(file_vector.begin(), file_vector.end(), compare_namesT);
                std::sort(folder_vector.begin(), folder_vector.end(), compare_namesT);
        }

        if(mode == 0 || mode == 2 || mode == 5 || mode == 6) {
                for(int i = 0; i < file_vector.size(); i++){
                        file_struct forprint;
                        forprint = file_vector[i];
                        sprintf(htmltext, "<tr><td valign=\"top\"><img src=\"%s\" alt=\"[   ]\"></td><td><a href=\"%s\">%s</a></td><td align=\"right\">%s </td><td align=\"right\"> %s</td><td>&nbsp;</td></tr>\n", forprint.type.c_str(), forprint.path.c_str(), forprint.name.c_str(), forprint.time.c_str(), forprint.size.c_str());
                        write(fd,htmltext,strlen(htmltext));
                }
                for(int i = 0; i < folder_vector.size(); i++){
                        file_struct forprint;
                        forprint = folder_vector[i];
                        sprintf(htmltext, "<tr><td valign=\"top\"><img src=\"%s\" alt=\"[   ]\"></td><td><a href=\"%s\">%s</a></td><td align=\"right\">%s </td><td align=\"right\"> %s</td><td>&nbsp;</td></tr>\n", forprint.type.c_str(), forprint.path.c_str(), forprint.name.c_str(), forprint.time.c_str(), forprint.size.c_str());
                        write(fd,htmltext,strlen(htmltext));
                }
        }
        if(mode == 1 || mode == 3 || mode == 4 || mode == 7){
                for(int i = 0; i < folder_vector.size(); i++){
                        file_struct forprint;
                        forprint = folder_vector[i];
                        sprintf(htmltext, "<tr><td valign=\"top\"><img src=\"%s\" alt=\"[   ]\"></td><td><a href=\"%s\">%s</a></td><td align=\"right\">%s </td><td align=\"right\"> %s</td><td>&nbsp;</td></tr>\n", forprint.type.c_str(), forprint.path.c_str(), forprint.name.c_str(), forprint.time.c_str(), forprint.size.c_str());
                        write(fd,htmltext,strlen(htmltext));
                }
                for(int i = 0; i < file_vector.size(); i++){
                        file_struct forprint;
                        forprint = file_vector[i];
                        sprintf(htmltext, "<tr><td valign=\"top\"><img src=\"%s\" alt=\"[   ]\"></td><td><a href=\"%s\">%s</a></td><td align=\"right\">%s </td><td align=\"right\"> %s</td><td>&nbsp;</td></tr>\n", forprint.type.c_str(), forprint.path.c_str(), forprint.name.c_str(), forprint.time.c_str(), forprint.size.c_str());
                        write(fd,htmltext,strlen(htmltext));
                }
        }
        closedir (dir);

    } else {
        perror ("");
        exit(-1);
    }

}

void ThreadForEach (int master, sockaddr_in *socket_info, int* len, int slaveSocket) {
  while (1) {
    int clientSocket = accept(master, (struct sockaddr*) socket_info, (socklen_t *) len);
          pthread_t thread;
          pthread_attr_t attr;
          pthread_attr_init(&attr);
          pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);
          pthread_attr_setscope(&attr, PTHREAD_SCOPE_SYSTEM);
          pthread_create(&thread, &attr, (void *(*)(void *)) processHTTPthread,(void *) clientSocket);
  }
}

void poolSlave(int masterSocket, sockaddr_in *sockInfo, int *alen) {

    pool.masterSocket = masterSocket;
    pool.sockInfo = sockInfo;
    pool.alen = alen;
    for(int i = 0; i < 4; i++) {
        pthread_t thread;
        pthread_attr_t attr;
        pthread_attr_init(&attr);
        pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);
        pthread_attr_setscope(&attr, PTHREAD_SCOPE_SYSTEM);
        pthread_create(&thread, &attr, (void *(*)(void *)) pool_loop, (void *) &pool);
    }

    pool_loop(&pool);

}

void pool_loop(arg_struct *pool) {
  while(1) {
    int clientSocket = accept(pool->masterSocket, (struct sockaddr *) pool->sockInfo, (socklen_t *) pool->alen);
    if (clientSocket >= 0) {
      processHTTP(clientSocket);
    }
    shutdown(clientSocket, SHUT_WR);
    close(clientSocket);
  }
}

void processHTTPthread (int socket) {
    processHTTP(socket);
    shutdown(socket, SHUT_WR);
    close(socket);
}


void module(int fd, char* cwd, char* cgi){
        //printf("I am in the module and CWD is = %s\n and cgi is %s\n", cwd, cgi);
        void * lib = dlopen(cwd, RTLD_LAZY);
        if(lib != NULL){
                httprunfunc httprun;
                httprun = (httprunfunc) dlsym(lib, "httprun");
                if(httprun != NULL){
                        const char *protocol = "HTTP/1.0 200 Document follows";
                        const char *crlf = "\r\n";
                        const char *server = "Server: CS 252 lab5/1.0";
                        const char *content_type = "Content-type: ";
                        write(fd, protocol, strlen(protocol));
                        write(fd, crlf, strlen(crlf));
                        write(fd, server, strlen(server));
                        write(fd, crlf, strlen(crlf));
                        httprun(fd, cgi);

                } else {
                        write(fd, "HTTP/1.1 404 File Not Found", 27);
                        write(fd, "\r\n", 2);
                        write(fd, "Server: cs 252", 14);
                        write(fd, "\r\n", 2);
                        write(fd, "Content-type: text/html", 23);
                        write(fd, "\r\n", 2);
                        write(fd, "\r\n", 2);
                        write(fd, "File not found", 14);
                }
        } else {
                        write(fd, "HTTP/1.1 404 File Not Found", 27);
                        write(fd, "\r\n", 2);
                        write(fd, "Server: cs 252", 14);
                        write(fd, "\r\n", 2);
                        write(fd, "Content-type: text/html", 23);
                        write(fd, "\r\n", 2);
                        write(fd, "\r\n", 2);
                        write(fd, "File not found", 14);
        }
}


void processHTTP (int fd) {

   auto begin= std::chrono::system_clock::now();

   pthread_mutex_lock(&mutex);
   request_count++;
   pthread_mutex_unlock(&mutex);

  const int MaxBuffer = 40960;
  char buffer[ MaxBuffer + 1 ];
  char docPath[ MaxBuffer + 1];
  int bufferLength = 0;
  int n;

 
  unsigned char newChar;

  // Last character read
  unsigned char lastChar = 0;

  //
  // The http client should send GET <file> other data
  // <cr<lf>><cr><lf>
  // Read the data client character by character until a
  // <CR><LF><CR><LF> is found.

  int gotGet = 0;
  int gotDoc = 0;
  int CRLF = 0;
  int x = ' ';
  int y = ' ';
 // printf("I am in line 262\n");
/*
  const char * getA = 
    "HTTP/1.1 401 Unauthorized\r\nWWW-Authenticate: Basic realm=\"";

  write(fd, getA, strlen(getA));
  write(fd, realm, strlen(realm));
  write(fd, "\"", 1);
  write(fd, "\r\n\r\n", strlen("\r\n\r\n"));
  */

  while ((n = read(fd, &newChar, sizeof(newChar)))>0 && bufferLength < 1024 ) {
    
    if (newChar == ' ') {
  
        if (gotGet == 0) {
          gotGet = 1;
        } else if (gotGet == 1) {
    
           buffer[bufferLength] = 0;
           strcpy(docPath, buffer);
           gotGet = 2;
        }
    } else if (newChar == '\n' && lastChar == '\r') {
        break;
    } else {
    
      y = x;
      x = lastChar;
      lastChar = newChar;
      if (gotGet == 1) {
      buffer[bufferLength] = newChar;
      bufferLength++;
      }
    }

  }
  printf("buffer=%s\n", buffer);
  printf("docpath=%s\n", docPath);
  char path[2000];
  getcwd(path, 200);


  printf("cwd = %s\n", path);

  char newPath[2000] = {0};
  
  char cwd[512] = {0};
  int check_dir = 0;
  char cgi[4096] = {0};
  int check_cgi = 0;
  int mode = 0;
  getcwd(cwd, sizeof(cwd));
  if(strncmp(docPath,"/icons", strlen("/icons")) == 0 || strncmp(docPath, "/htdocs", strlen("/htdocs")) == 0) {
    newPath[0] = 0;
  
    strcat(cwd, "/http-root-dir/");
    strcat(cwd, docPath);


    printf("doc path inside icon %s\n", docPath);
  } 
   else if(strncmp(docPath, "/cgi-bin", strlen("/cgi-bin")) == 0){
                 strcat(cwd, "/http-root-dir/");
                //strcat(cwd, docPath);
                check_cgi = 1;
                if(strstr(docPath, "?")!=0){
                        char * temp = strstr(docPath, "?");
                        memcpy(cgi, &temp[1], strlen(temp)-1 );
                        strncat(cwd, docPath,strlen(docPath) - strlen(cgi) - 1);
                }else{
                        strcpy(cgi,"");
                        strcat(cwd, docPath);
                }
                if(strstr(docPath, ".so")!=0){
                        printf("I am in 641 the cwd is %s the docPath is %s the cgi is %s \n", cwd, docPath, cgi);
                        module(fd, cwd, cgi);
                        close(fd);
                        return;
                }
   }
    else

  if (strcmp(docPath, "/") == 0) {
    //newPath[0] = 0;
    //strcat(newPath, cwd);
    //strcat(newPath, "/http-root-dir/");
    //strcat(newPath, docPath);
    strcat(cwd, "/http-root-dir/");
    strcat(cwd, "htdocs/index.html");

  } else if (strncmp(docPath, "/dir", strlen("/dir")) == 0) {
      check_dir = 1;
                strcat(cwd, "/http-root-dir/htdocs");
                newPath[0] = 0;
                char _newPath [512] = {0};
                if(strstr(docPath, "C=N;O=A") != 0){
                        mode = 0;
                        strncpy (_newPath, docPath,strlen(docPath) - 8);
                }else if(strstr(docPath, "C=M;O=A") != 0){
                        mode = 2;
                        strncpy (_newPath, docPath, strlen(docPath) - 8);
                }else if(strstr(docPath, "C=N;O=D") != 0){
                        mode = 1;
                        strncpy (_newPath, docPath,strlen(docPath) - 8);
                }else if(strstr(docPath, "C=S;O=A") != 0){
                        mode = 4;
                        strncpy (_newPath, docPath,strlen(docPath) - 8);
                }else if(strstr(docPath, "C=M;O=D") != 0){
                        mode = 3;
                        strncpy (_newPath, docPath,strlen(docPath) - 8);
                }else if(strstr(docPath, "C=S;O=D") != 0){
                        mode = 5;
                        strncpy (_newPath, docPath,strlen(docPath) - 8);
                }else if(strstr(docPath, "C=D;O=A") != 0){
                        mode = 6;
                        strncpy (_newPath, docPath,strlen(docPath) - 8);
                }else if(strstr(docPath, "C=D;O=D") != 0){
                        mode = 7;
                        strncpy (_newPath, docPath,strlen(docPath) - 8);
                }else{
                strcpy (_newPath, docPath);
                }
                strcat(cwd, _newPath);
                strcat(newPath, cwd);
    } else {
    newPath[0] = 0;
    strcat(newPath, cwd);
    strcat(newPath, "/http-root-dir/htdocs");
    strcat(newPath, docPath);

    strcat(cwd, "/http-root-dir/");
    strcat(cwd, "htdocs");
    strcat(cwd, docPath);
  }

  printf("the new path is %s\n", newPath);


   char hostname[512] = {0};
        if(gethostname(hostname,sizeof(hostname))==0){
                logs = fopen("./http-root-dir/htdocs/logs", "a");
                fprintf(logs, "%s:%d %s\n", hostname,port, docPath);
                fclose(logs);
        }


  if (strstr(docPath, "..") != 0) {
      char temp[1024] = {0};
      char *temp_x = realpath(cwd, temp);
      if (temp_x) {
          if (strlen(temp) >= strlen(cwd) + 21) {
                  strcpy(cwd, temp);
          }
      }
  }

  int im = 0;
  char conType[1024];

  if (strstr(docPath, ".html") != 0) {
      strcpy(conType, "text/html");
  } else if (strstr(docPath, ".jpg") != 0) {
      strcpy(conType, "image/jpeg");
      im = 1;
  } else if (strstr(docPath, ".gif") != 0) {
     strcpy(conType, "image/gif");
     im = 1;
  } else {
      strcpy(conType, "text/plain");
  }
  int gotAuth = 0;

  /*
  if (strcmp(buffer, "/") == 0) {

    newPath[0]= 0;
    strcat(newPath, cwd);
    strcat(newPath, "/http-root-dir/htdocs/index.html");
  if (gotAuth != 1) {
  const char * getA = 
    "HTTP/1.1 401 Unauthorized\r\nWWW-Authenticate: Basic realm=\"";

  write(fd, getA, strlen(getA));
  write(fd, realm, strlen(realm));
  write(fd, "\"", 1);
  write(fd, "\r\n\r\n", strlen("\r\n\r\n"));
  
  gotAuth = 1;
  }

  } */


  
/*
  const char * getA = 
    "HTTP/1.1 401 Unauthorized\r\nWWW-Authenticate: Basic realm=\"";

  write(fd, getA, strlen(getA));
  write(fd, realm, strlen(realm));
  write(fd, "\"", 1);
  write(fd, "\r\n\r\n", strlen("\r\n\r\n"));
*/

  FILE *fdr;
  if (check_cgi == 0 && check_dir == 0) {
  fdr = fopen(cwd, "rb");
  //printf("I am in line 370\n");
  if (fdr > 0) {
  
    /*
  const char * hdrResponse = 
    "HTTP/1.1 200 Document follows\r\n"
    "Server: CS252 Summer 2021\r\n"
    "Content-type: text/html\r\n"
    "\r\n";
  const char * simpleHTML = 
    "<H1> CS252 SUMMER 2021 </H1>"
    "WELCOME TO OUR SERVER"
    ;*/
     const char *protocol = "HTTP/1.0 200 Document follows";
                        const char *crlf = "\r\n";
                        const char *server = "Server: CS 252 lab5/1.0";
                        const char *content_type = "Content-type: ";

                        write(fd, protocol, strlen(protocol));
                        write(fd, crlf, strlen(crlf));
                        write(fd, server, strlen(server));
                        write(fd, crlf, strlen(crlf));
                        write(fd, content_type, strlen(content_type));
                        write(fd, crlf, strlen(crlf));
                        write(fd, conType, strlen(conType));
                        write(fd, crlf, strlen(crlf));
                        write(fd, crlf, strlen(crlf));



  
   long co = 0;
   char c;
   int count = 0;
   while (co = read(fileno(fdr), &c, sizeof(c))) {
     //printf("I am in line 398");
        if (co != write(fd, &c, sizeof(c))){
            perror("write");
            //exit(0);
        }
   }
   fclose(fdr);
   } else {
      perror("in the else");
    
   }
  


  } else if (check_dir == 1) {
      list_dir(mode, cwd, fd);
  } else if (check_cgi == 1) {

                printf("env: %s\n",getenv("QUERY_STRING"));
                printf("cgi: %s\n",cgi);
                int ret = fork();
                int defout = dup(1);
                dup2(fd, 1);
                if (ret == 0){
                        setenv("REQUEST_METHOD", "GET", 1);
                        setenv("QUERY_STRING", cgi, 1);
                        const char *protocol = "HTTP/1.0 200 Document follows";
                        const char *crlf = "\r\n";
                        const char *server = "Server: CS 252 lab5/1.0";
                        const char *content_type = "Content-type: ";

                        write(fd, protocol, strlen(protocol));
                        write(fd, crlf, strlen(crlf));
                        write(fd, server, strlen(server));
                        write(fd, crlf, strlen(crlf));
                        //write(socket, crlf, strlen(crlf));
                        std::vector<char*> chararg{};
                        chararg.push_back(const_cast<char*>(cgi));
                        printf("cwd: %s, cgi: %s\n", cwd, chararg.data()[0]);
                        //char *env_arg[1000];
                        execvp(cwd, chararg.data());
                        //printf("the cwd is %s\n", cwd);
                        //system(cwd);
                        //execvp(cwd, env_arg);
                        //printf("the envs are %s\n", env_arg);
                }else{
                        waitpid(ret, NULL, 0);
                }
                dup2(defout, 1);
                close(defout);


           }

        auto end= std::chrono::system_clock::now();;
        auto duration= end-begin;
        typedef std::chrono::duration<long double,std::ratio<1,1000>> MyMilliSecondTick;
        MyMilliSecondTick milli(duration);
        if(milli.count() > max_time){
                max_time = milli.count();
                strcpy(max_url, docPath);
        }
        if(milli.count() < min_time){
                min_time = milli.count();
                strcpy(min_url, docPath);
        }
        stats = fopen("./http-root-dir/htdocs/stats", "w");
        fprintf(stats, "Student Name: Sultan Alahmadi (alahmadi).\nServer set up time: %s",asctime(time_info));
        fprintf(stats, "Number of requests since the server started: %d\n",request_count);
        fprintf(stats, "minimum service time is %Lf ms when the URL is %s\n", min_time,min_url);
        fprintf(stats, "maximum service time is %Lf ms when the URL is %s\n", max_time,max_url);
        fclose(stats);



}





