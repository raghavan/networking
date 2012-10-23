#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <unistd.h>
#include <string.h>
#include <sys/types.h>
#include <sys/uio.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include "dns.h"

#define NAME "The name "
#define RESOLVES_TO " resolves to IP addr: "


void usage() {
	printf("Usage: hw2 [-d] -n nameserver -i domain/ip_address\n\t-d: debug\n");
	exit(1);
}

int debug,nameserver_flag,counter=0;
char *hostname_original;

/* constructs a DNS query message for the provided hostname */
int construct_query(uint8_t* query, int max_query, char* hostname) {
	memset(query,0,max_query);

	in_addr_t rev_addr=inet_addr(hostname);
	if(rev_addr!=INADDR_NONE) {
		static char reverse_name[255];		
		sprintf(reverse_name,"%d.%d.%d.%d.in-addr.arpa",
						(rev_addr&0xff000000)>>24,
						(rev_addr&0xff0000)>>16,
						(rev_addr&0xff00)>>8,
						(rev_addr&0xff));
		hostname=reverse_name;
	}

	// first part of the query is a fixed size header
	struct dns_hdr *hdr = (struct dns_hdr*)query;

	// generate a random 16-bit number for session
	uint16_t query_id = (uint16_t) (random() & 0xffff);
	hdr->id = htons(query_id);
	// set header flags to request recursive query
	hdr->flags = htons(0x0100);	
	// 1 question, no answers or other records
	hdr->q_count=htons(1);

	// add the name
	int query_len = sizeof(struct dns_hdr); 
	int name_len=to_dns_style(hostname,query+query_len);
	query_len += name_len; 
	
	// now the query type: A or PTR. 
	uint16_t *type = (uint16_t*)(query+query_len);
	if(rev_addr!=INADDR_NONE)
		*type = htons(12);
	else
		*type = htons(1);
	query_len+=2;

	// finally the class: INET
	uint16_t *class = (uint16_t*)(query+query_len);
	*class = htons(1);
	query_len += 2;
 
	return query_len;	
}

