#ifndef _CONFIG_H_
#define _CONFIG_H_

// Includes NDS
#include <nds.h>

// Variables externas
extern char ip_gestor[16];
extern char ip_agente[16];
extern char ip_tftp[16];
extern char community[32];
extern u16 puerto_snmp;
extern u16 puerto_trap;
extern u8 mac_gestor[6];

// Cabeceras de funciones
void cargarConfig();
void eliminarConfig();
void estado_config();
void cargarIntroducirDatos(u8 nsprites);
void eliminarIntroducirDatos(u8 nsprites);

#endif
