#ifndef _PROTOCOL_H_
#define _PROTOCOL_H_

#define START_MARKER 0x7E

#define TYPE_ACK 0x00             // ack 00000
#define TYPE_NACK 0x01            // nack 00001
#define TYPE_LIST 0x0A            // lista 01010
#define TYPE_DOWNLOAD 0x0B        // baixar 01011
#define TYPE_SHOW 0x10            // mostra na tela 10000
#define TYPE_FILE_DESCRIPTOR 0x11 // descritor arquivo 10001
#define TYPE_DATA 0x12            // dados 10010
#define TYPE_END_TX 0x1E          // fim tx 11110
#define TYPE_ERROR 0x1F           // erro 11111

#define ERROR_ACCESS_DENIED 0x01
#define ERROR_NOT_FOUND 0x02
#define ERROR_DISK_FULL 0x03

#endif