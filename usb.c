#include <stdio.h>
#include <stdlib.h>
#include "avr/io.h"
#include "avr/wdt.h"
#include "usb.h"


// Globale Variablen
usb_device_t usb_device;			// Struktur zur Verwaltung der Gerätedaten
usb_endpoint_t 	usb_ep0;  			// Struktur zur Verwaltung des EP 0
usb_srqb_t 	usb_setup_packet; 		// Buffer für das SETUP-Paket


// Devicedescriptor für eine HID-Maus, VID und PID bei Windows als Host beliebig, da Windows 
// hier einen HID-Klassentreiber verwendet - der ist nicht an eine VID gebunden
uint8_t usb_device_descriptor[]=
	{ 
	 0x12, 0x01, 0x10, 0x01, 0x00, 0x00, 0x00, 64,
     0xd9, 0x15, 0x4c, 0x0a, 0x00, 0x01, 0x00, 0x01, 
	 0x00, 0x01
	};


// Konfiguration des Geräts, gefolgt vom Interface, Klassendeskriptoren und Endpunktdeskriptoren
// Diese Deskriptoren stehen in einem einzigen Feld, damit sie auf jeden Fall im Speicher unmittelbar hintereinander liegen
// So können auf Anfrage alle Deskriptoren ohne Aufwand in einem Control Read übertragen werden.
uint8_t usb_configuration_descriptor[]=
  	{ 
  	 0x09, 0x02, 0x22, 0x00, 0x01, 0x01, 0x00, 0xA0, 0x32, 	// Configuration Descriptor (9 Byte)
	 0x09, 0x04, 0x00, 0x00, 0x01, 0x03, 0x01, 0x02, 0x00, 	// Interface Descriptor (9 Bytes), deklariert Interface als HID-Maus
	 0x09, 0x21, 0x11, 0x01, 0x00, 0x01, 0x22,   50, 0x00, 	// HID-Deskriptor, deklariert eínen Report-Deskriptor mit 52 Bytes Länge
	 0x07, 0x05, 0x81, 0x03,    3, 0x00, 10 				// Endpunkt-Deskriptor, definiert Endpunkt 1 (Interrupt in. Pakete max. 3 Bytes, alle 10ms)
	};
	
// HID-Class: Report Descriptor (erzeugt mittels HID Descriptor Tool von www.usb.org)

uint8_t usb_hid_report_descriptor[]=
{
    0x05, 0x01,                    // 	  USAGE_PAGE (Generic Desktop)
    0x09, 0x02,                    // 	  USAGE (Mouse)
    0xa1, 0x01,                    // 	  COLLECTION (Application)
    0x09, 0x01,                    //     USAGE (Pointer)
    0xa1, 0x00,                    //     COLLECTION (Physical)
    0x05, 0x09,                    //     USAGE_PAGE (Button)
    0x19, 0x01,                    //     USAGE_MINIMUM (Button 1)
    0x29, 0x02,                    //     USAGE_MAXIMUM (Button 2)
    0x15, 0x00,                    //     LOGICAL_MINIMUM (0)
    0x25, 0x01,                    //     LOGICAL_MAXIMUM (1)
    0x95, 0x02,                    //     REPORT_COUNT (2)
    0x75, 0x01,                    //     REPORT_SIZE (1)
    0x81, 0x02,                    //     INPUT (Data,Var,Abs)
    0x95, 0x01,                    //     REPORT_COUNT (1)
    0x75, 0x06,                    //     REPORT_SIZE (6)
    0x81, 0x03,                    //     INPUT (Cnst,Var,Abs)
    0x05, 0x01,                    //     USAGE_PAGE (Generic Desktop)
    0x09, 0x30,                    //     USAGE (X)
    0x09, 0x31,                    //     USAGE (Y)
    0x15, 0x81,                    //     LOGICAL_MINIMUM (-127)
    0x25, 0x7f,                    //     LOGICAL_MAXIMUM (127)
    0x75, 0x08,                    //     REPORT_SIZE (8)
    0x95, 0x02,                    //     REPORT_COUNT (2)
    0x81, 0x06,                    //     INPUT (Data,Var,Rel)
    0xc0,                          //     END_COLLECTION
    0xc0                           // 	  END_COLLECTION
};		
// String-Deskriptoren (optional)
wchar_t usb_string_0[]=L"\x0304""\x0409";				// Sprachunterstützung
wchar_t usb_string_1[]=L"\x033A""Heidi, die SUPERMAUS";			// Produktbezeichnung
wchar_t *usb_string_descriptors[]=
	{
	 usb_string_0,		// Index 0: Unterstützte Sprachen, hier US-Englisch 
	 usb_string_1		// Index 1: Produktbezeichnung (hier mit Absicht 24 Bytes lang, um Paketzerlegung zu testen)
	};


