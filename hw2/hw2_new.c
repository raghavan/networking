#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/types.h>
#include <sys/uio.h>
#include <sys/socket.h>
#include <sys/select.h>
#include <netinet/in.h>
#include <errno.h>
#include <sys/stat.h>
#include <glob.h>
#include <pthread.h>

#define RESPONSE_HEADER_TPL "HTTP/1.0 200 OK\r\nstatus: 200 OK\r\nversion: HTTP/1.0\r\nContent-Type: %s\r\n\r\n"
#define CONTENT_TYPE_TEXT "text/html"
#define CONTENT_TYPE_JPG  "image/jpeg"
#define CONTENT_TYPE_GIF  "image/gif"
#define CONTENT_TYPE_ICO  "image/ico"
#define CONTENT_TYPE_PNG  "image/png"
#define CONTENT_TYPE_PDF  "application/pdf"
#define CONTENT_TYPE_404   "404"
#define PAGE_SIZE 1024

char *pagenotfound  =  "<html><body>404 page not found</body></html>\0";

glob_t readDirContents(char *folderName){
  glob_t data;   
   char *folderPath;
   char *tpl = "./%s/*";
   folderPath = (char *)malloc(strlen(folderName));
   sprintf(folderPath, tpl, folderName);
   printf("Folder path --> %s\n",folderPath);
   glob(folderPath, 0, NULL, &data ); 
   return data;
}

off_t getFileSize(const char *filename){
	struct stat file_stat;
 	int exists = stat(filename, &file_stat);
 	if (exists < 0) {
    	return strlen(pagenotfound);
 	} else {
      //printf("%ld is the file size of %s\n", file_stat.st_size, filename);
 	}
 	return file_stat.st_size;
}

static char* readcontent(const char *filename){
    char c,*fcontent=NULL;
    FILE *fp;
    fp = fopen(filename, "rb");
  		unsigned long filelen = getFileSize(filename);
    	fcontent = (char*)malloc(filelen);
    	memset(fcontent,0,filelen);
	    if(fp){
	    	fread(fcontent, 1, filelen, fp);	
	    }else{
	    	return pagenotfound;
	    	//strcpy(fcontent,pagenotfound);
	    }	
	    //printf("pagenotfound --> %s\n",fcontent); 
    return fcontent;
}

typedef struct send_recv_parameter{
	int server_sock;
	unsigned int socklen; 
	struct sockaddr_in remote_addr;
	char* filename;
}parameter_t;

