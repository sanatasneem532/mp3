
#define _POSIX_C_SOURCE 200809L

#include <errno.h>
#include <inttypes.h>
#include <limits.h>
#include <pthread.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#define INF LLONG_MAX

// ============================================================
// DATA STRUCTURES
// ============================================================

typedef struct {
  int *neighbors;
  size_t count;
  size_t capacity;
} AdjList;

typedef struct {
  pthread_mutex_t mutex;
  pthread_cond_t cond;
  size_t waiting;
  size_t total;
  unsigned long long generation;
} Barrier;

typedef struct {
  pthread_mutex_t mutex;
  pthread_cond_t cond;
  int released;
  int abort;
} LaunchGate;

typedef struct {
  AdjList *adj;

  int start;
  int end;
  int thread_id;

  long long *dist;
  long long *new_dist;

  int *thread_updated;
  int *stop;

  Barrier *start_barrier;
  Barrier *done_barrier;

  LaunchGate *gate;

} ThreadArgs;

// ============================================================
// INPUT HANDLING
// ============================================================

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

// ============================================================
// GRAPH MEMORY MANAGEMENT
// ============================================================

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

// ============================================================
// BARRIER IMPLEMENTATION
// ============================================================

int barrier_init(Barrier *b, size_t total) {

  b->waiting = 0;
  b->total = total;
  b->generation = 0;

  if (pthread_mutex_init(&b->mutex, NULL) != 0) {
    return 0;
  }

  if (pthread_cond_init(&b->cond, NULL) != 0) {

    pthread_mutex_destroy(&b->mutex);

    return 0;
  }

  return 1;
}

void barrier_wait(Barrier *b) {

  pthread_mutex_lock(&b->mutex);

  unsigned long long current_generation = b->generation;

  b->waiting++;

  if (b->waiting == b->total) {

    b->waiting = 0;
    b->generation++;

    pthread_cond_broadcast(&b->cond);

  } else {

    while (current_generation == b->generation) {

      pthread_cond_wait(&b->cond, &b->mutex);
    }
  }

  pthread_mutex_unlock(&b->mutex);
}

void barrier_destroy(Barrier *b) {

  pthread_cond_destroy(&b->cond);
  pthread_mutex_destroy(&b->mutex);
}

// ============================================================
// LAUNCH GATE
// ============================================================

int launch_gate_init(LaunchGate *gate) {

  gate->released = 0;
  gate->abort = 0;

  if (pthread_mutex_init(&gate->mutex, NULL) != 0) {
    return 0;
  }

  if (pthread_cond_init(&gate->cond, NULL) != 0) {

    pthread_mutex_destroy(&gate->mutex);

    return 0;
  }

  return 1;
}

void release_workers(LaunchGate *gate, int abort) {

  pthread_mutex_lock(&gate->mutex);

  gate->abort = abort;
  gate->released = 1;

  pthread_cond_broadcast(&gate->cond);

  pthread_mutex_unlock(&gate->mutex);
}

void launch_gate_destroy(LaunchGate *gate) {

  pthread_cond_destroy(&gate->cond);
  pthread_mutex_destroy(&gate->mutex);
}

// ============================================================
// WORKER THREAD FUNCTION
// ============================================================

void *worker_function(void *arg) {

  ThreadArgs *args = (ThreadArgs *)arg;

  // Wait until all threads have been created.

  pthread_mutex_lock(&args->gate->mutex);

  while (!args->gate->released) {

    pthread_cond_wait(&args->gate->cond, &args->gate->mutex);
  }

  int abort = args->gate->abort;

  pthread_mutex_unlock(&args->gate->mutex);

  if (abort) {
    return NULL;
  }

  // Process Bellman-Ford iterations.

  while (1) {

    // Wait for main thread to start the iteration.

    barrier_wait(args->start_barrier);

    // Check whether execution should stop.

    if (*args->stop) {
      break;
    }

    // Process only assigned destination vertices.

    for (int v = args->start; v < args->end; v++) {

      long long best = args->dist[v];

      // Examine all neighbours of destination v.

      for (size_t j = 0; j < args->adj[v].count; j++) {

        int u = args->adj[v].neighbors[j];

        // Skip unreachable neighbours.

        if (args->dist[u] == INF) {
          continue;
        }

        // Every edge has weight 1.

        long long candidate = args->dist[u] + 1;

        // Relaxation.

        if (candidate < best) {
          best = candidate;
        }
      }

      // Write only to our assigned destination.

      args->new_dist[v] = best;

      // Record whether this thread found an update.

      if (best < args->dist[v]) {
        args->thread_updated[args->thread_id] = 1;
      }
    }

    // Wait until all workers finish their calculations.

    barrier_wait(args->done_barrier);
  }

  return NULL;
}

