// Includes C
#include <stdio.h>

// Includes NDS
#include <nds.h>
#include <fat.h>

// Includes propios
#include "common.h"
#include "wifi.h"
#include "snmp.h"
#include "graphics.h"
#include "menu.h"

// Funcion main()
// Bloque principal del programa
int main(void) {
	
	// Inicializa FAT
	consoleDemoInit();
	iprintf("\n Inicializando FAT...\n\n");
	if(!fatInitDefault()){
		iprintf(" Error: tu flashcard no soporta  FAT\n Parchea el ejecutable con un\n parche DLDI y vuelve a\n intentarlo\n\n Apaga la consola");
		while(1) swiWaitForVBlank();
	}
	chdir("fat:/");
	consoleClear();
	
	// Inicializa los graficos
	iniciarGfx();
	
	// Inicializa el wifi
	iniciarWifi();
	
	// Comienza mostrando la pantalla principal
	setBrightness(3, -16);
	cambiarEstado(estado_inicio, cargarInicio, NULL);
	
	// Variables locales de recepcion de traps
	fd_set readFDS;
	int sin_size;
	int status;
	struct sockaddr_in from;
 
	// Bucle principal
	while(1) {
		
		// Procesa el estado actual
		estado();
		
		// Lee el buffer de recepcion por si llega un TRAP
		if(socket_trap != -1 && estado == estado_menu){
			FD_ZERO(&readFDS);
			FD_SET(socket_trap, &readFDS);
			status = select(socket_trap + 1, &readFDS, NULL, NULL, &wifi_nowait);
			if((status > 0) && FD_ISSET(socket_trap, &readFDS)){
				if(recvfrom(socket_trap, rx_buffer, RX_BUFFER_SIZE, 0, (struct sockaddr*)&from, &sin_size) > 0){
					SNMP_ReadTrap();						// Lee el TRAP que ha llegado
				}
			}
		}
		
		// Espera a la sincronizacion vertical
		swiWaitForVBlank();
		
		// Actualiza la OAM
		oamUpdate(&oamMain);
		oamUpdate(&oamSub);
		
		// Lee entradas
		scanKeys();
		keyHeld = keysHeld();
		keyDown = keysDown();
		keyUp = keysUp();
		touchRead(&touchXY);
	}

	// Fin del programa
	apagarWifi();
	return 0;
}
