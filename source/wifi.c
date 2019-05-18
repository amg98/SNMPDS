// Includes C
#include <stdio.h>

// Includes NDS
#include <nds.h>
#include <dswifi9.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <netdb.h>

// Includes propios
#include "common.h"
#include "wifi.h"
#include "menu.h"
#include "graphics.h"
#include "config.h"

// Variables locales
static int apSeleccionado = 0;
static int apComienzo = 0;
static int apEstado = 0;
static Wifi_AccessPoint apActual;
static sprite *arrow = NULL;
static char wepkey[64];
static const char *estado_conexion[] = {
	"Desconectado",
	"Buscando AP...",
	"Autenticando...",
	"Asociando...",
	"Obteniendo direccion IP",
	"Conectado",
	"Error, pulsa A para volver",
};

// Variables globales
int socket_snmp = -1;
int socket_trap = -1;
int socket_tftp = -1;
struct sockaddr_in server_snmp;
struct sockaddr_in server_trap;
struct sockaddr_in server_tftp;
u8 tx_buffer[TX_BUFFER_SIZE];
u8 rx_buffer[RX_BUFFER_SIZE];
struct timeval wifi_timeout;
struct timeval wifi_nowait;
struct timeval tftp_timeout;

// Inicializa el wifi
void iniciarWifi(){
	
	// Inicializa el wifi
	Wifi_InitDefault(false);
	Wifi_ScanMode();
	
	// Rellena las direcciones del servidor
	server_snmp.sin_family = AF_INET;
	server_snmp.sin_port = htons(puerto_snmp);
	server_snmp.sin_addr.s_addr = inet_addr(ip_agente);
	server_trap.sin_family = AF_INET;
	server_trap.sin_port = htons(puerto_trap);
	server_trap.sin_addr.s_addr = htonl(INADDR_ANY);
	server_tftp.sin_family = AF_INET;
	server_tftp.sin_port = htons(PORT_TFTP);
	server_tftp.sin_addr.s_addr = inet_addr(ip_tftp);
	
	// Configura el timeout
	wifi_timeout.tv_sec = TX_TIMEOUT_SECS;
	wifi_timeout.tv_usec = TX_TIMEOUT_USECS;
	wifi_nowait.tv_sec = 0;
	wifi_nowait.tv_usec = 0;
	tftp_timeout.tv_sec = TFTP_TIMEOUT_SECS;
	tftp_timeout.tv_usec = TFTP_TIMEOUT_USECS;
}

// Apaga el wifi
void apagarWifi(){
	
	// Destruye los sockets
	if(socket_snmp >= 0){
		shutdown(socket_snmp, 0);
		closesocket(socket_snmp);
	}
	if(socket_trap >= 0){
		shutdown(socket_trap, 0);
		closesocket(socket_trap);
	}
	if(socket_tftp >= 0){
		shutdown(socket_tftp, 0);
		closesocket(socket_tftp);
	}
	
	// Apaga el wifi
	Wifi_DisconnectAP();
	Wifi_DisableWifi();
}

// Carga el menu de escanear AP's
void cargarEscanearAP(){
	
	// Carga las imagenes
	loadBitmap8("proyectoGestion/images/wifi", 0);
	loadBitmap8("proyectoGestion/images/wifi_sub", 1);
	
	// Carga los sprites
	arrow = loadSprite("proyectoGestion/sprites/arrow.img", 0, 0, SpriteSize_16x16);
	
	// Prepara las consolas de texto
	consoleSelect(&topConsole);
}

// Elimina el menu de escanear AP's
void eliminarEscanearAP(){
	
	// Esconde los fondos
	bgHide(bitmapLayerTop);
	bgHide(bitmapLayerBottom);
	
	// Elimina los sprites
	freeSprite(arrow);
	
	// Limpia las pantallas
	consoleSelect(&topConsole);
	consoleClear();
	consoleSelect(&bottomConsole);
	consoleClear();
}

