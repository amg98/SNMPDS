// Includes C
#include <stdio.h>

// Includes NDS
#include <nds.h>

// Includes propios
#include "interfaces.h"
#include "menu.h"
#include "common.h"
#include "graphics.h"
#include "snmp.h"
#include "config.h"

// Variables locales
static const char *interfaces_fields[7] = {
	"ifDescr\t\t  ",
	"ifMtu\t\t\t  ",
	"ifSpeed\t\t  ",
	"ifPhysAddress ",
	"ifOperStatus  ",
	"ifInOctets\t  ",
	"ifOutOctets\t  "
};
static u16 interfaces_oid[8][11] = {
	{1, 3, 6, 1, 2, 1, 2, 2, 1, 2, 1},
	{1, 3, 6, 1, 2, 1, 2, 2, 1, 4, 1},
	{1, 3, 6, 1, 2, 1, 2, 2, 1, 5, 1},
	{1, 3, 6, 1, 2, 1, 2, 2, 1, 6, 1},
	{1, 3, 6, 1, 2, 1, 2, 2, 1, 8, 1},
	{1, 3, 6, 1, 2, 1, 2, 2, 1, 10, 1},
	{1, 3, 6, 1, 2, 1, 2, 2, 1, 16, 1},
	{1, 3, 6, 1, 2, 1, 2, 2, 1, 7, 1},		// ifAdminStatus
};
static u16 ninterfaces_oid[9] = {
	1, 3, 6, 1, 2, 1, 2, 1, 0
};
static u8 interfaces_type[7] = {
	SNMP_OCTET_STRING, SNMP_INTEGER, SNMP_GAUGE, SNMP_OCTET_STRING, SNMP_INTEGER, SNMP_COUNTER, SNMP_COUNTER
};
static char *interfaces_operstatus[6] = {
	"up", "down", "testing", "unknown", "dormant", "notPresent"
};
static int nInterfaces = 0;			// Numero de interfaces
static int selectedInterface = 1;	// Interfaz seleccionada
static int currentInterface = 0;	// Numero de interfaz actual (no tiene por que ser selectedInterface)
static u8 readSide = 1;				// Lado de lectura (0 = anterior, 1 = siguiente, 2 = actual)
static sprite *pencil;
static sprite *slide;
static char *slideString;
static u16 slideCounter;

