// Includes C
#include <stdio.h>

// Includes propios
#include "common.h"
#include "config.h"
#include "graphics.h"
#include "menu.h"
#include "wifi.h"

// Variables globales
char ip_gestor[16] = "0.0.0.0";
char ip_agente[16] = "192.168.100.7";
char ip_tftp[16] = "192.168.100.21";
char community[32] = "public";
u16 puerto_snmp = PORT_SNMP;
u16 puerto_trap = PORT_TRAP;
u8 mac_gestor[6];

// Variables locales
static const char *config_fields[7] = {
	"IP Gestor",
	"MAC Gestor",
	"Puerto SNMP",
	"Puerto TRAP",
	"IP Agente",
	"Servidor TFTP",
	"Comunidad"
};
static sprite *pencil[5];

// Escribe los campos de la tabla
static void printTable(){
	iprintf("\n\n  %s\t\t %s\n", config_fields[0], ip_gestor);
	iprintf("\n\n  %s\t  %02X%02X%02X%02X%02X%02X\n", config_fields[1], mac_gestor[0], mac_gestor[1], mac_gestor[2], mac_gestor[3], mac_gestor[4], mac_gestor[5]);
	iprintf("\n\n  %s\t  %d\n", config_fields[2], puerto_snmp);
	iprintf("\n\n  %s\t  %d\n", config_fields[3], puerto_trap);
	iprintf("\n\n  %s\t\t %s\n", config_fields[4], ip_agente);
	iprintf("\n\n  %s %s\n", config_fields[5], ip_tftp);
	iprintf("\n\n  %s\t\t %s\n", config_fields[6], community);
}

// Carga la pantalla de configuracion del gestor
void cargarConfig(){
	
	// Variables
	u8 i;
	
	// Carga las imagenes necesarias
	loadBitmap8("proyectoGestion/images/config", 0);
	loadBitmap8("proyectoGestion/images/systemtable", 1);
	
	// Carga los sprites
	pencil[0] = loadSprite("proyectoGestion/sprites/pencil.img", 1, 0, SpriteSize_16x16);
	pencil[1] = cloneSprite(pencil[0], 1);
	pencil[2] = cloneSprite(pencil[0], 2);
	pencil[3] = cloneSprite(pencil[0], 3);
	pencil[4] = cloneSprite(pencil[0], 4);
	for(i = 0; i < 5; i++){
		pencil[i]->y = 12 + 24 * 2 + i * 24;
		updateSprite(pencil[i]);
	}
	swiWaitForVBlank();
	oamUpdate(&oamSub);			// Para que aparezcan los sprites en el fade in
	
	// Escribe los campos de la tabla
	consoleSelect(&bottomConsole);
	printTable();
}

// Eliminar la pantalla de configuracion del gestor
void eliminarConfig(){
	
	// Esconde los fondos
	bgHide(bitmapLayerTop);
	bgHide(bitmapLayerBottom);
	
	// Elimina los sprites
	u8 i;
	for(i = 0; i < 5; i++){
		freeSprite(pencil[i]);
	}
	
	// Limpia la consola de texto
	consoleClear();
}

// Procesa el estado de configuracion del gestor
void estado_config(){
	
	// Si tocas algun icono de edicion
	u8 i;
	in_addr_t addr;
	int value;
	char ip[16];
	for(i = 0; i < 5; i++){
		if(spriteTouched(pencil[i], 16, 16)){
			
			// Prepara la pantalla de introducir datos
			cargarIntroducirDatos(5);
			
			// Segun el sprite que has tocado
			switch(i){
				case 0:
					iprintf("Puerto SNMP:\n\t ");
					scanf("%d", &value);
					if(value > 0 && value <= 0xFFFF){					// Tiene que estar dentro del rango
						server_snmp.sin_port = htons(value);
						puerto_snmp = value;
					}
					break;
				case 1:
					iprintf("Puerto de los TRAP:\n\t ");
					scanf("%d", &value);
					if(value > 0 && value <= 0xFFFF){					// Tiene que estar dentro del rango
						server_trap.sin_port = htons(value);
						puerto_trap = value;
					}
					break;
				case 2:
					iprintf("IP del agente:\n\t ");
					scanf("%15s", ip);
					addr = inet_addr(ip);
					if(addr != -1){										// Si es una direccion IP valida
						server_snmp.sin_addr.s_addr = addr;
						strcpy(ip_agente, ip);
					}
					break;
				case 3:
					iprintf("Servidor TFTP:\n\t ");						// Servidor TFTP
					scanf("%15s", ip);
					addr = inet_addr(ip);
					if(addr != -1){
						server_tftp.sin_addr.s_addr = addr;
						strcpy(ip_tftp, ip);
					}
					break;
				case 4:
					iprintf("Comunidad:\n\t ");							// Comunidad
					scanf("%32s", community);
					break;
			}
			
			// Vuelve al estado original
			eliminarIntroducirDatos(5);
			printTable();
		}
	}
	
	// Si pulsas B, vuelve al menu
	if(keyDown &KEY_B){
		cambiarEstado(estado_menu, cargarMenu, eliminarConfig);
	}
}

// Prepara la escena de introducir datos
void cargarIntroducirDatos(u8 nsprites){
	
	// Fade out
	s8 i;
	for(i = 0; i >= -16; i--){
		setBrightness(2, i);
		swiWaitForVBlank();
	}
	
	// Quita los sprites
	for(i = 0; i < nsprites; i++){
		oamSetHidden(&oamSub, i, true);
	}
	
	// Carga el fondo
	loadBitmap8("proyectoGestion/images/wifi_sub", 1);
	consoleClear();
	iprintf("\n\n\n\t");		// Posiciona el cursor de la consola
	
	// Fade in
	for(i = -16; i <= 0; i++){
		swiWaitForVBlank();
		oamUpdate(&oamSub);
		setBrightness(2, i);
	}
}

// Elimina la escena de introducir datos
void eliminarIntroducirDatos(u8 nsprites){
	
	// Fade out
	s8 i;
	for(i = 0; i >= -16; i--){
		setBrightness(2, i);
		swiWaitForVBlank();
	}
	
	// Vuelve a poner los sprites
	for(i = 0; i < nsprites; i++){
		oamSetHidden(&oamSub, i, false);
	}
	
	// Carga el fondo
	loadBitmap8("proyectoGestion/images/systemtable", 1);
	consoleClear();
	
	// Fade in
	for(i = -16; i <= 0; i++){
		swiWaitForVBlank();
		oamUpdate(&oamSub);
		setBrightness(2, i);
	}
}
