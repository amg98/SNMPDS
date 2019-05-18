#ifndef _SNMP_H_
#define _SNMP_H_

// Includes
#include <nds.h>

// Defines tipos de peticiones SNMP (tags de contexto)
#define SNMP_GETREQUEST			0
#define SNMP_GETNEXTREQUEST		1
#define SNMP_GETRESPONSE		2
#define SNMP_SETREQUEST			3
#define SNMP_TRAP				4

// Defines tipos de datos SNMP
#define SNMP_INTEGER 			2
#define SNMP_OCTET_STRING 		4
#define SNMP_NULL 				5
#define SNMP_OID 				6
#define SNMP_IPADDR				0x40		// [APPLICATION 0] OCTET STRING (SIZE(4))
#define SNMP_TIMETICKS			0x43		// [APPLICATION 3] IMPLICIT u32
#define SNMP_COUNTER			0x41		// [APPLICATION 1] IMPLICIT u32
#define SNMP_GAUGE				0x42		// [APPLICATION 2] IMPLICIT u32
#define SNMP_OPAQUE				0x44		// [APPLICATION 4] IMPLICIT u32

// Defines codigos de error SNMP
#define SNMP_OK					0
#define SNMP_ERROR_TOOBIG		1
#define SNMP_ERROR_NOSUCHNAME 	2
#define SNMP_ERROR_BADVALUE		3
#define SNMP_ERROR_READONLY		4
#define SNMP_ERROR_GENERR		5

// Variables externas
extern u16 txIndex;
extern u16 rxIndex;
extern char trapFile[32];

// Estructura de un campo SNMP
typedef struct{
	u8 oid_n;			// Numero de enteros del OID
	u8 oid_size;		// Tamaño del OID
	u16 *oid;			// OID de este campo
	u8 oid_updated;		// Si el OID que pedimos se ha actualizado (solo para GETNEXT)
	u8 type;			// Tipo de datos (los definidos como SNMP_*)
	u16 size;			// Tamaño de los datos (en bytes), en cadenas sin contar el terminador
	u8 oid_n_data;		// Si los datos son de tipo OID, cuantos enteros tiene (de 16 bits)
	u8 freeable;		// Son liberables los datos? (true si es un GET/GETNEXT)
	void *data;			// Datos de este campo
}SNMP_Field;

// Estructura de una peticion SNMP
typedef struct{
	u8 type;			// Tipo de peticion (GET/GETNEXT/SET)
	u8 nfields;			// Numero de campos pedidos
	SNMP_Field *field;	// Campos de la peticion
}SNMP_Request;

// Cabeceras de funciones
SNMP_Request *SNMP_CreateRequest(u8 type, u8 nfields);
void SNMP_FreeRequest(SNMP_Request *req);
void SNMP_SetVarbind(SNMP_Request *req, u8 index, u16 *oid, u8 oid_size, u8 type, u8 size, void *data, u8 freeable);
int SNMP_RequestData(SNMP_Request *req);
void SNMP_ReadTrap();

#endif
