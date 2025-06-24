#include "sched.h"

/* Filas para diferentes escalonadores */
static struct fifo_queue fifo_ready_queue = {
    .front = 0, .rear = 0, .count = 0
};

static struct rr_pure_queue rr_pure_ready_queue = {
    .front = 0, .rear = 0, .count = 0
};

/* ============ IMPLEMENTAÇÃO FIFO ============ */

static int fifo_enqueue(struct schedproc *rmp)
{
    if (fifo_ready_queue.count >= NR_PROCS) {
        return EAGAIN;
    }
    
    /* Verificar se já está na fila */
    for (int i = 0; i < fifo_ready_queue.count; i++) {
        int idx = (fifo_ready_queue.front + i) % NR_PROCS;
        if (fifo_ready_queue.processes[idx] == rmp->endpoint) {
            return OK; /* Já está na fila */
        }
    }
    
    /* Adicionar à fila */
    int rear_idx = fifo_ready_queue.rear;
    fifo_ready_queue.processes[rear_idx] = rmp->endpoint;
    fifo_ready_queue.arrival_time[rear_idx] = getticks();
    fifo_ready_queue.rear = (fifo_ready_queue.rear + 1) % NR_PROCS;
    fifo_ready_queue.count++;
    
    rmp->arrival_time = fifo_ready_queue.arrival_time[rear_idx];
    
    printf("FIFO: Process %d enqueued (count: %d)\n", 
           rmp->endpoint, fifo_ready_queue.count);
    
    return OK;
}

static int fifo_dequeue(struct schedproc **rmp)
{
    if (fifo_ready_queue.count == 0) {
        *rmp = NULL;
        return EAGAIN;
    }
    
    endpoint_t proc_ep = fifo_ready_queue.processes[fifo_ready_queue.front];
    fifo_ready_queue.front = (fifo_ready_queue.front + 1) % NR_PROCS;
    fifo_ready_queue.count--;
    
    /* Encontrar o schedproc correspondente */
    int proc_nr;
    if (sched_isokendpt(proc_ep, &proc_nr) != OK) {
        return ESRCH;
    }
    
    *rmp = &schedproc[proc_nr];
    
    printf("FIFO: Process %d dequeued (count: %d)\n", 
           proc_ep, fifo_ready_queue.count);
    
    return OK;
}

static int fifo_noquantum(struct schedproc *rmp)
{
    /* Em FIFO, quando o quantum acaba, o processo vai para o final da fila */
    fifo_remove_process(rmp->endpoint);
    fifo_enqueue(rmp);
    
    /* Resetar quantum */
    rmp->time_left = USER_QUANTUM;
    
    printf("FIFO: Process %d quantum expired, moved to end\n", rmp->endpoint);
    
    return OK;
}

static int fifo_start_scheduling(struct schedproc *rmp)
{
    printf("FIFO: Starting scheduling for process %d\n", rmp->endpoint);
    return fifo_enqueue(rmp);
}

static int fifo_stop_scheduling(struct schedproc *rmp)
{
    printf("FIFO: Stopping scheduling for process %d\n", rmp->endpoint);
    return fifo_remove_process(rmp->endpoint);
}

/* Função auxiliar para remover processo específico */
int fifo_remove_process(endpoint_t proc_ep)
{
    for (int i = 0; i < fifo_ready_queue.count; i++) {
        int idx = (fifo_ready_queue.front + i) % NR_PROCS;
        if (fifo_ready_queue.processes[idx] == proc_ep) {
            /* Shift elementos para preencher o espaço */
            for (int j = i; j < fifo_ready_queue.count - 1; j++) {
                int curr_idx = (fifo_ready_queue.front + j) % NR_PROCS;
                int next_idx = (fifo_ready_queue.front + j + 1) % NR_PROCS;
                fifo_ready_queue.processes[curr_idx] = fifo_ready_queue.processes[next_idx];
                fifo_ready_queue.arrival_time[curr_idx] = fifo_ready_queue.arrival_time[next_idx];
            }
            fifo_ready_queue.count--;
            fifo_ready_queue.rear = (fifo_ready_queue.rear - 1 + NR_PROCS) % NR_PROCS;
            
            printf("FIFO: Process %d removed from queue\n", proc_ep);
            return OK;
        }
    }
    return ESRCH;
}

/* Função para obter estatísticas da fila FIFO */
void fifo_print_stats(void)
{
    printf("FIFO Queue Stats: count=%d, front=%d, rear=%d\n",
           fifo_ready_queue.count, fifo_ready_queue.front, fifo_ready_queue.rear);
    
    if (fifo_ready_queue.count > 0) {
        printf("FIFO Queue contents: ");
        for (int i = 0; i < fifo_ready_queue.count; i++) {
            int idx = (fifo_ready_queue.front + i) % NR_PROCS;
            printf("%d ", fifo_ready_queue.processes[idx]);
        }
        printf("\n");
    }
}

/* ============ IMPLEMENTAÇÃO ROUND-ROBIN PURO ============ */

