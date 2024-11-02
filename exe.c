#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <unistd.h>
#include <string.h>
#include <time.h>

#define NUM_ACCOUNTS 5
#define NUM_WORKERS 4
#define NUM_CLIENTS 5
#define MAX_QUEUE_SIZE 10
#define TOTAL_OPERATIONS 50

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
    pthread_cond_t not_empty;
    pthread_cond_t not_full;
} OperationQueue;

void queue_init(OperationQueue *queue)
{
    queue->head = 0;
    queue->tail = 0;
    queue->size = 0;
    pthread_mutex_init(&queue->mutex, NULL);
    pthread_cond_init(&queue->not_empty, NULL);
    pthread_cond_init(&queue->not_full, NULL);
}

void enqueue(OperationQueue *queue, Operation op)
{
    pthread_mutex_lock(&queue->mutex);
    while (queue->size == MAX_QUEUE_SIZE)
    {
        pthread_cond_wait(&queue->not_full, &queue->mutex);
    }
    queue->operations[queue->tail] = op;
    queue->tail = (queue->tail + 1) % MAX_QUEUE_SIZE;
    queue->size++;
    pthread_cond_signal(&queue->not_empty);
    pthread_mutex_unlock(&queue->mutex);
}

Operation dequeue(OperationQueue *queue)
{
    pthread_mutex_lock(&queue->mutex);
    while (queue->size == 0)
    {
        pthread_cond_wait(&queue->not_empty, &queue->mutex);
    }
    Operation op = queue->operations[queue->head];
    queue->head = (queue->head + 1) % MAX_QUEUE_SIZE;
    queue->size--;
    pthread_cond_signal(&queue->not_full);
    pthread_mutex_unlock(&queue->mutex);
    return op;
}

Account accounts[NUM_ACCOUNTS];
OperationQueue op_queue;
int total_operations = 0;
pthread_mutex_t total_ops_mutex;

int operations_processed = 0;
pthread_mutex_t operations_processed_mutex;

int operations_since_last_balance = 0;
pthread_mutex_t balance_mutex;
pthread_cond_t balance_cond;
int workers_waiting = 0;
int processing_paused = 0;

void initialize_accounts()
{
    srand(time(NULL));
    for (int i = 0; i < NUM_ACCOUNTS; i++)
    {
        accounts[i].id = i + 1;
        accounts[i].balance = rand() % 1000;
        pthread_mutex_init(&accounts[i].mutex, NULL); // Inicializa o mutex para cada conta
    }
}

void print_accounts()
{
    printf("\n===== Balanço Geral =====\n");

    // Bloquear todos os mutexes das contas em ordem
    for (int i = 0; i < NUM_ACCOUNTS; i++)
    {
        pthread_mutex_lock(&accounts[i].mutex);
    }

    // Agora é seguro ler todos os saldos
    for (int i = 0; i < NUM_ACCOUNTS; i++)
    {
        printf("Conta ID: %d, Saldo: %.2f\n", accounts[i].id, accounts[i].balance);
    }

    // Desbloquear todos os mutexes das contas em ordem inversa
    for (int i = NUM_ACCOUNTS - 1; i >= 0; i--)
    {
        pthread_mutex_unlock(&accounts[i].mutex);
    }

    printf("=========================\n");
}

