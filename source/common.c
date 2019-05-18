// Includes C
#include <stdio.h>

// Includes propios
#include "common.h"

// Definicion de variables globales
estado_callback estado;
touchPosition touchXY;
u32 keyDown, keyHeld, keyUp;

// Error al reservar memoria
void *allocate(u32 size){
	
	// Reserva memoria
	void *data = calloc (1, size);
	
	// Si no ha sido posible...
	if(data == NULL){
		consoleDemoInit();
		consoleClear();
		setBrightness(3, 0);
		iprintf("\n Error reservando %ld bytes\n Apague la consola\n", size);
		while(1){
			swiWaitForVBlank();
		}
	}
	
	// Devuelve los datos
	return data;
}

// Error al encontrar un archivo
void fileNotFoundError(const char *filename){
	consoleDemoInit();
	consoleClear();
	setBrightness(3, 0);
	iprintf("\n Archivo \"%s\" no encontrado\n Apague la consola\n", filename);
	while(1){
		swiWaitForVBlank();
	}
}

// Cambia el estado de la aplicacion
void cambiarEstado(estado_callback nuevo_estado, estado_callback carga, estado_callback elimina){
	
	// Si tenemos funcion de eliminar...
	if(elimina){
		
		// Realiza un fade out
		fadeOut(3);
		
		// Elimina los recursos de la pantalla anterior
		elimina();
	}
	
	// Carga la siguiente pantalla
	if(carga) carga();
	
	// Establece el nuevo estado
	estado = nuevo_estado;
	
	// Realiza un fade in
	fadeIn(3);
}

// Realiza un efecto de fade in
void fadeIn(u8 screen){
	s8 i;
	for(i = -16; i <= 0; i++){
		setBrightness(screen, i);
		swiWaitForVBlank();
	}
}

// Realiza un efecto de fade out
void fadeOut(u8 screen){
	s8 i;
	for(i = 0; i >= -16; i--){
		setBrightness(screen, i);
		swiWaitForVBlank();
	}
}
