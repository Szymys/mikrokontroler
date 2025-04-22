 #define F_CPU 16000000UL

#include <stdlib.h>  // Dla funkcji itoa
#include <stdio.h>
#include <avr/io.h>
#include <util/delay.h>
#include <avr/interrupt.h>

#define magistralaDDR DDRA   // bo do portu A podlaczeone
#define magistrala PORTA
#define sterowanieDDR DDRC
#define sterowanie PORTC
#define RS 0					//BO TE 2 kabelki sa do portu C 0i1 i do RS i E
#define E 1
#define BUZZER 2				//brzêczek

int timer_sekundy = 3599; // stan timera - max (59*60 + 59)

int timer_on = 0;		//stan dzia³ania timera
int sek_interrupt;		//flaga zmieniana co jedn¹ sekunde 

//timer 1 to 16 bitowy czyli liczy od 0 do 65535
//preskaler 1024 czyli taktowanie timera 16 Mhz / 1024
// 1/(16000000/1024) to jeden krok timera 0,000064 s
//czyli potrzeba 15625 krokow zeby odliczyc 1 sekunde

void timer1_init() {
	// Ustawienie trybu licznika na Normal Mode
	TCCR1A = 0;
	TCCR1B = 0;

	// Ustawienie preskalera na 1024
	TCCR1B |= (1 << CS12) | (1 << CS10);

	// W³¹czenie przerwañ od przepe³nienia licznika
	TIMSK |= (1 << TOIE1);
	TCNT1 = (65536 - 15625) ;	//ustawiamy stan poczatkowy tak zeby odliczyc 15625 kroków

}

void secondsToMinutesAndSeconds(int totalSeconds, int *minutes, int *seconds) {
	*minutes = totalSeconds / 60;   // Obliczanie minut (dzielenie ca³kowite)
	*seconds = totalSeconds % 60;   // Obliczanie pozosta³ych sekund (reszta z dzielenia przez 60)
}

void LCD_zapis(uint8_t co)
{
	magistrala = co;
	sterowanie |= (1<<E); // znaczy ze na wyprowadzeniu E jest 1 E=1
	_delay_us(1);
	sterowanie &= ~(1<<E);  //czyli zerujemy E=0
	_delay_us(40);
}

void LCD_zapis_ASCII(uint8_t co)
{
	sterowanie |= (1<<RS);     //RS=1 czyli ASCII
	LCD_zapis(co);
}

void LCD_zapis_KOM(uint8_t co)
{
	sterowanie &= ~(1<<RS);    //RS=0 czyli instrukcje
	LCD_zapis(co);
}
void LCD_xy(int x, int y)
{
	LCD_zapis_KOM(0x80+y+x);	//instrukcja do ustawienia wspolrzednych x/y
}

//wysylamy ciag znaków
void LCD_putstr(char* s)
{
	register char c;
	while ((c = *s++))	//petla po lancuchu znaków ,póki nie 0 wysy³amy kolejny znak na lcd
		LCD_zapis_ASCII(c);
}


#define BUTTON1_PIN PD5		//MIN
#define BUTTON2_PIN PD6		//SEK
#define BUTTON3_PIN PD4		//START/STOP

volatile uint16_t timer0_overflow_count = 0;

volatile uint8_t button1_state = 0;
volatile uint8_t button2_state = 0;
volatile uint8_t button3_state = 0;

//timer 0 jest 8 bitowy czyli 256 kroków
//jezeli preskaler 1024, to jeden krok 0,000064 s czyli przepe³nienie calego timera (256 kroków) 0,016384 s
//zeby zak³ócenia/drgania ze styków wyeliminowaæ mozna sprawdzac stan przyciskó co ok 200 ms czyli co okolo 12 przepelnien timera

void timer0_init() {
    // Ustawienie preskalera na 1024 (64 us na tick przy F_CPU = 16 MHz)
    TCCR0 |= (1 << CS02) | (1 << CS00);
    // W³¹czenie przerwañ dla przepe³nienia Timer0
    TIMSK |= (1 << TOIE0);
	TCNT0 = 0 ;
}

