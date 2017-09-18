#include <arpa/inet.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "global.h"
#include <limits.h>
#include <string.h>

#define INFINITY                INT_MAX

struct gossip {
    struct gossip *next;
    struct sockaddr_in src;
    long counter;
    char *latest;
};
static struct gossip *gossip;

extern struct sockaddr_in my_addr;

void print_graph(int graph[], int nnodes);
void print_nl(struct node_list *nl);

struct gossip* gossip_next(struct gossip* gossip) {
    return gossip->next;
}

struct sockaddr_in gossip_src(struct gossip* gossip) {
    return gossip->src;
}

char* gossip_latest(struct gossip* gossip) {
    return gossip->latest;
}

/* A gossip message has the following format:
 *
 *  G<src_addr:src_port>/counter/payload\n
 *
 * Here <src_addr:src_port>/counter uniquely identify a message from
 * the given source.
 */
void gossip_received(struct file_info *fi, char *line, struct node_list* nl){
    printf("gossip received: %s\n", line);
    char *port = index(line, ':');
    if (port == 0) {
        fprintf(stderr, "do_gossip: format is G<addr>:<port>/counter/payload\n");
        return;
    }
    *port++ = 0;

    char *ctr = index(port, '/');
    if (ctr == 0) {
        fprintf(stderr, "do_gossip: no counter\n");
        return;
    }
    *ctr++ = 0;

    char *payload = index(ctr, '/');
    if (payload == 0) {
        fprintf(stderr, "do_gossip: no payload\n");
        return;
    }
    *payload++ = 0;

    /* Get the source and message identifier.
     */
    struct sockaddr_in addr;
    if (addr_get(&addr, line, atoi(port)) < 0) {
        return;
    }
    long counter = atol(ctr);

    /* See if we already have this gossip.
     */
    struct gossip *g;

    for (g = gossip; g != 0; g = g->next) {
        if (addr_cmp(g->src, addr) != 0) {
            continue;
        }
        if (g->counter >= counter) {
            printf("already know about this gossip\n");
            return;
        }
        free(g->latest);
        break;
    }

    if (g == 0) {
        g = calloc(1, sizeof(*g));
        g->src = addr;
        g->next = gossip;
        gossip = g;
    };
    nl_add(nl,addr_to_string(addr));

    /* Restore the line.
     */
    *--port = ':';
    *--ctr = '/';
    *--payload = '/';

    /* Save the gossip.
     */
    int len = strlen(line);
    g->latest = malloc(len + 1);
    memcpy(g->latest, line, len + 1);
    g->counter = counter;

    /* Send the gossip to all connections except the one it came in on.
     */
    char *msg = malloc(len + 3);
    sprintf(msg, "G%s\n", g->latest);
    file_broadcast(msg, len + 2, fi);
    free(msg);
    //printf("gossip_received\n");
}

/* Send all gossip I have to the given peer.
 */
void gossip_to_peer(struct file_info *fi){
    struct gossip *g;

    for (g = gossip; g != 0; g = g->next) {
        file_info_send(fi, "G", 1);
        file_info_send(fi, g->latest, strlen(g->latest));
        file_info_send(fi, "\n", 1);
    }
    //printf("gossip_to_peer\n");
}

// helper to build graph, only to be used when sending messages with S
// returns nodes list
void build_graph(struct node_list* nl, int graph[], int nnodes, int rep){
    // make node graph
    char buf[30];                // temp var to hold substrings from delimiters
    char* token = buf;

    //char payload[512];
    char src[25];               // str ver of gossip address
    int fst_ind, snd_ind;       // indices for indexing into graph

    int size,ct;
    int delim_ind;
    size = strlen(gossip->latest);
    char src_cp_alloc[size+1];
    char *line_cp = src_cp_alloc;
    char *payload,*dest,*ttl, *semi1, *semi2;
    char temp[25];

    // for each gossip
    struct gossip *g;
    for(g = gossip; g!= 0; g = g->next){
        // get gossip payload
        strcpy(line_cp, g->latest);
        strcpy(src, addr_to_string(g->src));   // set src string to str of gossip src addr
        
        payload = strchr(line_cp, ';'); // cut counter        

        if (!payload){  // if no payload, break
            break;
        }

        fst_ind = nl_index(nl,src);             // sets 1st graph index to gossip src node

        //printf("%s\n", "pre-loop");

        semi1 = strchr(payload, ';');
        // get gossip src's neighbors
        while(semi1){
            semi1++;
            semi2 = strchr(semi1, ';');

            if (semi2) {
                strncpy(token, semi1, semi2-semi1);
                *(token+(semi2-semi1)) = '\0';
            } else {
                strcpy(token, semi1);
            }

            //print_nl(nl);
            
            snd_ind = nl_index(nl,token);       // sets 2nd graph index to neighbor node

            graph[fst_ind+snd_ind*nnodes] = 1;  // set dist in graph
            semi1 = semi2;
        }

    }
    int i;
    for(i = 0; i < nnodes*nnodes; i++){
        if (graph[i] != 1){
            graph[i] = INFINITY;
        }
    }

}

