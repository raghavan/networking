#include <stdlib.h>
#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <openssl/sha.h>
#include <bencodetools/bencode.h>
#include <curl/curl.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <errno.h>
#include "hw4.h"

#ifndef FD_COPY
#define FD_COPY(f, t)   (void)(*(t) = *(f))
#endif

fd_set fdrset;
fd_set fdwset;
char announce_url[255];

enum {PIECE_EMPTY=0, PIECE_PENDING=1, PIECE_FINISHED=2} *piece_status;

#define BUFSIZE piece_length*4
struct peer_state *peers=0;

int debug=0;  //set this to zero if you don't want all the debugging messages 
char screenbuf[10000];

int shut_down(struct peer_state* peer,int lineno);
int managePeers(struct peer_state* peer);
void send_handshake(struct peer_state* peer);
int recv_handshake(struct peer_state* peer);
int reconnect_peer(struct peer_state* peer);
void disconnet_peer(struct peer_state *peer);
void broadcast_have_msg(int piece);
void dev_notifier();//handy for debugging purposes
void send_choke_unchoke_msg(struct peer_state* peer, int is_choke);
void decode_request_send_piece_to_peer(struct peer_state* peer);
void send_requested_piece_to_peer(struct peer_state* peer,int piece,int offset,int piece_length);
void push_into_msg_outgoing_buf(struct peer_state *peer, char *buf, int count);
void send_buffed_msg(struct peer_state* peer);
int get_total_pieces_available();
void print_bencode(struct bencode*);
void start_peers();
int choked_peers();
int missing_blocks();

// The SHA1 digest of the document we're downloading this time. 
// using a global means we can only ever download a single torrent in a single process. That's ok.
unsigned char digest[20];

// we generate a new random peer id every time we start
char peer_id[21]="-UICCS450-";
struct bencode_dict *torrent;
struct bencode *info;
int file_length=0;
int piece_length;
int *piece_occurence_value;

void dev_notifier(){
    printf("waiting for developer to note\n");
    int test;
    scanf("%d",&test);//debugging the msg and pause in wireshark
}

int active_peers() {
    struct peer_state *peer = peers;
    int count=0;
    while(peer) {
        if((peer->connected && !peer->choked) || peer->trying_to_connect)
            count++;
        peer = peer->next;
    }
    return count;
}

void* draw_state() {
        printf("\033[2J\033[1;1H");
        int pieces = file_length/piece_length+1;
        printf("%d byte file, %d byte pieces, %d pieces, %d active, %d choked\n",
            file_length,piece_length,file_length/piece_length+1,active_peers(),choked_peers()); 
        for(int i=0;i<pieces;i++) {
            if(i%80 == 0) printf("\n");
            switch(piece_status[i]) {
                case PIECE_EMPTY: printf("."); break;
                case PIECE_PENDING: printf("x"); break;
                case PIECE_FINISHED: printf("X"); break;
                default: printf("?");
            }
        }
    fflush(stdout);
}

void send_choke_unchoke_msg(struct peer_state* peer, int is_choke){

    //1 byte for type + 4 for msg len
    char *choke_unchoke_msg =  malloc(5);
    memset(choke_unchoke_msg,0,5);
    choke_unchoke_msg[3] = 1;
    if(is_choke){
        //Choking value set
        choke_unchoke_msg[4] = 0;
    }else{
        //Unchoke value set
        choke_unchoke_msg[4] = 1;
    }
    push_into_msg_outgoing_buf(peer,choke_unchoke_msg,5);
    //dev_notifier();
}