void* send_and_recv_pages(void* parameter){
	parameter_t *p =(parameter_t*)parameter;
	char *filename = p->filename;
		/*
			// wait for a connection
		int res = listen(p->server_sock,0);
		if(res < 0) {
			perror("Error listening for connection");
			exit(1);
		}*/
		//Accept a connection
		int sock;
		sock = accept(p->server_sock, (struct sockaddr*)&p->remote_addr, &p->socklen);
		if(sock < 0) {
			perror("Error accepting connection");
			exit(1);
		}
		
		//Receive message from client and parse it
		char buf[255], path[100], *msgText,*resp_content_type;
		memset(&buf,0,sizeof(buf));
		int recv_count = recv(sock, buf, 255, 0);
		if(recv_count<0) {
		 perror("Receive failed");	
		 exit(1); 
		}		
	    sscanf(buf,"GET /%s",path);
	    //printf("path requested-->%s\n",path);
	    char *buffer,*tpl_folder="./%s/%s";
	    unsigned long filesize;
	  	if(strcmp(path,"HTTP/1.1") == 0 || strstr(path,"..") != NULL){
	  		resp_content_type = (char *)malloc(strlen(CONTENT_TYPE_TEXT));
	  		resp_content_type = CONTENT_TYPE_TEXT;
	   		char folderStruct[strlen(filename)+strlen("index.html")];
	   		sprintf(folderStruct,tpl_folder,filename,"index.html");
	   		//printf("folderStruct for index --> %s\n",folderStruct);
	   		filesize = getFileSize(folderStruct);
	   		msgText = (char*)malloc(filesize);
	   		memset(msgText,0,filesize);
    		msgText = readcontent(folderStruct);
	   }else{
	   		if(strstr(path,"jpg") != NULL){
	   			resp_content_type = (char *)malloc(strlen(CONTENT_TYPE_JPG));
	   			resp_content_type = CONTENT_TYPE_JPG;
	   		}else if(strstr(path,"gif") != NULL){
	   			resp_content_type = (char *)malloc(strlen(CONTENT_TYPE_GIF));
	   			resp_content_type = CONTENT_TYPE_GIF;
	   		}else if(strstr(path,"pdf") != NULL){
	   			resp_content_type = (char *)malloc(strlen(CONTENT_TYPE_PDF));
	   			resp_content_type = CONTENT_TYPE_PDF;
	   		}else if(strstr(path,"ico") != NULL){
	   			resp_content_type = (char *)malloc(strlen(CONTENT_TYPE_ICO));
	   			resp_content_type = CONTENT_TYPE_ICO;
	   		}else if(strstr(path,"png") != NULL){
	   			resp_content_type = (char *)malloc(strlen(CONTENT_TYPE_PNG));
	   			resp_content_type = CONTENT_TYPE_PNG;
	   		}else{
	   			resp_content_type = (char *)malloc(strlen(CONTENT_TYPE_TEXT));
	   			resp_content_type = CONTENT_TYPE_TEXT;
	   		}
	   		//printf("filesize --> %lu\n",filesize);
	   		char folderStruct[strlen(filename)+strlen(path)];
	   		memset(folderStruct,0,(strlen(filename)+strlen(path)));
	   		sprintf(folderStruct,tpl_folder,filename,path);
	   		//printf("size of folderStruct after allocation--> %d\n",sizeof(folderStruct) );
	   		//printf("folderStruct for others --> %s\n",folderStruct);
	   		filesize = getFileSize(folderStruct);
	   		msgText = (char*)malloc(filesize);
	   		memset(msgText,0,filesize);
	   		msgText = readcontent(folderStruct);
	   		//printf(" msgText --> %s\n",msgText); 	
	   		if(strstr(msgText,"404")){
	   			resp_content_type = (char *)malloc(strlen(CONTENT_TYPE_404));
	   			resp_content_type = CONTENT_TYPE_404;
	   		}
	   		//printf("size of msgText --> %d\n",strlen(msgText));  	
	   }
	       		
  		//To send message back to client
	   	char resp_header[strlen(RESPONSE_HEADER_TPL)+strlen(resp_content_type)];
	   	sprintf(resp_header,RESPONSE_HEADER_TPL,resp_content_type);
	   	//printf("RESPONSE_HEADER --> %s\n",resp_header);

/*	   	char *tpl_new = "%s%s";
	   	printf("resp_header size --> %d\n",strlen(resp_header));
	   	printf("filesize  --> %ld\n",filesize);
	   	unsigned long msgToClientSize = strlen(resp_header)+filesize;
   		char msgToClient[msgToClientSize];
   		sprintf(msgToClient, tpl_new, resp_header,msgText);
   		printf("size of msgToClient --> %ld\n", msgToClientSize);*/


   		unsigned long msgToClientSize = strlen(resp_header)+filesize;
   		char msgToClient[msgToClientSize];
   		memset(msgToClient,0,msgToClientSize);
   		strcpy(msgToClient,resp_header);
   		//printf("header-->%s",msgToClient);
   		//printf("current length of msgToClient --> %d\n",strlen(msgToClient));
   		int counter = strlen(resp_header),i=0;
   		for(i;i<filesize;i++){
   			msgToClient[counter++] = msgText[i];
   		}
   		//printf("msgToClient-->%s",msgToClient);
   		//printf("current length of msgToClient --> %d\n",strlen(msgToClient));


		int sendStatus = send(sock, msgToClient, msgToClientSize, 0);
		if(sendStatus<0){
			perror("Not able to send");
		}
		shutdown(sock,SHUT_RDWR);
		close(sock);
}

int main(int argc, char* argv[]) {
	//Create socket
	int server_sock = socket(AF_INET, SOCK_STREAM, 0);
	if(server_sock < 0) {
		perror("Creating socket failed: ");
		exit(1);
	}
	
	// Allow fast reuse of ports 
	int reuse_true = 1;
	setsockopt(server_sock, SOL_SOCKET, SO_REUSEADDR, &reuse_true, sizeof(reuse_true));

	struct sockaddr_in addr; 	// internet socket address data structure
	addr.sin_family = AF_INET;	
	addr.sin_port = htons(atoi(argv[1])); // byte order is significant
	addr.sin_addr.s_addr = INADDR_ANY; // listen to all interfaces
	
	int res = bind(server_sock, (struct sockaddr*)&addr, sizeof(addr));
	if(res < 0) {
		perror("Error binding to port");
		exit(1);
	}

	struct sockaddr_in remote_addr;
	unsigned int socklen = sizeof(remote_addr); 
	

	pthread_t threadHandle[5];
	static long i=0;
	printf("New call coming In!!!");
	parameter_t parameter = {server_sock,socklen,remote_addr,argv[2]};
	int ret;
	while(1) {
			// wait for a connection
		int res = listen(server_sock,0);
		if(res < 0) {
			perror("Error listening for connection");
			exit(1);
		}
		 ret = pthread_create(&threadHandle[i],NULL,send_and_recv_pages,(void*)&parameter);
		//printf("theard count --> %l\n",i);
		if(ret != 0){
			perror("Thread creation failed");
			return 1;
		}
		pthread_join(threadHandle[i],0);
		i++;
		//send_and_recv_pages(server_sock,socklen,remote_addr,ar;	
	}
       //globfree(&dataFromFolder);
       shutdown(server_sock,SHUT_RDWR);
}
