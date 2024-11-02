#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <unistd.h>
#include <string.h>
#include <time.h>
#include <sched.h>
#include <math.h>

#define NUM_ACCOUNTS 5
#define NUM_WORKERS 4
#define NUM_CLIENTS 1
#define MAX_QUEUE_SIZE 10
#define TOTAL_OPERATIONS 20

typedef struct {
    int id;
    double balance;
} Account;

typedef enum {
    SAQUE_DEPOSITO,
    TRANSFERENCIA,
    BALANCO_GERAL,
    TERMINATE
} OperationType;

typedef struct {
    OperationType type;
    int client_id;
    int account_id;
    int target_account_id;
    double amount;
} Operation;

typedef struct {
    volatile int locked;
} MyMutex;

typedef struct {
    int count;
    MyMutex mutex;
} MySemaphore;

typedef struct {
    Operation operations[MAX_QUEUE_SIZE];
    int head;
    int tail;
    int size;
    MyMutex mutex;
    MySemaphore items;
    MySemaphore spaces;
} OperationQueue;

void my_mutex_init(MyMutex *mutex) {
    mutex->locked = 0;
}

void my_mutex_lock(MyMutex *mutex) {
    while (__sync_lock_test_and_set(&(mutex->locked), 1)) {
        sched_yield();
    }
}

void my_mutex_unlock(MyMutex *mutex) {
    __sync_lock_release(&(mutex->locked));
}

void my_semaphore_init(MySemaphore *sem, int value) {
    sem->count = value;
    my_mutex_init(&sem->mutex);
}

void my_semaphore_wait(MySemaphore *sem) {
    while (1) {
        my_mutex_lock(&sem->mutex);
        if (sem->count > 0) {
            sem->count--;
            my_mutex_unlock(&sem->mutex);
            break;
        }
        my_mutex_unlock(&sem->mutex);
        sched_yield();
    }
}

void my_semaphore_signal(MySemaphore *sem) {
    my_mutex_lock(&sem->mutex);
    sem->count++;
    my_mutex_unlock(&sem->mutex);
}

void queue_init(OperationQueue *queue) {
    queue->head = 0;
    queue->tail = 0;
    queue->size = 0;
    my_mutex_init(&queue->mutex);
    my_semaphore_init(&queue->items, 0);
    my_semaphore_init(&queue->spaces, MAX_QUEUE_SIZE);
}

void enqueue(OperationQueue *queue, Operation op) {
    my_semaphore_wait(&queue->spaces);
    my_mutex_lock(&queue->mutex);
    queue->operations[queue->tail] = op;
    queue->tail = (queue->tail + 1) % MAX_QUEUE_SIZE;
    queue->size++;
    my_mutex_unlock(&queue->mutex);
    my_semaphore_signal(&queue->items);
}

Operation dequeue(OperationQueue *queue) {
    my_semaphore_wait(&queue->items);
    my_mutex_lock(&queue->mutex);
    Operation op = queue->operations[queue->head];
    queue->head = (queue->head + 1) % MAX_QUEUE_SIZE;
    queue->size--;
    my_mutex_unlock(&queue->mutex);
    my_semaphore_signal(&queue->spaces);
    return op;
}

Account accounts[NUM_ACCOUNTS];
OperationQueue op_queue;
int total_operations = 0;
MyMutex total_ops_mutex;

void initialize_accounts() {
    srand(time(NULL));
    for (int i = 0; i < NUM_ACCOUNTS; i++) {
        accounts[i].id = i + 1;
        accounts[i].balance = rand() % 1000;
    }
}

void print_accounts() {
    printf("\n===== Balanço Geral =====\n");
    for (int i = 0; i < NUM_ACCOUNTS; i++) {
        printf("Conta ID: %d, Saldo: %.2f\n", accounts[i].id, accounts[i].balance);
    }
    printf("=========================\n");
}

