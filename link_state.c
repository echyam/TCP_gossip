#include "global.h"
#include <assert.h>
#include <limits.h>
#include <stdlib.h>
#include <stdio.h>
#include <arpa/inet.h>
#include <string.h>

#define INFINITY                INT_MAX
#define UNDEFINED               (-1)
#define INDEX(x, y, nnodes)         ((x) + (nnodes) * (y))

struct node_list {
    char **nodes;
    int nnodes;
    int unsorted;
};

int nl_index(struct node_list *nl, char *node);

struct node_list *nl_create(void) {
    return (struct node_list *) calloc(1, sizeof(struct node_list));
}

int nl_nsites(struct node_list *nl){
    return nl->nnodes;
}

void nl_add(struct node_list *nl, char *node){
    /* No duplicate nodes.
     */
    if (nl_index(nl, node) != -1) {
        return;
    }
    /* Create a copy of the site.
     */
    int len = strlen(node);
    char *copy = malloc(len + 1);
    strcpy(copy, node);

    /* Add this copy to the list.
     */
    nl->nodes= (char **) realloc(nl->nodes, sizeof(char *) * (nl->nnodes + 1));
    nl->nodes[nl->nnodes++] = copy;
    nl->unsorted = 1;
}

int nl_compare(const void *e1, const void *e2){
    const char **p1 = (const char **) e1, **p2 = (const char **) e2;
    return strcmp(*p1, *p2);
}

void nl_sort(struct node_list *nl){
    qsort(nl->nodes, nl->nnodes, sizeof(char *), nl_compare);
    nl->unsorted = 0;
}

/* Return the rank of the given site in the given site list.
 */
int nl_index(struct node_list *nl, char *node){
    /* Sort the list if not yet sorted.
     */
    if (nl->unsorted) {
        nl_sort(nl);
    }

    /* Binary search.
     */
    int lb = 0, ub = nl->nnodes;
    while (lb < ub) {
        int i = (lb + ub) / 2;
        int cmp = strcmp(node, nl->nodes[i]);
        if (cmp < 0) {
            ub = i;
        }
        else if (cmp > 0) {
            lb = i + 1;
        }
        else {
            return i;
        }
    }
    return -1;
}

char *nl_name(struct node_list *nl, int index){
    if (index < 0) {
        return "UNDEFINED";
    }
    return nl->nodes[index];
}

void nl_destroy(struct node_list *nl){
    int i;

    for (i = 0; i < nl->nnodes; i++) {
        free(nl->nodes[i]);
    }
    free(nl->nodes);
    free(nl);
}

/* Set the distance from src to dst.
 */
void set_dist(struct node_list *nl, int graph[], int nnodes, char *src, char *dst, int dist){
    int x = nl_index(nl, src), y = nl_index(nl, dst);
    if (x < 0 || y < 0) {
        fprintf(stderr, "set_dist: bad source or destination\n");
        return;
    }
    graph[INDEX(x, y, nnodes)] = dist;
    graph[INDEX(y, x, nnodes)] = dist;
}

char* addr_to_string (struct sockaddr_in addr) {
    char* addr_string = malloc(40);
    strcpy(addr_string, inet_ntoa(addr.sin_addr));
    strcat(addr_string, ":");
    char* port = malloc(12);
    sprintf(port, "%d", ntohs(addr.sin_port));
    strcat(addr_string, port);
    free(port);
    return addr_string;
}

struct sockaddr_in string_to_addr(char* string) {
    char *port = index(string, ':');
    *port++ = '\0';
    
    struct sockaddr_in addr;
    memset((void*)&addr, 0, sizeof(addr));
    addr_get(&addr, string, atoi(port));
    *--port = ':';
    return addr;
}


/*************************************************
    Dijkstra's algorithm
*************************************************/

/* Dijkstra's algorith.  graph[INDEX(x, y, nnodes)] contains the
 * distance of node x to node y.  nnodes is the number of nodes.  src
 * is that starting node.  Output dist[x] gives the distance from src
 * to x.  Output prev[x] gives the last hop from src to x.
 */
int find_min_node_frontier(int *frontier, int*dist, int nnodes){
    int i;
    int min_distance = INFINITY;
    int next = UNDEFINED;

    for (i = 0; i <nnodes; i++){
        // if alt dist via frontier node is shorter
        if (frontier[i] == 1 && dist[i] < min_distance){ 
            min_distance = dist[i];
            next = i;
        }
    }
    frontier[next] = 0; // remove the min node from frontier
    return next;
}

void dijkstra(int graph[], int nnodes, int src, int dist[], int prev[]){
    // check for valid inputs into dijkstra's; if invalid, just break out
    if ( graph == NULL || nnodes == 0 || src < 0  || src >= nnodes || dist == NULL || prev == NULL )
        return;

    int i, j, current_min_node, neighbor_dist;
    int frontier_counter = 1;
    int *frontier = (int*)malloc(nnodes*sizeof(int));

    for (i = 0; i < nnodes; i++){
        dist[i] = INFINITY;
        frontier[i] = 0;
    }
    
    dist[src] = 0;
    prev[src] = UNDEFINED;
    frontier[src] = 1;

    // for all nodes in the graph
    while(frontier_counter > 0){            // while frontier is not empty
        // get next node with minimum distance
        frontier_counter--;
        current_min_node = find_min_node_frontier(frontier, dist, nnodes);

        for (j=0; j <nnodes; j++){          // find the min dist from src to j
            // take away the minimum item from the frontier
            neighbor_dist = graph[INDEX(current_min_node, j, nnodes)];

            // if node is reachable
            if (neighbor_dist != INFINITY){
                neighbor_dist += dist[current_min_node];

                // add neighbor to frontier list
                if (dist[j] == INFINITY){
                    frontier_counter++;
                    frontier[j] = 1;
                }

                if (neighbor_dist < dist[j]){
                    //printf("prev [%d]is being set to %d\n", j, current_min_node);
                    prev[j] = current_min_node;
                    dist[j] = neighbor_dist;
                }
            }
        }
    }
    free(frontier);
}
