// Includes C
#include <stdio.h>
#include <time.h>

// Includes propios
#include "snmp.h"
#include "wifi.h"
#include "asn1.h"
#include "common.h"
#include "config.h"

// Variables globales
u16 txIndex = 0;		// Indice del buffer de tranmision
u16 rxIndex = 0;		// Indice del buffer de recepcion
static u8 requestID = 1;
char trapFile[32] = "/proyectoGestion/traps.html";

// Variables locales
static char *html_base = "<!DOCTYPE HTML5>\n<html>\n\t<head>\n\t\t<title>Informe de traps</title>\n\t\t<meta charset=\"utf-8\">\n\t</head>\n\t<body>\n\t\t<u><b><h1 style=\"text-align: center; color: blue; font-family: Algerian; font-size: 48px; background-color: cyan\">Informe de traps</h1></b></u>\n";
static char *html_table = "\t\t<table border=\"1\" style=\"margin: 0 auto;\">\n\t\t<caption style=\"font-family: Comic Sans MS; font-size: 28px\">Trap %02d/%02d/%d %02d:%02d</caption>\n\t\t<tr style=\"background-color: orange\">\n\t\t\t<th>Nombre</th>\n\t\t\t<th>Valor</th>\n\t\t</tr>";
static char *html_table_end = "\t\t</table>\n";
static char *html_end = "\t\t<hr style=\"color: #0056b2;\" />\n\t\t<footer>\n\t\t\t<p><em>Creado por: SNMPDS</em></p>\n\t\t</footer>\n\t</body>\n</html>\n";

// Cabeceras de funciones locales
int snmp_getresponse(SNMP_Request *req);
int snmp_checkresponse();

// Crea una peticion SNMP
SNMP_Request *SNMP_CreateRequest(u8 type, u8 nfields){
	
	// Crea la estructura de la peticion
	SNMP_Request *req = (SNMP_Request*) allocate (sizeof(SNMP_Request));
	
	// Crea los campos
	req->type = type;
	req->nfields = nfields;
	req->field = (SNMP_Field*) allocate (nfields * sizeof(SNMP_Field));
	
	// Devuelve la peticion creada
	return req;
}

// Libera una peticion SNMP
void SNMP_FreeRequest(SNMP_Request *req){
	
	// Comprueba que no es NULL
	if(req == NULL) return;
	if(req->field == NULL) return;
	
	// Elimina cada campo
	u8 i;
	for(i = 0; i < req->nfields; i++){
		SNMP_Field *field = &req->field[i];
		if(field->data && field->size > 0 && field->freeable) free(field->data);
		if(req->type == SNMP_GETNEXTREQUEST && field->oid_updated){		// Si es un GETNEXT y se ha actualizado el OID
			free(field->oid);											// Libera el OID modificado
		}
	}
	
	// Elimina la estructura de la peticion
	free(req->field);
	free(req);
}

// Configura una variable de la peticion SNMP
void SNMP_SetVarbind(SNMP_Request *req, u8 index, u16 *oid, u8 oid_size, u8 type, u8 size, void *data, u8 freeable){
	
	// Comprueba que no es NULL
	if(req == NULL) return;
	if(req->field == NULL) return;
	if(index >= req->nfields) return;
	
	// Escribe los datos de este Varbind
	SNMP_Field *field = &req->field[index];
	field->oid_size = asn1_oid_size(oid, oid_size);
	field->oid_n = oid_size;
	field->oid = oid;
	field->type = type;
	field->oid_updated = 0;
	field->freeable = freeable;
	switch(type){
		case SNMP_OID:
			field->size = asn1_oid_size((u16*)data, size);	// Tamaño en bytes
			field->oid_n_data = size;						// Numero de enteros
			break;
		case SNMP_OCTET_STRING:
		case SNMP_IPADDR:
			field->size = size;
			break;
		default:
		{
			int a = *(int*)data;
			field->size = 1;
			if(a > 0xFF) field->size ++;
			if(a > 0xFFFF) field->size ++;
			if(a > 0xFFFFFF) field->size ++;
		}
	}
	field->data = data;
}

