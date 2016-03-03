#include <stdio.h>

// Ein n�tzliches Makro
// Makro f�r die Minimumbildung, Nutzung im Programm: y=MIN(x1,x2);
#ifndef MIN
#define MIN(X,Y) ((X) < (Y) ? (X) : (Y))
#endif

#ifndef CLEAR
#define CLEAR(reg, bit) (reg&=~(1<<bit))
#endif

#ifndef SET
#define SET(reg, bit) (reg|= (1<<bit))
#endif

#ifndef VIEW
#define VIEW(reg, bit) (reg&  (1<<bit))
#endif
// Deklarationen f�r Schritt 1
typedef enum {NOT_ATTACHED, ATTACHED, DEFAULT, ADDRESSED, CONFIGURED} usb_devstate_t;
typedef enum {FULL, LOW} usb_speed_t;

typedef struct
{
 usb_devstate_t 	state;		// Ger�tezustand
 usb_speed_t		speed;		// Attach-Geschwindigkeit
 uint8_t 			address;	// Ger�teadresse
} usb_device_t;


// Die in usb.c definierte Variable �berall sichtbar machen
extern usb_device_t usb_device;


// Deklarationen f�r Schritt 3
// Zust�nde eines Endpunkts
typedef enum {HALTED, SETUP, DATA_IN, DATA_OUT, STATUS_R, STATUS_W} usb_ep_state_t;


// Verwaltung eines Endpunkts
typedef struct
{
  usb_ep_state_t state;	// Zustand des Endpunkts
  uint16_t     size;	// L�nge des EP (hier von 8 bis 256)
  uint8_t      *p;		// Zeiger auf zu sendende Daten (IN) oder auf Empfangsbuffer (OUT)
  uint16_t	   rem;	    // noch zu sendende Datenmenge in Bytes
} usb_endpoint_t;


// Deklarationen f�r Schritt 4
// Aufbau eines 8 Byte Standardrequests (ausser get/set descriptor)
// F�r get/set descriptor zerf�llt das Feld wValue des Standards in die beiden Teile index und type
typedef struct 
{
  uint8_t 	bRequestType;
  uint8_t 	bRequest;
  uint8_t 	index;
  uint8_t 	type;
  uint16_t 	wIndex;
  uint16_t 	wLength;
} usb_srqb_t;


// Funktionen

// Deklarationen f�r Schritt 1
uint8_t usb_poweron_device(void);
void usb_init_device(void);


// Deklarationen f�r Schritt 2
void usb_attach(usb_speed_t);


// Deklarationen f�r Schritt 3
uint8_t usb_reset(void);
uint8_t usb_init_ep0(void);


// Deklarationen f�r Schritt 4
void usb_ep0_event(void);
void usb_copy_setup(void);


// Deklarationen f�r Schritt 5
uint8_t usb_decode_request(void);
uint8_t usb_write_chunk(void);

// Deklarationen f�r Schritt 6
uint8_t usb_init_ep1(void);
void usb_event_ep1(void);
