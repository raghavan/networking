#include <stdio.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <unistd.h>
#include <err.h>
#include <stdlib.h>
#include <sys/stat.h>

#define BUFSIZE 5000
#define USERAGENT "Mozilla/5.0 (Macintosh; Intel Mac OS X 10_8_0) AppleWebKit/537.1 (KHTML, like Gecko) Chrome/21.0.1180.89 Safari/537.1"

int main(int argc,char *argv[]){
   int sock = socket(AF_INET,SOCK_STREAM,0);
   if(sock<0){
      perror("sock could not be formed successfully");
      exit(1);
   }
   
   struct addrinfo hints, *res,*p;
   int status;

   if (argc != 2) {
       perror("usage: showip hostname\n");
       exit(1);
   }

   memset(&hints, 0, sizeof hints);
   hints.ai_family = AF_INET;
   hints.ai_socktype = SOCK_STREAM;

   
   //Strip url from the given argument
   char *url = argv[1], domainname[25], getpage[50];         
   url += 7; 


   
   //To get the domain name and the get page
   int i=0,j=0;
   for(i=0;i<strlen(url);i++){
      if(url[i] != '/'){
         domainname[j] = url[i];      
         j++;
      }else{        
         break;
      }
   }
   domainname[j] = 0;   
   j=0;
   for(i++;i<strlen(url);i++){
      getpage[j] = url[i];
      j++;
   }
   getpage[j]=0;

   if ((status = getaddrinfo(domainname, NULL, &hints, &res) != 0)) {
      perror("Not able to retrieve address");
      exit(1);
   }

 
   //To get an ip address for the given url
   char *ipcontact = NULL, ipstr[INET6_ADDRSTRLEN];//getIpStr(res);
   for(p = res;p != NULL; p = p->ai_next) {
      void *addr;
      char *ipver;
      if (p->ai_family == AF_INET) { //For IPv4
         struct sockaddr_in *ipv4 = (struct sockaddr_in *)p->ai_addr;
         addr = &(ipv4->sin_addr);
         ipver = "IPv4";
         inet_ntop(p->ai_family, addr, ipstr, sizeof ipstr);
         ipcontact = ipstr;
      }  
   }



   //Make a socket connection
   struct sockaddr_in sock_addr;
   sock_addr.sin_family = AF_INET;
   sock_addr.sin_port = htons(80);
   sock_addr.sin_addr.s_addr=inet_addr(ipcontact);
   int connection = connect(sock,(struct sockaddr*)&sock_addr,sizeof(sock_addr));
   if(connection < 0){
      perror("Not able to connect");   
   }

   
   //Make a GET request
   char *getmsg;
   char *tpl = "GET /%s HTTP/1.0\nHOST: %s\nUser-Agent: %s\n\n";
   getmsg = (char *)malloc(strlen(domainname)+strlen(url)+strlen(USERAGENT)+strlen(tpl)-5);
   sprintf(getmsg, tpl, getpage, domainname, USERAGENT);
   send(sock,getmsg,strlen(getmsg),0);
   

   //Constructing File name
   char filename[20],tmpFile[20];
    i=0,j=0;
    int slash_flag=0,filenameexist=0;    
   for(i=0;i<strlen(url);i++){
      if(slash_flag == 0 && url[i] == '/') {
         slash_flag = 1;
         continue;         
      }else if(slash_flag == 1 && url[i] == '/') {
         j=0;        
         continue; 
      }
      if(slash_flag == 1){
         tmpFile[j] = url[i];
         j++;
         if(url[i] == '.'){
            filenameexist = 1;
         }
      }
   }
   tmpFile[j] = 0;
   if(j!=0 || filenameexist == 1){
      strcpy(filename,tmpFile);
   }else{
      strcpy(filename,"index.html\0");
   }

   
   //Open file connection
   FILE *fp = fopen(filename, "a");  
     
   //Read the html content   
   int contentStarts = 0;
   char *htmlcontent,*errcode;
   char buf[BUFSIZE+10];
   int datasize=1, totalsize=0;

    while (datasize > 0){
      datasize = read(sock, buf, BUFSIZE);

      if(contentStarts == 0){
         errcode = strstr(buf,"404");
         if(errcode == NULL){
            errcode = strstr(buf,"302");
          }
         if(errcode != NULL){
            perror("Page does not exist");
            exit(1);
         }

         int k=0;
         for(k;k<datasize;k++){
            if(buf[k] == '\r' && buf[k+1] == '\n' && buf[k+2] == '\r' && buf[k+3] == '\n'){
               break;
            }
         }

         htmlcontent = strstr(buf, "\r\n\r\n");

         if(htmlcontent != NULL){
            contentStarts = 1;
            htmlcontent+=4;            
            fwrite(htmlcontent, sizeof(char), (datasize - (k+4)), fp);   
            continue;                                        
       }else{
           htmlcontent = buf;
         }
      }else if(contentStarts == 1){      
         fwrite(buf, sizeof(char), datasize, fp);
         totalsize += datasize;
      }
      memset(buf, 0, BUFSIZE);
    }
    fflush(fp);

    fclose(fp);

   shutdown(sock,SHUT_RDWR);

}