// Manda al servidor una peticion GET
int SNMP_RequestData(SNMP_Request *req){
	
	// Comprueba que no es NULL
	if(req == NULL) return -1;
	if(req->field == NULL) return -2;
	
	// Obten los tamaños de los campos de la PDU
	u8 i;
	u16 varbindlist_size = 0;
	for(i = 0; i < req->nfields; i++){
		SNMP_Field *field = &req->field[i];
		u16 varbind_size = field->oid_size + 2;														// OID
		if(req->type == SNMP_GETREQUEST || req->type == SNMP_GETNEXTREQUEST) varbind_size += 2;		// + VALUE = NULL si es un GET/GETNEXT
		else if(req->type == SNMP_SETREQUEST) varbind_size += field->size + asn1_sequence_length(field->size);	// + VALUE = valor si es un SET
		varbindlist_size += varbind_size + asn1_sequence_length(varbind_size);						// OID + VALUE + SEQUENCE
	}
	u16 data_size = varbindlist_size + asn1_sequence_length(varbindlist_size) + 9;					// 9 son el tamaño de 3 enteros de 1 byte (3 * 3)
	u16 packet_size = data_size + asn1_sequence_length(data_size) + strlen(community) + 2 + 3;		// El texto de comunidad no excede 127 caracteres
	
	// Comienza la escritura del paquete
	txIndex = 0;
	
	// Escribe los campos de la PDU
	asn1_tx_sequence(ASN1_SEQUENCE, packet_size);
		asn1_tx_integer(SNMP_INTEGER, 0);									// Version
		asn1_tx_octetstring(SNMP_OCTET_STRING, community);					// Comunidad
		asn1_tx_sequence(ASN1_CONTEXT_TAG | req->type, data_size);
			asn1_tx_integer(SNMP_INTEGER, requestID);						// RequestID
			asn1_tx_integer(SNMP_INTEGER, 0);								// Error status
			asn1_tx_integer(SNMP_INTEGER, 0);								// Error index
			asn1_tx_sequence(ASN1_SEQUENCE, varbindlist_size);				// SEQUENCE OF VarBind (VarBindList)
			for(i = 0; i < req->nfields; i++){
				SNMP_Field *field = &req->field[i];
				if(req->type == SNMP_GETREQUEST || req->type == SNMP_GETNEXTREQUEST){		// VarBind
					asn1_tx_sequence(ASN1_SEQUENCE, field->oid_size + 2 + 2);
				} else if(req->type == SNMP_SETREQUEST){
					asn1_tx_sequence(ASN1_SEQUENCE, field->oid_size + 2 + field->size + asn1_sequence_length(field->size));
				}
					asn1_tx_oid(field->oid, field->oid_n, field->oid_size);												// Name (OID)
					if(req->type == SNMP_GETREQUEST || req->type == SNMP_GETNEXTREQUEST){
						asn1_tx_null();																					// Value (NULL para GET/GETNEXT)
					} else if(req->type == SNMP_SETREQUEST){															// Si es un SetRequest...
						switch(field->type){
							case SNMP_INTEGER:
							case SNMP_TIMETICKS:
							case SNMP_COUNTER:
							case SNMP_GAUGE:
							case SNMP_OPAQUE:
								asn1_tx_integer(field->type, *(s32*)field->data);
								break;
							case SNMP_OCTET_STRING:
							case SNMP_IPADDR:
								asn1_tx_octetstring(field->type, (char*)field->data);
								break;
							case SNMP_NULL:
								asn1_tx_null();
								break;
							case SNMP_OID:
								asn1_tx_oid((u16*)field->data, field->oid_n_data, field->size);
								break;
							default:
								return -3;
						}
					} else {
						return -4;
					}
			}
	
	// Manda el GET
	if(sendto(socket_snmp, tx_buffer, txIndex, 0, (struct sockaddr*)&server_snmp, sizeof(struct sockaddr)) < 0){
		return -5;
	}
	
	// Hemos enviado un paquete mas
	requestID ++;
	
	// Configura el timeout
	fd_set readFDS;
	FD_ZERO(&readFDS);
	FD_SET(socket_snmp, &readFDS);
	int status = select(socket_snmp + 1, &readFDS, NULL, NULL, &wifi_timeout);
	if((status <= 0) || !FD_ISSET(socket_snmp, &readFDS)){
		return -6;
	}
	
	// Recibe datos
	int sin_size;
	struct sockaddr_in from;
	recvfrom(socket_snmp, rx_buffer, RX_BUFFER_SIZE, 0, (struct sockaddr*)&from, &sin_size);
	
	// Comprueba la cabecera
	status = snmp_checkresponse();
	if(status < 0) return status;
	
	// Debemos obtener un get-response
	if(rx_buffer[rxIndex++] != 0xA2){
		return -10;
	}
		
	// Comprueba que lo recibimos del switch
	if((from.sin_port != server_snmp.sin_port) || (from.sin_addr.s_addr != server_snmp.sin_addr.s_addr)){
		return -11;
	}
	
	// Saltate la longitud de este SEQUENCE
	asn1_rx_length();
	
	// El request-id debe ser el mismo que el get-request
	if(asn1_rx_uinteger(SNMP_INTEGER) != (u32)(requestID - 1)){
		return -12;
	}
	
	// Comprueba si hay errores
	int error = asn1_rx_integer(SNMP_INTEGER);
	if(error != 0){
		return error;
	}
	asn1_rx_integer(SNMP_INTEGER);						// No hay errores, error-index deberia ser 0
	
	// Ahora deberiamos tener el VarBindList
	if(rx_buffer[rxIndex++] != 0x30){
		return -12;
	}
	asn1_rx_length();
	
	// Si era un SetRequest, no hace falta leer lo que hemos recibido, solo nos interesaba si habia errores
	if(req->type == SNMP_SETREQUEST) return 0;
	
	// Recorre la VarBindList
	for(i = 0; i < req->nfields; i++){
		
		// Obten el campo actual
		SNMP_Field *field = &req->field[i];
		
		// Ahora deberiamos tener el VarBind
		if(rx_buffer[rxIndex++] != 0x30){
			return -13;
		}
		asn1_rx_length();			// Por definicion, deben ser de longitud definida (corta o larga)
		
		// Obten el OID (suponemos que el switch los responde en el orden en el que se lo enviamos)
		u16 oid_size;
		u16 *new_oid = asn1_rx_oid(&oid_size, !SNMP_GETNEXTREQUEST);		// Saltatelo si no es un GetNext
		if(new_oid == NULL){
			return -14;
		}
		
		// Si tenemos un GetNextRequest, actualiza el OID pedido por el recibido
		if(req->type == SNMP_GETNEXTREQUEST){
			field->oid_n = oid_size / sizeof(u16);
			field->oid_size = asn1_oid_size(new_oid, field->oid_n);
			field->oid = new_oid;
			field->oid_updated = 1;		// Este OID queda actualizado (se liberara su memoria)
		}
		
		// Obten el valor
		switch(field->type){
			case SNMP_INTEGER:
			case SNMP_TIMETICKS:
			case SNMP_COUNTER:
			case SNMP_GAUGE:
			case SNMP_OPAQUE:
				field->size = sizeof(s32);
				field->data = allocate (sizeof(s32));
				((s32*)field->data)[0] = asn1_rx_integer(field->type);
				break;
			case SNMP_OCTET_STRING:
			case SNMP_IPADDR:
			{
				char *data = asn1_rx_octetstring(field->type, &field->size);
				if(data == NULL) return -1;
				field->data = allocate ((field->size + 1) * sizeof(char));	// Para el terminador de cadena
				if(field->size > 0) memcpy(field->data, data, field->size);	// Copia si la cadena no esta vacia
			}
				break;
			case SNMP_NULL:
				rxIndex += 2;		// Saltate el NULL
				break;
			case SNMP_OID:
				field->data = asn1_rx_oid(&field->size, 0);
				if(field->data == NULL) return -1;
				field->oid_n_data = field->size / sizeof(u16);
				field->size = asn1_oid_size(field->data, field->oid_n_data);
				break;
			default:
				return -15;
		}
	}
	
	// Aqui deberian estar las marcas de fin de contenidos si la longitud de los SEQUENCE
	// han sido indefinidas, pero no nos sirve de nada leerlas
	return 0;
}

