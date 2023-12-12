#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <time.h>
#include <unistd.h>

#include "log/log.h"
#include "os_graph.h"
#include "os_threadpool.h"
#include "utils.h"

#define NUM_THREADS 4

static int sum;
static os_graph_t *graph;
static os_threadpool_t *tp;
static pthread_mutex_t sum_lock = PTHREAD_MUTEX_INITIALIZER;
static pthread_mutex_t graph_lock = PTHREAD_MUTEX_INITIALIZER;

void process_node(unsigned int idx) {
    os_node_t *node = graph->nodes[idx];

    pthread_mutex_lock(&graph_lock);
    if (graph->visited[idx] != NOT_VISITED) {
        pthread_mutex_unlock(&graph_lock);
        return;
    }
    graph->visited[idx] = PROCESSING;
    pthread_mutex_unlock(&graph_lock);

    for (unsigned int i = 0; i < node->num_neighbours; ++i) {
        unsigned int neighbor_idx = node->neighbours[i];
        process_node(node->neighbours[i]);
    }

    pthread_mutex_lock(&sum_lock);
    sum += node->info;
    pthread_mutex_unlock(&sum_lock);

    pthread_mutex_lock(&graph_lock);
    graph->visited[idx] = DONE;
    pthread_mutex_unlock(&graph_lock);
}

int main(int argc, char *argv[]) {
    FILE *input_file;

    if (argc != 2) {
        fprintf(stderr, "Usage: %s input_file\n", argv[0]);
        exit(EXIT_FAILURE);
    }

    input_file = fopen(argv[1], "r");
    DIE(input_file == NULL, "fopen");

    graph = create_graph_from_file(input_file);

    graph->visited = malloc(graph->num_nodes * sizeof(*graph->visited));
    for (unsigned int i = 0; i < graph->num_nodes; ++i) {
        graph->visited[i] = NOT_VISITED;
    }

    tp = create_threadpool(NUM_THREADS);
    process_node(0);
    pthread_mutex_init(&graph_lock, NULL);
    os_task_t *initial_task = create_task((void (*)(void *))process_node,
                                          (void *)(unsigned int)0, NULL);
    enqueue_task(tp, initial_task);
    wait_for_completion(tp);
    destroy_threadpool(tp);
    printf("%d", sum);
    return 0;
}
