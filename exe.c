#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <unistd.h>
#include <string.h>
#include <time.h>
#include <sched.h>

// Definição dos parâmetros, agora parametrizados
#define NUM_ACCOUNTS 5
#define NUM_WORKERS 4
#define NUM_CLIENTS 2
#define MAX_QUEUE_SIZE 100
#define TOTAL_OPERATIONS 10

// Estrutura da conta bancária
typedef struct
{
    int id;
    double balance;
} Account;

// Tipos de operações
typedef enum
{
    SAQUE_DEPOSITO,
    TRANSFERENCIA,
    BALANCO_GERAL,
    TERMINATE // Operação especial para sinalizar término
} OperationType;

// Estrutura da operação
typedef struct
{
    OperationType type;
    int client_id; // ID do cliente que gerou a operação
    int account_id;
    int target_account_id; // Para transferência
    double amount;
} Operation;

// Implementação de um mutex personalizado (spinlock)
typedef struct
{
    volatile int locked;
} MyMutex;

void my_mutex_init(MyMutex *mutex)
{
    mutex->locked = 0;
}

void my_mutex_lock(MyMutex *mutex)
{
    while (__sync_lock_test_and_set(&(mutex->locked), 1))
    {
        sched_yield(); // Cede a CPU para evitar busy-wait agressivo
    }
}

void my_mutex_unlock(MyMutex *mutex)
{
    __sync_lock_release(&(mutex->locked));
}

// Implementação de um semáforo personalizado
typedef struct
{
    int count;
    MyMutex mutex;
} MySemaphore;

void my_semaphore_init(MySemaphore *sem, int value)
{
    sem->count = value;
    my_mutex_init(&sem->mutex);
}

void my_semaphore_wait(MySemaphore *sem)
{
    while (1)
    {
        my_mutex_lock(&sem->mutex);
        if (sem->count > 0)
        {
            sem->count--;
            my_mutex_unlock(&sem->mutex);
            break;
        }
        my_mutex_unlock(&sem->mutex);
        sched_yield(); // Cede a CPU
    }
}

void my_semaphore_signal(MySemaphore *sem)
{
    my_mutex_lock(&sem->mutex);
    sem->count++;
    my_mutex_unlock(&sem->mutex);
}

// Implementação da fila de operações como um buffer circular
typedef struct
{
    Operation operations[MAX_QUEUE_SIZE];
    int head;
    int tail;
    int size;
    MyMutex mutex;
    MySemaphore items;  // Contador de itens na fila
    MySemaphore spaces; // Contador de espaços livres na fila
} OperationQueue;

void queue_init(OperationQueue *queue)
{
    queue->head = 0;
    queue->tail = 0;
    queue->size = 0;
    my_mutex_init(&queue->mutex);
    my_semaphore_init(&queue->items, 0);               // Inicialmente sem itens
    my_semaphore_init(&queue->spaces, MAX_QUEUE_SIZE); // Espaços disponíveis
}

void enqueue(OperationQueue *queue, Operation op)
{
    my_semaphore_wait(&queue->spaces); // Espera por espaço
    my_mutex_lock(&queue->mutex);
    queue->operations[queue->tail] = op;
    queue->tail = (queue->tail + 1) % MAX_QUEUE_SIZE;
    queue->size++;
    my_mutex_unlock(&queue->mutex);
    my_semaphore_signal(&queue->items); // Sinaliza que há um item disponível
}

Operation dequeue(OperationQueue *queue)
{
    my_semaphore_wait(&queue->items); // Espera por item
    my_mutex_lock(&queue->mutex);
    Operation op = queue->operations[queue->head];
    queue->head = (queue->head + 1) % MAX_QUEUE_SIZE;
    queue->size--;
    my_mutex_unlock(&queue->mutex);
    my_semaphore_signal(&queue->spaces); // Sinaliza que há espaço disponível
    return op;
}

// Variáveis globais
Account accounts[NUM_ACCOUNTS];
OperationQueue op_queue;
int total_operations = 0; // Contador total de operações geradas
MyMutex total_ops_mutex;

// Inicializa as contas com saldos aleatórios
void initialize_accounts()
{
    srand(time(NULL));
    for (int i = 0; i < NUM_ACCOUNTS; i++)
    {
        accounts[i].id = i + 1;
        accounts[i].balance = rand() % 1000; // Saldo aleatório entre 0 e 999
    }
}

// Imprime o saldo de todas as contas
void print_accounts()
{
    printf("\n===== Balanço Geral =====\n");
    for (int i = 0; i < NUM_ACCOUNTS; i++)
    {
        printf("Conta ID: %d, Saldo: %.2f\n", accounts[i].id, accounts[i].balance);
    }
    printf("=========================\n");
}