/*
void read_blocks(char* data, int piece, int offset, int len) 
{
    int accumulated_file_length = 0;
    int block_start = piece*piece_length+offset;

    struct bencode_list* files = (struct bencode_list*)ben_dict_get_by_str(info,"files");
    // multi-file case
    if(files) {
        for(int i=0;i<files->n;i++) {
            struct bencode* file = files->values[i];
            struct bencode_list* path = (struct bencode_list*)ben_dict_get_by_str(file,"path");
            int file_length=((struct bencode_int*)ben_dict_get_by_str(file,"length"))->ll; 

            printf("start %d len %d accum %d filelen %d\n",block_start,len,accumulated_file_length,file_length);

            // at least part of the block belongs in this file
            if((block_start >= accumulated_file_length) && (block_start < accumulated_file_length+file_length)) {
                char filename[255];
                
                mkdir(((struct bencode_str*)ben_dict_get_by_str(info,"name"))->s,0777);
                chmod(((struct bencode_str*)ben_dict_get_by_str(info,"name"))->s,07777);
                
                sprintf(filename,"%s/",((struct bencode_str*)ben_dict_get_by_str(info,"name"))->s);
                for(int j=0;j<path->n;j++) {                    
                    if(j<(path->n-1)) {
                        sprintf(filename+strlen(filename),"%s/",((struct bencode_str*)path->values[j])->s);
                        mkdir(filename,0777);
                        chmod(filename,07777);
                    }
                    else
                        sprintf(filename+strlen(filename),"%s",((struct bencode_str*)path->values[j])->s);
                }   
                
                int outfile = open(filename,O_RDONLY,0777);
                if(outfile == -1) {
                    fprintf(stderr,"filename: %s\n",filename);
                    perror("Couldn't open file for reading");
                    exit(1);
                }
                
                int offset_into_file = block_start - accumulated_file_length;
                int remaining_file_length = file_length - offset_into_file;
                lseek(outfile,offset_into_file,SEEK_SET);

                if(remaining_file_length > len) {
                    read(outfile,data,len);
                    close(outfile);
                    goto cleanup;
                }
                else {
                    read(outfile,data,remaining_file_length);
                    close(outfile);
                    read_block(data+remaining_file_length,piece,offset+remaining_file_length,len-remaining_file_length);
                    goto cleanup;
                }

            }
            accumulated_file_length+=file_length;
        }
    }
    // single-file case
    else {

        struct bencode_str* name = (struct bencode_str*)ben_dict_get_by_str(info,"name");
        if(name) {
            FILE *outfile = fopen(name->s,"r");
            file_length = ((struct bencode_int*)ben_dict_get_by_str(info,"length"))->ll;            

            // write the data to the right spot in the file
            fseek(outfile,piece*piece_length+offset,SEEK_SET);
            fread(data,1,len,outfile);
            fclose(outfile);
    
        }
    }
    
 cleanup:
    return;
}
*/

int missing_blocks() {
    int count=0;
    for(int i=0;i<file_length/piece_length+((file_length%piece_length)>0?1:0);i++) {
        if(piece_status[i]!=PIECE_FINISHED) {
            count++;
        }
    }
    return count;   
}

int is_bit_set(char *bitset, int piece)
{
    return (bitset[piece/8] >> (7 - (piece%8)))& 0x01;
}

/*
void send_requested_piece_testing(struct peer_state *peer, int piece, int offset, int len)
{
    struct piece_header{
        //4 bytes
        int len;
        //13 bytes
        char type;
        int index;
        int offset;
    } __attribute__((packed))*requested_msg;

    char *buf;
    buf = malloc(len + 13 );
    requested_msg = (struct piece_header *) buf;
    requested_msg->len = htonl(len + 9);
    requested_msg->type = 7;
    requested_msg->index = htonl(piece);
    requested_msg->offset = htonl(offset);
    read_block(buf + 13 , piece, offset, len);
    //printf("Before pushing msg\n");
    push_into_msg_outgoing_buf(peer, buf, len + 13);
    //dev_notifier();
    //Making the tit for tat logic
    peer->send_recv_balancer--;
    if(peer->send_recv_balancer <= -3){
        peer->i_choked_it = 1;
        send_choke_unchoke_msg(peer,1);//choke it
    }
}

*/

//Reused the write block with read statements
void read_block(char* data, int piece, int offset, int len) 
{
    int accumulated_file_length = 0;
    int block_start = piece*piece_length+offset;

    struct bencode_list* files = (struct bencode_list*)ben_dict_get_by_str(info,"files");
    // multi-file case
    if(files) {
        for(int i=0;i<files->n;i++) {
            struct bencode* file = files->values[i];
            struct bencode_list* path = (struct bencode_list*)ben_dict_get_by_str(file,"path");
            int file_length=((struct bencode_int*)ben_dict_get_by_str(file,"length"))->ll; 

            printf("start %d len %d accum %d filelen %d\n",block_start,len,accumulated_file_length,file_length);

            // at least part of the block belongs in this file
            if((block_start >= accumulated_file_length) && (block_start < accumulated_file_length+file_length)) {
                char filename[255];
                
                mkdir(((struct bencode_str*)ben_dict_get_by_str(info,"name"))->s,0777);
                chmod(((struct bencode_str*)ben_dict_get_by_str(info,"name"))->s,07777);
                
                sprintf(filename,"%s/",((struct bencode_str*)ben_dict_get_by_str(info,"name"))->s);
                for(int j=0;j<path->n;j++) {                    
                    if(j<(path->n-1)) {
                        sprintf(filename+strlen(filename),"%s/",((struct bencode_str*)path->values[j])->s);
                        mkdir(filename,0777);
                        chmod(filename,07777);
                    }
                    else
                        sprintf(filename+strlen(filename),"%s",((struct bencode_str*)path->values[j])->s);
                }   
                
                int outfile = open(filename,O_RDONLY,0777);
                if(outfile == -1) {
                    fprintf(stderr,"filename: %s\n",filename);
                    perror("Couldn't open file for reading");
                    exit(1);
                }
                
                int offset_into_file = block_start - accumulated_file_length;
                int remaining_file_length = file_length - offset_into_file;
                lseek(outfile,offset_into_file,SEEK_SET);

                if(remaining_file_length > len) {
                    read(outfile,data,len);
                    close(outfile);
                    goto cleanup;
                }
                else {
                    read(outfile,data,remaining_file_length);
                    close(outfile);
                    read_block(data+remaining_file_length,piece,offset+remaining_file_length,len-remaining_file_length);
                    goto cleanup;
                }

            }
            accumulated_file_length+=file_length;
        }
    }
    // single-file case
    else {

        struct bencode_str* name = (struct bencode_str*)ben_dict_get_by_str(info,"name");
        if(name) {
            FILE *outfile = fopen(name->s,"r");
            file_length = ((struct bencode_int*)ben_dict_get_by_str(info,"length"))->ll;            

            // write the data to the right spot in the file
            fseek(outfile,piece*piece_length+offset,SEEK_SET);
            fread(data,1,len,outfile);
            fclose(outfile);
    
        }
    }
    
 cleanup:
    return;
}

