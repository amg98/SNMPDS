// Includes C
#include <stdio.h>

// Includes propios
#include "common.h"
#include "systeminfo.h"
#include "graphics.h"
#include "menu.h"
#include "config.h"
#include "snmp.h"

// Variables locales
static const char *systeminfo_fields[7] = {
	"sysDescr\t\t  ",
	"sysObjectID\t  ",
	"sysUpTime\t  ",
	"sysContact\t  ",
	"sysName\t\t  ",
	"sysLocation\t  ",
	"sysServices\t  "
};
static u16 systeminfo_oid[7][9] = {
	{1, 3, 6, 1, 2, 1, 1, 1, 0},
	{1, 3, 6, 1, 2, 1, 1, 2, 0},
	{1, 3, 6, 1, 2, 1, 1, 3, 0},
	{1, 3, 6, 1, 2, 1, 1, 4, 0},
	{1, 3, 6, 1, 2, 1, 1, 5, 0},
	{1, 3, 6, 1, 2, 1, 1, 6, 0},
	{1, 3, 6, 1, 2, 1, 1, 7, 0}
};
static u8 systeminfo_type[7] = {
	SNMP_OCTET_STRING, SNMP_OID, SNMP_TIMETICKS, SNMP_OCTET_STRING, SNMP_OCTET_STRING, SNMP_OCTET_STRING, SNMP_INTEGER
};
static sprite *pencil[3];
static sprite *slide[2];
static char *slideStrings[2];
static u16 slideCounter[2];

// Carga la pantalla de informacion del agente
void cargarSystemInfo(){
	
	// Variables
	u8 i;
	
	// Carga las imagenes necesarias
	loadBitmap8("proyectoGestion/images/systeminfo", 0);
	loadBitmap8("proyectoGestion/images/systemtable", 1);
	
	// Carga los sprites
	pencil[0] = loadSprite("proyectoGestion/sprites/pencil.img", 1, 0, SpriteSize_16x16);
	pencil[1] = cloneSprite(pencil[0], 1);
	pencil[2] = cloneSprite(pencil[0], 2);
	for(i = 0; i < 3; i++){
		pencil[i]->y = 12 + 24 * 3 + i * 24;
		updateSprite(pencil[i]);
	}
	slide[0] = loadSprite("proyectoGestion/sprites/arrow.img", 1, 3, SpriteSize_16x16);
	slide[1] = cloneSprite(slide[0], 4);
	slide[0]->y = 12;
	slide[1]->y = 12 + 24;
	updateSprite(slide[0]);
	updateSprite(slide[1]);
	swiWaitForVBlank();
	oamUpdate(&oamSub);			// Para que aparezcan los sprites en el fade in
	
	// Crea los strings deslizables
	slideStrings[0] = (char*) allocate (256 * sizeof(char));
	slideStrings[1] = (char*) allocate (256 * sizeof(char));
	slideCounter[0] = 0;
	slideCounter[1] = 0;
}

// Eliminar la pantalla de informacion del agente
void eliminarSystemInfo(){
	
	// Esconde los fondos
	bgHide(bitmapLayerTop);
	bgHide(bitmapLayerBottom);
	
	// Elimina los sprites
	u8 i;
	for(i = 0; i < 3; i++){
		freeSprite(pencil[i]);
	}
	freeSprite(slide[0]);
	freeSprite(slide[1]);
	
	// Libera los strings deslizables
	free(slideStrings[0]);
	free(slideStrings[1]);
	
	// Limpia la consola de texto
	consoleClear();
}