void *worker_thread(void *arg)
{
    while (1)
    {
        // Verifica se o processamento está pausado antes de prosseguir
        pthread_mutex_lock(&balance_mutex);
        while (processing_paused)
        {
            workers_waiting++;
            if (workers_waiting == NUM_WORKERS)
            {
                pthread_cond_signal(&balance_cond); // Notifica o servidor que todos os trabalhadores estão esperando
            }
            pthread_cond_wait(&balance_cond, &balance_mutex);
            workers_waiting--;
        }
        pthread_mutex_unlock(&balance_mutex);

        // Dequeue da operação
        Operation op = dequeue(&op_queue);

        if (op.type == TERMINATE)
        {
            break;
        }

        usleep(1000000); // Atraso de 1 segundo

        if (op.type == SAQUE_DEPOSITO)
        {
            int account_index = op.account_id - 1;

            // Valida o ID da conta
            if (account_index < 0 || account_index >= NUM_ACCOUNTS)
            {
                fprintf(stderr, "ID de conta inválido: %d\n", op.account_id);
                continue;
            }

            pthread_mutex_lock(&accounts[account_index].mutex);

            // Verifica saldo suficiente para saque
            if (op.amount < 0 && accounts[account_index].balance + op.amount < 0)
            {
                fprintf(stderr, "[Cliente %d] Saldo insuficiente para saque na conta %d\n",
                        op.client_id, op.account_id);
                pthread_mutex_unlock(&accounts[account_index].mutex);
                continue;
            }

            accounts[account_index].balance += op.amount;
            double new_balance = accounts[account_index].balance;
            pthread_mutex_unlock(&accounts[account_index].mutex);

            if (op.amount >= 0)
            {
                printf("[Cliente %d] Depósito: +%.2f na Conta %d, Novo saldo: %.2f\n",
                       op.client_id, op.amount, op.account_id, new_balance);
            }
            else
            {
                printf("[Cliente %d] Saque: %.2f da Conta %d, Novo saldo: %.2f\n",
                       op.client_id, -op.amount, op.account_id, new_balance);
            }
        }
        else if (op.type == TRANSFERENCIA)
        {
            int from_index = op.account_id - 1;
            int to_index = op.target_account_id - 1;

            // Valida os IDs das contas
            if (from_index < 0 || from_index >= NUM_ACCOUNTS ||
                to_index < 0 || to_index >= NUM_ACCOUNTS)
            {
                fprintf(stderr, "IDs de contas inválidos para transferência: %d -> %d\n",
                        op.account_id, op.target_account_id);
                continue;
            }

            // Tranca as contas em ordem para evitar deadlocks
            if (from_index < to_index)
            {
                pthread_mutex_lock(&accounts[from_index].mutex);
                pthread_mutex_lock(&accounts[to_index].mutex);
            }
            else if (from_index > to_index)
            {
                pthread_mutex_lock(&accounts[to_index].mutex);
                pthread_mutex_lock(&accounts[from_index].mutex);
            }
            else
            {
                // Mesma conta
                pthread_mutex_lock(&accounts[from_index].mutex);
            }

            // Verifica saldo suficiente
            if (accounts[from_index].balance - op.amount < 0)
            {
                fprintf(stderr, "[Cliente %d] Saldo insuficiente para transferência da conta %d para a conta %d\n",
                        op.client_id, op.account_id, op.target_account_id);
                if (from_index != to_index)
                {
                    pthread_mutex_unlock(&accounts[from_index].mutex);
                    pthread_mutex_unlock(&accounts[to_index].mutex);
                }
                else
                {
                    pthread_mutex_unlock(&accounts[from_index].mutex);
                }
                continue;
            }

            accounts[from_index].balance -= op.amount;
            accounts[to_index].balance += op.amount;

            double from_balance = accounts[from_index].balance;
            double to_balance = accounts[to_index].balance;

            if (from_index < to_index)
            {
                pthread_mutex_unlock(&accounts[to_index].mutex);
                pthread_mutex_unlock(&accounts[from_index].mutex);
            }
            else if (from_index > to_index)
            {
                pthread_mutex_unlock(&accounts[from_index].mutex);
                pthread_mutex_unlock(&accounts[to_index].mutex);
            }
            else
            {
                pthread_mutex_unlock(&accounts[from_index].mutex);
            }

            printf("[Cliente %d] Transferência: %.2f da Conta %d (Novo saldo: %.2f) para Conta %d (Novo saldo: %.2f)\n",
                   op.client_id, op.amount, op.account_id, from_balance, op.target_account_id, to_balance);
        }

        // Incrementa operations_processed
        pthread_mutex_lock(&operations_processed_mutex);
        operations_processed++;
        pthread_mutex_unlock(&operations_processed_mutex);

        // Incrementa operations_since_last_balance
        pthread_mutex_lock(&balance_mutex);
        operations_since_last_balance++;
        pthread_mutex_unlock(&balance_mutex);
    }
    return NULL;
}

