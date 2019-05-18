// Includes C
#include <stdio.h>
#include <string.h>

// Includes propios
#include "tftp.h"
#include "wifi.h"
#include "snmp.h"

// Envia el write request
int sendWriteRequest(char *filename){
	
	// Comienza una transmision
	txIndex = 0;
	
	// Que sea la peticion por el puerto TFTP
	server_tftp.sin_port = htons(PORT_TFTP);
	
	// Escribe 2 (Write request)
	tx_buffer[txIndex++] = 0;
	tx_buffer[txIndex++] = 2;
	
	// Escribe el nombre del archivo
	int filename_length = strlen(filename);
	memcpy(&tx_buffer[txIndex], filename, filename_length);
	txIndex += filename_length;
	tx_buffer[txIndex++] = 0;
	
	// Escribe "octet" (transmision por octetos)
	memcpy(&tx_buffer[txIndex], "octet", 5);
	txIndex += 5;
	tx_buffer[txIndex++] = 0;
	
	// Envia los datos
	if(sendto(socket_tftp, tx_buffer, txIndex, 0, (struct sockaddr*)&server_tftp, sizeof(struct sockaddr)) < 0){
		return -1;
	}
	
	// Configura el timeout
	fd_set readFDS;
	FD_ZERO(&readFDS);
	FD_SET(socket_tftp, &readFDS);
	int status = select(socket_tftp + 1, &readFDS, NULL, NULL, &tftp_timeout);
	if((status <= 0) || !FD_ISSET(socket_tftp, &readFDS)){
		return -2;
	}
	
	// Recibe datos
	int sin_size;
	struct sockaddr_in from;
	if(recvfrom(socket_tftp, rx_buffer, RX_BUFFER_SIZE, 0, (struct sockaddr*)&from, &sin_size) <= 0) return -12;
	
	// Comprueba que los datos recibidos son correctos
	if(from.sin_addr.s_addr != server_tftp.sin_addr.s_addr) return -3;	// No es del servidor
	if(rx_buffer[0] != 0 || rx_buffer[1] != 4) return -4;				// No es un ACK
	if(rx_buffer[2] != 0 || rx_buffer[3] != 0) return -5;				// El numero de bloque no es 0
	
	// Guarda el puerto destino
	server_tftp.sin_port = from.sin_port;
	
	// Todo ha ido bien
	return 0;
}

// Lee un ACK
int readACK(){
	
	// Configura el timeout
	fd_set readFDS;
	FD_ZERO(&readFDS);
	FD_SET(socket_tftp, &readFDS);
	int status = select(socket_tftp + 1, &readFDS, NULL, NULL, &tftp_timeout);
	if((status <= 0) || !FD_ISSET(socket_tftp, &readFDS)){
		return 0;
	}
	
	// Recibe datos
	int sin_size;
	struct sockaddr_in from;
	if(recvfrom(socket_tftp, rx_buffer, RX_BUFFER_SIZE, 0, (struct sockaddr*)&from, &sin_size) <= 0) return 0;
	
	// Comprueba que los datos recibidos son correctos
	if(from.sin_addr.s_addr != server_tftp.sin_addr.s_addr) return -8;						// No es del servidor
	if(rx_buffer[0] != 0 || rx_buffer[1] != 4) return -9;									// No es un ACK
	
	// Si el numero de bloque no es el actual, lee otro ACK
	if(rx_buffer[2] != tx_buffer[2] || rx_buffer[3] != tx_buffer[3]){
		return 1;
	}
	
	// Todo bien
	return 2;
}

// Envia el siguiente paquete de datos
int sendNextPacket(void *data, int size, u16 blockNumber){
	
	// Comienza una transmision
	txIndex = 0;
	
	// Escribe 3 (Data packet)
	tx_buffer[txIndex++] = 0;
	tx_buffer[txIndex++] = 3;
	
	// Escribe el numero de bloque
	tx_buffer[txIndex++] = (u8)((blockNumber >> 8) &0xFF);
	tx_buffer[txIndex++] = (u8)(blockNumber &0xFF);
	
	// Escribe los datos a enviar
	if(size > 0){
		memcpy(&tx_buffer[txIndex], data, size);
		txIndex += size;
	}
	
	// Envia los datos
	if(sendto(socket_tftp, tx_buffer, txIndex, 0, (struct sockaddr*)&server_tftp, sizeof(struct sockaddr)) < 0){
		return -6;
	}
	
	// Lee el ACK
	int res = 0;
	do {
		res = readACK();
	} while(res == 1);
	if(res <= 0) return res;
	
	// Todo ha ido bien
	return size;
	
}

// Envia un archivo por TFTP
int TFTP_SendFile(char *filename, void *data, u32 size){
	
	// Cierra el socket TFTP e inicia uno nuevo
	if(socket_tftp >= 0){
		shutdown(socket_tftp, 0);
		closesocket(socket_tftp);
	}
	socket_tftp = socket(AF_INET, SOCK_DGRAM, 0);
	if(socket_tftp < 0) return -30;
	
	// Enviar peticion de escritura (al puerto 69)
	// [opcode(WRQ = 2)(2bytes) | filename/mail (nullstring) | mode (nullstring) ("octet"/"mail")]
	// Block number (en los paquetes empieza por 1, ACK por 0)
	// Recibir ACK (nos dice el puerto donde enviar los datos) [opcode(ACK = 4)(2 bytes) | numero bloque(2 bytes)]
	int sendResult = sendWriteRequest(filename);
	if(sendResult < 0) return sendResult;
	
	// Mandar paquetes de 512 bytes / Recibir ACK
	// Paquetes: [opcode(DATA=3)(2 bytes) | numero bloque(2 bytes) | datos]
	u32 sent_size = 0;
	u16 packet_size = 0;
	u16 blockNumber = 1;
	u8 tries = 0;
	while(sent_size < size){
		
		// Obten el tamaño del siguiente paquete a enviar
		packet_size = size - sent_size;
		if(packet_size > 512) packet_size = 512;
		
		// Envia el siguiente paquete
		if((sendResult = sendNextPacket(&((u8*)data)[sent_size], packet_size, blockNumber)) < 0) return sendResult;
		if(sendResult > 0){
			sent_size += sendResult;		// Si no ha habido error, actualiza los bytes enviados
			blockNumber ++;					// Tenemos un bloque mas
			tries = 0;
		} else if(sendResult == 0){
			tries ++;
			if(tries == 4) return -15;
		}
		
		// Espera que se procese la parte wifi
		swiWaitForVBlank();
	}
	
	// Enviar paquete de 0 bytes si el tamaño es multiplo de 512 para indicar el fin de la conexion (recibiendo el ACK)
	if((size % 512) == 0){
		if((sendResult = sendNextPacket(NULL, 0, blockNumber)) < 0) return sendResult;
	}
	return 0;
}
