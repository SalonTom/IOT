#define LED_RGB_RED_1 22
#define LED_RGB_GREEN_1 24
#define LED_RGB_BLUE_1 26
#define LED_RGB_RED_2 31
#define LED_RGB_GREEN_2 33
#define LED_RGB_BLUE_2 35
#define LED_RGB_RED_3 37
#define LED_RGB_GREEN_3 39
#define LED_RGB_BLUE_3 41
#define LED_RGB_RED_4 43
#define LED_RGB_GREEN_4 45
#define LED_RGB_BLUE_4 47
#define LED_RGB_RED_5 49
#define LED_RGB_GREEN_5 51
#define LED_RGB_BLUE_5 53

#define BUZZER 13

#define LIGHT_SENSOR A0

#define DAYLIGHT_TRESHOLD 500

#define PASSWORD_LENGTH 4

#define MAX_WRONG_COMBINATIONS 3

#define BYTE_SLAVE_ADDRESS 0x01

#include "Wire.h"
#include "LiquidCrystal_AIP31068_I2C.h"
#include "Keypad.h"
#include "RTClib.h"

LiquidCrystal_AIP31068_I2C LCD(0x3E,16,2); // définit le type d'écran lcd 16 x 2
RTC_DS1307 CLOCK; // Horloge

// Définition et configuration du keypad
char keypadKeys[4][3] = {
	{'1', '2', '3'},
	{'4', '5', '6'},
	{'7', '8', '9'},
	{'*', '0', '#'}
};

byte rows[4] = {2, 3, 4, 5};
byte cols[3] = {8, 7, 6};
Keypad keypad = Keypad( makeKeymap(keypadKeys), rows, cols, 4, 3);

// Booléen indiquant si le système est en mode IDLE ou non.
bool systemIsInActiveMode = false;

// Holding register pour les mots de passes et un booléen indiquant si le système est en mode sécurité ou non.
struct HoldingRegister {
	char daylightPassword[PASSWORD_LENGTH] = {'8', '5', '9', '1'};
	char nightPassword[PASSWORD_LENGTH] = {'9', '4', '8', '2'};
	bool securityModeActivated = false;
	// Booléen indiquant si le système est en mode jour ou nuit.
	bool dayModeActivated = false;
};

HoldingRegister holdingRegister;

// Mot de passe saisi par l'utilisateur
char inputPassword[PASSWORD_LENGTH];

// Permet de gérer la saisie du mdp.
int indexDigitPassword = 0;

// Booléen indiquant si le mdp saisi correspond au mdp de jour.
bool isDaylightPasswordCorrect = true;

// Nombre de mauvaises saisies du mdp.
int wrongCombinations = 0;

// Compteur pour gérer la machine d'êtat du chenillard.
int count = 0;
// Indique la prochaine led à allumer du chenillard
int nextLedToLightUp = LED_RGB_RED_1;

// Méthode gérant le passage en mode IDLE
void setIdleMode() {

	systemIsInActiveMode = false;
	digitalWrite(LED_RGB_RED_1, LOW);
	digitalWrite(LED_RGB_RED_2, LOW);
	digitalWrite(LED_RGB_RED_3, LOW);
	digitalWrite(LED_RGB_RED_4, LOW);
	digitalWrite(LED_RGB_RED_5, LOW);
	
	digitalWrite(LED_RGB_BLUE_1, LOW);
	digitalWrite(LED_RGB_BLUE_2, LOW);
	digitalWrite(LED_RGB_BLUE_3, LOW);
	digitalWrite(LED_RGB_BLUE_4, LOW);
	digitalWrite(LED_RGB_BLUE_5, LOW);

	digitalWrite(LED_RGB_GREEN_1, HIGH);
	digitalWrite(LED_RGB_GREEN_2, HIGH);
	digitalWrite(LED_RGB_GREEN_3, HIGH);
	digitalWrite(LED_RGB_GREEN_4, HIGH);
	digitalWrite(LED_RGB_GREEN_5, HIGH);
	
	LCD.clear();
	LCD.print("BIENVENUE");
	LCD.noDisplay();
	
	clearPassword(inputPassword);
	
	wrongCombinations = 0;
}

// Méthode pour bip. Le temps en ms est passé en paramètre
void bip(int time) {
	tone(BUZZER, 600);
	delay(time);
	noTone(BUZZER);
}

// RAZ du mdp
void clearPassword(char password[PASSWORD_LENGTH]) {
	for(int i = 0; i < PASSWORD_LENGTH; i++) {
		password[i] = '*';
	}
}

// Affihchage sur l'écran LCD du mdp
void lcdPrintPassword(char password[PASSWORD_LENGTH]) {
	LCD.setCursor(0,1);
	LCD.print("");
	for(int i = 0; i < PASSWORD_LENGTH; i++) {
		LCD.print(password[i]);
	}
}