int main(void)
{
	timer1_init();
	timer0_init();
	sei();

	magistralaDDR = 0xff;
	sterowanieDDR |= (1<<RS)|(1<<E) | (1 << BUZZER);
	sterowanie &= ~(1<<RS);    //RS=0
	LCD_zapis(0x38);		//magistrala 8-bitowa      00111000 = 0x38(hex)   (okienko jest 5 na 7)
	LCD_zapis(0x06);				//inkrementacja	adresu						00000110=0x06
	LCD_zapis(0x0C);			//wlaczenie wyswietlacza i wylaczenie kursora
	LCD_zapis(0x01);			//czyszczenie ekranu
	_delay_ms(2);				//bo trzeba poczekac 2ms
	
	// przyciski - port D - wejscie i w³¹czenie pull-up resistorów  
	DDRD = 0x00;
	PORTD = 0xFF;
 
	while (1)
	{
		_delay_ms(1);

		 int min, sec;
		 secondsToMinutesAndSeconds(timer_sekundy, &min, &sec);
		 char str[4];       // Tablica o rozmiarze 4 (3 cyfry + '\0') do konwersji

		 LCD_xy(0,0);			
		 if(min < 10) LCD_putstr("0");		//jezeli minuty < 10 to uzupelnianiamy 0 z przodu
		 LCD_putstr(itoa(min, str, 10));	//konwersja liczby dziesiêtnej na wartoœæ znakow¹ metod¹ itoa()
		
		 LCD_xy(0,3);
		 if(sec < 10) LCD_putstr("0");
		 LCD_putstr(itoa(sec, str, 10));
		 
		 LCD_xy(0,2);
		 if( timer_on)			//jezeli timer wlaczony to uruchamiamy efekt migania :, flaga sek_interrupt zmienia sie w przerwaniu co 1 s 
		 {
			if(sek_interrupt)	LCD_zapis_ASCII(' '); else  LCD_zapis_ASCII(':');
		 } else
			 LCD_zapis_ASCII(':');
													 	 
	}

}

	
	//przerwanie co 1 sekunde - 15625 kroków
	ISR(TIMER1_OVF_vect) {

		sek_interrupt = !sek_interrupt; //flaga odliczania kolejnej sekundy (do migania)

		if(timer_on)	//jezeli timer wlaczony zmniejszamy liczbe sekund o jeden ,jezeli zeszlismy do 0 wlaczamy brzeczek
		{
			if(timer_sekundy > 0)
			{
				timer_sekundy--;
				if(timer_sekundy == 0 )
					sterowanie |= (1<<BUZZER);  
			}
		}
		// Ponowne ustawienie wartoœci startowej licznika
		TCNT1 = (65536 - 15625) ;
	}

	//obsluga przyciskow
	ISR(TIMER0_OVF_vect) {

	 timer0_overflow_count++;
	 if (timer0_overflow_count >= 14)	//odliczono 14 przerwan - 14 * 0,016384 okolo 220 ms (doswiadczalnie)

	 {
			timer0_overflow_count = 0;

			// Odczyt stanu przycisków
			button1_state = (PIND & (1 << BUTTON1_PIN)) ? 1 : 0;
			button2_state = (PIND & (1 << BUTTON2_PIN)) ? 1 : 0;
			button3_state = (PIND & (1 << BUTTON3_PIN)) ? 1 : 0;
			//wewnetrzne podciaganie wejsc przez rezystory - przyciski aktywne stanem '0'

			if(button3_state == 0)	// start/stop
			{
				timer_on =!timer_on; //stan na przeciwny

				if(!timer_on)	//timer wy³¹czamy - wylaczamy buzzer i ustawiamy 59:59
				{
					sterowanie &= ~(1<<BUZZER);
					if(timer_sekundy == 0 )
						timer_sekundy = 59*60 + 59;
				}
			}
 
 			if(timer_on)
 				return;		//timer dziala,przyciski min/sec beda nieaktywne

			int min, sec;
			secondsToMinutesAndSeconds(timer_sekundy, &min, &sec);

			//przyciski min/sec 
			//dodajemy minute albo sekunde i przeliczamy wstepna wartosc timera - trzyman¹ w zmiennej timer_sekundy
			if(button1_state == 0)
			{
				min++;
				if(min>59) min = 0;
				timer_sekundy = min*60 + sec;
			}

			if(button2_state == 0)
			{
				sec++;
				if(sec>59) sec = 0;
				timer_sekundy = min*60 + sec;
			}
		}
	}