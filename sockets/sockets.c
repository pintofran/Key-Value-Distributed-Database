#include "sockets.h"

int strToHeader(char* header){

	if(strcmp(header,"SELECT")==0){
		return SELECT;
	}else if(strcmp(header,"INSERT")==0){
		return INSERT;
	}else if(strcmp(header,"DESCRIBE")==0){
		return DESCRIBE;
	}else if(strcmp(header,"DROP")==0){
		return DROP;
	}else if(strcmp(header,"CREATE")==0){
		return CREATE;
	}else if(strcmp(header,"JOURNAL")==0){
		return JOURNAL;
	}else if(strcmp(header,"RUN")==0){
		return RUN;
	}else if(strcmp(header,"ADD")==0){
		return ADD;
	}
	else if(strcmp(header,"exit")==0){
		return -1;
	}
	return 0;
}

void fill_package(t_PackagePosta *package){
	/* Me guardo el mensaje que manda */

	char* entrada = malloc(MAX_MESSAGE_SIZE);
	fgets(entrada, MAX_MESSAGE_SIZE, stdin);

	if(strcmp(entrada,"exit\n")==0){
		package->header = -1;
		free(entrada);
		return;
	}

	char** entradaSeparada = string_split(entrada, " ");

	package->header = strToHeader(entradaSeparada[0]);

	char* sinHeader = string_substring_from(entrada, strlen(entradaSeparada[0])+1);

	free(entrada);
	free(entradaSeparada);

	memcpy(package->message,sinHeader,strlen(sinHeader)-1);
	(package->message)[strlen(sinHeader)-1] = '\0';

	free(sinHeader);
	package->message_long = strlen(package->message) + 1;	// Me guardo lugar para el \0

	package->total_size = sizeof(package->message_long) + package->message_long + sizeof(package->header);
}

char* serializarOperandos(t_PackagePosta *package){

	char *serializedPackage = malloc(package->total_size);

	int offset = 0;
	int size_to_send;

	size_to_send =  sizeof(package->header);
	memcpy(serializedPackage + offset, &(package->header), size_to_send);
	offset += size_to_send;

	size_to_send =  sizeof(package->message_long);
	memcpy(serializedPackage + offset, &(package->message_long), size_to_send);
	offset += size_to_send;

	size_to_send =  package->message_long;
	memcpy(serializedPackage + offset, package->message, size_to_send);

	return serializedPackage;
}

void dispose_package(char **package){
	free(*package);
}

int recieve_and_deserialize(t_PackagePosta *package, int socketCliente){

	int status;
	int buffer_size;
	char *buffer = malloc(buffer_size = sizeof(uint32_t));

	uint32_t header;
	status = recv(socketCliente, buffer, sizeof(package->header), 0);
	memcpy(&(header), buffer, buffer_size);
	if (!status) return 0;

	package->header = header;

	uint32_t message_long;
	status = recv(socketCliente, buffer, sizeof(package->message_long), 0);
	memcpy(&(message_long), buffer, buffer_size);
	if (!status) return 0;

	package->message = malloc(message_long+1);

	status = recv(socketCliente, package->message, message_long, 0);
	if (!status) return 0;

	free(buffer);

	package->message_long = message_long;
	package->total_size = buffer_size*2 + message_long;

	return status;
}

