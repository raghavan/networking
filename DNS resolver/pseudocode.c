char* hostname_original;
resolveAddress(char* hostname, char* nameserver){
	char *nslookup_received = NULL,*ipaddress_received = NULL,*nslookup_dns_resoved=NULL,*ipaddress_dns_received=NULL;
	for(){
		if(rectype==A && ipaddress_received == NULL){
			ipaddress_received = (char*)malloc(strlen(getipaddress));
			ipaddress_received = getipaddress;
		}else if(rectype = NS && nslookup_received == NULL){
			nslookup_dns_resoved = (char*)malloc(strlen(getns_dns_resolved));
			nslookup_dns_resoved = getns_dns_resolved;
			nslookup_received = (char*)malloc(strlen(getns));
			nslookup_received = getns;
		}
	}

	if(ipaddress_received != NULL){
		if(hostname == ipaddress_dns_received && hostname == hostname_original){
			print(ip);
		}else if(hostname = ipaddress_dns_received && hostname != hostname_original){
			return resolveAddress(hostname_original,ipaddress_received);
		}else{
			return resolveAddress(hostname,ipaddress_received);
		}
		/*if(hostname_original == hostname_receieved){
			return ipaddress_received;
		}*/
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
}