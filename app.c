#include <stdio.h>
#include <stdlib.h>
#include "avr/io.h"
#include "avr/wdt.h"
#include "usb.h"


#ifdef USB_DEBUG		//Debugfunktion

// Ausgabefunktion, die vom stdout-stream für ein Zeichen benutzt wird.
int uart_putchar(char c, FILE *stream)
{
 while(!(UCSR1A & 0x20));
 UDR1=c;
 return 0;
}


// input function
int uart_getchar(FILE *stream)
{
 uint8_t e,d;

 // wait until there is a character without errors
 do 
  	{
     e=UCSR1A;
	 if (e & (1<<7))
	 	{
	  	 d=UDR1;
	  	 if ((e & 0x1c) == 0) break;
		}
   } while (1);
  
 return (uint16_t) d;
}

// Struktur für den Ausgabestrom
FILE uart_stream = FDEV_SETUP_STREAM(uart_putchar, uart_getchar, _FDEV_SETUP_WRITE|_FDEV_SETUP_READ);

#endif


// Initialisierung der Prozessorperipherie

void system_init(void)
{
 // den Watchdog abschalten
 MCUSR=0;		
 wdt_disable();

 //LED einstellen
	DDRB  |= (1<<PINB4);
	PORTB &= ~(1<<PINB4);

  
 // JTAG abschalten (PORTF kann dann voll genutzt werden)
 MCUCR=0x80;		
 MCUCR=0x80;
 
 
 //Eingänge setzten
	DDRF = 0x00;
	PORTF = 0xFF;	
  
 // Takt für die CPU 1:1
 CLKPR=0x80;
 CLKPR=0x00;	

 #ifdef USB_DEBUG
 // RS232 für die Debug-Ausgabe initialisieren (38400 Baud)
 DDRD |= (1<<3); // just in case, TxD out
 UCSR1A=0x40;
 UCSR1B=0x18;
 UCSR1C=0x06;
 UBRR1=25;
 // Standard-Ein-/Ausgabe an RS232
 stdout=&uart_stream;
 #endif
}


// Warten auf (neuen) Tastendruck
void tastendruck()
{
 uint8_t i;

 // zuerst muss die taste sicher ausgelassen sein
 for (i=0; i<100; i++)
 	{
     if ((PINE & 0x04) != 0) i=0;
  	} 

 // dann muss die taste sicher gedrückt werden
 for (i=0; i<100; i++)
  	{
     if ((PINE & 0x04) == 0) i=0;
  	} 
}


//mainfunktion
int main(void)
{
 // Grundkonfiguration des uC einstellen
 system_init();

 // Jetzt das USB-Makro in der Rolle als Device betriebsbereit machen
 usb_init_device();

 // Hier auf einen Tastendruck (Boot) oder ein Zeichen an der UART warten ...
 tastendruck();

 // Gerät anmelden
 usb_attach(FULL);

 do
  	{
     // USB-Reset?
     if (usb_device.state != NOT_ATTACHED) 
		{ usb_reset(); }

	// Ereignisse am EP0 bearbeiten
	if ( (usb_device.state==DEFAULT) || (usb_device.state==ADDRESSED) || (usb_device.state==CONFIGURED))
		{ usb_ep0_event(); }

	if (usb_device.state==CONFIGURED)
		{
 		 usb_event_ep1();
		}

  	} while (1);
}
