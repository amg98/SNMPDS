// Includes C
#include <stdio.h>

// Includes propios
#include "asn1.h"
#include "snmp.h"
#include "wifi.h"
#include "common.h"

// Escribe el campo longitud
void asn1_tx_length(u16 len){
	if(len == 0){									// Longitud indefinida
		tx_buffer[txIndex++] = (1 << 7);
	} else if(len <= 127){							// Longitud corta
		tx_buffer[txIndex++] = (u8)(len &0x7F);
	} else {										// Longitud larga  (16 bits)
		tx_buffer[txIndex++] = (1 << 7) | 2;
		tx_buffer[txIndex++] = (u8)((len >> 8) &0xFF);
		tx_buffer[txIndex++] = (u8)(len &0xFF);
	}
}

// Escribe un integer en el buffer
void asn1_tx_integer(u8 type, s32 data){
	tx_buffer[txIndex++] = type;
	if(data > 0xFFFFFF){					// 32 bits
		tx_buffer[txIndex++] = 4;
		tx_buffer[txIndex++] = (data >> 24) &0xFF;
		tx_buffer[txIndex++] = (data >> 16) &0xFF;
		tx_buffer[txIndex++] = (data >> 8) &0xFF;
	}else if(data > 0xFFFF){				// 24 bits
		tx_buffer[txIndex++] = 3;
		tx_buffer[txIndex++] = (data >> 16) &0xFF;
		tx_buffer[txIndex++] = (data >> 8) &0xFF;
	}else if(data > 0xFF){					// 16 bits
		tx_buffer[txIndex++] = 2;
		tx_buffer[txIndex++] = (data >> 8) &0xFF;
	} else {								// 8 bits
		tx_buffer[txIndex++] = 1;
	}
	tx_buffer[txIndex++] = data &0xFF;
}

// Escribe un octet string en el buffer
void asn1_tx_octetstring(u8 type, char *str){
	u16 l = strlen(str);
	tx_buffer[txIndex++] = type;
	asn1_tx_length(l);
	memcpy(&tx_buffer[txIndex], str, l);
	txIndex += l;
}

// Escribe un sequence
// Los campos se escriben aparte
void asn1_tx_sequence(u8 type, u16 len){
	tx_buffer[txIndex++] = (1 << 5) | type;		// Estructurado
	asn1_tx_length(len);
}

// Escribe una marca de final de contenidos
void asn1_tx_contentend(){
	tx_buffer[txIndex++] = 0;
	tx_buffer[txIndex++] = 0;
}

// Escribe un NULL
void asn1_tx_null(){
	tx_buffer[txIndex++] = SNMP_NULL;
	tx_buffer[txIndex++] = 0;
}

// Escribe un OID
void asn1_tx_oid(u16 *oid, u8 n, u8 size){
	
	// Variables
	u8 i;
	u16 temp = oid[1];
	
	// Escribe el tipo y la longitud
	tx_buffer[txIndex++] = SNMP_OID;
	tx_buffer[txIndex++] = size;						// Muy raro que un OID tenga mas de 127 elementos (63 enteros de 16 bits)
	
	// Calcula los 2 primeros
	oid[1] = oid[0] * 40 + oid[1];						// X * 40 + Y
	
	// Escribe los siguientes
	for(i = 1; i < n; i++){
		if(oid[i] > 127){
			tx_buffer[txIndex++] = (u8)(((oid[i] >> 7) &0x7F) | (1 << 7));
		}
		tx_buffer[txIndex++] = (u8)(oid[i] &0x7F);
	}
	
	// Deja el OID como estaba
	oid[1] = temp;
}

// Obten el tamaño (en bytes) de un OID
u8 asn1_oid_size(u16 *oid, u8 n){
	
	u8 size = 1;
	u8 i;
	
	if((oid[0] * 40 + oid[1]) > 127){
		size ++;
	}
	
	for(i = 2; i < n; i++){
		if(oid[i] > 127){
			size += 2;
		} else {
			size ++;
		}
	}
	
	return size;
}

