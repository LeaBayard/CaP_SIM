//BIG ENDIAN


//#include <io.h>
//#include <sys/io.h>
#include <inttypes.h>
#include <avr/io.h>
#include <avr/eeprom.h>
#include <avr/pgmspace.h>
#include <stdarg.h>
#include <stdlib.h>

//------------------------------------------------
// Programme "sim" pour carte à puce
// 
//------------------------------------------------

// déclaration des fonctions d'entrée/sortie définies dans "io.c"
void sendbytet0(uint8_t b);
uint8_t recbytet0(void);

// Procedure qui renvoie l'ATR
void atr(uint8_t n, char* hist)
{
  sendbytet0(0x3b);	// definition du protocole
  sendbytet0(n);	// nombre d'octets d'historique
  while(n--)		// Boucle d'envoi des octets d'historique
    {
      sendbytet0(*hist++);
    }
}

// variables globales en static ram
uint8_t cla, ins, p1, p2, p3;	// entete de commande
uint8_t sw1, sw2;		// status word

#define T_PINPUK 8

//variables globales en eeprom
uint8_t ee_pin[T_PINPUK] EEMEM;
uint8_t ee_puk[T_PINPUK] EEMEM;

//0xff signifie que l'etat est vierge
uint8_t ee_essais_restants EEMEM = 0xff;

typedef enum{vierge, verouille, deverouille, bloque} ram_etat;
ram_etat state;

ram_etat etat() {
  ram_etat etatcur;
  switch (eeprom_read_byte(&ee_essais_restants)) {
  case 0xff:
    etatcur = vierge;
    break;
  case 0:
    etatcur = bloque;
    break;
  default:
    etatcur = verouille;
    break;
  }

  return etatcur;
}

void intro_puk() {
  if (state != vierge) {
    sw1 = 0x6d;
    return;
  }
  if (p3 != T_PINPUK) {
    sw1 = 0x6c;
    sw2 = T_PINPUK;
    return;
  }

  sendbytet0(ins); //acquitement
  uint8_t nvpuk[T_PINPUK];
  int i;

  for (i = 0; i < T_PINPUK; i++) {
    nvpuk[i] = recbytet0();
  }

  for (i = 0; i < T_PINPUK; i++) {
    if (nvpuk[i] < '0' || nvpuk[i] > '9') {
      sw1 = 0x6e;
      return;
    }
    nvpuk[i] -= '0';
  }

  eeprom_write_block(nvpuk, ee_puk, T_PINPUK);
  eeprom_write_byte(&ee_essais_restants, 3);
  state = verouille;
  sw1 = 0x90;
}

int sont_egaux(uint8_t* a, uint8_t* b, uint8_t taille) {
  int i;
  for (i = 0; i < taille; i++) {
    if (a[i] != b[i]) {
      return 0;
    }
  }

  return 1;
}

void intro_pin() {
  
  if (state != verouille) {
    sw1 = 0x6d;
    return;
  }

  if (p3 != T_PINPUK) {
    sw1 = 0x6c;
    sw2 = T_PINPUK;
    return;
  }

  sendbytet0(ins);

  int i;
  uint8_t t[T_PINPUK];
  for (i = 0; i < T_PINPUK; i++) {
    t[i] = recbytet0();
  }

  int pinfaux = 0;
  for (i = 0; i < T_PINPUK; i++) {
    if (t[i] < '0') {
      pinfaux = 1;
      break;
    }
    t[i] -= '0';
  }

  uint8_t pin[T_PINPUK];
  eeprom_read_block(pin, ee_pin, T_PINPUK);
  
  if (!sont_egaux(t, pin, T_PINPUK) || pinfaux) {
    uint8_t essais_restants = eeprom_read_byte(&ee_essais_restants) - 1;
    if (essais_restants == 0) {
      state = bloque;
    }
    sw1 = 0x98;
    sw2 = 0x40 + essais_restants;
    eeprom_write_byte(&ee_essais_restants, essais_restants);
    return;
  }

  eeprom_write_byte(&ee_essais_restants, 3); //on redonne les 3 essais
  eeprom_write_block(t, ee_pin, T_PINPUK);
  state = deverouille;
  sw1 = 0x90;
}