int peer_connected(in_addr_t addr,unsigned int port) {
    struct peer_state *peer = peers;
    while(peer) {
        if(peer->ip == addr && peer->connected && peer->port == port) {
            return 1;
        }
        peer = peer->next;
    }   
    return 0;
}

void send_requested_piece_to_peer(struct peer_state *peer, int piece, int offset, int len)
{
    struct piece_header{
        //4 bytes
        int len;
        //13 bytes
        char type;
        int index;
        int offset;
    } __attribute__((packed))*requested_msg;

    char *buf;
    buf = malloc(len + 13 );
    requested_msg = (struct piece_header *) buf;
    requested_msg->len = htonl(len + 9);
    requested_msg->type = 7;
    requested_msg->index = htonl(piece);
    requested_msg->offset = htonl(offset);
    read_block(buf + 13 , piece, offset, len);
    //printf("Before pushing msg\n");
    push_into_msg_outgoing_buf(peer, buf, len + 13);
    //dev_notifier();
    //Making the tit for tat logic
    peer->send_recv_balancer--;
    if(peer->send_recv_balancer <= -3){
        peer->i_choked_it = 1;
        send_choke_unchoke_msg(peer,1);//choke it
    }
}

void disconnet_peer(struct peer_state *peer){
    /*
    peer->connected = 0;
    peer->send_count =0;
    peer->requested_piece = -1;
    peer->send_recv_balancer=0;
    peer->trying_to_connect = 0;
    peer->choked = 0;
    peer->count = 0;
    */
        FD_CLR(peer->socket, &fdrset);
         FD_CLR(peer->socket, &fdwset);
        peer->connected = 0;
         peer->send_count =0;
        peer->requested_piece = -1;
        peer->send_recv_balancer=0;
        peer->trying_to_connect = 0;
        peer->choked = 0;
        peer->count = 0;
     close(peer->socket);
 }


int choked_peers() {
    struct peer_state *peer = peers;
    int count=0;
    while(peer) {
        if(peer->connected && peer->choked)
            count++;
        peer = peer->next;
    }
    return count;
}

void decode_request_send_piece_to_peer(struct peer_state *peer)
{
  //Same as the header in request_block
   struct request{
        int length;
        char id;
        int index;
        int begin;
        int len;
    } __attribute__((packed)) *request;
    request = (struct request *) peer->incoming;
    //dev_notifier();
    //printf("calling sent request piece\n");
    send_requested_piece_to_peer(peer, ntohl(request->index),ntohl(request->begin),ntohl(request->len));
}

void broadcast_have_msg(int piece)
{  
    //printf("Inside broadcase msg function\n");
    fflush(stdout);
    char *havemsg =  malloc(9);
    memset(havemsg,0,9);
    havemsg[3] =5;
    havemsg[4]=4;
    *(int*)(havemsg+5) = htonl(piece);   
    struct peer_state *peer = peers;

    while(peer){
            if(peer->connected && !is_bit_set(peer->bitfield, piece) && !peer->not_interested){
                push_into_msg_outgoing_buf(peer, havemsg, 9);
            }
        peer = peer->next;
    }
    //printf("Exit broadcast msg function\n");
}

/*
void broadcast_have_msg_testing(int piece)
{  
    //printf("Inside broadcase msg function\n");
    fflush(stdout);
    char *havemsg =  malloc(9);
    memset(havemsg,0,9);
    havemsg[3] =5;
    havemsg[4]=4;
    *(int*)(havemsg+5) = htonl(piece);   
    struct peer_state *peer = peers;

    while(peer){
            if(peer->connected && !is_bit_set(peer->bitfield, piece) && !peer->not_interested){
                push_into_msg_outgoing_buf(peer, havemsg, 9);
            }
        peer = peer->next;
    }
    //printf("Exit broadcast msg function\n");
}

*/

