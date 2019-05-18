// Includes C
#include <stdio.h>

// Includes NDS
#include <nds.h>

// Includes propios
#include "common.h"
#include "alarm.h"
#include "graphics.h"
#include "menu.h"
#include "config.h"
#include "snmp.h"

// Lista de OID para la creacion de una alarma
static u16 alarm_oid[14][12] = {
	{1, 3, 6, 1, 2, 1, 16, 9, 1, 1, 2, 1},		// eventDescription (ultimo valor es el indice del evento, y debe estar creado de antes)
	{1, 3, 6, 1, 2, 1, 16, 9, 1, 1, 3, 1},		// eventType
	{1, 3, 6, 1, 2, 1, 16, 9, 1, 1, 4, 1},		// eventCommunity
	{1, 3, 6, 1, 2, 1, 16, 9, 1, 1, 6, 1},		// eventOwner
	{1, 3, 6, 1, 2, 1, 16, 3, 1, 1, 2, 1},		// alarmInterval (ultimo valor es el indice de la alarma)
	{1, 3, 6, 1, 2, 1, 16, 3, 1, 1, 3, 1},		// alarmVariable (OID de la interfaz)
	{1, 3, 6, 1, 2, 1, 16, 3, 1, 1, 4, 1},		// alarmSampleType (1 absolute, 2 delta)
	{1, 3, 6, 1, 2, 1, 16, 3, 1, 1, 6, 1},		// alarmStartupAlarm (1 rising, 2 falling, 3 los dos)
	{1, 3, 6, 1, 2, 1, 16, 3, 1, 1, 7, 1},		// alarmRisingThreshold
	{1, 3, 6, 1, 2, 1, 16, 3, 1, 1, 8, 1},		// alarmFallingThreshold
	{1, 3, 6, 1, 2, 1, 16, 3, 1, 1, 9, 1},		// alarmRisingEventIndex
	{1, 3, 6, 1, 2, 1, 16, 3, 1, 1, 10, 1},		// alarmFallingEventIndex (el mismo que rising en nuestro ejemplo)
	{1, 3, 6, 1, 2, 1, 16, 3, 1, 1, 11, 1},		// alarmOwner (el mismo que eventOwner)
	{1, 3, 6, 1, 2, 1, 16, 3, 1, 1, 12, 1},		// alarmStatus (poner a 2 (create-request), despues a 1 (valid))
};
static u16 interface_pkts_oid[12] = {
	1, 3, 6, 1, 2, 1, 16, 1, 1, 1, 4, 1			// El ultimo numero es el numero de la interfaz (obtenemos los bytes recibidos en este ejemplo)
};

// Carga la pantalla de alarma
void cargarAlarma(){
	
	// Carga los fondos
	loadBitmap8("proyectoGestion/images/alarma", 0);
	loadBitmap8("proyectoGestion/images/wifi_sub", 1);
	
	// Selecciona la consola inferior
	consoleSelect(&bottomConsole);
	iprintf("\n\n\n\t");		// Posiciona el cursor de la consola
}

// Elimina la pantalla de alarma
void eliminarAlarma(){
	
	// Esconde los fondos
	bgHide(bitmapLayerTop);
	bgHide(bitmapLayerBottom);
	
	// Limpia la consola de texto
	consoleClear();
}

// Pide datos al usuario
static void pedirDatos(const char *msg, const char *str, void *data){
	consoleClear();
	iprintf("\n\n\n\t%s\n\t ", msg);
	scanf(str, data);
}