// ============================================================
// MULTITHREADED BELLMAN-FORD
// ============================================================

int bellman_ford(AdjList *adj, int V, int N, int T, long long **result) {

  // Check allocation-size overflow.

  if ((size_t)V > SIZE_MAX / sizeof(long long) ||
      (size_t)T > SIZE_MAX / sizeof(pthread_t) ||
      (size_t)T > SIZE_MAX / sizeof(ThreadArgs) ||
      (size_t)T > SIZE_MAX / sizeof(int)) {

    return 0;
  }

  // Allocate distance arrays.

  long long *dist = malloc((size_t)V * sizeof(long long));

  long long *new_dist = malloc((size_t)V * sizeof(long long));

  // Allocate thread-related arrays.

  pthread_t *threads = malloc((size_t)T * sizeof(pthread_t));

  ThreadArgs *args = malloc((size_t)T * sizeof(ThreadArgs));

  int *thread_updated = calloc((size_t)T, sizeof(int));

  // Check allocations.

  if (dist == NULL || new_dist == NULL || threads == NULL || args == NULL ||
      thread_updated == NULL) {

    free(dist);
    free(new_dist);
    free(threads);
    free(args);
    free(thread_updated);

    return 0;
  }

  // Initialize distances.

  for (int i = 0; i < V; i++) {

    dist[i] = INF;
    new_dist[i] = INF;
  }

  dist[N] = 0;

  // Shared stop flag.

  int stop = 0;

  // Initialize launch gate.

  LaunchGate gate;

  if (!launch_gate_init(&gate)) {
    goto allocation_failure;
  }

  // Initialize barriers.

  Barrier start_barrier;
  Barrier done_barrier;

  if (!barrier_init(&start_barrier, (size_t)T + 1)) {

    launch_gate_destroy(&gate);

    goto allocation_failure;
  }

  if (!barrier_init(&done_barrier, (size_t)T + 1)) {

    barrier_destroy(&start_barrier);
    launch_gate_destroy(&gate);

    goto allocation_failure;
  }

  // Create worker threads.

  int created = 0;

  for (int i = 0; i < T; i++) {

    // Divide vertices evenly among threads.

    int base = V / T;
    int remainder = V % T;

    int start = i * base + (i < remainder ? i : remainder);

    int end = start + base + (i < remainder ? 1 : 0);

    // Initialize thread arguments.

    args[i].adj = adj;

    args[i].start = start;
    args[i].end = end;

    args[i].thread_id = i;

    args[i].dist = dist;
    args[i].new_dist = new_dist;

    args[i].thread_updated = thread_updated;

    args[i].stop = &stop;

    args[i].start_barrier = &start_barrier;
    args[i].done_barrier = &done_barrier;

    args[i].gate = &gate;

    // Create thread.

    if (pthread_create(&threads[i], NULL, worker_function, &args[i]) != 0) {

      fprintf(stderr, "Error: Could not create thread %d\n", i);

      // Release already-created threads without entering barriers.

      release_workers(&gate, 1);

      for (int j = 0; j < created; j++) {
        pthread_join(threads[j], NULL);
      }

      barrier_destroy(&start_barrier);
      barrier_destroy(&done_barrier);

      launch_gate_destroy(&gate);

      goto allocation_failure;
    }

    created++;
  }

  // All threads have been created.
  // Allow workers to begin.

  release_workers(&gate, 0);

  // ========================================================
  // BELLMAN-FORD ITERATIONS
  // ========================================================

  for (int iter = 0; iter < V - 1; iter++) {

    // Copy previous iteration distances.

    for (int i = 0; i < V; i++) {
      new_dist[i] = dist[i];
    }

    // Reset update flags.

    for (int i = 0; i < T; i++) {
      thread_updated[i] = 0;
    }

    // Release all workers.

    barrier_wait(&start_barrier);

    // Wait for all workers to finish.

    barrier_wait(&done_barrier);

    // Check whether any thread updated a distance.

    int updated = 0;

    for (int i = 0; i < T; i++) {

      if (thread_updated[i]) {

        updated = 1;
        break;
      }
    }

    // Swap the distance arrays.

    long long *temp = dist;

    dist = new_dist;
    new_dist = temp;

    // Update pointers for the next iteration.

    for (int i = 0; i < T; i++) {

      args[i].dist = dist;
      args[i].new_dist = new_dist;
    }

    // Early termination.

    if (!updated) {
      break;
    }
  }

  // ========================================================
  // STOP WORKER THREADS
  // ========================================================

  stop = 1;

  // Release workers so they can observe the stop flag.

  barrier_wait(&start_barrier);

  // Wait for all workers to exit.

  for (int i = 0; i < T; i++) {
    pthread_join(threads[i], NULL);
  }

  // Destroy synchronization objects.

  barrier_destroy(&start_barrier);
  barrier_destroy(&done_barrier);

  launch_gate_destroy(&gate);

  // Free unused memory.

  free(new_dist);
  free(threads);
  free(args);
  free(thread_updated);

  // Return final distances.

  *result = dist;

  return 1;

allocation_failure:

  free(dist);
  free(new_dist);
  free(threads);
  free(args);
  free(thread_updated);

  return 0;
}