int next_piece(int previous_piece, struct peer_state *peer) {
    /*
    for(int i=0;i<get_total_pieces_available();i++){
        if(piece_occurence_value[i] >0 && piece_status[i] == PIECE_EMPTY && 
            is_bit_set(peer->bitfield,i)){
            if(next_piece != -2 && piece_occurence_value[i] <= piece_occurence_value[next_piece])
                next_piece = i;
        }
    }
    */
   //printf("Insice next piece\n");
   fflush(stdout);
   if(previous_piece!=-1){
        piece_status[previous_piece]=PIECE_FINISHED;
    }
    draw_state();
    int next_piece=-2;
    //using the bitfield count and checking next rare piece
    for(int i=0;i<get_total_pieces_available();i++){
        if(piece_occurence_value[i] >0 && piece_status[i] == PIECE_EMPTY && 
            is_bit_set(peer->bitfield,i)){
            if(next_piece == -2) 
                next_piece = i;  
            if(next_piece != -2 && piece_occurence_value[i] <= piece_occurence_value[next_piece])
                next_piece = i;
        }
    }
    piece_status[next_piece]=PIECE_PENDING;           
    return next_piece;
}


void write_block(char* data, int piece, int offset, int len, int acquire_lock) {
    FILE *outfile;

    //dev_notifier(); 
    int accumulated_file_length = 0;
    int block_start = piece*piece_length+offset;

    struct bencode_list* files = (struct bencode_list*)ben_dict_get_by_str(info,"files");
    // multi-file case
    if(files) {
        for(int i=0;i<files->n;i++) {
            struct bencode* file = files->values[i];
            struct bencode_list* path = (struct bencode_list*)ben_dict_get_by_str(file,"path");
            int file_length=((struct bencode_int*)ben_dict_get_by_str(file,"length"))->ll; 

            printf("start %d len %d accum %d filelen %d\n",block_start,len,accumulated_file_length,file_length);

            // at least part of the block belongs in this file
            if((block_start >= accumulated_file_length) && (block_start < accumulated_file_length+file_length)) {
                char filename[255];
                
                mkdir(((struct bencode_str*)ben_dict_get_by_str(info,"name"))->s,0777);
                chmod(((struct bencode_str*)ben_dict_get_by_str(info,"name"))->s,07777);
                
                sprintf(filename,"%s/",((struct bencode_str*)ben_dict_get_by_str(info,"name"))->s);
                for(int j=0;j<path->n;j++) {                    
                    if(j<(path->n-1)) {
                        sprintf(filename+strlen(filename),"%s/",((struct bencode_str*)path->values[j])->s);
                        mkdir(filename,0777);
                        chmod(filename,07777);
                    }
                    else
                        sprintf(filename+strlen(filename),"%s",((struct bencode_str*)path->values[j])->s);
                }   
                
                int outfile = open(filename,O_RDWR|O_CREAT,0777);
                if(outfile == -1) {
                    fprintf(stderr,"filename: %s\n",filename);
                    perror("Couldn't open file for writing");
                    exit(1);
                }
                
                int offset_into_file = block_start - accumulated_file_length;
                int remaining_file_length = file_length - offset_into_file;
                lseek(outfile,offset_into_file,SEEK_SET);

                if(remaining_file_length > len) {
                    write(outfile,data,len);
                    close(outfile);
                    goto cleanup;
                }
                else {
                    write(outfile,data,remaining_file_length);
                    close(outfile);
                    write_block(data+remaining_file_length,piece,offset+remaining_file_length,len-remaining_file_length,0);
                    goto cleanup;
                }

            }
            accumulated_file_length+=file_length;
        }
    }
    // single-file case
    else {

        struct bencode_str* name = (struct bencode_str*)ben_dict_get_by_str(info,"name");
        if(name) {
            FILE *outfile = fopen(name->s,"r+");
            file_length = ((struct bencode_int*)ben_dict_get_by_str(info,"length"))->ll;            

            // write the data to the right spot in the file
            fseek(outfile,piece*piece_length+offset,SEEK_SET);
            fwrite(data,1,len,outfile);
            fclose(outfile);
    
        }
        else {
            printf("No name?\n");
            exit(1);
        }
    }
    
 cleanup:
    return;
}
void push_into_msg_outgoing_buf(struct peer_state *peer, char *buf, int buf_length){
    /*
        int sent_val = send(peer->socket, peer->outgoing, peer->send_count, 0);
        peer->send_count = peer->send_count - sent_val;
        if(peer->send_count){
            memmove(peer->outgoing, peer->outgoing + sent_val, peer->send_count);
        }
    */
    memcpy(peer->outgoing + peer->send_count, buf, buf_length);
    peer->send_count += buf_length;
    /*
     if(!peer->choked) {
         memcpy(peer->outgoing + peer->send_count, &request, sizeof(request));
         peer->send_count += sizeof(request);
     }
    */
}
 
