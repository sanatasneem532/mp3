#include <stdio.h>
#include <stdlib.h>

typedef struct {
    int *neighbors;
    int count;
    int capacity;
} AdjList;

void add_neighbor(AdjList *adj, int u, int v) {
    if (adj[u].count == adj[u].capacity) {
        adj[u].capacity = (adj[u].capacity == 0) ? 2 : adj[u].capacity * 2;
        adj[u].neighbors = (int *)realloc(adj[u].neighbors, adj[u].capacity * sizeof(int));
    }
    adj[u].neighbors[adj[u].count++] = v;
}

int main(void) {
    int V, E, N, T;
    if (scanf("%d %d %d %d", &V, &E, &N, &T) != 4) {
        fprintf(stderr, "Error reading graph parameters\n");
        return 1;
    }

    // Allocate adjacency list for V vertices
    AdjList *adj = (AdjList *)calloc(V, sizeof(AdjList));
    if (!adj) {
        fprintf(stderr, "Memory allocation failed\n");
        return 1;
    }

    // Read edges and populate bidirectional adjacency list
    for (int i = 0; i < E; i++) {
        int u, v;
        if (scanf("%d %d", &u, &v) != 2) {
            fprintf(stderr, "Error reading edge %d\n", i);
            return 1;
        }
        add_neighbor(adj, u, v);
        add_neighbor(adj, v, u);
    }

    // Print adjacency list to verify
    printf("Graph details: V = %d, E = %d, Source N = %d, Threads T = %d\n", V, E, N, T);
    printf("Adjacency List:\n");
    for (int i = 0; i < V; i++) {
        printf("Node %d: ", i);
        for (int j = 0; j < adj[i].count; j++) {
            printf("%d ", adj[i].neighbors[j]);
        }
        printf("\n");
    }

    // Free allocated memory
    for (int i = 0; i < V; i++) {
        free(adj[i].neighbors);
    }
    free(adj);

    return 0;
}

