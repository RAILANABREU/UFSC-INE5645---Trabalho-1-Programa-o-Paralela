
# Projeto: Sistema Bancário Concorrente em C

## Principais Decisões e Estratégias de Implementação

Este projeto implementa um sistema bancário concorrente utilizando a linguagem C e programação com threads. As principais decisões e estratégias de implementação incluem:

- **Uso de Threads**: Foram utilizadas threads para simular múltiplos clientes e trabalhadores (workers) que operam simultaneamente. Os clientes geram operações bancárias aleatórias, enquanto os trabalhadores processam essas operações.

- **Sincronização com Mutexes e Variáveis de Condição**: 
  - Cada conta possui um mutex individual para controlar o acesso concorrente ao seu saldo, prevenindo condições de corrida.
  - Uma fila de operações (`OperationQueue`) foi implementada utilizando mutexes e variáveis de condição para sincronizar o acesso entre produtores (clientes) e consumidores (trabalhadores).

- **Fila de Operações com Capacidade Limitada**: A fila de operações tem um tamanho máximo definido (`MAX_QUEUE_SIZE`), o que simula uma capacidade limitada de processamento e força a sincronização entre threads produtoras e consumidoras.

- **Timeout no Bloqueio de Mutex**: Implementação de uma função `try_lock_with_timeout` que tenta adquirir o mutex de uma conta com um número limitado de tentativas e espera entre elas. Isso evita deadlocks e threads bloqueadas indefinidamente.

- **Tipos de Operações**:
  - **Saque/Depósito**: Atualiza o saldo de uma conta com um valor positivo (depósito) ou negativo (saque).
  - **Transferência**: Movimenta um valor de uma conta de origem para uma conta de destino, exigindo o bloqueio de ambos os mutexes das contas envolvidas.
  - **Balanço Geral**: Exibição do saldo de todas as contas a cada 9 operações processadas ou quando todas as operações foram concluídas.
  - **Terminação**: Sinaliza os trabalhadores para encerrarem suas atividades após o processamento de todas as operações.

- **Contadores e Sinalizações**: Utilização de variáveis compartilhadas e condições para controlar o número total de operações e sincronizar a exibição do balanço geral.

## Instruções para Compilar e Executar o Código

### Pré-requisitos

- **Compilador GCC**: Certifique-se de ter o GCC instalado em seu sistema.
- **Biblioteca Pthread**: Necessária para programação com threads em C.

### Compilação

Abra o terminal na pasta onde o código-fonte (`exe.c`) está localizado e execute o seguinte comando:

```bash
gcc -o exe exe.c -lpthread -lm
```

Explicação dos parâmetros:

- `-lpthread`: Inclui a biblioteca pthread necessária para manipulação de threads POSIX. Essa biblioteca permite ao programa criar e gerenciar threads, essencial para concorrência e paralelismo em C.
- `-o exe`: Especifica o nome do executável gerado.
- `-lm`: Linka a biblioteca matemática necessária para algumas funções utilizadas.

### Execução

Após a compilação bem-sucedida, execute o programa com:

```bash
./exe
```

## Exemplos de Saídas com Diferentes Parametrizações

### Parâmetros Padrão

Com os valores padrão definidos no código:

```c
#define NUM_ACCOUNTS 10
#define NUM_WORKERS 5
#define NUM_CLIENTS 10
#define MAX_QUEUE_SIZE 10
#define TOTAL_OPERATIONS 100
```

**Saída Exemplo**:

```
[Cliente 2] Depósito: +250.00 na Conta 5
[Cliente 7] Saque: 100.00 da Conta 3
[Cliente 5] Transferência: 150.00 da Conta 2 para Conta 8
...

===== Balanço Geral =====
Conta ID: 1, Saldo: 850.00
Conta ID: 2, Saldo: 920.00
Conta ID: 3, Saldo: 780.00
...
=========================
```

### Alterando o Número de Trabalhadores

Modificando o número de trabalhadores para 2:

```c
#define NUM_WORKERS 2
```

**Saída Exemplo**:

Com menos trabalhadores, as operações podem se acumular na fila, e os clientes podem esperar mais tempo para adicionar novas operações.

```
[Worker] Timeout ao tentar acessar a Conta 7
[Cliente 4] Depósito: +500.00 na Conta 2
...

===== Balanço Geral =====
Conta ID: 1, Saldo: 650.00
Conta ID: 2, Saldo: 1420.00
...
=========================
```

### Alterando o Número de Clientes

Modificando o número de clientes para 5:

```c
#define NUM_CLIENTS 5
```

**Saída Exemplo**:

Com menos clientes, a geração de novas operações é mais lenta, e os trabalhadores podem ficar ociosos ocasionalmente.

```
[Cliente 2] Transferência: 200.00 da Conta 3 para Conta 5
[Cliente 5] Saque: 150.00 da Conta 1
...

===== Balanço Geral =====
Conta ID: 1, Saldo: 700.00
Conta ID: 2, Saldo: 950.00
...
=========================
```

## Discussão sobre os Resultados Obtidos ao Variar as Configurações

### Impacto do Número de Trabalhadores (`NUM_WORKERS`)

- **Menor Número de Trabalhadores**: Com menos threads trabalhadoras, as operações se acumulam na fila, aumentando o tempo de espera dos clientes para adicionar novas operações. Pode ocorrer timeout ao tentar acessar contas devido à competição entre as poucas threads disponíveis.

- **Maior Número de Trabalhadores**: Aumentar o número de trabalhadores permite processar operações mais rapidamente. No entanto, um número muito alto pode causar overhead no sistema, devido ao gerenciamento de múltiplas threads e à possível contenção em mutexes de contas.

### Impacto do Número de Clientes (`NUM_CLIENTS`)

- **Menor Número de Clientes**: Com menos clientes, há uma redução na taxa de geração de novas operações. Os trabalhadores podem ficar ociosos se não houver operações suficientes na fila.

- **Maior Número de Clientes**: Um número maior de clientes aumenta a concorrência para adicionar operações à fila, que pode ficar cheia (`MAX_QUEUE_SIZE`), fazendo com que os clientes esperem por espaço disponível.

### Impacto do Total de Operações (`TOTAL_OPERATIONS`)

- **Menor Total de Operações**: A simulação termina mais rapidamente, útil para testes rápidos ou demonstrações.

- **Maior Total de Operações**: Permite uma simulação mais prolongada, testando a estabilidade e a performance do sistema em condições de carga prolongada.

### Ajuste do Timeout no Bloqueio (`LOCK_TIMEOUT`)

- **Menor Timeout**: Reduz o tempo que uma thread espera para bloquear uma conta, aumentando a chance de timeout e possíveis falhas ao processar uma operação.

- **Maior Timeout**: As threads esperam mais tempo para adquirir um mutex, o que pode reduzir a ocorrência de timeouts, mas aumenta o tempo total de processamento de uma operação, podendo afetar a responsividade do sistema.

- **Alertas de Timeout**: O sistema exibe o aviso ``` [Worker] Timeout ao tentar acessar a Conta {X} para transferência ``` quando uma thread não consegue adquirir o mutex da conta dentro do tempo limite especificado. Esse mecanismo evita deadlocks ao interromper a operação caso o bloqueio não seja possível, garantindo que o sistema continue funcionando sem que as threads fiquem presas indefinidamente esperando pelo recurso.

## Participantes:

- Bruno Butzke de Souza (20204857)
- Railan Gomes de Abreu (22201641)
- Vitor Sebajos Schweighofer (20205119)