static int rr_pure_enqueue(struct schedproc *rmp)
{
    if (rr_pure_ready_queue.count >= NR_PROCS) {
        return EAGAIN;
    }
    
    /* Verificar se já está na fila */
    for (int i = 0; i < rr_pure_ready_queue.count; i++) {
        int idx = (rr_pure_ready_queue.front + i) % NR_PROCS;
        if (rr_pure_ready_queue.processes[idx] == rmp->endpoint) {
            return OK; /* Já está na fila */
        }
    }
    
    /* Adicionar à fila circular */
    int rear_idx = rr_pure_ready_queue.rear;
    rr_pure_ready_queue.processes[rear_idx] = rmp->endpoint;
    rr_pure_ready_queue.last_scheduled[rear_idx] = 0; /* Nunca executou */
    rr_pure_ready_queue.rear = (rr_pure_ready_queue.rear + 1) % NR_PROCS;
    rr_pure_ready_queue.count++;
    
    /* Preservar prioridade original mas não usar para escalonamento */
    if (rmp->original_priority == 0) {
        rmp->original_priority = rmp->priority;
    }
    
    printf("RR-Pure: Process %d enqueued (count: %d)\n", 
           rmp->endpoint, rr_pure_ready_queue.count);
    
    return OK;
}

static int rr_pure_dequeue(struct schedproc **rmp)
{
    if (rr_pure_ready_queue.count == 0) {
        *rmp = NULL;
        return EAGAIN;
    }
    
    /* Selecionar próximo processo da fila circular */
    endpoint_t proc_ep = rr_pure_ready_queue.processes[rr_pure_ready_queue.front];
    
    /* Encontrar o schedproc correspondente */
    int proc_nr;
    if (sched_isokendpt(proc_ep, &proc_nr) != OK) {
        /* Processo não existe mais, remover da fila */
        rr_pure_ready_queue.front = (rr_pure_ready_queue.front + 1) % NR_PROCS;
        rr_pure_ready_queue.count--;
        return rr_pure_dequeue(rmp); /* Tentar próximo */
    }
    
    *rmp = &schedproc[proc_nr];
    (*rmp)->last_run = getticks();
    
    /* Atualizar timestamp */
    int front_idx = rr_pure_ready_queue.front;
    rr_pure_ready_queue.last_scheduled[front_idx] = getticks();
    
    printf("RR-Pure: Process %d selected for execution\n", proc_ep);
    
    return OK;
}

static int rr_pure_noquantum(struct schedproc *rmp)
{
    /* No Round-Robin puro, quando o quantum acaba, processo continua na fila */
    /* Apenas avança para o próximo processo na fila circular */
    
    /* Mover para próximo na fila circular */
    if (rr_pure_ready_queue.count > 0) {
        rr_pure_ready_queue.front = (rr_pure_ready_queue.front + 1) % NR_PROCS;
    }
    
    /* Resetar quantum */
    rmp->time_left = USER_QUANTUM;
    
    printf("RR-Pure: Process %d quantum expired, next in rotation\n", rmp->endpoint);
    
    return OK;
}

static int rr_pure_start_scheduling(struct schedproc *rmp)
{
    printf("RR-Pure: Starting scheduling for process %d\n", rmp->endpoint);
    return rr_pure_enqueue(rmp);
}

static int rr_pure_stop_scheduling(struct schedproc *rmp)
{
    printf("RR-Pure: Stopping scheduling for process %d\n", rmp->endpoint);
    return rr_pure_remove_process(rmp->endpoint);
}

/* Função auxiliar para remover processo da fila RR puro */
int rr_pure_remove_process(endpoint_t proc_ep)
{
    for (int i = 0; i < rr_pure_ready_queue.count; i++) {
        int idx = (rr_pure_ready_queue.front + i) % NR_PROCS;
        if (rr_pure_ready_queue.processes[idx] == proc_ep) {
            /* Shift elementos para preencher o espaço */
            for (int j = i; j < rr_pure_ready_queue.count - 1; j++) {
                int curr_idx = (rr_pure_ready_queue.front + j) % NR_PROCS;
                int next_idx = (rr_pure_ready_queue.front + j + 1) % NR_PROCS;
                rr_pure_ready_queue.processes[curr_idx] = rr_pure_ready_queue.processes[next_idx];
                rr_pure_ready_queue.last_scheduled[curr_idx] = rr_pure_ready_queue.last_scheduled[next_idx];
            }
            rr_pure_ready_queue.count--;
            rr_pure_ready_queue.rear = (rr_pure_ready_queue.rear - 1 + NR_PROCS) % NR_PROCS;
            
            printf("RR-Pure: Process %d removed from queue\n", proc_ep);
            return OK;
        }
    }
    return ESRCH;
}

/* Função para obter estatísticas da fila RR puro */
void rr_pure_print_stats(void)
{
    printf("RR-Pure Queue Stats: count=%d, front=%d, rear=%d\n",
           rr_pure_ready_queue.count, rr_pure_ready_queue.front, rr_pure_ready_queue.rear);
    
    if (rr_pure_ready_queue.count > 0) {
        printf("RR-Pure Queue contents: ");
        for (int i = 0; i < rr_pure_ready_queue.count; i++) {
            int idx = (rr_pure_ready_queue.front + i) % NR_PROCS;
            printf("%d ", rr_pure_ready_queue.processes[idx]);
        }
        printf("\n");
    }
}