void drop_message(struct peer_state* peer) {
    //dev_notifier();
    int msglen = ntohl(((int*)peer->incoming)[0]); 
    if(peer->count < msglen+4) {
        fprintf(stderr,"Trying to drop %d bytes, we have %d!\n",msglen+4,peer->count);
        peer->connected=0;
    }
    peer->count -= msglen+4; // size of length prefix is not part of the length
    if(peer->count) {
        memmove(peer->incoming,peer->incoming+msglen+4,peer->count);
    }
 }

 void request_block(struct peer_state* peer, int piece, int offset) {   

     /* requests have the following format */
     struct {
         int len;
         char id;
         int index;
         int begin;
         int length;
     } __attribute__((packed)) request;

     request.len=htonl(13);
     request.id=6;  
     request.index=htonl(piece);
     request.begin=htonl(offset);
     request.length=htonl(1<<14);                       

     // the last block is likely to be of non-standard size
     int maxlen = file_length - (piece*piece_length+offset);
     if(maxlen < (1<<14))
         request.length = htonl(maxlen);

     if(!peer->choked) {
         memcpy(peer->outgoing + peer->send_count, &request, sizeof(request));
         peer->send_count += sizeof(request);
     }
 }


void send_interested(struct peer_state* peer) {
    struct {
        int len;
        char id;
    } __attribute__((packed)) msg;
    msg.len = htonl(1);
    msg.id = 2;

    memcpy(peer->outgoing + peer->send_count, &msg, sizeof(msg));
    peer->send_count += sizeof(msg);
}


int reconnect_peer(struct peer_state* peer) 
{
    //printf("Inside reconnect peer\n");
    struct in_addr a;
    a.s_addr = peer->ip;

     if(peer->connected) {
         fprintf(stderr,"Already connected\n");
         return 0;
     }

     int s = socket(AF_INET, SOCK_STREAM, 0);
     fcntl(s, F_SETFL, O_NONBLOCK);
     struct sockaddr_in addr;
     addr.sin_family = AF_INET;
     addr.sin_addr.s_addr = peer->ip;
     addr.sin_port = peer->port;

     int con = connect(s, (struct sockaddr*)&addr, sizeof(addr));
    if(con != 0 && errno != EINPROGRESS) {//&& errno != EINPROGRESS 
        perror("Connect call failed");
        return 0;
     }
    FD_SET(s,&fdrset); 
    FD_SET(s,&fdwset); 
     peer->socket = s;
     peer->connected = 0;
     peer->trying_to_connect = 1;
     peer->handshaked = 0;
     peer->choked = 1;
     peer->send_recv_balancer=3;
     //printf("Exiting reconnect peer\n");
}

void read_bit_maps(char *bitfield, unsigned int field_len)
{
    //measure bit fields counts, to be used in next piece
    char *bitval = bitfield;
    int piece = 0;
    while(bitval < (bitfield + field_len)){
        for(int i=7; i>=0; i--){
            if((((*bitval) >> i) & 0x01) == 1)
                    piece_occurence_value[piece]++;
            piece++;
        }
        bitval++;
    }

}

void send_buffed_msg(struct peer_state* peer){
        //to place messages in a buffer and will be send when writeset it selected
        //printf("Entering send buffed msg\n");
        int sent_val = send(peer->socket, peer->outgoing, peer->send_count, 0);
        peer->send_count = peer->send_count - sent_val;
        if(peer->send_count){
            memmove(peer->outgoing, peer->outgoing + sent_val, peer->send_count);
        }
        //dev_notifier();
        //printf("Exiting send buffed msg\n");
}

void send_handshake(struct peer_state* peer){
    /* send the handshake message */
    char protocol[] = "BitTorrent protocol";
    unsigned char pstrlen = strlen(protocol); // not sure if this should be with or without terminator
    unsigned char buf[pstrlen+49];
    buf[0]=pstrlen;
    memcpy(buf+1,protocol,pstrlen); 
    memcpy(buf+1+pstrlen+8,digest,20);
    memcpy(buf+1+pstrlen+8+20,peer_id,20);               

    send(peer->socket,buf,sizeof(buf),0);
    peer->connected = 1;
    peer->trying_to_connect = 0;
}

int get_total_pieces_available(){
    //to calculate the no. of pieces available
    if(!file_length || !piece_length)
        return 0;
    return (int)(file_length/piece_length+1);
}