// Thread trabalhadora que processa as operações
void *worker_thread(void *arg)
{
    while (1)
    {
        Operation op = dequeue(&op_queue);

        // Verifica se é uma operação de término
        if (op.type == TERMINATE)
        {
            break;
        }

        // Simula o tempo de processamento
        sleep(1);

        // Processa a operação
        if (op.type == SAQUE_DEPOSITO)
        {
            accounts[op.account_id - 1].balance += op.amount;
            if (op.amount >= 0)
            {
                printf("[Cliente %d] Depósito: +%.2f na Conta %d\n", op.client_id, op.amount, op.account_id);
            }
            else
            {
                printf("[Cliente %d] Saque: %.2f da Conta %d\n", op.client_id, op.amount, op.account_id);
            }
        }
        else if (op.type == TRANSFERENCIA)
        {
            accounts[op.account_id - 1].balance -= op.amount;
            accounts[op.target_account_id - 1].balance += op.amount;
            printf("[Cliente %d] Transferência: %.2f da Conta %d para Conta %d\n",
                   op.client_id, op.amount, op.account_id, op.target_account_id);
        }
        else if (op.type == BALANCO_GERAL)
        {
            printf("[Worker] Iniciando Balanço Geral solicitado pelo Cliente %d\n", op.client_id);
            print_accounts();
        }
    }
    return NULL;
}

// Thread cliente que gera operações aleatórias
void *client_thread(void *arg)
{
    int client_id = *((int *)arg);
    int local_operations = 0;

    while (1)
    {
        // Gera operações em intervalos aleatórios
        usleep(500000); // 500 ms

        // Cria uma nova operação
        Operation op;
        op.client_id = client_id; // Define o ID do cliente na operação
        int op_type = rand() % 2; // 0: saqueDeposito, 1: transferencia

        if (op_type == 0)
        {
            // Decide aleatoriamente entre saque e depósito
            op.type = SAQUE_DEPOSITO;
            op.account_id = rand() % NUM_ACCOUNTS + 1;
            if (rand() % 2 == 0)
            {
                // Depósito
                op.amount = (double)(rand() % 1000);
            }
            else
            {
                // Saque
                op.amount = -(double)(rand() % 500);
            }
            printf("[Cliente %d] Solicitou %s de %.2f na Conta %d\n", client_id,
                   (op.amount >= 0) ? "depósito" : "saque", op.amount, op.account_id);
        }
        else if (op_type == 1)
        {
            // Transferência
            op.type = TRANSFERENCIA;
            op.account_id = rand() % NUM_ACCOUNTS + 1;
            op.target_account_id = rand() % NUM_ACCOUNTS + 1;
            op.amount = (double)(rand() % 500);
            printf("[Cliente %d] Solicitou transferência de %.2f da Conta %d para Conta %d\n",
                   client_id, op.amount, op.account_id, op.target_account_id);
        }

        // Enfileira a operação
        enqueue(&op_queue, op);

        local_operations++;

        // Após cada 10 operações, enfileira uma operação de balanço geral
        if (local_operations % 10 == 0)
        {
            Operation balanco_op;
            balanco_op.type = BALANCO_GERAL;
            balanco_op.client_id = client_id; // Cliente que solicitou o balanço
            enqueue(&op_queue, balanco_op);
            printf("[Cliente %d] Solicitou Balanço Geral\n", client_id);
        }

        // Incrementa o contador total de operações
        my_mutex_lock(&total_ops_mutex);
        total_operations++;
        if (total_operations >= TOTAL_OPERATIONS)
        {
            my_mutex_unlock(&total_ops_mutex);
            break;
        }
        my_mutex_unlock(&total_ops_mutex);
    }
    return NULL;
}

int main()
{
    initialize_accounts();
    queue_init(&op_queue);
    my_mutex_init(&total_ops_mutex);

    pthread_t workers[NUM_WORKERS];
    pthread_t clients[NUM_CLIENTS];
    int client_ids[NUM_CLIENTS];

    // Cria threads trabalhadoras
    for (int i = 0; i < NUM_WORKERS; i++)
    {
        if (pthread_create(&workers[i], NULL, worker_thread, NULL) != 0)
        {
            perror("Erro ao criar thread trabalhadora");
            exit(1);
        }
    }

    // Cria threads clientes
    for (int i = 0; i < NUM_CLIENTS; i++)
    {
        client_ids[i] = i + 1;
        if (pthread_create(&clients[i], NULL, client_thread, &client_ids[i]) != 0)
        {
            perror("Erro ao criar thread cliente");
            exit(1);
        }
    }

    // Aguarda as threads clientes terminarem
    for (int i = 0; i < NUM_CLIENTS; i++)
    {
        if (pthread_join(clients[i], NULL) != 0)
        {
            perror("Erro ao aguardar thread cliente");
            exit(1);
        }
    }

    // Envia sinal de término para as threads trabalhadoras
    for (int i = 0; i < NUM_WORKERS; i++)
    {
        Operation terminate_op;
        terminate_op.type = TERMINATE;
        enqueue(&op_queue, terminate_op);
    }

    // Aguarda as threads trabalhadoras terminarem
    for (int i = 0; i < NUM_WORKERS; i++)
    {
        if (pthread_join(workers[i], NULL) != 0)
        {
            perror("Erro ao aguardar thread trabalhadora");
            exit(1);
        }
    }

    // Imprime o balanço final
    print_accounts();

    return 0;
}