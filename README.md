# Universidade Federal de Santa Catarina – UFSC
## Departamento em Informática e Estatística  
### INE 5645 – Programação Paralela e Distribuída  
### Semestre 2024/2  

---

## Definição do Trabalho 1: Programação Paralela

Este trabalho visa explorar o uso de padrões para programação multithread. Cada grupo irá explorar (pelo menos) os modelos de programação produtor/consumidor e pool de threads na solução do problema.

---

## Descrição do problema: Implementação de um servidor de contas com atendimento baseado em pool de threads

Este trabalho consiste em desenvolver um servidor multithreaded. A ideia geral é que clientes possam gerar requisições sobre contas bancárias, que serão atendidas por um servidor. Para aumentar a vazão do serviço e tirar proveito de arquiteturas com múltiplos núcleos de processamento, o servidor delega o processamento das requisições dos clientes a threads trabalhadoras.

### Componentes do serviço bancário

#### Contas bancárias
O serviço deve manter informação sobre contas de usuários. Cada conta de usuário tem:
- Um **identificador** (número inteiro positivo)
- Um **saldo atual** (número real, positivo ou negativo).

O conjunto de contas de todos os usuários pode ser armazenado em uma estrutura de dados de sua preferência (ex.: tabela hash, lista, etc.). O importante é que as operações do serviço possam acessar e atualizar essas contas.

#### Operações
Cada grupo deve implementar 3 operações do serviço:

1. **Depósito em conta corrente:**  
   - Recebe um **identificador de conta** (inteiro positivo) e o **valor do depósito** (número real, positivo ou negativo). 
   - Se o valor do depósito for negativo, a operação realiza um saque.

2. **Transferência entre contas:**  
   - Dadas duas contas (origem e destino) e um valor de transferência, deve-se debitar o valor da conta de origem e creditar na conta de destino.

3. **Balanço geral:**  
   - Gera uma lista com o saldo de todas as contas no momento em que a operação foi solicitada.

Cada operação deve conter uma função `sleep` com um valor definido para simular o tempo de processamento.

#### Servidor
- O servidor é responsável por receber as requisições dos clientes, atribuí-las às threads trabalhadoras, e periodicamente gerar um balanço geral.
- O servidor usa um **pool de threads** para lidar com múltiplas requisições.
- A cada 10 operações de clientes, o servidor adiciona uma operação de balanço geral na fila de requisições.

#### Threads trabalhadoras
- O sistema disponibiliza um **pool de threads trabalhadoras**.
- Cada thread tem dois estados: livre ou em execução.
- As threads executam operações solicitadas pelo servidor e retornam ao estado livre após a conclusão.

#### Clientes
- Threads clientes geram operações (depósitos e transferências) aleatoriamente entre contas, com valores e contas escolhidos aleatoriamente.

---

### Execução
O programa deve permitir a variação dos seguintes parâmetros:
- Tamanho do pool de threads
- Número de clientes
- Taxa de geração de requisições
- Tempo de serviço (definido pelo `sleep`)

O programa pode rodar por tempo indefinido ou até que se atinja um critério de parada definido pelo grupo (por exemplo, um número de requisições atendidas ou tempo de execução).

---

### Regras adicionais
- O **controle de concorrência** deve ser implementado pelo grupo. Não é permitido usar estruturas de dados com exclusão mútua já implementada.
- O **modelo produtor/consumidor** e o **pool de threads** devem ser implementados do zero pelo grupo.

---

### Entrega
A entrega do trabalho consiste em:
1. Implementação de um programa que simule o serviço descrito.
2. Um relatório contendo:
   - As principais decisões e estratégias de implementação.
   - Instruções para compilar e executar o código.
   - Exemplos de saídas com diferentes parametrizações.
   - Discussão sobre os resultados observados ao variar os parâmetros.

O trabalho pode ser realizado em grupos de até 3 participantes e deve ser enviado pelo Moodle.

---

### Observações
Os nomes dos participantes devem estar no relatório enviado pelo Moodle. Participantes que não forem referenciados não serão considerados membros do grupo.
