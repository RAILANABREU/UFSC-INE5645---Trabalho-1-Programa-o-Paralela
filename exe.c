#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <unistd.h>
#include <string.h>
#include <time.h>
#include <math.h>

#define NUM_ACCOUNTS 5
#define NUM_WORKERS 4
#define NUM_CLIENTS 1
#define MAX_QUEUE_SIZE 10
#define TOTAL_OPERATIONS 50
#define LOCK_TIMEOUT 5 // Número de tentativas para o timeout do bloqueio

int operations_processed = 0; // Nova variável para rastrear operações processadas

typedef struct
{
    int id;
    double balance;
    pthread_mutex_t mutex; // Mutex para cada conta
} Account;

typedef enum
{
    SAQUE_DEPOSITO,
    TRANSFERENCIA,
    BALANCO_GERAL,
    TERMINATE
} OperationType;

typedef struct
{
    OperationType type;
    int client_id;
    int account_id;
    int target_account_id;
    double amount;
} Operation;

typedef struct
{
    Operation operations[MAX_QUEUE_SIZE];
    int head;
    int tail;
    int size;
    pthread_mutex_t mutex;
    pthread_cond_t items;
    pthread_cond_t spaces;
} OperationQueue;

Account accounts[NUM_ACCOUNTS];
OperationQueue op_queue;
int total_operations = 0;
int operation_counter = 0; // Contador para rastrear operações
pthread_mutex_t total_ops_mutex;
pthread_cond_t balance_cond;   // Condição para sinalizar balanço geral
pthread_cond_t operation_cond; // Condição para threads aguardarem durante o balanço

// Função para tentar bloquear o mutex com timeout
int try_lock_with_timeout(pthread_mutex_t *mutex)
{
    int attempts = 0;
    while (attempts < LOCK_TIMEOUT)
    {
        if (pthread_mutex_trylock(mutex) == 0)
        {
            return 0; // Sucesso no bloqueio
        }
        attempts++;
        usleep(100000); // Espera 0.1 segundo antes de tentar novamente
    }
    return -1; // Falha após o timeout
}

void initialize_accounts()
{
    srand(time(NULL));
    for (int i = 0; i < NUM_ACCOUNTS; i++)
    {
        accounts[i].id = i + 1;
        accounts[i].balance = rand() % 1000;
        pthread_mutex_init(&accounts[i].mutex, NULL); // Inicializar mutex de cada conta
    }
}

void print_accounts()
{
    printf("\n===== Balanço Geral =====\n");
    for (int i = 0; i < NUM_ACCOUNTS; i++)
    {
        printf("Conta ID: %d, Saldo: %.2f\n", accounts[i].id, accounts[i].balance);
    }
    printf("=========================\n");
}

void queue_init(OperationQueue *queue)
{
    queue->head = 0;
    queue->tail = 0;
    queue->size = 0;
    pthread_mutex_init(&queue->mutex, NULL);
    pthread_cond_init(&queue->items, NULL);
    pthread_cond_init(&queue->spaces, NULL);
}

void enqueue(OperationQueue *queue, Operation op)
{
    pthread_mutex_lock(&queue->mutex);
    while (queue->size == MAX_QUEUE_SIZE)
    {
        pthread_cond_wait(&queue->spaces, &queue->mutex);
    }
    queue->operations[queue->tail] = op;
    queue->tail = (queue->tail + 1) % MAX_QUEUE_SIZE;
    queue->size++;
    pthread_cond_signal(&queue->items);
    pthread_mutex_unlock(&queue->mutex);
}

Operation dequeue(OperationQueue *queue)
{
    pthread_mutex_lock(&queue->mutex);
    while (queue->size == 0)
    {
        pthread_cond_wait(&queue->items, &queue->mutex);
    }
    Operation op = queue->operations[queue->head];
    queue->head = (queue->head + 1) % MAX_QUEUE_SIZE;
    queue->size--;
    pthread_cond_signal(&queue->spaces);
    pthread_mutex_unlock(&queue->mutex);
    return op;
}