/*
   Konfigurieren des USB-Makros bis zur Betriebsbereitschaft als Device.
   Das Makro kann jetzt benutzt werden, das Gerät ist aber noch nicht 
   am USB angemeldet (attached).

   Rückgabewert:
   0: Fehler
   1: OK
*/

uint8_t usb_poweron_device(void)
{
 // Grundkonfiguration
 USBCON=0x00;	// USB Makro Reset (für mode select nötig)
 UHWCON=0x81;	// Device mode,SW select, Pad Power Enable

 // PLL starten
 PLLCSR=0;		// Lockbit löschen
 PLLCSR=0x16;	// PLL mit 16 MHZ und at90usb1287

 // auf PLL Lock warten
 do
 {} while (!(PLLCSR & 1));

 // USB-HW im Hostmode aktivieren
 // Die Reihenfolge ist nötig, gleichzeitig geht es nicht
 USBCON = 0x30;	// VCC (OTG) einschalten
 USBCON = 0xb0;	// USB-Makro aus dem Reset holen
 USBCON = 0x90;	// Takt einschalten

 // Alle Endpunkte unter Reset halten
 UERST=0x7f;

 return 1;
}


// Schritt 1
void usb_init_device(void)
{
 usb_poweron_device(); 			//USB-Makro einschalten
 usb_device.state = NOT_ATTACHED; 	//State=Not Attached
 #ifdef USB_DEBUG
 printf("\n\rMakro initialisiert");
 #endif
}


// Schritt 2
/*
    Anmelden (Attach) des Geräts am USB
	Der Modus wird auch in der Gerätekontrollstruktur gespeichert.
	Das ist wichtig, weil sich dami die maximal zulässige Größe des EP0
	ändert.

    Parameter;
	s: FULL, LOW
*/
void usb_attach(usb_speed_t s)
{
// s = FULL oder LOW!!!!
 #ifdef USB_DEBUG
 printf("\n\rAttach angefangen");
 #endif

	/*

	µC-Datenblatt Seite 278
	
	UDCON-Register
	
	    7     6    5     4     3     2      1      0
	 _____ _____ _____ _____ _____ _____ ______ ______
	|  -  |  -  |  -  |  -  |  -  | LSM |RMWKUP|DETACH| - UDCON
	|_____|_____|_____|_____|_____|_____|______|______|

	2 - LSM - USB Device Low Speed Mode Selection
	When configured USB is configured in device mode, this bit allows to select the USB the USB
	Low Speed or Full Speed Mod.
	Clear to select full speed mode (D+ internal pull-up will be activate with the ATTACH bit will be
	set) .
	Set to select low speed mode (D- internal pull-up will be activate with the ATTACH bit will be
	set). This bit has no effect when the USB interface is configured in HOST mode. 

	0 - DETACH - Detach Bit
	Set to physically detach de device (disconnect internal pull-up on D+ or D-).
	Clear to reconnect the device. See Section 22.10, page 270 for more details.

	*/

 if(s == FULL)
	{	
	 UDCON &= ~0x04;			//Null bei LSM erzeugen -> Full-Speed
	}
 else if(s == LOW)
	{
	 UDCON |= 0x04;				//Einser bei LSM erzeugen -> Low-Speed
	}
		
 usb_device.speed = s;			// Speed aktualisieren
 usb_device.state= ATTACHED;	// Statewechsel -> NOT ATTACHED -> ATTACHED

 // Attach ausfuehren
 UDCON &= ~0x01;				// DETACH == 0

 #ifdef USB_DEBUG
 printf("\n\rAttach beendet");
 #endif
	
}


// Schritt 3