// Escribe la tabla de interfaces
void interfaces_printTable(){
	
	// Obten el numero de interfaces (si no es 0)
	SNMP_Request *req = NULL;
	if(nInterfaces == 0){
		req = SNMP_CreateRequest(SNMP_GETREQUEST, 1);
		SNMP_SetVarbind(req, 0, ninterfaces_oid, 9, SNMP_INTEGER, 0, NULL, true);
		if(SNMP_RequestData(req) != SNMP_OK){
			SNMP_FreeRequest(req);
			cambiarEstado(estado_menu, cargarMenu, eliminarInterfaces);
			return;
		}
		nInterfaces = *(int*)req->field[0].data;
		SNMP_FreeRequest(req);
	}
	
	// Obten los datos de la interfaz seleccionada
	req = SNMP_CreateRequest(SNMP_GETREQUEST, 7);
	u8 i;
	int ret;
	do{
		if(readSide == 0){
			currentInterface --;
		} else if(readSide == 1){
			currentInterface ++;
		}
		for(i = 0; i < 7; i++){
			interfaces_oid[i][10] = currentInterface;			// Selecciona la interfaz a pedir
			SNMP_SetVarbind(req, i, interfaces_oid[i], 11, interfaces_type[i], 0, NULL, true);
		}
		ret = SNMP_RequestData(req);
		if((ret != SNMP_OK && ret != SNMP_ERROR_NOSUCHNAME) || (ret == SNMP_ERROR_NOSUCHNAME && readSide == 2)){
			SNMP_FreeRequest(req);
			if(readSide == 0){
				currentInterface ++;
				selectedInterface ++;
			} else if(readSide == 1){
				currentInterface --;
				selectedInterface --;
			}
			//cambiarEstado(estado_menu, cargarMenu, eliminarInterfaces);
			return;
		}
	} while(ret == SNMP_ERROR_NOSUCHNAME);
	
	// Muestra los datos obtenidos en la tabla
	consoleClear();
	iprintf("%d %d %d", currentInterface, selectedInterface, nInterfaces);
	for(i = 0; i < 7; i++){
		iprintf("\n\n\t%s", interfaces_fields[i]);
		switch(interfaces_type[i]){
			case SNMP_OCTET_STRING:
				if(i == 3  && req->field[i].size > 0){				// Si es el campo de la direccion MAC (y tiene direccion MAC)
					u8 *mac = (u8*)req->field[i].data;
					iprintf("%02X%02X%02X%02X%02X%02X", mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
				} else {
					if(i == 0) strcpy(slideString, (char*)req->field[i].data);			// Solo la descripcion
					iprintf("%.12s", (char*)req->field[i].data);
				}
				break;
			case SNMP_INTEGER:
			case SNMP_GAUGE:
			case SNMP_COUNTER:
			{
				int val = *(int*)req->field[i].data;
				if(i == 4 && val >= 1 && val <= 6){					// Si es el campo operStatus y es un estado que conocemos...
					iprintf("%s", interfaces_operstatus[val - 1]);
				} else {
					iprintf("%d", val);
				}
			}
				break;
		}
		iprintf("\n");
	}
	
	// Libera los datos
	SNMP_FreeRequest(req);
	
	// Pasa al siguiente estado
	estado = estado_interfaces;
}

// Carga la pantalla de interfaces
void cargarInterfaces(){
	
	// Carga los fondos necesarios
	loadBitmap8("proyectoGestion/images/interfaces", 0);
	loadBitmap8("proyectoGestion/images/systemtable", 1);
	
	// Carga los sprites
	pencil = loadSprite("proyectoGestion/sprites/pencil.img", 1, 0, SpriteSize_16x16);
	pencil->y = 12 + 24 * 4;
	updateSprite(pencil);
	slide = loadSprite("proyectoGestion/sprites/arrow.img", 1, 1, SpriteSize_16x16);
	slide->y = 12;
	updateSprite(slide);
	swiWaitForVBlank();
	oamUpdate(&oamSub);			// Para que aparezcan los sprites en el fade in
	
	// Crea los strings deslizables
	slideString = (char*) allocate(256 * sizeof(char));
	slideCounter = 0;
	
	// Selecciona la consola de texto
	consoleSelect(&bottomConsole);
	
	// Inicializa variables
	selectedInterface = 1;
	nInterfaces = 0;
	currentInterface = 0;
	readSide = 1;
}

// Estado "visualizacion de interfaces"
void estado_interfaces(){
	
	// Si tocas la flecha de deslizamiento...
	if(spriteTouched(slide, 16, 16)){
		slideCounter ++;
		if(slideCounter > strlen(slideString) - 12) slideCounter = 0;
		iprintf("\x1b[2;17H%.12s", &slideString[slideCounter]);
	}
	
	// Si pulsas L o R, cambia la interfaz mostrada
	if((keyDown &KEY_L) && (selectedInterface > 1)){
		selectedInterface --;
		readSide = 0;
		interfaces_printTable();
	} else if((keyDown &KEY_R)  && (selectedInterface < nInterfaces)){
		selectedInterface ++;
		readSide = 1;
		interfaces_printTable();
	}
	
	// Si tocas el icono de edicion...
	else if(spriteTouched(pencil, 16, 16)){
		
		// Prepara la pantalla de introducir datos
		cargarIntroducirDatos(2);
			
		// Pide el estado del enlace al usuario
		int state;
		iprintf("Estado del enlace(1-3):\n\tup(1), down(2), testing(3)\n\t ");
		scanf("%d", &state);
			
		// Manda y elimina la peticion SNMP
		SNMP_Request *req = SNMP_CreateRequest(SNMP_SETREQUEST, 1);
		interfaces_oid[7][10] = currentInterface;
		SNMP_SetVarbind(req, 0, interfaces_oid[7], 11, SNMP_INTEGER, sizeof(int), &state, 0);
		SNMP_RequestData(req);			// Nos da igual si salio bien o mal
		SNMP_FreeRequest(req);			// Elimina la peticion
			
		// Vuelve al estado original
		readSide = 2;
		eliminarIntroducirDatos(2);
		interfaces_printTable();
	}
	
	// Si pulsas B, vuelve al menu
	else if(keyDown &KEY_B){
		cambiarEstado(estado_menu, cargarMenu, eliminarInterfaces);
	}
}

// Elimina la pantalla de interfaces
void eliminarInterfaces(){
	
	// Esconde los fondos
	bgHide(bitmapLayerTop);
	bgHide(bitmapLayerBottom);
	
	// Elimina los sprites
	freeSprite(pencil);
	freeSprite(slide);
	
	// Elimina los strings deslizables
	free(slideString);
	
	// Limpia la consola de texto
	consoleClear();
}