char* getResolvedValues(char *hostname, char *nameserver){

	printf("counter-->%d\n",counter );
	printf("debug-->%d\n",debug );
    printf("nameserver_flag-->%d\n",nameserver_flag );
    printf("hostname-->%s\n",hostname );
    printf("nameserver-->%s\n",nameserver );

 	char *nslookup_received = NULL,*ipaddress_received = NULL,*nslookup_dns_resoved_host=NULL,*ipaddress_dns_received=NULL;

	// using a constant name server address for now.
	in_addr_t nameserver_addr=inet_addr(nameserver);
	
	int sock = socket(AF_INET, SOCK_DGRAM, 0);
	if(sock < 0) {
		perror("Creating socket failed: ");
		exit(1);
	}
	
	// construct the query message
	uint8_t query[1500];
	int query_len=construct_query(query,1500,hostname);

	struct sockaddr_in addr; 	// internet socket address data structure
	addr.sin_family = AF_INET;
	addr.sin_port = htons(53); // port 53 for DNS
	addr.sin_addr.s_addr = nameserver_addr; // destination address (any local for now)
	
	int send_count = sendto(sock, query, query_len, 0,(struct sockaddr*)&addr,sizeof(addr));
	if(send_count<0) { perror("Send failed");	exit(1); }	

	// await the response 
	uint8_t answerbuf[1500];
	int rec_count = recv(sock,answerbuf,1500,0);
	
	// parse the response to get our answer
	struct dns_hdr *ans_hdr=(struct dns_hdr*)answerbuf;
	uint8_t *answer_ptr = answerbuf + sizeof(struct dns_hdr);
	
	// now answer_ptr points at the first question. 
	int question_count = ntohs(ans_hdr->q_count);
	int answer_count = ntohs(ans_hdr->a_count);
	int auth_count = ntohs(ans_hdr->auth_count);
	int other_count = ntohs(ans_hdr->other_count);

	// skip past all questions
	int q;
	for(q=0;q<question_count;q++) {
		char string_name[255];
		memset(string_name,0,255);
		int size=from_dns_style(answerbuf,answer_ptr,string_name);
		answer_ptr+=size;
		answer_ptr+=4; //2 for type, 2 for class
	}

	int a;
	int got_answer=0;

	// now answer_ptr points at the first answer. loop through
	// all answers in all sections
	for(a=0;a<answer_count+auth_count+other_count;a++) {
		// first the name this answer is referring to 
		char string_name[255];
		int dnsnamelen=from_dns_style(answerbuf,answer_ptr,string_name);
		answer_ptr += dnsnamelen;

		// then fixed part of the RR record
		struct dns_rr* rr = (struct dns_rr*)answer_ptr;
		answer_ptr+=sizeof(struct dns_rr);

		const uint8_t RECTYPE_A=1;
		const uint8_t RECTYPE_NS=2;
		const uint8_t RECTYPE_CNAME=5;
		const uint8_t RECTYPE_SOA=6;
		const uint8_t RECTYPE_PTR=12;
		const uint8_t RECTYPE_AAAA=28;

		
		if(htons(rr->type)==RECTYPE_A && ipaddress_received == NULL) {

			char *resolved_host_addr=inet_ntoa(*((struct in_addr *)answer_ptr));
			got_answer=1;
			printf("The name %s resolves to IP addr: %s\n",string_name,resolved_host_addr);

			ipaddress_received = (char*)malloc(strlen(resolved_host_addr)+1);
			strcpy(ipaddress_received,resolved_host_addr);

			ipaddress_dns_received = (char*)malloc(strlen(string_name)+1);
			strcpy(ipaddress_dns_received,string_name);

		}
		// NS record
		else if(htons(rr->type)==RECTYPE_NS && nslookup_received == NULL) {

			char ns_string[255];
			int ns_len=from_dns_style(answerbuf,answer_ptr,ns_string);
			if(debug)
				printf("The name %s can be resolved by NS: %s\n",
						 string_name, ns_string);					
			got_answer=1;

			nslookup_dns_resoved_host = (char*)malloc(strlen(string_name));
			nslookup_dns_resoved_host = string_name;
			nslookup_received = (char*)malloc(strlen(ns_string));
			nslookup_received = ns_string;
		}
		// CNAME record
		else if(htons(rr->type)==RECTYPE_CNAME) {
			char ns_string[255];
			int ns_len=from_dns_style(answerbuf,answer_ptr,ns_string);
			if(debug)
				printf("The name %s is also known as %s.\n",
						 string_name, ns_string);								
			got_answer=1;
			hostname_original = (char*)malloc(strlen(ns_string)+1);
			strcpy(hostname_original,ns_string);

			hostname = (char*)malloc(strlen(ns_string)+1);
			strcpy(hostname,ns_string);

		}
		// PTR record
		else if(htons(rr->type)==RECTYPE_PTR) {
			char ns_string[255];
			int ns_len=from_dns_style(answerbuf,answer_ptr,ns_string);
			printf("The host at %s is also known as %s.\n",
						 string_name, ns_string);								
			got_answer=1;
			hostname_original = (char*)malloc(strlen(ns_string)+1);
			strcpy(hostname_original,ns_string);
		}
		// SOA record
		else if(htons(rr->type)==RECTYPE_SOA) {
			if(debug)
				printf("Ignoring SOA record\n");
		}
		// AAAA record
		else if(htons(rr->type)==RECTYPE_AAAA)  {
			if(debug)
				printf("Ignoring IPv6 record\n");
		}
		else {
			if(debug)
				printf("got unknown record type %hu\n",htons(rr->type));
		} 

		answer_ptr+=htons(rr->datalen);
	}
	
	if(!got_answer) printf("Host %s not found.\n",hostname);
	
	shutdown(sock,SHUT_RDWR);
	close(sock);

	printf("ipaddress_received --> %s\n",ipaddress_received );
	printf("ipaddress_dns_received --> %s\n",ipaddress_dns_received );
	printf("nslookup_dns_resoved_host --> %s\n",nslookup_dns_resoved_host );
	printf("nslookup_received --> %s\n",nslookup_received );
	printf("hostname_original --> %s\n",hostname_original );
	printf("hostname --> %s\n", hostname);

	if(ipaddress_received != NULL){
		if(strcmp(hostname,ipaddress_dns_received)==0 && strcmp(hostname,hostname_original)==0){
			char* tpl = "%s%s%s%s";
			char *template = (char *)malloc(strlen(NAME)+strlen(hostname)+strlen(RESOLVES_TO)+strlen(ipaddress_received));
			printf("The name %s resolves to IP addr: %s\n",hostname,ipaddress_received);
			sprintf(template,tpl,NAME, hostname,RESOLVES_TO,ipaddress_received);
			exit(1);
			//return template;
		}else if(strcmp(hostname,ipaddress_dns_received)==0 && strcmp(hostname,hostname_original)!=0){
			return getResolvedValues(hostname_original,ipaddress_received);
		}else{
			return getResolvedValues(hostname,ipaddress_received);
		}
	}else if(nslookup_received != NULL){
		if(strcmp(hostname,nslookup_dns_resoved_host)==0){
			hostname_original = nslookup_dns_resoved_host;
		}else if(strcmp(hostname_original,nslookup_dns_resoved_host)!=0){
			hostname_original =hostname;
			hostname = nslookup_dns_resoved_host;
		}
		return getResolvedValues(nslookup_received,"192.33.4.12");
	}
	exit(1);
} 

