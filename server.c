#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <unistd.h>
#include <string.h>
#include <time.h>

#define NUM_ACCOUNTS 5
#define NUM_WORKERS 4
#define MAX_OPERATIONS 10

typedef struct
{
    int id;
    double balance;
} Account;

typedef struct
{
    char operation[20];
    int account_id;
    int count;
    int target_account_id; // For transfer operation
    double amount;
} Operation;

Account accounts[NUM_ACCOUNTS];
Operation operations[MAX_OPERATIONS];
int op_count = 0;
pthread_mutex_t lock;
pthread_cond_t cond;

void initialize_accounts()
{
    srand(time(NULL));
    for (int i = 0; i < NUM_ACCOUNTS; i++)
    {
        accounts[i].id = i + 1;
        accounts[i].balance = rand() % 1000; // Saldo aleatório entre 0 e 999
    }
}

void print_accounts()
{
    printf("Contas e Saldo:\n");
    for (int i = 0; i < NUM_ACCOUNTS; i++)
    {
        printf("Conta ID: %d, Saldo: %.2f\n", accounts[i].id, accounts[i].balance);
    }
}

void *worker_thread(void *arg)
{
    while (1)
    {
        pthread_mutex_lock(&lock);
        while (op_count == 0)
        {
            pthread_cond_wait(&cond, &lock);
        }

        Operation op = operations[--op_count];
        pthread_mutex_unlock(&lock);

        // Process the operation
        if (strcmp(op.operation, "saqueDeposito") == 0)
        {
            accounts[op.account_id - 1].balance += op.amount;
            printf("Operação: %s - Conta: %d, Valor: %.2f\n", op.operation, op.account_id, op.amount);
        }
        else if (strcmp(op.operation, "transferencia") == 0)
        {
            accounts[op.account_id - 1].balance -= op.amount;
            accounts[op.target_account_id - 1].balance += op.amount;
            printf("Operação: %s - De Conta: %d Para Conta: %d, Valor: %.2f\n", op.operation, op.account_id, op.target_account_id, op.amount);
        }

        // Verifica se é a décima operação
        if ((MAX_OPERATIONS - op_count) % 10 == 0)
        {
            print_accounts();
        }

        sleep(1); // Intervalo de 1 segundo entre as operações
    }
    return NULL;
}

void *client_thread(void *arg)
{
    while (1)
    {
        sleep(1); // Intervalo de 500ms (pode ser ajustado para 1 segundo para simplificar)

        Operation op;
        int type = rand() % 3;                     // 0: saqueDeposito, 1: transferencia
        op.account_id = rand() % NUM_ACCOUNTS + 1; // ID aleatório da conta
        op.amount = (double)(rand() % 100) - 50;   // Valor entre -50 e 49

        if (type == 1)
        { // Transferência
            op.target_account_id = rand() % NUM_ACCOUNTS + 1;
            op.amount = abs(op.amount); // Valor deve ser positivo
            strcpy(op.operation, "transferencia");
        }
        else
        { // Saque ou depósito
            strcpy(op.operation, "saqueDeposito");
        }

        pthread_mutex_lock(&lock);
        op.count = op_count;
        operations[op_count++] = op;
        pthread_cond_signal(&cond); // Notifica uma thread trabalhadora
        pthread_mutex_unlock(&lock);

        // Imprime a operação adicionada

        int tamanho = sizeof(operations) / sizeof(operations[0]); // Calcula o tamanho do array
        printf("\n\n\n\n");
        printf("-------------------------\n");
        printf("Operação adicionada: %s - Conta: %d, Valor: %.2f\n", op.operation, op.account_id, op.amount);
        printf("----------LISTA---------------\n");

        // Usando um loop for para imprimir os elementos
        for (int i = 0; i < tamanho; i++)
        {
            printf("%d - %s\n", operations[i].count, operations[i].operation);
        }
        printf("-------------------------\n");

        if (op_count == 10)
        {
            return NULL;
        }
    }
    return NULL;
}

int main()
{
    initialize_accounts();

    pthread_t clients, workers[NUM_WORKERS];

    pthread_mutex_init(&lock, NULL);
    pthread_cond_init(&cond, NULL);

    // Criar threads trabalhadoras
    for (int i = 0; i < NUM_WORKERS; i++)
    {
        pthread_create(&workers[i], NULL, worker_thread, NULL);
    }

    // Criar thread cliente
    pthread_create(&clients, NULL, client_thread, NULL);

    // Esperar as threads terminarem (nunca vão, pois são loops infinitos)
    for (int i = 0; i < NUM_WORKERS; i++)
    {
        pthread_join(workers[i], NULL);
    }
    pthread_join(clients, NULL);

    pthread_mutex_destroy(&lock);
    pthread_cond_destroy(&cond);

    return 0;
}