void setup () {
	pinMode(LED_RGB_RED_1, OUTPUT);
	pinMode(LED_RGB_GREEN_1, OUTPUT);
	pinMode(LED_RGB_BLUE_1, OUTPUT);
	
	pinMode(LED_RGB_RED_2, OUTPUT);
	pinMode(LED_RGB_GREEN_2, OUTPUT);
	pinMode(LED_RGB_BLUE_2, OUTPUT);
	
	pinMode(LED_RGB_RED_3, OUTPUT);
	pinMode(LED_RGB_GREEN_3, OUTPUT);
	pinMode(LED_RGB_BLUE_3, OUTPUT);
	
	pinMode(LED_RGB_RED_4, OUTPUT);
	pinMode(LED_RGB_GREEN_4, OUTPUT);
	pinMode(LED_RGB_BLUE_4, OUTPUT);
	
	pinMode(LED_RGB_RED_5, OUTPUT);
	pinMode(LED_RGB_GREEN_5, OUTPUT);
	pinMode(LED_RGB_BLUE_5, OUTPUT);
	
	pinMode(BUZZER, OUTPUT);
	
	CLOCK.begin();
	
	Serial.begin(9600);
	
	LCD.init();
	
	setIdleMode();
}

#define BUFFER_SIZE 32

int incomingByte = 0;
byte trame_question[BUFFER_SIZE];
byte trame_response[BUFFER_SIZE];

