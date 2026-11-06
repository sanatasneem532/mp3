
#include <errno.h>
#include <inttypes.h>
#include <limits.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

typedef struct {
  int *neighbors;
  size_t count;
  size_t capacity;
} AdjList;

// Read and validate an integer safely.
// Returns 1 on success, 0 on EOF, -1 on invalid input.
int read_int(int *value) {
  char token[128];
  char *end;

  if (scanf("%127s", token) != 1) {
    return 0;
  }

  errno = 0;
  intmax_t num = strtoimax(token, &end, 10);

  if (errno == ERANGE || end == token || *end != '\0' || num < INT_MIN ||
      num > INT_MAX) {
    return -1;
  }

  *value = (int)num;
  return 1;
}

// Free the entire adjacency list
void free_graph(AdjList *adj, int V) {
  if (adj == NULL) {
    return;
  }

  for (int i = 0; i < V; i++) {
    free(adj[i].neighbors);
  }

  free(adj);
}

// Add v as a neighbor of u.
// Returns 1 on success, 0 on failure.
int add_neighbor(AdjList *adj, int u, int v) {

  if (adj[u].count == adj[u].capacity) {

    size_t new_capacity;

    if (adj[u].capacity == 0) {
      new_capacity = 2;
    } else {

      // Prevent capacity overflow
      if (adj[u].capacity > SIZE_MAX / 2) {
        return 0;
      }

      new_capacity = adj[u].capacity * 2;
    }

    // Prevent allocation-size overflow
    if (new_capacity > SIZE_MAX / sizeof(int)) {
      return 0;
    }

    int *temp = realloc(adj[u].neighbors, new_capacity * sizeof(int));

    if (temp == NULL) {
      return 0;
    }

    adj[u].neighbors = temp;
    adj[u].capacity = new_capacity;
  }

  adj[u].neighbors[adj[u].count++] = v;

  return 1;
}

int main(void) {

  int V, E, N, T;

  // 1. Read graph parameters
  if (read_int(&V) != 1 || read_int(&E) != 1 || read_int(&N) != 1 ||
      read_int(&T) != 1) {

    fprintf(stderr, "Error: Invalid or incomplete graph parameters\n");
    return 1;
  }

  // 2. Validate graph parameters

  if (V <= 0) {
    fprintf(stderr, "Error: Number of vertices must be positive\n");
    return 1;
  }

  if (E < 0) {
    fprintf(stderr, "Error: Number of edges cannot be negative\n");
    return 1;
  }

  if (T <= 0) {
    fprintf(stderr, "Error: Number of threads must be positive\n");
    return 1;
  }

  if (N < 0 || N >= V) {
    fprintf(stderr, "Error: Invalid source vertex\n");
    return 1;
  }

  // 3. Check allocation-size overflow

  if ((size_t)V > SIZE_MAX / sizeof(AdjList)) {
    fprintf(stderr, "Error: Graph is too large\n");
    return 1;
  }

  // 4. Allocate adjacency list

  AdjList *adj = calloc((size_t)V, sizeof(AdjList));

  if (adj == NULL) {
    fprintf(stderr, "Error: Memory allocation failed\n");
    return 1;
  }

  // 5. Read edges and construct bidirectional adjacency list

  for (int i = 0; i < E; i++) {

    int u, v;

    if (read_int(&u) != 1 || read_int(&v) != 1) {
      fprintf(stderr, "Error: Invalid or incomplete edge %d\n", i + 1);
      free_graph(adj, V);
      return 1;
    }

    // Validate both endpoints

    if (u < 0 || u >= V || v < 0 || v >= V) {
      fprintf(stderr, "Error: Invalid vertex in edge %d\n", i + 1);
      free_graph(adj, V);
      return 1;
    }

    // Add edge in both directions

    if (!add_neighbor(adj, u, v)) {
      fprintf(stderr, "Error: Memory allocation failed\n");
      free_graph(adj, V);
      return 1;
    }

    if (!add_neighbor(adj, v, u)) {
      fprintf(stderr, "Error: Memory allocation failed\n");
      free_graph(adj, V);
      return 1;
    }
  }

  // 6. Check for unexpected extra input

  char extra;

  if (scanf(" %c", &extra) == 1) {
    fprintf(stderr, "Error: Unexpected extra input\n");
    free_graph(adj, V);
    return 1;
  }

  // 7. Print graph details

  printf("Graph details:\n");

  printf("V = %d, E = %d, Source N = %d, Threads T = %d\n", V, E, N, T);

  // 8. Print adjacency list

  printf("\nAdjacency List:\n");

  for (int i = 0; i < V; i++) {

    printf("Node %d: ", i);

    if (adj[i].count == 0) {
      printf("(no neighbors)");
    }

    for (size_t j = 0; j < adj[i].count; j++) {
      printf("%d ", adj[i].neighbors[j]);
    }

    printf("\n");
  }

  // 9. Free allocated memory

  free_graph(adj, V);

  return 0;
}
