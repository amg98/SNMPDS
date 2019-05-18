#ifndef _WIFI_H_
#define _WIFI_H_

// Includes
#include <nds.h>
#include <netinet/in.h>

// Defines
#define PORT_SNMP 161			// Puerto para paquetes SNMP
#define PORT_TRAP 162			// Puerto para TRAPS
#define PORT_TFTP 69			// Puerto para TFTP
#define TX_BUFFER_SIZE 2048		// Tamaño del buffer de envio
#define RX_BUFFER_SIZE 4092		// Tamaño del buffer de recepcion
#define TX_TIMEOUT_SECS 6		// Timeout (segundos)
#define TX_TIMEOUT_USECS 0		// Timeout (microsegundos)
#define TFTP_TIMEOUT_SECS 2		// Timeout para transmisiones TFTP (segundos)
#define TFTP_TIMEOUT_USECS 0	// Timeout para transmisiones TFTP (microsegundos)
#define AP_SHOWN 5				// AP a mostrar en la pantalla

// Variables externas
extern int socket_snmp;
extern int socket_trap;
extern int socket_tftp;
extern struct sockaddr_in server_snmp;
extern struct sockaddr_in server_trap;
extern struct sockaddr_in server_tftp;
extern u8 tx_buffer[TX_BUFFER_SIZE];
extern u8 rx_buffer[RX_BUFFER_SIZE];
extern struct timeval wifi_timeout;
extern struct timeval wifi_nowait;
extern struct timeval tftp_timeout;

// Cabeceras de funciones
void iniciarWifi();
void apagarWifi();
void cargarEscanearAP();
void estado_escanearAP();
void eliminarEscanearAP();
void estado_introducir_clave();
void estado_conectando();

#endif