// Escanea las AP de la zona
// Usa: apSeleccionado, apComienzo
void estado_escanearAP(){
	
	// Cuenta los AP en el area
	consoleSelect(&bottomConsole);
	consoleClear();
	int count = Wifi_GetNumAP();
	iprintf("\n\n\n\tAP encontradas: %d", count);
	consoleSelect(&topConsole);
	consoleClear();
	iprintf("\n\n");
	
	// Calcula que AP se van a mostrar
	int apFinal = apComienzo + AP_SHOWN;
	if (apFinal > count) apFinal = count;
	
	// Muestra informacion de las AP encontradas
	int i;
	for(i = apComienzo; i < apFinal; i++){
		
		// Obten cada AP
		Wifi_GetAPData(i, &apActual);
	
		// Muestra datos
		iprintf("\t%.29s\n\t Wep: %s\t\t\t  %i%%\n\n\n",
			apActual.ssid, 
			apActual.flags &WFLAG_APDATA_WEP ? "Si " : "No ",
			apActual.rssi * 100 / 0xFF);
	}
	
	// Mueve la flecha
	arrow->y = 16 + ((apSeleccionado - apComienzo) << 5);
	updateSprite(arrow);
	
	// Selecciona el AP
	if(keyDown &KEY_UP) {
		apSeleccionado --;
		if(apSeleccionado < 0) {
			apSeleccionado = 0;
		}
		if(apSeleccionado < apComienzo) apComienzo = apSeleccionado;
	} else if(keyDown &KEY_DOWN) {
		apSeleccionado ++;
		if(apSeleccionado >= count) {
			apSeleccionado = count - 1;
		}
		apComienzo = apSeleccionado - (AP_SHOWN - 1);
		if(apComienzo < 0) apComienzo = 0;
	}
	
	// Pasa al estado de introducir clave si seleccionas uno
	if(keyDown &KEY_A){
		Wifi_GetAPData(apSeleccionado, &apActual);				// Obten los datos del AP
		Wifi_GetData(WIFIGETDATA_MACADDRESS, 6, mac_gestor);	// Obten la MAC de la Nintendo DS
		Wifi_SetIP(0,0,0,0,0);									// Usa DHCP
		consoleSelect(&bottomConsole);							// Usamos la pantalla inferior para escribir
		estado = estado_introducir_clave;
	}
}

// Introduce la clave del AP e intenta una conexion
// Usa: apActual
void estado_introducir_clave(){
	
	// Definicion de variables
	int wepmode = WEPMODE_NONE;
	
	// Pregunta por la clave, si tiene
	if(apActual.flags &WFLAG_APDATA_WEP) {
		while (wepmode == WEPMODE_NONE) {
			consoleClear();
			iprintf("\n\n\n\tIntroduce la clave WEP:\n\t ");
			scanf("%13s", wepkey);
			if(strlen(wepkey) == 13) {
				wepmode = WEPMODE_128BIT;
			} else if (strlen(wepkey) == 5) {
				wepmode = WEPMODE_40BIT;
			}
		}
	}
	
	// Conectate al AP
	Wifi_ConnectAP(&apActual, wepmode, 0, (u8*)wepkey);
	apEstado = ASSOCSTATUS_DISCONNECTED;
	estado = estado_conectando;
}

// Conectate al AP
// Usa: apEstado
void estado_conectando(){
	
	// Muestra el estado de la conexion
	consoleClear();
	apEstado = Wifi_AssocStatus();
	iprintf("\n\n\n\tConectando a %s ...\n\t%s\n", apActual.ssid, estado_conexion[apEstado]);
	
	// Segun el estado de la conexion...
	if(apEstado == ASSOCSTATUS_ASSOCIATED){
		
		// Crea os sockets
		socket_snmp = socket(AF_INET, SOCK_DGRAM, 0);
		socket_trap = socket(AF_INET, SOCK_DGRAM, 0);
		socket_tftp = socket(AF_INET, SOCK_DGRAM, 0);
		if(socket_snmp < 0 || socket_trap < 0 || socket_tftp < 0){
			iprintf("\n\n\n\tError creando sockets\n\tApague la consola");
			while(1) swiWaitForVBlank();
		}
		
		// Pon el socket de los TRAP en el puerto correspondiente
		bind(socket_trap, (struct sockaddr*)&server_trap, sizeof(server_trap));
		
		// Guarda la IP de la consola
		u32 ip = Wifi_GetIP();
		sprintf(ip_gestor, "%u.%u.%u.%u", (u8)(ip &0xFF), (u8)((ip >> 8) &0xFF), (u8)((ip >> 16) &0xFF), (u8)((ip >> 24) &0xFF));
		
		// Pasa al menu de la aplicacion
		cambiarEstado(estado_menu, cargarMenu, eliminarEscanearAP);
		
	} else if(apEstado == ASSOCSTATUS_CANNOTCONNECT){
		if(keyDown &KEY_A){
			Wifi_ScanMode();
			apSeleccionado = 0;
			apComienzo = 0;
			estado = estado_escanearAP;
		}
	}
}