/* 
   Auf Reset prüfen und ggf. USB-Teil in den DEFAULT-Zustand bringen.
   Nach einem USB Reset wird einzig der Endpunkt 0 aktiviert
   Alle anderen Endpunkte bleiben deaktiviert.


   Ergebnis: 
   1, falls kein Reset oder Reset und Ausführung OK
   0, falls Reset und Fehler bei der Ausführung

   Bei Erfolg befindet sich das Gerät im Zustand "Default"
*/
uint8_t usb_reset(void)
{
	/*

	µC-Datenblatt Seite 279
	
	UDINT-Register
	
	    7         6        5        4        3        2        1         0
	 ________ ________ ________ ________ ________ ________ _________ _________
	|   -    | UPRSMI | EORSMI |WAKEUPI | EORSTI |  SOFI  |    -    |  SUSPI  | - UDINT
	|________|________|________|________|________|________|_________|_________|

	3 - EORSTI - End Of Reset Interrupt Flag
	Set by hardware when an “End Of Reset” has been detected by the USB controller. This triggers
	an USB interrupt if EORSTE is set.
	Shall be cleared by software. Setting by software has no effect.


	µC-Datenblatt Seite 279
	
	UERST-Register
	
	    7         6        5        4        3        2        1         0
	 ________ ________ ________ ________ ________ ________ _________ _________
	|   -    | EPRST6 | EPRST5 | EPRST4 | EPRST3 | EPRST2 |  EPRST1 |  EPRST0 | - UERST
	|________|________|________|________|________|________|_________|_________|

	• 6-0 - EPRST6:0 - Endpoint FIFO Reset Bits
	Set to reset the selected endpoint FIFO prior to any other operation, upon hardware reset or
	when an USB bus reset has been received. See Section 22.4, page 268 for more information
	Then, clear by software to complete the reset operation and start using the endpoint.

	
	*/

	//Prüfen, ob USB-Reset stattgefunden hat
 if(UDINT & (1<<EORSTI))					
	{
	 #ifdef USB_DEBUG
	 printf("\n\rUSB-Reset erkannt");	//Debug-Meldung
	 #endif
			
	 UDINT &= ~(1<<EORSTI);				// Ereignis in dem Register löschen ..."Shall be cleared by software"
	 UERST = 0x7f;						// alle Endpunkte im Reset halten -> Register UERST alle auf 1
	 usb_device.state = DEFAULT;		// Statewechsel: ATTACHED -> DEFAULT
	 return usb_init_ep0();
	}
 else 
	{
	 return 1; 
    }
}


