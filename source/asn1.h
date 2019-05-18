#ifndef _ASN1_H_
#define _ASN1_H_

// Includes
#include <nds.h>

// Defines
#define ASN1_SEQUENCE 			16
#define ASN1_CONTEXT_TAG 		(1 << 7)
#define ASN1_APPLICATION_TAG	(1 << 6)
#define ASN1_PRIVATE_TAG		((1 << 7) | (1 << 6))
#define asn1_sequence_length(x) (2 + (((x) > 127) << 1))
#define asn1_rx_uinteger(type) ((s32)asn1_rx_integer(type))

// Cabeceras de funciones
void asn1_tx_length(u16 len);
void asn1_tx_integer(u8 type, s32 data);
void asn1_tx_octetstring(u8 type, char *str);
void asn1_tx_sequence(u8 type, u16 len);
void asn1_tx_contentend();
void asn1_tx_null();
void asn1_tx_oid(u16 *oid, u8 n, u8 size);
u8 asn1_oid_size(u16 *oid, u8 n);
u16 asn1_rx_length();
s32 asn1_rx_integer(u8 type);
char *asn1_rx_octetstring(u8 type, u16 *size);
u16 *asn1_rx_oid(u16 *size, u8 skip);
void asn1_print_oid(FILE *f, u16 *oid, u8 n_oid, char endchar);

#endif