void change_chv() {
  if (state != deverouille) {
    sw1 = 0x6d;
    return;
  }
  if (p3 != T_PINPUK * 2) {
    sw1 = 0x6c;
    sw2 = T_PINPUK * 2;
    return;
  }

  sendbytet0(ins);

  int i;
  uint8_t old[T_PINPUK];
  uint8_t new[T_PINPUK];
  
  for (i = 0; i < T_PINPUK; i++) {
    old[i] = recbytet0();
  }
  for (i = 0; i < T_PINPUK; i++) {
    new[i] = recbytet0();
  }

  int pinfaux = 0;
  for (i = 0; i < T_PINPUK; i++) {
    if (old[i] < '0') {
      pinfaux = 1;
      break;
    }
    old[i] -= '0';
  }

  uint8_t pin[T_PINPUK];
  eeprom_read_block(pin, ee_pin, T_PINPUK);

  if (!sont_egaux(old, pin, T_PINPUK) || pinfaux) {
    uint8_t essais_restants = eeprom_read_byte(&ee_essais_restants) - 1;
    if (essais_restants == 0) {
      state = bloque;
    }
    sw1 = 0x98;
    sw2 = 0x40 + essais_restants;
    eeprom_write_byte(&ee_essais_restants, essais_restants);
    return;
  }

  eeprom_write_byte(&ee_essais_restants, 3); //on redonne les 3 essais

  for (i = 0; i < T_PINPUK; i++) {
    if (new[i] < '0' || new[i] > '9') {
      sw1 = 0x6e;
      return;
    }
    new[i] -= '0';
  }

  eeprom_write_block(new, ee_pin, T_PINPUK);
  sw1 = 0x90;
}

void unblock() {
  if (state != bloque) {
    sw1 = 0x6d;
    return;
  }
  if (p3 != T_PINPUK * 2) {
    sw1 = 0x6c;
    sw2 = T_PINPUK * 2;
    return;
  }

  sendbytet0(ins);

  int i;
  uint8_t puk[T_PINPUK];
  uint8_t new[T_PINPUK];
  
  for (i = 0; i < T_PINPUK; i++) {
    puk[i] = recbytet0();
  }
  for (i = 0; i < T_PINPUK; i++) {
    new[i] = recbytet0();
  }

  uint8_t vraipuk[T_PINPUK];
  eeprom_read_block(vraipuk, ee_puk, T_PINPUK);
  
  if (!sont_egaux(puk, vraipuk, T_PINPUK)) {
    sw1 = 0x6e;
    return;
  }

  for (i = 0; i < T_PINPUK; i++) {
    if (new[i] < '0' || new[i] > '9') {
      sw1 = 0x6e;
      return;
    }
    new[i] -= '0';
  }

  eeprom_write_block(new, ee_pin, T_PINPUK);
  sw1 = 0x90;
}

//--------------------
// Programme principal
//--------------------
int main(void)
{

  
  // initialisation des ports
  ACSR=0x80;
  DDRB=0xff;
  DDRC=0xff;
  DDRD=0;
  PORTB=0xff;
  PORTC=0xff;
  PORTD=0xff;
  ASSR=1<<EXCLK;
  TCCR2A=0;
  ASSR|=1<<AS2;


  // ATR
  atr(9,"Sim scard");

  state = etat();

  sw2=0;		// pour éviter de le répéter dans toutes les commandes
  // boucle de traitement des commandes
  for(;;)
    {
      // lecture de l'entête
      cla=recbytet0();
      ins=recbytet0();
      p1=recbytet0();
      p2=recbytet0();
      p3=recbytet0();
      sw2=0;
      switch (cla)
	{
	case 0xA0:
	  switch(ins)
	    {
	    case 0x20:
	      intro_pin();
	      break;
	    case 0x24:
	      change_chv();
	      break;
	    case 0x2c:
	      unblock();
	      break;
	    case 0x40:
	      intro_puk();
	      break;
	    default:
	      sw1=0x6d; // code erreur ins inconnu
	    }
	  break;
	default:
	  sw1=0x6e; // code erreur classe inconnue
	}
      sendbytet0(sw1); // envoi du status word
      sendbytet0(sw2);
    }
  return 0;
}