void listen_from_peers() 
{
    struct peer_state* peer = peers;
    unsigned int msglen = 0;
    while(missing_blocks() > 0)
    {
        fd_set fdrset_clone;
        fd_set fdwset_clone;
        FD_COPY(&fdrset,&fdrset_clone);
        FD_COPY(&fdwset,&fdwset_clone);
        int rdy = select(FD_SETSIZE,&fdrset_clone,&fdwset_clone,0, 0);
        if(rdy <= 0) {
            continue;
        }
        peer = peers;
        if(active_peers() == 0){
            break;
        }
        while(peer){
        
            if(FD_ISSET(peer->socket,&fdwset_clone)){
                if(peer->connected == 0){
                    //to send the handshake if it is not connected
                    send_handshake(peer);
                }
                else if(peer->send_count > 0) {
                    //to send the have/interested/piece messages to peers
                    send_buffed_msg(peer);
                }
            }

            if(FD_ISSET(peer->socket,&fdrset_clone)) 
            {
                int newbytes = recv(peer->socket,peer->incoming+peer->count,BUFSIZE-peer->count,0);
                if(newbytes <= 0){ 
                    peer->trying_to_connect = 0;
                    if(newbytes == 0){
                        piece_status[peer->requested_piece] = PIECE_EMPTY;
                    }
                    disconnet_peer(peer);
                    reconnect_peer(peer);
                    peer = peer->next;
                    continue; 
                } 
                peer->count += newbytes;
                if(!peer->handshaked){                    
                    peer->count -= peer->incoming[0]+49;
                    if(peer->count) {
                        memmove(peer->incoming,peer->incoming + peer->incoming[0]+49,peer->count);
                    }
                    peer->handshaked = 1;
                }

               if(memcmp(peer->incoming+peer->incoming[0]+8+20,"-UICCS450-",strlen("-UICCS450-"))==0) {
                    fprintf(stderr,"Caught a CS450 peer, exiting.\n");
                    disconnet_peer(peer);
                    continue;
                }   

                while(peer->count >= (ntohl(((int*)peer->incoming)[0])) + 4){ 
                msglen = ntohl(((int*)peer->incoming)[0]);

                   switch(peer->incoming[4]) {
                        // CHOKE
                        case 0: {
                                    if(debug)
                                        fprintf(stderr,"Choke\n");
                                    peer->choked = 1;
                                    piece_status[peer->requested_piece]=PIECE_EMPTY;
                                    peer->requested_piece = -1;
                                    break;
                                }
                                // UNCHOKE
                        case 1: {
                                    if(debug)
                                        fprintf(stderr,"Unchoke\n");
                                    peer->choked = 0;
                                    peer->requested_piece = next_piece(-1,peer);    
                                    request_block(peer,peer->requested_piece,0);
                                    break;
                                }
                                //Interested
                        case 2: {
                                    //dev_notifier();
                                    send_choke_unchoke_msg(peer,0); //ischoked = 0
                                    peer->not_interested = 0;
                                    break;
                                }
                                //Not interested
                        case 3: {
                                    //dev_notifier();
                                    peer->not_interested = 1;
                                    break;
                                }
                                // HAVE -- update the bitfield for this peer
                        case 4: {
                                    int piece = ntohl(*((int*)&peer->incoming[5]));
                                    int bitfield_byte = piece/8;
                                    int bitfield_bit = piece%8;
                                    if(debug)
                                        fprintf(stderr,"Have %d\n",piece);

                                    peer->bitfield[bitfield_byte] |= 1 << (7 - bitfield_bit);
                                    piece_occurence_value[piece]++;
                                    send_interested(peer);
                                    break;
                                }
                                // BITFIELD -- set the bitfield for this peer
                        case 5:
                                //peer->choked = 0; //commenting it according to prof's note in class
                                if(debug) 
                                    printf("Bitfield of length %d\n",msglen-1);
                                int fieldlen = msglen - 1;
                                if(fieldlen != (file_length/piece_length/8+1)) {
                                    disconnet_peer(peer);
                                    if(active_peers() == 0){
                                        break;
                                    }
                                    peer = peer->next;
                                    continue;
                                }               
                                memcpy(peer->bitfield,peer->incoming+5,fieldlen);
                                read_bit_maps(peer->bitfield,fieldlen);
                                send_interested(peer);
                                break;
                                //Request piece
                        case 6: { 
                                if(peer->i_choked_it == 1) 
                                    break;
                                decode_request_send_piece_to_peer(peer);
                                }       
                                break;                         
                                // PIECE
                        case 7: {
                                    //make the tit for tatter
                                    peer->send_recv_balancer++;
                                    if(peer->i_choked_it && peer->send_recv_balancer == 0){
                                        peer->i_choked_it = 0;
                                        send_choke_unchoke_msg(peer,0); //unchoke peer
                                    }
                                    int piece = ntohl(*((int*)&peer->incoming[5]));
                                    int offset = ntohl(*((int*)&peer->incoming[9]));
                                    int datalen = msglen - 9;

                                    fprintf(stderr,"Writing piece %d, offset %d, ending at %d\n",piece,offset,piece*piece_length+offset+datalen);
                                    write_block(peer->incoming+13,piece,offset,datalen,1);
                                    draw_state();
                                    offset+=datalen;
                                    if(offset==piece_length || (piece*piece_length+offset == file_length) ) {
                                        broadcast_have_msg(piece);
                                        draw_state();
                                        if(debug) 
                                            fprintf(stderr,"Reached end of piece %d at offset %d\n",piece,offset);

                                        peer->requested_piece=next_piece(piece,peer);
                                        offset = 0;
                                    }
                                    request_block(peer,peer->requested_piece,offset);
                                    break;                                  
                                }
                    }
                    drop_message(peer);      
                }
            }
            peer = peer->next;
        }
    }
    return;
}