void *client_thread(void *arg)
{
    int client_id = *((int *)arg);

    while (1)
    {
        usleep(100000); // Atraso de 0,1 segundo

        pthread_mutex_lock(&total_ops_mutex);
        if (total_operations >= TOTAL_OPERATIONS)
        {
            pthread_mutex_unlock(&total_ops_mutex);
            break;
        }
        total_operations++;
        pthread_mutex_unlock(&total_ops_mutex);

        Operation op;
        op.client_id = client_id;
        int op_type = rand() % 2;

        if (op_type == 0)
        {
            op.type = SAQUE_DEPOSITO;
            op.account_id = rand() % NUM_ACCOUNTS + 1;
            op.target_account_id = 0; // Não usado
            if (rand() % 2 == 0)
            {
                op.amount = (double)(rand() % 1000 + 1); // Depósito
            }
            else
            {
                op.amount = -((double)(rand() % 500 + 1)); // Saque
            }
        }
        else
        {
            op.type = TRANSFERENCIA;
            op.account_id = rand() % NUM_ACCOUNTS + 1;
            op.target_account_id = rand() % NUM_ACCOUNTS + 1;
            op.amount = (double)(rand() % 500 + 1);
        }

        enqueue(&op_queue, op);
    }
    return NULL;
}

int main()
{
    initialize_accounts();
    queue_init(&op_queue);
    pthread_mutex_init(&total_ops_mutex, NULL);
    pthread_mutex_init(&operations_processed_mutex, NULL);
    pthread_mutex_init(&balance_mutex, NULL);
    pthread_cond_init(&balance_cond, NULL);

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

    int last_ops_processed = 0;

    while (1)
    {
        usleep(100000); // Dorme por 0,1 segundo

        pthread_mutex_lock(&operations_processed_mutex);
        int ops_processed = operations_processed;
        pthread_mutex_unlock(&operations_processed_mutex);

        if (ops_processed >= TOTAL_OPERATIONS)
        {
            break;
        }

        pthread_mutex_lock(&balance_mutex);
        if (operations_since_last_balance >= 9)
        {
            processing_paused = 1;
            workers_waiting = 0;

            // Notifica os trabalhadores para verificar o estado de pausa
            pthread_mutex_unlock(&balance_mutex);

            // Aguarda até que todos os trabalhadores estejam esperando
            pthread_mutex_lock(&balance_mutex);
            while (workers_waiting < NUM_WORKERS)
            {
                pthread_cond_wait(&balance_cond, &balance_mutex);
            }
            pthread_mutex_unlock(&balance_mutex);

            // Realiza o balanço geral
            printf("[Servidor] Realizando Balanço Geral após %d operações.\n", ops_processed);
            print_accounts();

            // Reseta o contador e retoma o processamento
            pthread_mutex_lock(&balance_mutex);
            operations_since_last_balance = 0;
            processing_paused = 0;
            pthread_cond_broadcast(&balance_cond);
            pthread_mutex_unlock(&balance_mutex);
        }
        else
        {
            pthread_mutex_unlock(&balance_mutex);
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

    // Envia operações TERMINATE para as threads trabalhadoras
    for (int i = 0; i < NUM_WORKERS; i++)
    {
        Operation terminate_op;
        terminate_op.type = TERMINATE;
        enqueue(&op_queue, terminate_op);
    }

    for (int i = 0; i < NUM_WORKERS; i++)
    {
        if (pthread_join(workers[i], NULL) != 0)
        {
            perror("Erro ao aguardar thread trabalhadora");
            exit(1);
        }
    }

    // Imprime o balanço final
    printf("Balanço final:\n");
    for (int i = 0; i < NUM_ACCOUNTS; i++)
    {
        pthread_mutex_lock(&accounts[i].mutex);
        printf("Conta ID: %d, Saldo: %.2f\n", accounts[i].id, accounts[i].balance);
        pthread_mutex_unlock(&accounts[i].mutex);
    }

    // Limpa os recursos
    for (int i = 0; i < NUM_ACCOUNTS; i++)
    {
        pthread_mutex_destroy(&accounts[i].mutex);
    }

    pthread_mutex_destroy(&op_queue.mutex);
    pthread_cond_destroy(&op_queue.not_empty);
    pthread_cond_destroy(&op_queue.not_full);

    pthread_mutex_destroy(&total_ops_mutex);
    pthread_mutex_destroy(&operations_processed_mutex);
    pthread_mutex_destroy(&balance_mutex);
    pthread_cond_destroy(&balance_cond);

    return 0;
}