// Comprueba un GET-RESPONSE que se haya recibido
int snmp_checkresponse(){
	
	// Inicia la lectura del buffer de recepcion
	rxIndex = 0;
	
	// Debemos tener un sequence al principio
	if(rx_buffer[rxIndex++] != 0x30){
		return -7;
	}
	asn1_rx_length();
	
	// Mira que la version sea SNMPv1
	if(asn1_rx_uinteger(SNMP_INTEGER) != 0){
		return -8;
	}
	
	// Lee la comunidad
	u16 strlength = 0;
	char *str = asn1_rx_octetstring(SNMP_OCTET_STRING, &strlength);
	if(str == NULL) return -1;
	if(strncmp(str, community, strlength) != 0){
		return -9;
	}
	
	// Comprobaciones correctas
	return 0;
}

// Lee un trap que se haya recibido
void SNMP_ReadTrap(){
	
	// Realiza las comprobaciones
	if(snmp_checkresponse() == -1){
		iprintf("Trap 1");
		return;
	}
	
	// Comprueba que se trata de un TRAP
	if(rx_buffer[rxIndex++] != 0xA4){
		iprintf("Trap 2");
		return;
	}
	asn1_rx_length();
	
	// Abre el archivo de TRAPS
	FILE *f = fopen(trapFile, "rb");
	if(f == NULL){						// Si no puedes abrir el archivo, es que no ha sido creado
		f = fopen(trapFile, "wb");
		if(f == NULL){
			iprintf("Imposible abrir: %s\n", trapFile);
			return;
		} else {
			// Escribe la base del documento HTML
			fprintf(f, html_base);
			fprintf(f, html_end);
		}
	}
	fclose(f);
	f = fopen(trapFile, "r+");
	if(f == NULL){
		iprintf("Imposible abrir: %s\n", trapFile);
		return;
	}
	
	// Situate en el final
	fseek(f, -strlen(html_end), SEEK_END);
	
	// Escribe el comienzo de la tabla
	time_t now = time(NULL);
	struct tm *t = localtime(&now);
	fprintf(f, html_table, (int)t->tm_mday, (int)(t->tm_mon + 1), (int)(1900 + t->tm_year), (int)t->tm_hour, (int)t->tm_min);
	
	// Lee el OID de la empresa
	u16 oid_size;
	u16 *oid = asn1_rx_oid(&oid_size, 0);
	if(oid == NULL) return;
	fprintf(f, "\t\t<tr>\n\t\t\t<td>Empresa</td>\n\t\t\t<td>");
	asn1_print_oid(f, oid, oid_size / sizeof(u16), ' ');
	fprintf(f, "</td>\n\t\t</tr>\n");
	free(oid);
	
	// Lee la IP del agente
	u16 agent_ipsize;
	char *agent_ip = asn1_rx_octetstring(SNMP_IPADDR, &agent_ipsize);
	if(agent_ip == NULL) return;
	fprintf(f, "\t\t<tr>\n\t\t\t<td>Generado por</td>\n\t\t\t<td>%d.%d.%d.%d</td>\n\t\t</tr>\n", (int)agent_ip[0], (int)agent_ip[1], (int)agent_ip[2], (int)agent_ip[3]);
	
	// Obten el tipo general de TRAP
	int traptype = asn1_rx_integer(SNMP_INTEGER);
	if(traptype == -1) return;
	
	// Obten el tipo especifico de TRAP
	int trapspecific = asn1_rx_integer(SNMP_INTEGER);
	if(trapspecific == -1) return;
	fprintf(f, "\t\t<tr>\n\t\t\t<td>Tipo</td>\n\t\t\t<td>%d</td>\n\t\t</tr>\n", traptype);
	fprintf(f, "\t\t<tr>\n\t\t\t<td>Tipo especifico</td>\n\t\t\t<td>%d</td>\n\t\t</tr>\n", trapspecific);
	
	// Obten cuando se ha producido el TRAP
	int traptime = asn1_rx_integer(SNMP_TIMETICKS);
	if(traptime == -1) return;
	
	// Leer el VarBindList
	if(rx_buffer[rxIndex++] != 0x30){
		iprintf("No hay VarBindList\n");
		return;
	}
	
	// Lee cada campo
	u16 length = asn1_rx_length() + rxIndex;
	while(rxIndex < length){
		
		// Lee el Varbind
		if(rx_buffer[rxIndex++] != 0x30){
			iprintf("No hay VarBind\n");
			return;
		}
		asn1_rx_length();
		
		// Obten el OID
		oid = asn1_rx_oid(&oid_size, 0);
		if(oid == NULL) return;
		
		// Escribe el OID
		fprintf(f, "\t\t<tr>\n\t\t\t<td>");
		asn1_print_oid(f, oid, oid_size / sizeof(u16), ' ');
		fprintf(f, "</td>\n\t\t\t<td>");
		free(oid);
		
		// Obten el valor
		switch(rx_buffer[rxIndex]){
			case SNMP_INTEGER:
			case SNMP_TIMETICKS:
			case SNMP_COUNTER:
			case SNMP_GAUGE:
			case SNMP_OPAQUE:
			{
				int val = asn1_rx_integer(rx_buffer[rxIndex]);
				if(val == -1) return;
				fprintf(f, "%d", val);
			}
				break;
			case SNMP_OCTET_STRING:
			case SNMP_IPADDR:
			{
				u16 str_size;
				u8 type = rx_buffer[rxIndex];
				char *str = asn1_rx_octetstring(type, &str_size);
				if(str == NULL || str_size == 0) return;
				u8 temp = str[str_size];
				str[str_size] = 0;				// Pon el terminador de la cadena de forma temporal
				if(type == SNMP_IPADDR){
					fprintf(f, "%d.%d.%d.%d", (int)str[0], (int)str[1], (int)str[2], (int)str[3]);
				} else {
					fprintf(f, "%s", str);
				}
				str[str_size] = temp;			// Restaura el valor original de ese byte
			}
				break;
			case SNMP_NULL:
				fprintf(f, "(null)");
				rxIndex += 2;					// Saltate el NULL
				break;
			case SNMP_OID:
			{
				oid = asn1_rx_oid(&oid_size, 0);
				if(oid == NULL) return;
				asn1_print_oid(f, oid, oid_size / sizeof(u16), ' ');
				free(oid);
			}
				break;
			default: break;
		}
		
		// Escribe el final de esa fila
		fprintf(f, "</td>\n\t\t</tr>\n");
	}
	
	// Escribe el final de la tabla
	fprintf(f, html_table_end);
	
	// Escribe el final del documento HTML
	fprintf(f, html_end);
	
	// Antes de terminar, obten el tamaño del archivo de traps
	fseek(f, 0, SEEK_END);
	consoleClear();
	int trapSize = ftell(f);
	iprintf("\t Traps: %d bytes", trapSize);
	
	// Hemos terminado este TRAP
	fclose(f);
}