/*
   Konfigurieren des EP0
   Da der EP0 immer unverändert bleibt, wird er zuerst konfiguriert..

   Ergebnis:
   0: HW konnte nicht konfiguriert werden
   1: Endpunkt 0 konfiguriert
*/
uint8_t usb_init_ep0(void)
{

 /*	UENUM-Register
	
	    7         6        5        4        3        2        1         0
	 ________ ________ ________ ________ ________ ________ _________ _________
	|   -    |    -   |    -   |    -   |    -   |          EPNUM2:0          | - UENUM
	|________|________|________|________|________|________ _________ _________|

	• 2-0 - EPNUM2:0 Endpoint Number Bits
	Load by software to select the number of the endpoint which shall be accessed by the CPU. See
	Section 22.6, page 268 for more details.
	EPNUM = 111b is forbidden.



 	UECFG0X-Register
	
	    7         6        5        4        3        2        1         0
	 ________ ________ ________ ________ ________ ________ _________ _________
	|    EPTYPE1:0    |    -   |    -   |    -   |    -   |    -    |  EPDIR  | - UECFG0X
	|________ ________|________|________|________|________|_________|_________|

	• 7-6 - EPTYPE1:0 - Endpoint Type Bits
	Set this bit according to the endpoint configuration:
	00b: Control || 10b: Bulk ||  01b: Isochronous || 11b: Interrupt

	• 0 - EPDIR - Endpoint Direction Bit
	Set to configure an IN direction for bulk, interrupt or isochronous endpoints.
	Clear to configure an OUT direction for bulk, interrupt, isochronous or control endpoints.



	UECFG1X-Register
	
	    7         6        5        4        3        2        1         0
	 ________ ________ ________ ________ ________ ________ _________ _________
	|   -    |         EPSIZE2:0        |     EPBK1:0     |  ALLOC  |    -    | - UECFG1X
	|________|________ ________ ________|________ ________|_________|_________|

	• 6-4 - EPSIZE2:0 - Endpoint Size Bits
	Set this bit according to the endpoint size:
	000b: 8 bytes 		100b: 128 bytes (only for endpoint 1)
	001b: 16 bytes 		101b: 256 bytes (only for endpoint 1)
	010b: 32 bytes 		110b: Reserved. Do not use this configuration.
	011b: 64 bytes 		111b: Reserved. Do not use this configuration.

	• 3-2 - EPBK1:0 - Endpoint Bank Bits
	Set this field according to the endpoint size:
	00b: One bank
	01b: Double bank
	1xb: Reserved. Do not use this configuration.

	• 1 - ALLOC - Endpoint Allocation Bit
	Set this bit to allocate the endpoint memory.
	Clear to free the endpoint memory.



	UECONX-Register

		 7         6        5        4        3        2        1         0
	 ________ ________ ________ ________ ________ ________ _________ _________
	|   -    |    -   |STALLRQ |STALLRQC|  RSTDT |    -   |    -    |   EPEN  | - UECONX
	|________|________|________|________|________|________|_________|_________|


	• 5 - STALLRQ - STALL Request Handshake Bit
	Set to request a STALL answer to the host for the next handshake.
	Cleared by hardware when a new SETUP is received. Clearing by software has no effect.

	• 4 - STALLRQC - STALL Request Clear Handshake Bit
	Set to disable the STALL handshake mechanism.
	Cleared by hardware immediately after the set. Clearing by software has no effect.

	3• RSTDT - Reset Data Toggle Bit
	Set to automatically clear the data toggle sequence:
	For OUT endpoint: the next received packet will have the data toggle 0.
	For IN endpoint: the next packet to be sent will have the data toggle 0.
	Cleared by hardware instantaneously. The firmware does not have to wait that the bit is cleared.

	• 0 - EPEN - Endpoint Enable Bit
	Set to enable the endpoint according to the device configuration. Endpoint 0 shall always be
	enabled after a hardware or USB reset and participate in the device configuration.
	Clear this bit to disable the endpoint. 

*/

  uint8_t r;
  r=UENUM;								//aktuellen EP retten

  UENUM = 0x00;							// EP 0  auswählen
  UERST |= 0x01;						// EP0 deaktivieren
  UECFG0X=0;							// EP OUT direction							
  UECFG1X = 0x32;						// 00110010 -> 64 bytes -> one bank -> allocate the endponit memory
  UECONX = 0x29;						// 00101001 -> STALLRQ -> RSTDT -> EPEN


 /*
 	UESTA0X-Register

		 7         6        5        4        3        2        1         0
	 ________ ________ ________ ________ ________ ________ _________ _________
	| CFGOK  | OVERFI |UNDERFI |    -   |     DTSEQ1:0    |     NBUSYBK1:0    | - UESTA0X
	|________|________|________|________|________ ________|_________ _________|
 
 	• 7 - CFGOK - Configuration Status Flag
 	Set by hardware when the endpoint X size parameter (EPSIZE) and the bank parametrization
 	(EPBK) are correct compared to the max FIFO capacity and the max number of allowed bank.
 	This bit is updated when the bit ALLOC is set.
 	If this bit is cleared, the user should reprogram the UECFG1X register with correct EPSIZE and
 	EPBK values.
 */
 //Prüfen, ob USB-Makro die Einstellungen akzeptiert hat
 if(UESTA0X & (1<<CFGOK))
 	{
	 UERST &= ~(1<<EPRST0);  			// Endpunkt aktivieren
	 UENUM = r;

	 #ifdef USB_DEBUG
	 printf("\n\rEP0 erfolgreich aktiviert");
	 #endif

	 usb_ep0.size = 64;					// EP0 Größe in Verwaltungsstruktur eintragen
	 usb_ep0.state = SETUP;				// State SETUP
	 UEINTX	= 0x00;						// alle Interuppt Flags löschen ???
	 return 1;
  	}
  else 
  	{
 	 #ifdef USB_DEBUG
 	 printf("\n\rEP0 NICHT aktiviert");
 	 #endif
     usb_ep0.state = HALTED;				// State HALTED
     UENUM = r;
  	 return 0;
  	}
}