int disconnect_all_peers(){
    //printf("disconnect all peers called\n");
    fflush(stdout);
    struct peer_state* peer = peers;
    while(peer){
        disconnet_peer(peer);
        peer = peer->next;
    }
    return 0;
}

int connect_all_peers(struct peer_addr* peeraddr) 
{
    struct in_addr a;
    a.s_addr = peeraddr->addr;

    fprintf(stderr,"Connecting...\n");
     if(peer_connected(peeraddr->addr,peeraddr->port)) {
         fprintf(stderr,"Already connected\n");
         return 0;
     }

     int s = socket(AF_INET, SOCK_STREAM, 0);
     fcntl(s, F_SETFL, O_NONBLOCK); 
     struct sockaddr_in addr;
     addr.sin_family = AF_INET;
     addr.sin_addr.s_addr = peeraddr->addr;
     addr.sin_port = peeraddr->port;

     int con = connect(s, (struct sockaddr*)&addr, sizeof(addr));

     if(con != 0 && errno != EINPROGRESS) {//&& errno != EINPROGRESS 
        perror("Connect call failed");
        return 0;
     }

    FD_SET(s,&fdrset); 
    FD_SET(s,&fdwset); 

     struct peer_state* peer = calloc(1,sizeof(struct peer_state));
     peer->socket = s;
     peer->ip = peeraddr->addr;
     peer->port = peeraddr ->port;
     peer->connected = 0;
     peer->handshaked = 0;
     peer->choked = 1;
     peer->incoming = malloc(BUFSIZE);
     peer->outgoing = malloc(BUFSIZE);
     peer->bitfield = calloc(1,file_length/piece_length/8+1);
     peer->next = peers;
     peer->trying_to_connect = 1;
     peers=peer;
}

/* handle_announcement reads an announcement document to find some peers to download from.
     start a new tread for each peer.
 */
void handle_announcement(char *ptr, size_t size) {
    struct bencode* anno = ben_decode(ptr,size);

    printf("Torrent has %lld seeds and %lld downloading peers. \n",
                 ((struct bencode_int*)ben_dict_get_by_str(anno,"complete"))->ll,
                 ((struct bencode_int*)ben_dict_get_by_str(anno,"incomplete"))->ll);
         
    struct bencode_list *peers = (struct bencode_list*)ben_dict_get_by_str(anno,"peers");

    // handle the binary case
    if(peers->type == BENCODE_STR) {
        printf("Got binary list of peers\n");
       struct peer_addr *peerlist = (struct peer_addr*)((struct bencode_str*)peers)->s;
        for(int i=0;i<((struct bencode_str*)peers)->len/6;i++) {                
            struct in_addr a;
            a.s_addr = peerlist[i].addr;
            printf("Found peer %s:%d\n",inet_ntoa(a),ntohs(peerlist[i].port));               

            connect_all_peers(&peerlist[i]);
        }            
    }
    // handle the bencoded case
    else {
        for(int i=0;i<peers->n;i++) {
            printf("Got bencoded list of peers\n");
            struct bencode *peer = peers->values[i];
            char *address = ((struct bencode_str*)ben_dict_get_by_str(peer,"ip"))->s;
            unsigned short port = ((struct bencode_int*)ben_dict_get_by_str(peer,"port"))->ll;
            printf("Found peer %s:%d\n",address,port);

            struct peer_addr *peeraddr = malloc(sizeof(struct peer_addr));
            peeraddr->addr=inet_addr(address);
            peeraddr->port=htons(port);
            connect_all_peers(peeraddr);
        }
    }
    
    listen_from_peers();
}

