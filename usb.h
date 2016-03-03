#include <stdio.h>

// Ein nützliches Makro
// Makro für die Minimumbildung, Nutzung im Programm: y=MIN(x1,x2);
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
// Deklarationen für Schritt 1
typedef enum {NOT_ATTACHED, ATTACHED, DEFAULT, ADDRESSED, CONFIGURED} usb_devstate_t;
typedef enum {FULL, LOW} usb_speed_t;

typedef struct
{
 usb_devstate_t 	state;		// Gerätezustand
 usb_speed_t		speed;		// Attach-Geschwindigkeit
 uint8_t 			address;	// Geräteadresse
} usb_device_t;


// Die in usb.c definierte Variable überall sichtbar machen
extern usb_device_t usb_device;


// Deklarationen für Schritt 3
// Zustände eines Endpunkts
typedef enum {HALTED, SETUP, DATA_IN, DATA_OUT, STATUS_R, STATUS_W} usb_ep_state_t;


// Verwaltung eines Endpunkts
typedef struct
{
  usb_ep_state_t state;	// Zustand des Endpunkts
  uint16_t     size;	// Länge des EP (hier von 8 bis 256)
  uint8_t      *p;		// Zeiger auf zu sendende Daten (IN) oder auf Empfangsbuffer (OUT)
  uint16_t	   rem;	    // noch zu sendende Datenmenge in Bytes
} usb_endpoint_t;


// Deklarationen für Schritt 4
// Aufbau eines 8 Byte Standardrequests (ausser get/set descriptor)
// Für get/set descriptor zerfällt das Feld wValue des Standards in die beiden Teile index und type
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

// Deklarationen für Schritt 1
uint8_t usb_poweron_device(void);
void usb_init_device(void);


// Deklarationen für Schritt 2
void usb_attach(usb_speed_t);


// Deklarationen für Schritt 3
uint8_t usb_reset(void);
uint8_t usb_init_ep0(void);


// Deklarationen für Schritt 4
void usb_ep0_event(void);
void usb_copy_setup(void);


// Deklarationen für Schritt 5
uint8_t usb_decode_request(void);
uint8_t usb_write_chunk(void);

// Deklarationen für Schritt 6
uint8_t usb_init_ep1(void);
void usb_event_ep1(void);