void usb_ep0_event(void)
{
 uint8_t r;

 r=UENUM;								// aktuellen EP retten
 UENUM = 0;								// EP 0 auswählen


 /*	UEINTX-Register

		 7         6        5        4        3        2        1         0
	 ________ ________ ________ ________ ________ ________ _________ _________
	|FIFOCON | NAKINI |  RWAL  |NAKOUTI | RXSTPI | RXOUTI |STALLEDI |  TXINI  | - UEINTX
	|________|________|________|________|________|________|_________|_________|



	• 7 - FIFOCON - FIFO Control Bit
	For OUT and SETUP Endpoint:
	Set by hardware when a new OUT message is stored in the current bank, at the same time than
	RXOUT or RXSTP.
	Clear to free the current bank and to switch to the following bank. Setting by software has no
	effect.
	For IN Endpoint:
	Set by hardware when the current bank is free, at the same time than TXIN.
	Clear to send the FIFO data and to switch the bank. Setting by software has no effect.

	• 6 - NAKINI - NAK IN Received Interrupt Flag
	Set by hardware when a NAK handshake has been sent in response of a IN request from the
	host. This triggers an USB interrupt if NAKINE is sent.
	Shall be cleared by software. Setting by software has no effect.

	• 5 - RWAL - Read/Write Allowed Flag
	Set by hardware to signal:
	- for an IN endpoint: the current bank is not full i.e. the firmware can push data into the FIFO,
	- for an OUT endpoint: the current bank is not empty, i.e. the firmware can read data from the FIFO.
	The bit is never set if STALLRQ is set, or in case of error.
	Cleared by hardware otherwise.
	This bit shall not be used for the control endpoint.

	• 4 - NAKOUTI - NAK OUT Received Interrupt Flag
	Set by hardware when a NAK handshake has been sent in response of a OUT/PING request
	from the host. This triggers an USB interrupt if NAKOUTE is sent.
	Shall be cleared by software. Setting by software has no effect.

	• 3 - RXSTPI - Received SETUP Interrupt Flag
	Set by hardware to signal that the current bank contains a new valid SETUP packet. An interrupt
	(EPINTx) is triggered (if enabled).
	Shall be cleared by software to handshake the interrupt. Setting by software has no effect.
	This bit is inactive (cleared) if the endpoint is an IN endpoint.

	• 2 - RXOUTI / KILLBK - Received OUT Data Interrupt Flag
	Set by hardware to signal that the current bank contains a new packet. An interrupt (EPINTx) is
	triggered (if enabled).
	Shall be cleared by software to handshake the interrupt. Setting by software has no effect.
	Kill Bank IN Bit
	Set this bit to kill the last written bank.
	Cleared by hardware when the bank is killed. Clearing by software has no effect.

	• 1 - STALLEDI - STALLEDI Interrupt Flag
	Set by hardware to signal that a STALL handshake has been sent, or that a CRC error has been
	detected in a OUT isochronous endpoint.
	Shall be cleared by software. Setting by software has no effect.

	• 0 - TXINI - Transmitter Ready Interrupt Flag
	Set by hardware to signal that the current bank is free and can be filled. An interrupt (EPINTx) is
	triggered (if enabled).
	Shall be cleared by software to handshake the interrupt. Setting by software has no effect.
*/
	
 if(UEINTX & (1<<RXSTPI))				//Wenn Setup-Paket da ist -> usb_copy_setup();
  	{
	 usb_copy_setup();					//Setup-Paket aus dem Speicher des USB-Makros in Struktur kopieren
	 
	 UEINTX	= 0x01;						//alle Flags löschen außer TXINI

	 if (usb_decode_request()==0)
	 	{
	  	 usb_ep0.state = SETUP;
	  	 UECONX |=(1<<STALLRQ);
	  	 #ifdef USB_DEBUG
      	 printf("\n\rUnbekannter Request");
      	 #endif
	 	}
	}
 else
	{
     switch(usb_ep0.state)
 		{
 	   	 case SETUP:
	     	#ifdef USB_DEBUG
         //	printf("\n\rState: Idle");
         	#endif
	 	 	break;
		
 		 case DATA_IN:  
			if (UEINTX & (1<<RXOUTI))		// RXOUTI -> Set by hardware to signal that the current bank contains a new packet
				{
	     		 #ifdef USB_DEBUG
         		 printf("\n\rStatuswelchsel von DATA_IN nach STATUS_R");
         		 #endif
	     		 usb_ep0.state = STATUS_R;
				}
			else
				{
			 	 if (UEINTX & (1<< TXINI))	// TXINI -> Set by hardware to signal that the current bank is free and can be filled.
					{
	    	 		 #ifdef USB_DEBUG
       	  	 		 printf("\n\rSendepuffer frei");
       	  	 		 #endif		
			 		 if (usb_write_chunk())	
			 			{
	    		  		 #ifdef USB_DEBUG
       			  		 printf("\n\rStatuswelchsel von DATA_IN nach STATUS_R");
       			  	 	 #endif
	    		  		 usb_ep0.state = STATUS_R;			 
						}
					}	
				}	
	    	break;

 		case DATA_OUT: 
			if (UEINTX & (1<<NAKINI))		// NAKINI -> Set by hardware when a NAK handshake has been sent in response of a IN request from the host
				{
		 		 UEINTX &= ~(1<<TXINI);		//leere Sendeparket vorbereiten (TXINI löschen) 
	     		 usb_ep0.state = STATUS_W;
	     		 #ifdef USB_DEBUG
         		 printf("\n\rStatuswelchsel von DATA_OUT nach STATUS_W");
         		 #endif
				}
		break;
	
		case STATUS_R: 		
		if (UEINTX & ~(1<<RXOUTI))		// RXOUTI -> Set by hardware to signal that the current bank contains a new packet	
			{
	     	 usb_ep0.state = SETUP;	
		 	 UECONX|=(1<<STALLRQ);
	 	 	 UEINTX	= 0x00;						//alle Flags löschen
	     	 #ifdef USB_DEBUG
       	 	 printf("\n\rStatuswelchsel von STATUS_R nach SETUP");
       	 	 #endif
			}
		break;
 	
		case STATUS_W: 
			if (UEINTX & (1<<TXINI))				//TXINI wieder gesetzt d.h, dass der Host das vorherige IN-Paket erfolgreich abgeholt hat.
				{

	     		 usb_ep0.state = SETUP;	
		 		 UECONX|=(1<<STALLRQ);				//Mit STALL antworten
	 	 		 UEINTX	= 0x00;						//alle Flags löschen 
	     		 #ifdef USB_DEBUG
       	 		 printf("\n\rStatuswechsel von STATUS_W nach SETUP");
       	 		 #endif
				}
			if (UDADDR > 0)							//Adresse aktivieren
				{ 
				 UDADDR = UDADDR + 128;
		 		 usb_device.state = ADDRESSED; 	//State=Adressed
	     		 #ifdef USB_DEBUG
       	 		 printf("\n\rNeue Adresse: %d",UDADDR-128);
         		 #endif	
				}
			
		break;

 		case HALTED:
			#ifdef USB_DEBUG
    		printf("\n\rStatuswelchsel von HALTEND nach SETUP");
    		#endif
			usb_ep0.state = SETUP;
		break;
	}

 }

 UENUM=r;
}


