
#include <errno.h>
#include <inttypes.h>
#include <limits.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#define INF LLONG_MAX

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

      if (adj[u].capacity > SIZE_MAX / 2) {
        return 0;
      }

      new_capacity = adj[u].capacity * 2;
    }

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

// Sequential Bellman-Ford algorithm.
// Returns 1 on success, 0 on allocation failure.

int bellman_ford(AdjList *adj, int V, int N, long long **result) {

  // Check allocation size
  if ((size_t)V > SIZE_MAX / sizeof(long long)) {
    return 0;
  }

  // Allocate two distance arrays
  long long *dist = malloc((size_t)V * sizeof(long long));
  long long *new_dist = malloc((size_t)V * sizeof(long long));

  if (dist == NULL || new_dist == NULL) {
    free(dist);
    free(new_dist);
    return 0;
  }

  // Initialize distances
  for (int i = 0; i < V; i++) {
    dist[i] = INF;
    new_dist[i] = INF;
  }

  // Source distance is zero
  dist[N] = 0;

  // Bellman-Ford: at most V-1 iterations
  for (int iter = 0; iter < V - 1; iter++) {

    // Initially assume no changes
    int updated = 0;

    // Copy previous distances into new distances
    for (int i = 0; i < V; i++) {
      new_dist[i] = dist[i];
    }

    // Calculate distances for every destination vertex
    for (int v = 0; v < V; v++) {

      // Examine all neighbours of destination v
      for (size_t j = 0; j < adj[v].count; j++) {

        int u = adj[v].neighbors[j];

        // Ignore unreachable neighbours
        if (dist[u] == INF) {
          continue;
        }

        // Every edge has weight 1
        long long candidate = dist[u] + 1;

        // Relaxation
        if (candidate < new_dist[v]) {
          new_dist[v] = candidate;
          updated = 1;
        }
      }
    }

    // Swap the arrays
    long long *temp = dist;
    dist = new_dist;
    new_dist = temp;

    // Stop if no distances changed
    if (!updated) {
      break;
    }
  }

  // Free the unused array
  free(new_dist);

  // Return final distances
  *result = dist;

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
  if (V <= 0 || E < 0 || T <= 0 || N < 0 || N >= V) {

    fprintf(stderr, "Error: Invalid input parameters\n");
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

    // Validate endpoints
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

  // 7. Run sequential Bellman-Ford
  long long *dist = NULL;

  if (!bellman_ford(adj, V, N, &dist)) {

    fprintf(stderr, "Error: Memory allocation failed\n");

    free_graph(adj, V);
    return 1;
  }

  // 8. Print shortest distances
  // Use -1 for unreachable vertices

  for (int i = 0; i < V; i++) {

    if (dist[i] == INF) {
      printf("-1");
    } else {
      printf("%lld", dist[i]);
    }

    if (i < V - 1) {
      printf(" ");
    }
  }

  printf("\n");

  // 9. Free allocated memory
  free(dist);
  free_graph(adj, V);

  return 0;
}