void send_received (struct file_info *fi, char line[], struct node_list *nl){
    //printf("%s", line);
    printf("at send_received\n");

    // get destination, ttl, payload substrings
    int size, nnodes;
    for(size = 0; line[size] != '\0'; ++size);
    char line_cp_alloc[size+1];
    char *line_cp = line_cp_alloc;
    char *dest;
    char *ttl;
    char *payload;

    memcpy(line_cp,line,size);
    int delim_ind = strchr(line_cp, '/')-line_cp;
    dest = line_cp;
    line_cp[delim_ind] = 0;
    line_cp = line_cp + delim_ind+1;

    delim_ind = strchr(line_cp, '/')-line_cp;
    ttl = line_cp;
    line_cp[delim_ind] = 0;
    line_cp = line_cp + delim_ind+1;

    delim_ind = strchr(line_cp, ';')-line_cp;
    payload = line_cp;


    struct sockaddr_in destination = string_to_addr((char *)dest);  
    //printf("This is my message %s/%s/%s\n", dest,ttl,payload);

    if (addr_cmp(destination,my_addr)==0){
        printf("%s\n",payload);
    }
    // if not destination, forward message and decrement ttl
    else {

        // build message for forwarding
        char msg[512];
        strcpy(msg, "S");
        strcat(msg, dest);
        strcat(msg, "/");

        int new_ttl = atoi(ttl)-1;
        if (new_ttl < 0) {
            printf("No steps remaining\n");
            return;
        }
        char ttl_str[25];
        sprintf(ttl_str, "%d", new_ttl);

        strcat(msg, ttl_str);
        strcat(msg, "/");
        strcat(msg, payload);
        strcat(msg, "\n");

        printf("This is my message %s\n", msg);

        // build graph and find paths
        nnodes = nl_nsites(nl);
        int graph[nnodes*nnodes];
        memset(graph, 0, nnodes*nnodes*sizeof(int));
        build_graph(nl, graph, nnodes,1);
        int dist[nnodes];
        int prev[nnodes];
        nnodes = nl_nsites(nl);

        dijkstra(graph, nnodes, nl_index(nl,addr_to_string(destination)), dist, prev);

        // find node index of next addr
        int next = prev[nl_index(nl, addr_to_string(my_addr))];
        if (next== nl_index(nl, addr_to_string(my_addr)))
            return;

        // turn node index into address string
        char new_addr[25];
        strcpy(new_addr,nl_name(nl,next));

        //print_graph(graph,nnodes);

        // turn string into address and then into file info
        struct file_info* new_fi = sockaddr_to_file(string_to_addr(new_addr));
        file_info_send(new_fi, msg, strlen(msg));
    }
}

void print_graph(int graph[], int nnodes){
    int i,j;
    for (i = 0; i < nnodes; i++){
        for (j = 0; j < nnodes; j++){
            printf("(%d,%d) is %d \n", j,i, graph[j+i*nnodes]);
        }
    }
}

void print_nl(struct node_list *nl){
    int nnodes = nl_nsites(nl);
    int i;
    printf("%s\n", "printing node list");
    for(i = 0; i < nnodes; i++){
        printf("%d is %s\n", i, nl_name(nl,i));
    }
    printf("%s\n", "done printing node list");
}