char* parseInputData(int argc,char* argv[]){
	char *optString = "-d-n:-i:";
	char *nameserver="192.33.4.12",*hostname;
 	int opt = getopt( argc, argv, optString );
    while( opt != -1 ) {
        switch( opt ) {      
        	case 'd':
        		debug = 1; 
        		break;
        	case 'n':
        		nameserver_flag = 1; 
        		nameserver = optarg;
        		break;	 		
            case 'i':
            	hostname = (char*)malloc(strlen(optarg)+1);
                hostname = optarg;
                hostname_original = optarg;
                break;	
            case '?':
				usage();
        		exit(1);               
            default:
            	usage();
            	exit(1);
        }
        opt = getopt( argc, argv, optString );
    }
    getResolvedValues(hostname,nameserver);
}


int main(int argc, char* argv[])
{

/*while(*argv != NULL){
	printf("before making with argv --> %s\n",*argv++);
}*/
/*if(argc < 6){
char* newargv[7];
int i=0,j=0;
while(argv[i] != NULL){
	char rootip[] = "192.33.4.12";
	if(strcmp(argv[i],"-d")==0){
		newargv[j] = malloc(sizeof(argv[i]));
		memcpy(newargv[j], argv[i], sizeof(argv[i]));
		j++; i++;
		newargv[j] = malloc(sizeof("-n"));
		memcpy(newargv[j], "-n", sizeof("-n"));
		j++;
		newargv[j] = malloc(sizeof(rootip));
		memcpy(newargv[j], rootip, sizeof(rootip));
		j++;
	}else{
		newargv[j] = malloc(strlen(argv[i]));
		strcpy(newargv[j],argv[i]);
		j++;
		i++;
	}
}
newargv[j] = NULL;
argc += 2;
char* resolvedIp = parseInputData(argc,newargv);
printf("Resolved IP --> %s\n",resolvedIp );
}else{
	char* resolvedIp = parseInputData(argc,argv);
	printf("Resolved IP --> %s\n",resolvedIp );
}*/
	parseInputData(argc,argv);

}