/*
   
   Diese Funktion hat die Aufgabe, ein Setup-Paket aus dem Speicher des uSB-Makros in eine 
   passende Struktur zu kopieren

   -> SETUP-Paket aus dem RAM des Makro holen
*/

void usb_copy_setup(void)
{

 /*	UEDATX-Register

		 7         6        5        4        3        2        1         0
	 ________ ________ ________ ________ ________ ________ _________ _________
	| DAT D7 | DAT D6 | DAT D5 | DAT D4 | DAT D3 | DAT D2 | DAT D1  | DAT D0  |
	|________|________|________|________|________|________|_________|_________|

	• 7-0 - DAT7:0 -Data Bits
	Set by the software to read/write a byte from/to the endpoint FIFO selected by EPNUM.
*/


 uint8_t *p;
 p=(uint8_t*)&usb_setup_packet;		//Hilfszeiger auf Speicherbereich zeigen

 for(int i=0; i<8; i++)				//Bytes kopieren
 	{			
     *p = UEDATX;
	 p++;
  	}

 #ifdef USB_DEBUG
 p=(uint8_t*)&usb_setup_packet;			//Debug-Medldung
 printf("\n\rSETUP empfangen:");
 for (int i=0; i<8; i++)
  	{
     printf(" %02x",*p++);
  	}
 #endif
}