void *worker_thread(void *arg)
{
    while (1)
    {
        Operation op = dequeue(&op_queue);

        if (op.type == TERMINATE)
        {
            break;
        }

        usleep(1000000); // Atraso de 1 segundo para simulação

        if (op.type == SAQUE_DEPOSITO)
        {
            if (try_lock_with_timeout(&accounts[op.account_id - 1].mutex) == 0)
            {
                accounts[op.account_id - 1].balance += op.amount;
                pthread_mutex_unlock(&accounts[op.account_id - 1].mutex);

                if (op.amount >= 0)
                {
                    printf("[Cliente %d] Depósito: +%.2f na Conta %d\n", op.client_id, op.amount, op.account_id);
                }
                else
                {
                    printf("[Cliente %d] Saque: %.2f da Conta %d\n", op.client_id, -op.amount, op.account_id);
                }
            }
            else
            {
                printf("[Worker] Timeout ao tentar acessar a Conta %d\n", op.account_id);
            }
        }
        else if (op.type == TRANSFERENCIA)
        {
            int first = op.account_id < op.target_account_id ? op.account_id - 1 : op.target_account_id - 1;
            int second = op.account_id > op.target_account_id ? op.account_id - 1 : op.target_account_id - 1;

            if (try_lock_with_timeout(&accounts[first].mutex) == 0)
            {
                if (try_lock_with_timeout(&accounts[second].mutex) == 0)
                {
                    accounts[op.account_id - 1].balance -= op.amount;
                    accounts[op.target_account_id - 1].balance += op.amount;
                    pthread_mutex_unlock(&accounts[second].mutex);
                    pthread_mutex_unlock(&accounts[first].mutex);

                    printf("[Cliente %d] Transferência: %.2f da Conta %d para Conta %d\n",
                           op.client_id, op.amount, op.account_id, op.target_account_id);
                }
                else
                {
                    pthread_mutex_unlock(&accounts[first].mutex);
                    printf("[Worker] Timeout ao tentar acessar a Conta %d para transferência\n", op.target_account_id);
                }
            }
            else
            {
                printf("[Worker] Timeout ao tentar acessar a Conta %d para transferência\n", op.account_id);
            }
        }

        // Incrementar o contador de operações
        pthread_mutex_lock(&total_ops_mutex);
        operation_counter++;
        operations_processed++; // Incrementa operações processadas
        if (operation_counter % 9 == 0 || operations_processed >= TOTAL_OPERATIONS)
        {
            // Notificar a thread principal para exibir o balanço
            pthread_cond_signal(&balance_cond);
        }
        pthread_mutex_unlock(&total_ops_mutex);
    }
    return NULL;
}

void *client_thread(void *arg)
{
    int client_id = *((int *)arg);
    int local_operations = 0;

    while (1)
    {
        usleep(100000); // Atraso de 0.1 segundo

        Operation op;
        op.client_id = client_id;
        int op_type = rand() % 2;

        if (op_type == 0)
        {
            op.type = SAQUE_DEPOSITO;
            op.account_id = rand() % NUM_ACCOUNTS + 1;
            op.amount = (rand() % 2 == 0) ? (double)(rand() % 1000 + 1) : -((double)(rand() % 500 + 1));
        }
        else
        {
            op.type = TRANSFERENCIA;
            op.account_id = rand() % NUM_ACCOUNTS + 1;
            op.target_account_id = rand() % NUM_ACCOUNTS + 1;
            op.amount = (double)(rand() % 500 + 1);
        }

        enqueue(&op_queue, op);
        local_operations++;

        pthread_mutex_lock(&total_ops_mutex);
        total_operations++;
        if (total_operations >= TOTAL_OPERATIONS)
        {
            pthread_mutex_unlock(&total_ops_mutex);
            break;
        }
        pthread_mutex_unlock(&total_ops_mutex);
    }
    return NULL;
}

int main()
{
    initialize_accounts();
    queue_init(&op_queue);
    pthread_mutex_init(&total_ops_mutex, NULL);
    pthread_cond_init(&balance_cond, NULL); // Inicializando condição para balanço geral

    pthread_t workers[NUM_WORKERS];
    pthread_t clients[NUM_CLIENTS];
    int client_ids[NUM_CLIENTS];

    for (int i = 0; i < NUM_WORKERS; i++)
    {
        if (pthread_create(&workers[i], NULL, worker_thread, NULL) != 0)
        {
            perror("Erro ao criar thread trabalhadora");
            exit(1);
        }
    }

    for (int i = 0; i < NUM_CLIENTS; i++)
    {
        client_ids[i] = i + 1;
        if (pthread_create(&clients[i], NULL, client_thread, &client_ids[i]) != 0)
        {
            perror("Erro ao criar thread cliente");
            exit(1);
        }
    }

    // Loop da thread principal para mostrar balanço a cada 9 operações
    while (1)
    {
        pthread_mutex_lock(&total_ops_mutex);
        while (operation_counter < 9 && operations_processed < TOTAL_OPERATIONS)
        {
            pthread_cond_wait(&balance_cond, &total_ops_mutex);
        }
        print_accounts();      // Mostra o balanço geral
        operation_counter = 0; // Reseta o contador
        pthread_mutex_unlock(&total_ops_mutex);

        if (operations_processed >= TOTAL_OPERATIONS)
            break;
    }

    // Enviar sinal de término para threads trabalhadoras
    for (int i = 0; i < NUM_WORKERS; i++)
    {
        Operation terminate_op;
        terminate_op.type = TERMINATE;
        enqueue(&op_queue, terminate_op);
    }

    for (int i = 0; i < NUM_WORKERS; i++)
    {
        pthread_join(workers[i], NULL);
    }

    for (int i = 0; i < NUM_CLIENTS; i++)
    {
        pthread_join(clients[i], NULL);
    }

    pthread_mutex_destroy(&total_ops_mutex);
    pthread_cond_destroy(&balance_cond);
    print_accounts(); // Mostra o balanço final
    return 0;
}