void loop () {
	char keyPressed = keypad.getKey();
	DateTime now = CLOCK.now();
	
	// Si l'intensité lumineuse est inférieure au seuil, on acive la backlight de l'écran
	if (analogRead(LIGHT_SENSOR) < DAYLIGHT_TRESHOLD) {
		// La bibliothèque ne contient plus la fonctionnalité backlight.
		Serial.print("Backlight active");
	}
	
	// Le mode jour est actif entre 6 heures et 19 heures
	if (now.hour() > 6 && now.hour() < 19) {
		holdingRegister.dayModeActivated = true;
	} else {
		holdingRegister.dayModeActivated = false;
	}
	
	
	if (Serial.available() > 0) {
		incomingByte = Serial.readBytesUntil((char)0x21, trame_question, BUFFER_SIZE);
		
		// On s'assure que la trame reçue soit de la bonne longueur
		if (incomingByte > 8) {
			// On checke si le numéro d'esclave est correct
			if (trame_question[0] == '0' && trame_question[1] == 'x' && trame_question[2] == '0' && trame_question[3] == '1') {
				
				// Esclave
				trame_response[0] = '0';
				trame_response[1] = 'x';
				trame_response[2] = '0';
				trame_response[3] = '1';

				// On gère la fonction (uniquement lecture pour le moment des holdings registers : fonction 3)
				switch (trame_question[7]) {
					case '3':
						// Fonction
						trame_response[4] = '0';
						trame_response[5] = 'x';
						trame_response[6] = '0';
						trame_response[7] = '3';
						// On ne gère que la lecture d'un seul registe
						// On gère l'adresse (uniquement lecture de l'état pour le moment : adresse 3)
						if (trame_question[15] == '3') {
							// Adresse
							trame_response[8] = '0';
							trame_response[9] = 'x';
							trame_response[10] = '0';
							trame_response[11] = '0';
							
							trame_response[12] = '0';
							trame_response[13] = 'x';
							trame_response[14] = '0';
							trame_response[15] = '3';
							
							// Valeur du mot
							trame_response[16] = '0';
							trame_response[17] = 'x';
							trame_response[18] = '0';
							trame_response[19] = '0';
							
							trame_response[20] = '0';
							trame_response[21] = 'x';
							trame_response[22] = '0';
							
							if (holdingRegister.securityModeActivated) {
								trame_response[23] = '1';
							} else {
								trame_response[23] = '0';
							}
							
							// Ajout CRC (factice pour le moment)
							trame_response[24] = '0';
							trame_response[25] = 'x';
							trame_response[26] = 'A';
							trame_response[27] = 'A';
							
							trame_response[28] = '0';
							trame_response[29] = 'x';
							trame_response[30] = 'A';
							trame_response[31] = 'A';
						} else {
							
							// Code erreur 0xFF
							trame_response[8] = '0';
							trame_response[9] = 'x';
							trame_response[10] = 'F';
							trame_response[11] = 'F';
							
							// Ajout CRC (factice pour le moment)
							trame_response[12] = '0';
							trame_response[13] = 'x';
							trame_response[14] = 'A';
							trame_response[15] = 'A';
							
							trame_response[16] = '0';
							trame_response[17] = 'x';
							trame_response[18] = 'A';
							trame_response[19] = 'A';
						}
						break;
					default:
						trame_response[4] = '0';
						trame_response[5] = 'x';
						trame_response[6] = trame_question[6];
						trame_response[7] = trame_question[7];

						// Code erreur 0xFF
						trame_response[8] = '0';
						trame_response[9] = 'x';
						trame_response[10] = 'F';
						trame_response[11] = 'F';
						
						// Ajout CRC (factice pour le moment)
						trame_response[12] = '0';
						trame_response[13] = 'x';
						trame_response[14] = 'A';
						trame_response[15] = 'A';
						
						trame_response[16] = '0';
						trame_response[17] = 'x';
						trame_response[18] = 'A';
						trame_response[19] = 'A';
						
						break;
				}
			}
		}
		Serial.write(trame_response, BUFFER_SIZE);
		Serial.flush();
		memset(trame_response, 0, sizeof(trame_response));
		memset(trame_question, 0, sizeof(trame_question));
	}

	// Chenillard orange actif lorsque le système n'est pas en mode IDLE et si le mode sécurité n'est pas activé
	if(systemIsInActiveMode && !holdingRegister.securityModeActivated) {
		if (count == 0) {
			digitalWrite(LED_RGB_RED_1, HIGH);
			digitalWrite(LED_RGB_GREEN_1, HIGH);
			nextLedToLightUp = 	LED_RGB_RED_2;
		}
		
		
		if (count % 10 == 0) {
			switch(nextLedToLightUp) {
				case LED_RGB_RED_1:
					digitalWrite(LED_RGB_RED_1, LOW);
					digitalWrite(LED_RGB_GREEN_1, LOW);	
					digitalWrite(LED_RGB_RED_2, HIGH);
					digitalWrite(LED_RGB_GREEN_2, HIGH);	
					nextLedToLightUp = 	LED_RGB_RED_2;
					break;
				case LED_RGB_RED_2:
					digitalWrite(LED_RGB_RED_2, LOW);	
					digitalWrite(LED_RGB_GREEN_2, LOW);	
					digitalWrite(LED_RGB_RED_3, HIGH);
					digitalWrite(LED_RGB_GREEN_3, HIGH);	
					nextLedToLightUp = 	LED_RGB_RED_3;
					break;
				case LED_RGB_RED_3:
					digitalWrite(LED_RGB_RED_3, LOW);
					digitalWrite(LED_RGB_GREEN_3, LOW);
					digitalWrite(LED_RGB_RED_4, HIGH);
					digitalWrite(LED_RGB_GREEN_4, HIGH);
					nextLedToLightUp = 	LED_RGB_RED_4;
					break;
				case LED_RGB_RED_4:
					digitalWrite(LED_RGB_RED_4, LOW);	
					digitalWrite(LED_RGB_GREEN_4, LOW);
					digitalWrite(LED_RGB_RED_5, HIGH);
					digitalWrite(LED_RGB_GREEN_5, HIGH);
					nextLedToLightUp = 	LED_RGB_RED_5;
					break;
				case LED_RGB_RED_5:
					digitalWrite(LED_RGB_RED_5, LOW);	
					digitalWrite(LED_RGB_GREEN_5, LOW);
					digitalWrite(LED_RGB_RED_1, HIGH);
					digitalWrite(LED_RGB_GREEN_1, HIGH);
					nextLedToLightUp = 	LED_RGB_RED_1;
					break;
			}
		}
		
		count += 1;
		delay(50);
	}

	if(keyPressed != NO_KEY) {
	
		bip(100);

		// Si mode IDLE activé, on le désactive et on active les leds et l'écran
		if (systemIsInActiveMode == false) {
			systemIsInActiveMode = true;
			
			LCD.display();
			
			bip(1000);
			delay(1000);
			bip(1000);
			delay(1000);
			bip(1000);
			
			LCD.clear();
			delay(100);
			LCD.print("MOT DE PASSE :");
			LCD.setCursor(0,1);
			lcdPrintPassword(inputPassword);
			
			digitalWrite(LED_RGB_GREEN_1, LOW);
			digitalWrite(LED_RGB_GREEN_2, LOW);
			digitalWrite(LED_RGB_GREEN_3, LOW);
			digitalWrite(LED_RGB_GREEN_4, LOW);
			digitalWrite(LED_RGB_GREEN_5, LOW);
		} else if (!holdingRegister.securityModeActivated) {
			// LCD.print(analogRead(LIGHT_SENSOR));

			// Dès lors qu'une touche est pressée (sauf * et #) on met à jour le mot de passe et on affiche le nouveau
			if (keyPressed != '*') {
				if (keyPressed != '#') {					
					if (indexDigitPassword < PASSWORD_LENGTH) {
						inputPassword[indexDigitPassword] = keyPressed;
						indexDigitPassword += 1;
						
						lcdPrintPassword(inputPassword);
					}
				}
			}

			// Passage au mode IDLE à l'appui sur la touche #
			if (keyPressed == '#') {
				LCD.clear();
				delay(100);
				LCD.setCursor(0,0);
				LCD.print("AU REVOIR");
				delay(2000);
				setIdleMode();
			}
			
			// Verification du (ou des) mot(s) de passe. Dépend du mode jour ou nuit. Pour le moment, seul le mode jour est géré.
			if (keyPressed == '*') {
				for(int i = 0; i < PASSWORD_LENGTH; i++) {
					if(inputPassword[i] != holdingRegister.daylightPassword[i]) isDaylightPasswordCorrect = false;
				}
				
				switch (isDaylightPasswordCorrect) {
					// Si mdp ok, on change les leds et l'affichage avant de repasser en mode IDLE.
					case true:
						tone(BUZZER, 600);
						LCD.clear();
						LCD.setCursor(0,0);
						LCD.print("MDP Correct");
						LCD.setCursor(0,1);
						LCD.print("Bonne journee");

						indexDigitPassword = 0;
						clearPassword(inputPassword);
						
						delay(1000);
						noTone(BUZZER);
						delay(2000);
						setIdleMode();
						break;
					// Sinon, on enregistre le nombre de mauvaises combinaisons. Si trop d'essais, on passe en mode sécurité.
					default:	
						wrongCombinations = wrongCombinations + 1;

						if (wrongCombinations != MAX_WRONG_COMBINATIONS) {
						
							LCD.clear();
							delay(100);
							LCD.setCursor(0,0);
							LCD.print("MDP Errone");
							LCD.setCursor(0,1);
							LCD.print(MAX_WRONG_COMBINATIONS - wrongCombinations);
							LCD.print(" essai(s) restant(s)");
							
							digitalWrite(LED_RGB_GREEN_1, LOW);
							digitalWrite(LED_RGB_GREEN_2, LOW);
							digitalWrite(LED_RGB_GREEN_3, LOW);
							digitalWrite(LED_RGB_GREEN_4, LOW);
							digitalWrite(LED_RGB_GREEN_5, LOW);
							
							tone(BUZZER,600);
							
							for (int j = 0; j < 2; j++) {
								for(int i = 0; i < 15; i++) {
									if (i < 8) {									
										digitalWrite(LED_RGB_RED_1, HIGH);
										digitalWrite(LED_RGB_RED_2, HIGH);
										digitalWrite(LED_RGB_RED_3, HIGH);
										digitalWrite(LED_RGB_RED_4, HIGH);
										digitalWrite(LED_RGB_RED_5, HIGH);
									} else {
										digitalWrite(LED_RGB_RED_1, LOW);
										digitalWrite(LED_RGB_RED_2, LOW);
										digitalWrite(LED_RGB_RED_3, LOW);
										digitalWrite(LED_RGB_RED_4, LOW);
										digitalWrite(LED_RGB_RED_5, LOW);
									}
									
									if ((i+1)*(j + 1)*50 >= 1000) {
										noTone(BUZZER);
									}

									delay(50);
								}
							}
							
							clearPassword(inputPassword);
							indexDigitPassword = 0;
	
							LCD.clear();
							
							LCD.print("MOT DE PASSE :");
							
							LCD.setCursor(0,1);
							lcdPrintPassword(inputPassword);
						} else {
							tone(BUZZER, 600);
							LCD.clear();
							delay(100);
							LCD.setCursor(0,0);
							LCD.print("TROP D'ESSAIS");
							LCD.setCursor(0,1);
							LCD.print("PASSAGE SECURITE");
							
							digitalWrite(LED_RGB_GREEN_1, LOW);
							digitalWrite(LED_RGB_GREEN_2, LOW);
							digitalWrite(LED_RGB_GREEN_3, LOW);
							digitalWrite(LED_RGB_GREEN_4, LOW);
							digitalWrite(LED_RGB_GREEN_5, LOW);
							
							digitalWrite(LED_RGB_BLUE_1, LOW);
							digitalWrite(LED_RGB_BLUE_2, LOW);
							digitalWrite(LED_RGB_BLUE_3, LOW);
							digitalWrite(LED_RGB_BLUE_4, LOW);
							digitalWrite(LED_RGB_BLUE_5, LOW);
							
							digitalWrite(LED_RGB_RED_1, HIGH);
							digitalWrite(LED_RGB_RED_2, HIGH);
							digitalWrite(LED_RGB_RED_3, HIGH);
							digitalWrite(LED_RGB_RED_4, HIGH);
							digitalWrite(LED_RGB_RED_5, HIGH);
							
							holdingRegister.securityModeActivated = true;
							
							delay(1900);
							
							noTone(BUZZER);
						}
				}
			}
		}
	}
}