// Lee el campo longitud
u16 asn1_rx_length(){
	u16 length = rx_buffer[rxIndex];
	if(length < 128){					// Longitud corta
		rxIndex ++;
	} else if(length == 128){			// Longitud indefinida
		rxIndex ++;
		length = 0xFFFF;
	} else {							// Longitud larga
		rxIndex ++;
		u16 l = length &0x7F;
		length = rx_buffer[rxIndex];
		if(l == 2){
			length = (length << 8) | rx_buffer[rxIndex+1];
		} else if(l > 2){
			length = 0xFFFF;			// Mas longitud de lo soportado (>64KB)
		}
		rxIndex += l;
	}
	return length;
}

// Obten un s32 del buffer de recepcion
s32 asn1_rx_integer(u8 type){
	if(rx_buffer[rxIndex++] == type){
		u8 length = rx_buffer[rxIndex++];
		s32 val = -1;
		if(length <= 4 && length > 0){
			val = rx_buffer[rxIndex++];
			while(--length > 0){
				val = (val << 8) | rx_buffer[rxIndex++];
			}
			return val;								// Devuelve el dato
		} else if(length > 4){						// Si es mas de 4 bytes (incumple el protocolo)
			rxIndex += length;						// El valor sera -1 y saltatelo
		}
	} else {
		iprintf("No es un INTEGER\n");
	}
	return -1;										// En caso de error
}

// Obten un octet string (o IpAddress) del buffer de recepcion
char *asn1_rx_octetstring(u8 type, u16 *size){
	if(rx_buffer[rxIndex++] == type){
		*size = asn1_rx_length();
		if(*size == 0xFFFF){					// Error en longitud indefinida o longitudes muy largas
			iprintf("Error leyendo OCTET STRING\n");
		} else {
			char *str = (char*)&rx_buffer[rxIndex];
			rxIndex += *size;
			return str;										// Devuelve la cadena
		}
	} else {
		iprintf("No es un OCTET STRING\n");
	}
	return NULL;
}

// Obten un OID del buffer de recepcion
u16 *asn1_rx_oid(u16 *size, u8 skip){
	
	// Comprueba que es un OID
	if(rx_buffer[rxIndex++] != SNMP_OID){
		iprintf("No es un OID\n");
		return NULL;
	}
	
	// Comprueba la longitud
	u16 length = asn1_rx_length();
	if(length == 0 || length == 0xFFFF){		// Error en longitud indefinida o longitudes muy largas
		iprintf("Error leyendo OID\n");
		return NULL;
	}
	
	// Decide si saltar
	if(skip){
		rxIndex += length;
		return (u16*)0x01;						// OK (simplemente para distinguirlo de un error)
	}
	
	// Si no lo saltas, obten el tamaño del OID
	u16 i = 0;
	u16 nintegers = 1;							// 1 porque el primer numero son en verdad 2
	while(i < length){
		if(rx_buffer[rxIndex + i] > 127){
			i += 2;								// Muy complicado que un elemento de un OID sea mas de 16 bits
		} else {
			i ++;
		}
		nintegers ++;
	}
	
	// Crea el buffer
	*size = nintegers * sizeof(u16);
	u16 *oid = (u16*) allocate(*size);
	
	// Lee el OID
	for(i = 0; i < nintegers; i++){
		
		// Lee el siguiente valor
		u16 data = rx_buffer[rxIndex++];
		if(data &(1 << 7)){
			data = ((data &0x7F) << 7) | (rx_buffer[rxIndex++] &0x7F);
		}
		
		if(i == 0){			// Primeros 2 valores
			oid[0] = data / 40;
			oid[1] = data - oid[0] * 40;
			i++;			// Suma otro mas puesto que hemos leido 2
		} else {
			oid[i] = data;
		}
	}
	
	// Devuelve el OID leido
	return oid;
}

// Escribe un OID en un archivo
void asn1_print_oid(FILE *f, u16 *oid, u8 n_oid, char endchar){
	if(f == NULL || oid == NULL) return;
	u8 i;
	for(i = 0; i < n_oid; i++){
		fprintf(f, "%d%c", oid[i], ((i + 1) < n_oid) ? '.' : endchar);
	}
}