// Schritt 5 

/*
   Dekodieren eines SETUP-Pakets
    
   Rückgabewert:
   0: Request nicht erkannt
   1: Request erkannt
*/

uint8_t usb_decode_request(void)
{  
 uint8_t *p;
 p=(uint8_t*)&usb_setup_packet;			//Zeiger 

 if( (usb_setup_packet.bRequestType == 0x80) && (usb_setup_packet.bRequest == 0x06) && (usb_setup_packet.index == 0) && (usb_setup_packet.type == 0x01) && (usb_setup_packet.wLength !=0) )
 	{
     usb_ep0.state=DATA_IN;						//Zustand wechsel
	 usb_ep0.p= &usb_device_descriptor[0];		//Datenzeiger auf den Beginn des Device Descriptor

     usb_ep0.rem=MIN(usb_setup_packet.wLength,sizeof(usb_device_descriptor));
	
	 #ifdef USB_DEBUG
     printf("\n\rRequest is: Get Device Descriptor");
     #endif
	 return 1;
	}

  // check for get conf request
 if( (usb_setup_packet.bRequestType == 0x80) && (usb_setup_packet.bRequest == 0x06) && (usb_setup_packet.type == 0x02) )
 	{
	 usb_ep0.state=DATA_IN;						//Zustand wechsel
	 usb_ep0.p= &usb_configuration_descriptor[0];		//Datenzeiger auf den Beginn des Device Descriptor

     usb_ep0.rem=MIN(usb_setup_packet.wLength,sizeof(usb_configuration_descriptor));
	
	 #ifdef USB_DEBUG
     printf("\n\rRequest is: Get Configuration Descriptor with %d bytes",usb_ep0.rem);
     #endif
	 return 1;
	}
	
	// get string req
 if( (usb_setup_packet.bRequestType == 0x80) && (usb_setup_packet.bRequest == 0x06) && (usb_setup_packet.type == 0x03) )
 	{
	 usb_ep0.state=DATA_IN;						//Zustand wechsel
	 if (usb_setup_packet.index<2)
		{
		 usb_ep0.p= (uint8_t*)usb_string_descriptors[usb_setup_packet.index ];		//Datenzeiger auf den Beginn des Device Descriptor
		 #ifdef USB_DEBUG
    	 printf("\n\rRequest is: Get String Descriptor");
    	 #endif	
		 usb_ep0.rem=*usb_ep0.p;
		 return 1;
		}
	else
		{return 0;}
	}

  // check for set address request
 if( (usb_setup_packet.bRequestType == 0x00) && (usb_setup_packet.bRequest == 0x05))
 	{
	 usb_ep0.state=DATA_OUT;						//Zustand wechsel
	 #ifdef USB_DEBUG
     printf("\n\rRequest is: Set Adress");
     #endif	
	 usb_device.address=usb_setup_packet.index & 0x7f;
	 UDADDR = usb_device.address;
	 return 1;
	}

  // check for set configuration 
 if( (usb_setup_packet.bRequestType == 0x00) && (usb_setup_packet.bRequest == 0x09) && (usb_setup_packet.index == 1))
 	{
	 usb_ep0.state=DATA_OUT;						//Zustand wechsel
	 #ifdef USB_DEBUG
     printf("\n\rset configuration");
     #endif	
	 if (1 == usb_init_ep1())
		{
		 usb_device.state = CONFIGURED;
	 	 #ifdef USB_DEBUG
		 printf("\n\rEP1 aktiviert");
	 	 #endif	

	 	}
	 return 1;//#####################################################################################################################################
	}

  // check for set idle
 if( (usb_setup_packet.bRequestType == 0x21) && (usb_setup_packet.bRequest == 0x10))
 	{
	 usb_ep0.state=DATA_OUT;						//Zustand wechsel
	 #ifdef USB_DEBUG
     printf("\n\rset idle rate");
     #endif	
	 return 1;
	}

   // check for HID report discriptor
 if( (usb_setup_packet.bRequestType == 0x81) && (usb_setup_packet.bRequest == 0x06) && (usb_setup_packet.index == 0) && (usb_setup_packet.type == 0x22))
 	{
	 usb_ep0.state=DATA_IN;						//Zustand wechsel
	 usb_ep0.p= &usb_hid_report_descriptor[0];		//Datenzeiger auf den Beginn des Device Descriptor
	 usb_ep0.rem=MIN(usb_setup_packet.wLength,sizeof(usb_hid_report_descriptor));
	 #ifdef USB_DEBUG
     printf("\n\rget HID report request");
     #endif	
 
	 return 1;
	}


 return 0;
}//end Subroutine