// ============================================================
// MAIN
// ============================================================

int main(void) {

  int V, E, N, T;

  // Read graph parameters.

  if (read_int(&V) != 1 || read_int(&E) != 1 || read_int(&N) != 1 ||
      read_int(&T) != 1) {

    fprintf(stderr, "Error: Invalid or incomplete graph parameters\n");

    return 1;
  }

  // Validate parameters.

  if (V <= 0 || E < 0 || T <= 0 || N < 0 || N >= V) {

    fprintf(stderr, "Error: Invalid input parameters\n");

    return 1;
  }

  // Check allocation-size overflow.

  if ((size_t)V > SIZE_MAX / sizeof(AdjList)) {

    fprintf(stderr, "Error: Graph is too large\n");

    return 1;
  }

  // Allocate adjacency list.

  AdjList *adj = calloc((size_t)V, sizeof(AdjList));

  if (adj == NULL) {

    fprintf(stderr, "Error: Memory allocation failed\n");

    return 1;
  }

  // Read edges and construct bidirectional adjacency list.

  for (int i = 0; i < E; i++) {

    int u, v;

    if (read_int(&u) != 1 || read_int(&v) != 1) {

      fprintf(stderr, "Error: Invalid or incomplete edge %d\n", i + 1);

      free_graph(adj, V);

      return 1;
    }

    // Validate endpoints.

    if (u < 0 || u >= V || v < 0 || v >= V) {

      fprintf(stderr, "Error: Invalid vertex in edge %d\n", i + 1);

      free_graph(adj, V);

      return 1;
    }

    // Add edge in both directions.

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

  // Reject unexpected extra input.

  char extra;

  if (scanf(" %c", &extra) == 1) {

    fprintf(stderr, "Error: Unexpected extra input\n");

    free_graph(adj, V);

    return 1;
  }

  // Run multithreaded Bellman-Ford.

  long long *dist = NULL;

  if (!bellman_ford(adj, V, N, T, &dist)) {

    fprintf(stderr, "Error: Bellman-Ford execution failed\n");

    free_graph(adj, V);

    return 1;
  }

  // Print ONLY the shortest distances.
  // Use -1 for unreachable vertices.

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

  // Free all memory

  free(dist);
  free_graph(adj, V);

  return 0;
}