void start_peers() {

    CURL *curl;
    CURLcode res;

    curl = curl_easy_init();
    {
        int bytes_left = 0;
        bytes_left = missing_blocks() * piece_length;
        if(piece_status[get_total_pieces_available()-1] == PIECE_EMPTY){
            bytes_left -= piece_length;
            bytes_left += file_length%piece_length;
        }
    printf("Announce URL: %s\n",announce_url);
    }
    if(curl) {
        curl_easy_setopt(curl, CURLOPT_URL, announce_url);
        curl_easy_setopt(curl, CURLOPT_TIMEOUT, 1);
        curl_easy_setopt(curl, CURLOPT_NOSIGNAL, 1);
        curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT, 1);

        FILE *anno = fopen("/tmp/anno.tmp","w+");
        if(!anno) {
            perror("couldn't create temporary file\n");
        }

        int attempts=0;
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, anno); 
        while((res = curl_easy_perform(curl)) !=CURLE_OK && 
                    attempts < 5) {
            fprintf(stderr, "curl_easy_perform() failed: %s\n",
                            curl_easy_strerror(res));
            attempts++;
        }
        fclose(anno);

        if (attempts<5) {
            struct stat anno_stat;
            if(stat("/tmp/anno.tmp",&anno_stat)) {
                perror("couldn't stat /tmp/anno.tmp");
                exit(1);
            }
            // the announcement document is in /tmp/anno.tmp. 
            // so map that into memory, then call handle_announcement on the returned pointer
            handle_announcement(mmap(0,anno_stat.st_size,PROT_READ,MAP_SHARED,open("/tmp/anno.tmp",O_RDONLY),0),anno_stat.st_size);
        }
        curl_easy_cleanup(curl);
    }
}

int main(int argc, char** argv) {

    //setvbuf(stdout,screenbuf,_IOFBF,10000);
    setbuf(stdout, NULL);
    setbuf(stderr, NULL);
        
    // create a global peer_id for this session. Every peer_id should include -CS450-.
    for(int i=strlen(peer_id);i<20;i++)
        peer_id[i]='0'+random()%('Z'-'0'); // random numbers/letters between 0 and Z
    
    // make sure the torrent file exists
    struct stat file_stat;
    if(stat(argv[1],&file_stat)) {
        perror("Error opening file.");
        exit(1);
    }

    // map .torrent file into memory, and parse contents
    int fd = open(argv[1],O_RDONLY);
    char *buf = mmap(0,file_stat.st_size,PROT_READ,MAP_SHARED,fd,0);
    if(buf==(void*)-1) {
        perror("couldn't mmap file");
        exit(1);
    }        
    size_t off = 0;
    int error = 0;
    torrent = (struct bencode_dict*)ben_decode2(buf,file_stat.st_size,&off,&error);
    if(!torrent) {
        printf("Got error %d, perhaps a malformed torrent file?\n",error);
        exit(1);
    }

    // pull out the .info part, which has stuff about the file we're downloading
    info = (struct bencode*)ben_dict_get_by_str((struct bencode*)torrent,"info");
    
    struct bencode_list* files = (struct bencode_list*)ben_dict_get_by_str(info,"files");
    // multi-file case
    if(files) {
        for(int i=0;i<files->n;i++) {
            struct bencode* file = files->values[i];
            struct bencode_list* path = (struct bencode_list*)ben_dict_get_by_str(file,"path");
            printf("Filename %s/%s\n",((struct bencode_str*)ben_dict_get_by_str(info,"name"))->s,((struct bencode_str*)path->values[0])->s);

            // accumulate a total length so we know how many pieces there are 
            file_length+=((struct bencode_int*)ben_dict_get_by_str(file,"length"))->ll; 
        }
    }
    // single-file case
    else {
        FILE *outfile;
        struct bencode_str* name = (struct bencode_str*)ben_dict_get_by_str(info,"name");
        if(name) {
            outfile = fopen(name->s,"r+");
            file_length = ((struct bencode_int*)ben_dict_get_by_str(info,"length"))->ll;            
        }
    }

    piece_length = ((struct bencode_int*)ben_dict_get_by_str(info,"piece length"))->ll;

    piece_status  =calloc(1,sizeof(int)*(int)(file_length/piece_length+1));
    /* compute the message digest and info_hash from the "info" field in the torrent */
    size_t len;
    char info_hash[100];  
    char* encoded = ben_encode(&len,(struct bencode*)info);
    SHA1(encoded,len,digest); // digest is a global that holds the raw 20 bytes
    piece_occurence_value = calloc(1,sizeof(int)*(int)(file_length/piece_length+1));
  
    // info_hash is a stringified version of the digest, for use in the announce URL
    memset(info_hash,0,100);
    for(int i=0;i<20;i++)
        sprintf(info_hash+3*i,"%%%02x",digest[i]);

    // compile a suitable announce URL for our document
    sprintf(announce_url,"%s?info_hash=%s&peer_id=%s&port=6881&left=%d",((struct bencode_str*)ben_dict_get_by_str((struct bencode*)torrent,"announce"))->s,info_hash,peer_id,file_length);
    printf("Announce URL: %s\n",announce_url);

    start_peers();
}