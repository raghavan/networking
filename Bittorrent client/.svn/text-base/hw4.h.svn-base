struct peer_addr {
	in_addr_t addr;
	short port;
} __attribute__((packed));

struct peer_state {
	struct peer_state *next;
	in_addr_t ip;
	unsigned int port;

	int socket;
	int connected;
	int trying_to_connect;
	char* bitfield;
	
	// buffer where we store incoming messages
	char* incoming;
	// buffer where we store outgoing messages
	char* outgoing;
	
    // number of bytes currently in the incoming buffer
	int count;
	int send_count;
	int choked;
	int not_interested;
    int handshaked;
    int i_choked_it;
	int requested_piece;
    int send_recv_balancer; //tit for tatter
};