// Procesa el estado de informacion del agente (cuando se realiza el GET)
void estado_systeminfo_get(){
	
	// Pide la configuracion del agente por SNMP
	u8 i;
	SNMP_Request *req = SNMP_CreateRequest(SNMP_GETREQUEST, 7);
	for(i = 0; i < 7; i++){
		SNMP_SetVarbind(req, i, systeminfo_oid[i], 9, systeminfo_type[i], 0, NULL, 1);		// Datos liberables (es un GET)
	}
	int ret;
	if((ret = SNMP_RequestData(req)) != SNMP_OK){								// Vuelve al menu si no recibimos datos
		SNMP_FreeRequest(req);
		cambiarEstado(estado_menu, cargarMenu, eliminarSystemInfo);
		return;
	}
	
	// Escribe los campos de la tabla
	consoleSelect(&bottomConsole);
	for(i = 0; i < 7; i++){
		iprintf("\n\n\t%s", systeminfo_fields[i]);
		switch(systeminfo_type[i]){
			case SNMP_OCTET_STRING:
				if(i == 0) strcpy(slideStrings[0], (char*)req->field[i].data);			// Solo la descripcion
				iprintf("%.12s", (char*)req->field[i].data);
				break;
			case SNMP_OID:																// Sabemos que solo hay 1
			{
				u8 j;
				u16 written = 0;
				for(j = 0; j < req->field[i].oid_n_data; j++){
					written += sprintf(&slideStrings[1][written], "%d%c", ((u16*)req->field[i].data)[j], ((j + 1) < req->field[i].oid_n_data) ? '.' : ' ');
				}
				iprintf("%.12s", slideStrings[1]);
			}
				break;
			case SNMP_TIMETICKS:
			{
				int ticks = *(int*)req->field[i].data;
				int hours = ticks / 360000;
				int minutes = ticks / 6000 - hours * 60;
				int secs = ticks / 100 - hours * 3600 - minutes * 60;
				iprintf("%dh %dmin %ds", hours, minutes, secs);
			}
				break;
			case SNMP_INTEGER:
				iprintf("%d", *(int*)req->field[i].data);
				break;
		}
		iprintf("\n");
	}
	
	// Libera la peticion realizada
	SNMP_FreeRequest(req);
	
	// Ahora podemos realizar acciones
	estado = estado_systeminfo;
}

// Procesa el estado de informacion del agente
void estado_systeminfo(){
	
	// Si tocas las flechas de deslizamiento...
	if(spriteTouched(slide[0], 16, 16)){
		slideCounter[0] ++;
		if(slideCounter[0] > strlen(slideStrings[0]) - 12) slideCounter[0] = 0;
		iprintf("\x1b[2;17H%.12s", &slideStrings[0][slideCounter[0]]);
	} else if(spriteTouched(slide[1], 16, 16)){
		slideCounter[1] ++;
		if(slideCounter[1] > strlen(slideStrings[1]) - 12) slideCounter[1] = 0;
		iprintf("\x1b[5;17H%.12s", &slideStrings[1][slideCounter[1]]);
	}
	
	// Si tocas los iconos de edicion...
	u8 i;
	for(i = 0; i < 3; i++){
		if(spriteTouched(pencil[i], 16, 16)){
			
			// Prepara la pantalla de introducir datos
			cargarIntroducirDatos(5);
			
			// Crea la peticion SNMP
			char text[32];				// Texto que pedimos a usuario
			SNMP_Request *req = SNMP_CreateRequest(SNMP_SETREQUEST, 1);
			
			// Segun el sprite que has tocado
			switch(i){
				case 0:
					iprintf("Contacto:\n\t ");
					break;
				case 1:
					iprintf("Nombre del agente:\n\t ");
					break;
				case 2:
					iprintf("Localizacion:\n\t ");
					break;
			}
			
			// Manda y elimina la peticion SNMP
			scanf("%32s", text);			// Pide el texto al usuario
			SNMP_SetVarbind(req, 0, systeminfo_oid[i + 3], 9, SNMP_OCTET_STRING, strlen(text), text, 0);
			SNMP_RequestData(req);			// Nos da igual si salio bien o mal
			SNMP_FreeRequest(req);			// Elimina la peticion
			
			// Vuelve al estado original
			eliminarIntroducirDatos(5);
			estado_systeminfo_get();
			return;
		}
	}
	
	// Si pulsas B, vuelve al menu
	if(keyDown &KEY_B){
		cambiarEstado(estado_menu, cargarMenu, eliminarSystemInfo);
	}
}
