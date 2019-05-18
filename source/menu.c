// Includes C
#include <stdio.h>

// Includes NDS
#include <nds.h>

// Includes propios
#include "common.h"
#include "menu.h"
#include "snmp.h"
#include "wifi.h"
#include "graphics.h"
#include "systeminfo.h"
#include "config.h"
#include "view.h"
#include "alarm.h"

// Variables ocales
static u16 phase = 0;			// Fase del seno, para el efecto onda
static s16 sinus = 0;			// Valor del seno, para el efecto onda
static sprite *icon[4];			// Iconos del menu
static const char *icon_path[4] = {
	"proyectoGestion/sprites/switch.img",
	"proyectoGestion/sprites/options.img",
	"proyectoGestion/sprites/view.img",
	"proyectoGestion/sprites/action.img"
};

// Efecto para la pantalla inferior
static void waveEffect(){
	if(VLINE < 192){
		sinus = sinLerp(degreesToAngle(phase + (VLINE * 3))) >> 2;
		REG_BG3X = sinus;
		REG_BG3X_SUB = sinus;
	}
}

// Carga el inicio de la aplicacion
void cargarInicio(){
	
	// Carga las imagenes
	loadBitmap8("proyectoGestion/images/snmpds", 0);
	loadBitmap8("proyectoGestion/images/toca", 1);
	
	// Inicializa el efecto onda
	irqSet(IRQ_HBLANK, waveEffect);
	irqEnable(IRQ_HBLANK);
	phase = 0;
	sinus = 0;
}

// Elimina el inicio de la aplicacion
void eliminarInicio(){
	
	// Esconde los fondos
	bgHide(bitmapLayerTop);
	bgHide(bitmapLayerBottom);
	
	// Desactiva la interrupcion por HBLANK
	irqDisable(IRQ_HBLANK);
}

// Procesa el inicio de la aplicacion
void estado_inicio(){
	
	// Si tocas la pantalla, pasa al menu siguiente
	if(keyDown &KEY_TOUCH){
		cambiarEstado(estado_escanearAP, cargarEscanearAP, eliminarInicio);
	}
	
	// Anima la onda
	phase ++;
}

// Carga el menu principal
void cargarMenu(){
	
	// Carga las imagenes necesarias
	loadBitmap8("proyectoGestion/images/snmpds", 0);
	loadBitmap8("proyectoGestion/images/menu", 1);
	
	// Carga los sprites y posicionalos
	u8 i;
	for(i = 0; i < 4; i++){
		icon[i] = loadSprite(icon_path[i], 1, i, SpriteSize_64x64);
		icon[i]->x = 32 + ((i&1) << 7);
		icon[i]->y = 16 + ((i&2) >> 1) * 88;
		updateSprite(icon[i]);
	}
	swiWaitForVBlank();
	oamUpdate(&oamSub);				// Muestra los sprites en pantalla
	
	// Selecciona la consola de la pantalla inferior
	consoleSelect(&bottomConsole);
	
	// Obten el tamaño del archivo de traps
	int trapSize = 0;
	FILE *f = fopen(trapFile, "rb");
	if(f != NULL){
		fseek(f, 0, SEEK_END);
		trapSize = ftell(f);
		fclose(f);
	}
	iprintf("\t Traps: %d bytes", trapSize);
}

// Elimina el menu principal
void eliminarMenu(){
	
	// Esconde los fondos
	bgHide(bitmapLayerTop);
	bgHide(bitmapLayerBottom);
	
	// Elimina los sprites
	u8 i;
	for(i = 0; i < 4; i++){
		freeSprite(icon[i]);
	}
	
	// Limpia la consola de texto
	consoleClear();
}

// Menu principal
void estado_menu(){
	
	// Ve a cualquier opcion del menu
	if(spriteTouched(icon[0], 64, 64)){
		cambiarEstado(estado_systeminfo_get, cargarSystemInfo, eliminarMenu);	// Menu de informacion del agente (switch)
	} else if(spriteTouched(icon[1], 64, 64)){
		cambiarEstado(estado_config, cargarConfig, eliminarMenu);				// Menu de configuracion de agente (Nintendo DS)
	} else if(spriteTouched(icon[2], 64, 64)){
		cambiarEstado(estado_view, cargarView, eliminarMenu);					// Menu de visualizacion
	} else if(spriteTouched(icon[3], 64, 64)){
		cambiarEstado(estado_alarma, cargarAlarma, eliminarMenu);				// Menu de alarmas
	}
}
