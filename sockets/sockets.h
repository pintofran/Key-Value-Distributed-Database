#ifndef SOCKETS_H_
#define SOCKETS_H_

#define SELECT 1
#define INSERT 2
#define DESCRIBE 3
#define DROP 4
#define CREATE 5
#define JOURNAL 6
#define RUN 7
#define ADD 8
#define ERROR 9
#define METRICS 14
#define GOSSIPING 88
#define EXIT_CONSOLE -1

#define SC 10
#define SHC 11
#define EC 12

#define MAX_MESSAGE_SIZE 300
#define MAX_TABLE_LENGTH 20

#define REGISTRO_ENCONTRADO 15
#define REGISTRO_NO_ENCONTRADO 16

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <unistd.h>
#include <stdint.h>
#include <semaphore.h>
#include <commons/collections/list.h>
#include <commons/string.h>

typedef struct t_PackagePosta {
	uint32_t header;
	uint32_t message_long;
	char* message;
	uint32_t total_size;
} t_PackagePosta;

typedef struct t_metadata {
	char nombre_tabla[MAX_TABLE_LENGTH];
	uint8_t consistencia;
} t_metadata;

typedef struct t_describe {
	uint16_t cant_tablas;
	t_metadata* tablas;
} t_describe;

typedef struct t_PackageMensaje {
	uint32_t mensaje_long;
	char* mensaje;
} t_PackageMensaje;

typedef struct t_PackageSelect {
	uint32_t header;
	uint32_t tabla_long;
	char* tabla;
	uint16_t key;
	uint32_t total_size;
} t_PackageSelect;

typedef struct t_PackageInsert {
	uint32_t header;
	uint32_t tabla_long;
	char* tabla;
	uint16_t key;
	uint32_t value_long;
	char* value;
	unsigned long long timestamp;
	uint32_t total_size;
} t_PackageInsert;

typedef struct t_PackageCreate {
	uint32_t header;
	uint32_t tabla_long;
	char* tabla;
	int consistency;
	int partitions;
	long int compaction_time;
	uint32_t total_size;
} t_PackageCreate;

typedef struct t_PackageDescribe {
	uint32_t header;
	uint32_t tabla_long;
	char* nombre_tabla;
	uint32_t total_size;
} t_PackageDescribe;

typedef struct t_PackageDrop {
	uint32_t header;
	uint32_t tabla_long;
	char* nombre_tabla;
	uint32_t total_size;
} t_PackageDrop;

typedef struct t_Respuesta_Select {
	char result;
	uint16_t value_long;
	char* value;
	unsigned long long timestamp;
} t_Respuesta_Select;

#define LFS 100
#define MEMORY 101
#define KERNEL 102
#define HANDSHAKE 103

typedef struct t_Handshake {
	uint32_t header;
	uint32_t id;
} t_Handshake;

typedef struct Seed {
	char ip[15];
	char puerto[6];
} Seed;

typedef struct t_PackageSeeds {
	uint16_t cant_seeds;
	Seed* seeds;
} t_PackageSeeds;

char* serializarOperandos(t_PackagePosta*);
void fill_package(t_PackagePosta*);
void dispose_package(char**);
int recieve_and_deserialize(t_PackagePosta*, int);
char* serializarHandShake(t_Handshake*);
char* consistency_to_str(int consistency);
unsigned long long getTS();

#endif
