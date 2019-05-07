/*
 ============================================================================
 Name        : LFS.c
 Author      : 
 Version     :
 Copyright   : Your copyright notice
 Description : Hello World in C, Ansi-style
 ============================================================================
 */

#include "LFS.h"
#include "sockets.h"
#include <dirent.h>
#include <commons/collections/dictionary.h>
#include <commons/collections/list.h>

int main() {

	struct addrinfo hints;
	struct addrinfo *serverInfo;

	t_log* logger = iniciar_logger();
	t_config* config = leer_config();

	char* puerto = config_get_string_value(config, "PUERTO_ESCUCHA");
	char* ruta = config_get_string_value(config, "PUNTO_MONTAJE");
	log_debug(logger, puerto);

	memset(&hints, 0, sizeof(hints));
	hints.ai_family = AF_UNSPEC;
	hints.ai_flags = AI_PASSIVE; // Asigna el address del localhost: 127.0.0.1
	hints.ai_socktype = SOCK_STREAM;	// Indica que usaremos el protocolo TCP

	getaddrinfo(NULL, puerto, &hints, &serverInfo); // Notar que le pasamos NULL como IP, ya que le indicamos que use localhost en AI_PASSIVE

	int listenningSocket;
	listenningSocket = socket(serverInfo->ai_family, serverInfo->ai_socktype,
			serverInfo->ai_protocol);

	bind(listenningSocket, serverInfo->ai_addr, serverInfo->ai_addrlen);
	freeaddrinfo(serverInfo); // Ya no lao vamos a necesitar

	listen(listenningSocket, BACKLOG); // IMPORTANTE: listen() es una syscall BLOQUEANTE.

	printf("Esperando memoria... \n");

	struct sockaddr_in addr; // Esta estructura contendra los datos de la conexion del cliente. IP, puerto, etc.
	socklen_t addrlen = sizeof(addr);

	int socketCliente = accept(listenningSocket, (struct sockaddr *) &addr,
			&addrlen);

	if (!recibir_handshake(MEMORY, socketCliente)) {
		printf("Handshake invalido \n");
		return 0;
	}

	t_PackagePosta package;
	int status = 1;		// Estructura que maneja el status de los recieve.

	printf("Memoria conectada. Esperando Envio de mensajes.\n");

	int headerRecibido;

	while (status) {

		headerRecibido = recieve_header(socketCliente);

		status = headerRecibido;

		log_debug(logger, string_itoa(headerRecibido));

		if (headerRecibido == SELECT && status) {

			t_PackageSelect package;
			status = recieve_and_deserialize_select(&package, socketCliente);

			ejecutar_comando(headerRecibido, &package, ruta);

		} else if (headerRecibido == INSERT && status) {

			t_PackageInsert package;
			status = recieve_and_deserialize_insert(&package, socketCliente);

			ejecutar_comando(headerRecibido, &package, ruta);

		}

	}

	printf("Cliente Desconectado.\n");

	close(socketCliente);
	close(listenningSocket);

	log_destroy(logger);
	config_destroy(config);

	return 0;
}

t_config* leer_config() {
	return config_create(CONFIG_PATH);
}

t_log* iniciar_logger(void) {

	return log_create(LOG_FILE_PATH, "LFS", 1, LOG_LEVEL_DEBUG);

}

void ejecutar_comando(int header, void* package, char* ruta) {
	switch (header) {
	case SELECT:
		lfs_select((t_PackageSelect*) package, ruta);
		break;
	case INSERT:
		lfs_insert((t_PackageInsert*) package);
		break;
	}
}