// Configura una alarma
void estado_alarma(){

	// Definicion de variables
	int eventID;
	char eventDescription[64];
	char eventOwner[16];
	int eventType = 3;				// Para que mande solo traps
	int alarmIndex;
	int alarmInterval;
	int alarmSampleType;
	int alarmStartup;
	int risingThreshold;
	int fallingThreshold;
	int interface;
	int alarmStatus = 2;
	
	// Obten las variables necesarias
	pedirDatos("Numero de evento:", "%d", &eventID);
	pedirDatos("Descripcion:", "%32s", eventDescription);
	pedirDatos("Responsable:", "%16s", eventOwner);
	pedirDatos("Numero de alarma:", "%d", &alarmIndex);
	pedirDatos("Intervalo (s):", "%d", &alarmInterval);
	pedirDatos("Muestreo (1=abs, 2=delta)", "%d", &alarmSampleType);
	pedirDatos("1 = subida, 2 = bajada\n\t3 = subida y bajada", "%d", &alarmStartup);
	pedirDatos("Limite superior (bytes)", "%d", &risingThreshold);
	pedirDatos("Limite inferior (bytes)", "%d", &fallingThreshold);
	pedirDatos("Numero de puerto", "%d", &interface);
	
	// Configura los OID
	u8 i;
	for(i = 0; i < 4; i++){
		alarm_oid[i][11] = eventID;
	}
	for(i = 4; i < 14; i++){
		alarm_oid[i][11] = alarmIndex;
	}
	interface_pkts_oid[11] = interface;
	
	// Realiza la peticion SNMP SET
	SNMP_Request *req = SNMP_CreateRequest(SNMP_SETREQUEST, 14);
	SNMP_SetVarbind(req, 0, alarm_oid[0], 12, SNMP_OCTET_STRING, strlen(eventDescription), eventDescription, false);
	SNMP_SetVarbind(req, 1, alarm_oid[1], 12, SNMP_INTEGER, sizeof(int), &eventType, false);
	SNMP_SetVarbind(req, 2, alarm_oid[2], 12, SNMP_OCTET_STRING, strlen(community), community, false);
	SNMP_SetVarbind(req, 3, alarm_oid[3], 12, SNMP_OCTET_STRING, strlen(eventOwner), eventOwner, false);
	SNMP_SetVarbind(req, 4, alarm_oid[4], 12, SNMP_INTEGER, sizeof(int), &alarmInterval, false);
	SNMP_SetVarbind(req, 5, alarm_oid[5], 12, SNMP_OID, 12, interface_pkts_oid, false);
	SNMP_SetVarbind(req, 6, alarm_oid[6], 12, SNMP_INTEGER, sizeof(int), &alarmSampleType, false);
	SNMP_SetVarbind(req, 7, alarm_oid[7], 12, SNMP_INTEGER, sizeof(int), &alarmStartup, false);
	SNMP_SetVarbind(req, 8, alarm_oid[8], 12, SNMP_INTEGER, sizeof(int), &risingThreshold, false);
	SNMP_SetVarbind(req, 9, alarm_oid[9], 12, SNMP_INTEGER, sizeof(int), &fallingThreshold, false);
	SNMP_SetVarbind(req, 10, alarm_oid[10], 12, SNMP_INTEGER, sizeof(int), &eventID, false);
	SNMP_SetVarbind(req, 11, alarm_oid[11], 12, SNMP_INTEGER, sizeof(int), &eventID, false);
	SNMP_SetVarbind(req, 12, alarm_oid[12], 12, SNMP_OCTET_STRING, strlen(eventOwner), eventOwner, false);
	SNMP_SetVarbind(req, 13, alarm_oid[13], 12, SNMP_INTEGER, sizeof(int), &alarmStatus, false);
	consoleClear();
	iprintf("\n\n\n\t");
	int ret;
	if((ret = SNMP_RequestData(req)) == SNMP_OK){
		SNMP_FreeRequest(req);
		alarmStatus = 1;				// Poner la alarma en estado valid (1)
		req = SNMP_CreateRequest(SNMP_SETREQUEST, 1);
		SNMP_SetVarbind(req, 0, alarm_oid[13], 12, SNMP_INTEGER, sizeof(int), &alarmStatus, false);
		if(SNMP_RequestData(req) == SNMP_OK){
			iprintf("Alarma creada correctamente");
		} else {
			iprintf("Error activando alarma");
		}
	} else {
		iprintf("Error creando alarma: %d", ret);
	}
	iprintf("\n\tPulsa A para salir");
	SNMP_FreeRequest(req);
	
	// Espera a que pulse A
	do {
		scanKeys();
		swiWaitForVBlank();
	} while(!(keysDown() &KEY_A));
	
	// Vuelve al menu
	cambiarEstado(estado_menu, cargarMenu, eliminarAlarma);
}
