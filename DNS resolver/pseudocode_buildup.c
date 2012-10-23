
	printf("counter-->%d\n",counter );
	printf("debug-->%d\n",debug );
    printf("nameserver_flag-->%d\n",nameserver_flag );
    printf("hostname-->%s\n",hostname );
    printf("nameserver-->%s\n",nameserver );

 	char *nslookup_received = NULL,*ipaddress_received = NULL,*nslookup_dns_resoved=NULL,*ipaddress_dns_received=NULL;

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

		char* tpl = "%s%s%s%s";
		
		if(htons(rr->type)==RECTYPE_A && ipaddress_received == NULL) {

			char *resolved_host_addr=inet_ntoa(*((struct in_addr *)answer_ptr));
			got_answer=1;
			char *template = (char *)malloc(strlen(NAME)+strlen(string_name)+strlen(RESOLVES_TO)+strlen(resolved_host_addr));
			printf("The name %s resolves to IP addr: %s\n",string_name,resolved_host_addr);
			sprintf(template,tpl,"The name ", string_name," resolves to IP addr: ",resolved_host_addr);

			ipaddress_received = (char*)malloc(strlen(resolved_host_addr));
			ipaddress_received = resolved_host_addr;

		}
		// NS record
		else if(htons(rr->type)==RECTYPE_NS) {

			char ns_string[255];
			int ns_len=from_dns_style(answerbuf,answer_ptr,ns_string);
			if(debug)
				printf("The name %s can be resolved by NS: %s\n",
						 string_name, ns_string);					
			got_answer=1;

			nslookup_dns_resoved = (char*)malloc(strlen(string_name));
			nslookup_dns_resoved = string_name;
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
			//return getResolvedValues(debug,nameserver_flag,hostname,ns_string);
		}
		// PTR record
		else if(htons(rr->type)==RECTYPE_PTR) {
			char ns_string[255];
			int ns_len=from_dns_style(answerbuf,answer_ptr,ns_string);
			printf("The host at %s is also known as %s.\n",
						 string_name, ns_string);								
			got_answer=1;
			//return getResolvedValues(debug,nameserver_flag,hostname,ns_string);
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

		if(ipaddress_received != NULL){
		if(hostname == ipaddress_dns_received && hostname == hostname_original){
			print(ip);
		}else if(hostname = ipaddress_dns_received && hostname != hostname_original){
			return resolveAddress(hostname_original,ipaddress_received);
		}else{
			return resolveAddress(hostname,ipaddress_received);
		}
	}else if(nslookup_received != NULL){
		hostname_original = hostname;
		return resolveAddress(nslookup_received,"192.33.4.12");
		if(hostname==nslookup_dns_resoved){
			hostname_original = hostname_receieved;
			nameserver = rootserver;
		}else if(hostname_original is_a_part_of hostname_receieved ){
			hostname_original =hostname;
			nameserver = rootserver;
			hostname = hostname_receieved;
		}
	}