// Schritt 5 

/*
   Paket über den aktuellen EP sendefertig macht
    
*/
uint8_t usb_write_chunk(void)
{
 uint8_t r;
 r=UENUM;							// aktuellen EP retten
 UENUM = 0;							// EP 0 auswählen

 #ifdef USB_DEBUG
 printf("\n\rGesedet:");
 #endif

 for(int i=0; i<usb_ep0.rem; i++)	//Bytes kopieren
 	{			
   	 UEDATX=*(usb_ep0.p+i);
  	 #ifdef USB_DEBUG
     printf(" %02x",*(usb_ep0.p+i));
  	 #endif
  	}
 UEINTX &= ~(1<<TXINI);				//Buffer abschliessen
 UENUM = r;
 return 1;
}//end Subroutine



uint8_t usb_init_ep1(void)
{
 uint8_t r;
 r=UENUM;				// aktuellen EP retten
 UENUM = 0x01;			// EP 1  auswählen
 UERST |= (1<<1);		// EP1 deaktivieren
 UECFG0X=0xC1;			// EP IN direction							
 UECFG1X = 0x02;		// 08 bytes -> one bank -> allocate the endponit memory
 UECONX = 0x01;	
 UECONX = 0x19;			// STALLRQ -> RSTDT -> EPEN
 UEINTX	= 0x01;			// alle Flags löschen

 if(UESTA0X & (1<<CFGOK))
  	{
     UENUM = r;
	 UERST &= ~(1<<1);		// Endpunkt aktivieren
	 return 1;
   	}

 UENUM = r;
 return 0;
 
}//end Subroutine


void usb_event_ep1()
{
  if ( ((PINF & 0x0f) != 0x0f) || ((PINE & 0x04) == 0))
  	{
 	 uint8_t r;
 	 r=UENUM;					//aktuellen EP retten
 	 UENUM = 1;	
	 printf("\n\r%02x",PINF);
	 if (UEINTX & (1<< TXINI))	//Prüfen ob Sendepuffer frei ist7
		{
		 if (PINE & (1<<2))
		 	{UEDATX=1;}
		 else
		 	{UEDATX=0;}

		 switch (PINF)
		 	{
		 	 case 254:
			 UEDATX=1; 
			 UEDATX=0; 
			 break;
		 	 case 253: 
			 UEDATX=-1; 
			 UEDATX=0; 
			 break;
		 	 case 252: 
			 UEDATX=0; 
			 UEDATX=0; 
			 break;
		 	 case 251: 
			 UEDATX=0; 
			 UEDATX=-1; 
			 break;
		 	 case 247: 
			 UEDATX=0; 
			 UEDATX=1; 
		 	 case 243: 
			 UEDATX=0; 
			 UEDATX=0; 
			 break;
		 	 case 250: 
			 UEDATX=1; 
			 UEDATX=-1; 
			 break;
		 	 case 249: 
			 UEDATX=-1; 
			 UEDATX=-1; 
			 break;
		 	 case 246: 
			 UEDATX=1; 
			 UEDATX=1; 
			 break;
		 	 case 245: 
			 UEDATX=1; 
			 UEDATX=-1; 
			 break;

			 default: UEDATX=0; UEDATX=0;
			}
		 UEINTX= UEINTX & 0xFE; 	 	 
  		}	 

 	 UENUM = r; 
	}

}//end Subroitine