void *worker_thread(void *arg) {
    while (1) {
        Operation op = dequeue(&op_queue);

        if (op.type == TERMINATE) {
            break;
        }

        usleep(1000000); // Atraso de 1 segundo (10 vezes o atraso do cliente)

        if (op.type == SAQUE_DEPOSITO) {
            accounts[op.account_id - 1].balance += op.amount;
            if (op.amount >= 0) {
                printf("[Cliente %d] Depósito: +%.2f na Conta %d\n", op.client_id, op.amount, op.account_id);
            } else {
                printf("[Cliente %d] Saque: %.2f da Conta %d\n", op.client_id, -op.amount, op.account_id);
            }
        } else if (op.type == TRANSFERENCIA) {
            accounts[op.account_id - 1].balance -= op.amount;
            accounts[op.target_account_id - 1].balance += op.amount;
            printf("[Cliente %d] Transferência: %.2f da Conta %d para Conta %d\n",
                   op.client_id, op.amount, op.account_id, op.target_account_id);
        } else if (op.type == BALANCO_GERAL) {
            printf("[Worker] Iniciando Balanço Geral solicitado pelo Cliente %d\n", op.client_id);
            print_accounts();
        }
    }
    return NULL;
}

void *client_thread(void *arg) {
    int client_id = *((int *)arg);
    int local_operations = 0;

    while (1) {
        usleep(100000); // Atraso de 0.1 segundo

        Operation op;
        op.client_id = client_id;
        int op_type = rand() % 2;

        if (op_type == 0) {
            op.type = SAQUE_DEPOSITO;
            op.account_id = rand() % NUM_ACCOUNTS + 1;
            if (rand() % 2 == 0) {
                op.amount = (double)(rand() % 1000 + 1);
            } else {
                op.amount = -((double)(rand() % 500 + 1));
            }
            printf("[Cliente %d] Solicitou %s de %.2f na Conta %d\n", client_id,
                   (op.amount >= 0) ? "depósito" : "saque", fabs(op.amount), op.account_id);
        } else {
            op.type = TRANSFERENCIA;
            op.account_id = rand() % NUM_ACCOUNTS + 1;
            op.target_account_id = rand() % NUM_ACCOUNTS + 1;
            op.amount = (double)(rand() % 500 + 1);
            printf("[Cliente %d] Solicitou transferência de %.2f da Conta %d para Conta %d\n",
                   client_id, op.amount, op.account_id, op.target_account_id);
        }

        enqueue(&op_queue, op);
        local_operations++;

        if (local_operations % 9 == 0) {
            // Enviar o balanço geral após 9 operações
            Operation balanco_op;
            balanco_op.type = BALANCO_GERAL;
            balanco_op.client_id = client_id;
            enqueue(&op_queue, balanco_op);
            printf("[Cliente %d] Solicitou Balanço Geral\n", client_id);
        }

        my_mutex_lock(&total_ops_mutex);
        total_operations++;
        if (total_operations >= TOTAL_OPERATIONS) {
            my_mutex_unlock(&total_ops_mutex);
            break;
        }
        my_mutex_unlock(&total_ops_mutex);
    }
    return NULL;
}

int main() {
    initialize_accounts();
    queue_init(&op_queue);
    my_mutex_init(&total_ops_mutex);

    pthread_t workers[NUM_WORKERS];
    pthread_t clients[NUM_CLIENTS];
    int client_ids[NUM_CLIENTS];

    for (int i = 0; i < NUM_WORKERS; i++) {
        if (pthread_create(&workers[i], NULL, worker_thread, NULL) != 0) {
            perror("Erro ao criar thread trabalhadora");
            exit(1);
        }
    }

    for (int i = 0; i < NUM_CLIENTS; i++) {
        client_ids[i] = i + 1;
        if (pthread_create(&clients[i], NULL, client_thread, &client_ids[i]) != 0) {
            perror("Erro ao criar thread cliente");
            exit(1);
        }
    }

    for (int i = 0; i < NUM_CLIENTS; i++) {
        if (pthread_join(clients[i], NULL) != 0) {
            perror("Erro ao aguardar thread cliente");
            exit(1);
        }
    }

    for (int i = 0; i < NUM_WORKERS; i++) {
        Operation terminate_op;
        terminate_op.type = TERMINATE;
        enqueue(&op_queue, terminate_op);
    }

    for (int i = 0; i < NUM_WORKERS; i++) {
        if (pthread_join(workers[i], NULL) != 0) {
            perror("Erro ao aguardar thread trabalhadora");
            exit(1);
        }
    }

    print_accounts();

    return 0;
}
