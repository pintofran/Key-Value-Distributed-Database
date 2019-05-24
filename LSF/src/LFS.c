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
#include <unistd.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <commons/collections/dictionary.h>
#include <commons/collections/list.h>

t_log* logger;
t_list* mem_table;

int main() {

	struct addrinfo hints;
	struct addrinfo *serverInfo;

	mem_table = list_create();
	logger = iniciar_logger();
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

	int max_value_size = config_get_int_value(config,"TAMAÑO_VALUE");
	log_debug(logger,string_itoa(max_value_size));
	send(socketCliente, &max_value_size,sizeof(u_int16_t), 0);

	t_list* metadatas = lfs_describe(ruta);
	list_iterate(metadatas, (void*) loguear_metadata);

	t_describe* describe = malloc(sizeof(t_describe));
	int cantidad_de_tablas = metadatas->elements_count;
	describe->cant_tablas = cantidad_de_tablas;
	describe->tablas = malloc(cantidad_de_tablas * sizeof(t_metadata));

	for (int i = 0;  i < cantidad_de_tablas; i++) {
		Metadata* a_metadata = (Metadata*) list_get(metadatas, i);
		t_metadata* meta = malloc(sizeof(t_metadata));
		meta->consistencia = a_metadata->consistency;
		strcpy(meta->nombre_tabla, a_metadata->nombre_tabla);

		describe->tablas[i] = *meta;
	}


	char* serializedPackage;
	serializedPackage = serializarDescribe(describe);
	log_debug(logger, string_itoa(cantidad_de_tablas));
	send(socketCliente, serializedPackage, cantidad_de_tablas*sizeof(t_metadata) + sizeof(describe->cant_tablas), 0);
	dispose_package(&serializedPackage);


	t_PackagePosta package;
	int status = 1;		// Estructura que maneja el status de los recieve.

	printf("Memoria conectada. Esperando Envio de mensajes.\n");

	int headerRecibido;

	while (status) {

		log_debug(logger, "Header");
		headerRecibido = recieve_header(socketCliente);
		printf("End Header. \n");
		log_debug(logger, "Status");

		status = headerRecibido;

		if (status) {
			if (headerRecibido == SELECT) {

				log_debug(logger, "Got a SELECT");

				t_PackageSelect package;
				status = recieve_and_deserialize_select(&package, socketCliente);

				ejecutar_comando(headerRecibido, &package, ruta);

			} else if (headerRecibido == INSERT) {

				log_debug(logger, "Got an INSERT");

				t_PackageInsert package;
				status = recieve_and_deserialize_insert(&package, socketCliente);

				int fue_exitoso = ejecutar_comando(headerRecibido, &package, ruta);
				if(fue_exitoso){
					log_info(logger, "Se inserto exitosamente");
				} else {
					log_info(logger, "No se pudo insertar");
				}

				Tabla* tabluqui = list_get(mem_table, 0);
				Registro* registruli = list_get(tabluqui->registros, 0);
				log_debug(logger, registruli->value);

			} /*else if (headerRecibido == DESCRIBE) {

				log_debug(logger, "Got an DESCRIBE");

				t_describe package;


			}*/

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
		lfs_insert((t_PackageInsert*) package, ruta);
		break;
	}
}

//Falta agregar funcionalidad de que debe buscar a la tabla correspondiente el valor y demas...
void lfs_select(t_PackageSelect* package, char* ruta) {

	char* mi_ruta = string_new();
	string_append(&mi_ruta,ruta);

	log_debug(logger, mi_ruta);

	char* tables = "/Tables/";
	string_append(&mi_ruta, tables);
	string_append(&mi_ruta, package->tabla);

	log_debug(logger, mi_ruta);

	if (!existe_tabla(mi_ruta)) {
		log_debug(logger, "No existe la tabla");
		return;
	}
	log_debug(logger, "Existe tabla, BRO!");

	//2) Obtener Metadata
	Metadata* metadata = obtener_metadata(mi_ruta);

	loguear_metadata(metadata);

	//3) Calcular que particion contiene a KEY
	int particionObjetivo = calcular_particion(package->key, metadata->partitions);
	log_debug(logger, string_itoa(particionObjetivo));

	//4) Escanear particion objetivo, archivos temporales y memoria temporal
	t_list* keys = encontrar_keys(package->key, particionObjetivo, mi_ruta, ruta);
	//list_iterate(keys, (void*)loguear_registro);
	list_sort(keys,(void*)timestamp_mayor_entre);
	//list_iterate(keys,(void*)loguear_registro);

	Registro* registro_mayor = list_get(keys,0);
	log_debug(logger, "La value correspondiente al mayor timeStamp es: ");
	log_debug(logger,registro_mayor->value);

	/*
	t_Respuesta_Select respuesta;
	respuesta.value = malloc(strlen(registro_mayor->value));
	strcpy(respuesta.value,registro_mayor->value);
	*/



	//5) Devolver o mostrar el valor mayor
	//log_debug(logger, maximoTimestamp(valuesEncontrados));
	//struct Reg *reg = list_get(valuesEncontrados, 0);
	//log_debug(logger, reg->value);

	free(metadata);
	free(mi_ruta);



}

int lfs_insert(t_PackageInsert* package, char* ruta) {
	log_debug(logger, package->value);
	char* mi_ruta = string_new();
	string_append(&mi_ruta,ruta);

	log_debug(logger, mi_ruta);

	char* tables = "/Tables/";
	string_append(&mi_ruta, tables);
	string_append(&mi_ruta, package->tabla);

	log_debug(logger, mi_ruta);

	if (!existe_tabla(mi_ruta)) {
		log_debug(logger, "No existe la tabla");
		return 0;
	}
	log_debug(logger, "Existe tabla, BRO!");

	Metadata* metadata = obtener_metadata(mi_ruta);

	loguear_metadata(metadata);

	if(!existe_tabla_en_mem_table(package->tabla)) {
		if(agregar_tabla_a_mem_table(package->tabla)){
			return 0;
		}
	}

	log_debug(logger, "Voy a crear el registro");
	Registro* registro_a_insertar  = malloc(sizeof(Registro));
	log_debug(logger, "Hice malloc");
	registro_a_insertar->key = package->key;
	log_debug(logger, string_itoa(registro_a_insertar->key));
	registro_a_insertar->timeStamp = package->timestamp;
	log_debug(logger, string_itoa(registro_a_insertar->timeStamp));

	char* value = malloc(package->value_long);
	strcpy(value, package->value);

	log_debug(logger, (package->value));

	registro_a_insertar->value = malloc(strlen(value) + 1);
	strcpy(registro_a_insertar->value, value);
	log_debug(logger, (registro_a_insertar->value));



	return !(insertar_en_mem_table(registro_a_insertar, package->tabla));
}

int existe_tabla_en_mem_table(char* tabla_a_chequear) {

	int es_tabla(Tabla* tabla) {
		if (strcmp(tabla->nombre_tabla, tabla_a_chequear) == 0) {
			return 1;
		}
		return 0;
	}

	log_debug(logger, "Voy a chequear si existe la tabla");

	log_debug(logger, tabla_a_chequear);
	//signal
	Tabla* tabla_encontrada = (Tabla*) list_find(mem_table, (int) &es_tabla);
	//wait
	if(tabla_encontrada) {
		log_debug(logger, "Encontre tabla");
		return 1;
	}
	log_debug(logger, "No encontre tabla");



	return 0;
}



int agregar_tabla_a_mem_table(char* tabla) {
	Tabla* tabla_a_agregar = malloc(sizeof(Tabla));
	strcpy(tabla_a_agregar->nombre_tabla, tabla);
	tabla_a_agregar->registros = list_create();

	//signal
	int status = list_add(mem_table, tabla_a_agregar);
	//wait
	log_debug(logger, string_itoa(status));
	return status;
}

int insertar_en_mem_table(Registro* registro_a_insertar, char* nombre_tabla) {

	int es_tabla(Tabla* tabla) {
		if (strcmp(tabla->nombre_tabla, nombre_tabla) == 0) {
			return 1;
		}
		return 0;
	}
	//signal
	Tabla* tabla = (Tabla*) list_find(mem_table, (int) &es_tabla);
	int status = list_add(tabla->registros, registro_a_insertar);
	//wait

	return status;
}
t_list* lfs_describe(char* punto_montaje){

	t_list* metadatas = list_create();
	DIR *tables_directory;
	struct dirent *a_directory;
	char* tablas_path = string_new();
	string_append(&tablas_path, punto_montaje);
	string_append(&tablas_path, "/Tables/");
	log_debug(logger, tablas_path);
	tables_directory = opendir(tablas_path);
	if (tables_directory) {
		while ((a_directory = readdir(tables_directory)) != NULL) {
			if (strcmp(a_directory->d_name, ".") && strcmp(a_directory->d_name, "..")) {
				char* a_table_path = string_new();
				char* table_name = malloc(strlen(a_directory->d_name));
				memcpy(table_name, a_directory->d_name,strlen(a_directory->d_name) + 1);
				string_append(&a_table_path, tablas_path);
				string_append(&a_table_path, table_name);
				Metadata* metadata = obtener_metadata(a_table_path);
				strcpy(metadata->nombre_tabla, table_name);
				list_add(metadatas, metadata);
				free(table_name);
				free(a_table_path);
			}

		}
		closedir(tables_directory);
	}
	return metadatas;
}


int existe_tabla(char* tabla) {
	log_debug(logger, tabla);

	int status=1;
	DIR *dirp;

	dirp = opendir(tabla);

	if (dirp == NULL) {
		status= 0;
	}
	closedir(dirp);
	return status;
}



void loguear_metadata(Metadata* metadata) {
	//log_debug(logger, metadata->nombre_tabla);
	log_debug(logger, consistency_to_str(metadata->consistency));
	log_debug(logger, string_itoa(metadata->partitions));
	log_debug(logger, string_itoa(metadata->compaction_time));
}

void loguear_registro(Registro* registro) {
	log_debug(logger, string_itoa(registro->timeStamp));
	log_debug(logger, string_itoa(registro->key));
	log_debug(logger, registro->value);
}


Metadata* obtener_metadata(char* ruta) {
	char* mi_ruta = string_new();
	string_append(&mi_ruta, ruta);
	char* mi_metadata = "/Metadata";
	string_append(&mi_ruta, mi_metadata);
	log_debug(logger, mi_ruta);

	t_config* config_metadata = config_create(mi_ruta);

	Metadata* metadata = malloc(sizeof(Metadata));
	int particiones = config_get_int_value(config_metadata, "PARTITIONS");

	metadata->partitions = particiones;
	long tiempoDeCompactacion = config_get_long_value(config_metadata,"COMPACTION_TIME");
	metadata->compaction_time = tiempoDeCompactacion;


	char* consistencia = config_get_string_value(config_metadata, "CONSISTENCY");
	metadata->consistency = consistency_to_int(consistencia);

	config_destroy(config_metadata);
	free(mi_ruta);

	return metadata;
}

int calcular_particion(int key, int cantidad_particiones) {

	return (key % cantidad_particiones) + 1;
}

t_list* encontrar_keys(int keyBuscada, int particion_objetivo, char* ruta, char* montaje) {
	t_list* lista_registros = list_create();
	char* mi_ruta = string_new();
	string_append(&mi_ruta, ruta);
	char* barra = "/";
	string_append(&mi_ruta, barra);
	string_append(&mi_ruta, string_itoa(particion_objetivo));
	char* bin = ".bin";
	string_append(&mi_ruta, bin);

	log_debug(logger, mi_ruta);

	log_warning(logger, "Ahora voy a mostrar los datos de los bloques");

	t_config* particion = config_create(mi_ruta);

	int size = config_get_int_value(particion, "SIZE");
	char** blocks = config_get_array_value(particion, "BLOCKS");

	int i = 0;
	while(blocks[i] != NULL){
		char* ruta_a_bloque = string_new();
		string_append(&ruta_a_bloque, montaje);
		string_append(&ruta_a_bloque, "/Bloques/");
		string_append(&ruta_a_bloque, blocks[i]);
		string_append(&ruta_a_bloque, ".bin");

		log_debug(logger, ruta_a_bloque);


		int fd = open(ruta_a_bloque, O_RDONLY, S_IRUSR | S_IWUSR);

		log_debug(logger, "Abri el Bloque");

		struct stat s;
	    int status = fstat (fd, & s);
	    size = s.st_size;

	    //TODO: Leer solo hasta el \n o hasta el tercer ; y manejar el offset

	    char* f = mmap (NULL, size, PROT_READ, MAP_PRIVATE, fd, 0);
	    char** registros = string_split(f, "\n"); // Spliteo y obtengo una lista de los registros
	    int j = 0;
	    while (registros[j] != NULL) { //Recorro los registros hasta que no haya mas segun las commons
			//log_debug(logger, registros[j]); //Logueo el registro entero, se puede borrar
			char** datos_registro = string_split(registros[j], ";"); //Spliteo un Registro para tener una lista de sus datos
			int k = 0;
			Registro* registro = malloc(sizeof(Registro)); //Yo deberia hacer un free de este registro despues? Creo que no, porque yo lo guardo en la lista y pierdo la referencia si le hago el free
			if(atoi(datos_registro[1]) == keyBuscada){
				registro->timeStamp = atol(datos_registro[0]);
				registro->key = atoi(datos_registro[1]);
				registro->value = malloc(strlen(datos_registro[2])+1); // Lo mismo, el free deberia hacerlo en algun momento sobre esto?
				strcpy(registro->value, datos_registro[2]);
				list_add(lista_registros, registro);
			}
			while (datos_registro[k] != NULL) { //Recorro los datos hasta que no haya mas segun las commons
				//log_debug(logger, datos_registro[k]); //Logueo cada dato (timestamp, key, value)
				k++;
			}
			j++;

	    }
		close(fd);

		free(ruta_a_bloque);
		i++;
	}


	free(mi_ruta);

	return lista_registros;
}

int timestamp_mayor_entre(Registro* un_registro, Registro* otro_registro){
	if(un_registro->timeStamp>otro_registro->timeStamp){
		return 1;
	}else{
		return 0;
	}
}