//Falta agregar funcionalidad de que debe buscar a la tabla correspondiente el valor y demas...
void lfs_select(t_PackageSelect* package, char* ruta) {

	t_log* logger_select = iniciar_logger();
	log_debug(logger_select, "Got a SELECT");

	char* mi_ruta = string_new();
	string_append(&mi_ruta, ruta);
	free(ruta);

	log_debug(logger_select, mi_ruta);


	if (!existe_tabla(package->tabla, mi_ruta)) {
		log_debug(logger_select, "No existe la tabla");
		return;
	}
	log_debug(logger_select, "Existe tabla, BRO!");
	//2) Obtener Metadata
	t_dictionary* metadata = dictionary_create();

	obtener_metadata(metadata, mi_ruta);
	log_debug(logger_select, mi_ruta);
	log_debug(logger_select, dictionary_get(metadata, "consistencia"));
	log_debug(logger_select,
			string_itoa(dictionary_get(metadata, "particiones")));
	log_debug(logger_select,
			string_itoa(dictionary_get(metadata, "tiempoDeCompactacion")));
	//3) Calcular que particion contiene a KEY
	int particionObjetivo = calcular_particion(package->key,
			dictionary_get(metadata, "particiones"));
	log_debug(logger_select, string_itoa(particionObjetivo));

	//4) Escanear particion objetivo, archivos temporales y memoria temporal
	//t_list* valuesEncontrados = list_create();
	//encontrar_keys(key, particionObjetivo, valuesEncontrados);
	//5) Devolver o mostrar el valor mayor
	//log_debug(logger_select, maximoTimestamp(valuesEncontrados));
	//struct Reg *reg = list_get(valuesEncontrados, 0);
	//log_debug(logger_select, reg->value);

	log_destroy(logger_select);

}

void lfs_insert(t_PackageInsert* package) {
	t_log* logger_insert = iniciar_logger();
	log_debug(logger_insert, "Got an INSERT");

	log_debug(logger_insert, package->tabla);
	log_debug(logger_insert, string_itoa(package->key));
	log_debug(logger_insert, package->value);
	log_debug(logger_insert, string_itoa(package->timestamp));

	log_destroy(logger_insert);

}

int existe_tabla(char* nombre_tabla, char* ruta) {

	char* tables = "/tables/";
	string_append(&ruta, tables);
	string_append(&ruta, nombre_tabla);
	DIR *dirp = opendir(ruta);
	free(ruta);
	if (dirp == NULL) {
		free(dirp);
		return 0;
	}
	free(dirp);
	return 1;
}

void obtener_metadata(t_dictionary* metadata, char* ruta) {
	t_log* logger_tabla = iniciar_logger();
	log_debug(logger_tabla, ruta);
	char* mi_metadata = "/Metadata";
	string_append(&ruta, mi_metadata);
	log_debug(logger_tabla, ruta);
	t_config* config_metadata = config_create(ruta);
	char* consistencia = config_get_string_value(config_metadata, "CONSISTENCY");
	dictionary_put(metadata, "consistencia", consistencia);
	int particiones = config_get_int_value(config_metadata, "PARTITIONS");
	dictionary_put(metadata, "particiones", particiones);
	long tiempoDeCompactacion = config_get_long_value(config_metadata, "COMPACTION_TIME");
	config_destroy(config_metadata);
	dictionary_put(metadata, "tiempoDeCompactacion", tiempoDeCompactacion);
}

int calcular_particion(int key, int cantidad_particiones) {

	return (key % cantidad_particiones) + 1;
}

void encontrar_keys(int keyBuscada, int particion_objetivo, t_list* lista_values) {

	 char* ruta = string_new();
	 string_append(&ruta, "t1/1.bin");
	 FILE* lector = fopen(ruta, "rb");
	 struct Reg reg;
	 while (!feof(lector)) {
	 fread(&reg, sizeof(reg), 1, lector);
	 printf(reg.value);
	 if (reg.key == keyBuscada) {
	 list_add(lista_values, &reg);
	 }
	 }

}

//Preguntar que onda esta opcion, si pierdo la referencia al hacer malloc y devolverlo. Comparar con la otra funcion de abajo

/*char* obtener_nombre_tabla(char** parametros_separados){
 char* nombre_tabla = malloc(strlen(parametros_separados[0]));
 strcpy(nombre_tabla,parametros_separados[0]);
 return nombre_tabla;
 }
 */

/*void obtener_nombre_tabla(char* nombre_tabla, char** parametros_separados){
 nombre_tabla = malloc(strlen(parametros_separados[0]));
 strcpy(nombre_tabla,parametros_separados[0]);
 }*/
