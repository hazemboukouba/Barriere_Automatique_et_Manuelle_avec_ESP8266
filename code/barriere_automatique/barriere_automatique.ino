#include <ESP8266WiFi.h>
#include <PubSubClient.h>
#include <Servo.h>
#include <LiquidCrystal.h>
#include <time.h>

// Définition des broches du LCD 
#define RS 13
#define EN 15
#define D4 0
#define D5 4
#define D6 5
#define D7 1

// Définition des broches des capteurs et actionneurs
#define TRIG 14
#define ECHO 12
#define SERVO_PIN 16
#define LED_ROUGE 2
#define LED_VERTE 3

// Connexion WiFi
const char* ssid = "******"; 
const char* password = "*********"; 

// Connexion MQTT
const char* mqtt_server = "navycarder-xuurd8.a01.euc1.aws.hivemq.cloud";  
const int mqtt_port = 8883;  
const char* mqtt_user = "******";  
const char* mqtt_password = "*******";
const char* topic1 = "barriere/distance";  
const char* topic2 = "barriere/etat";  
const char* topic3 = "barriere/dateheure";   
const char* topic_cmd = "barriere/commande"; 

WiFiClientSecure espClient;
PubSubClient client(espClient);
Servo barriere;
LiquidCrystal lcd(RS, EN, D4, D5, D6, D7);

long duration;
int distance;
bool commandeManuelle = false; // Variable pour mode manuel

// Callback pour recevoir les messages MQTT
void callback(char* topic, byte* payload, unsigned int length) {
  String message = "";
  for (int i = 0; i < length; i++) {
    message += (char)payload[i];
  } 
  Serial.print(" Message reçu [");
  Serial.print(topic);
  Serial.print("] : ");
  Serial.println(message);

  if (String(topic) == topic_cmd) { 
    if (message == "ON") {
      commandeManuelle = true;  //  Active le mode manuel
      ouvrirBarriere();
       client.publish(topic2, "<font color='green'>Ouverte</font>");
    } 
    else if (message == "OFF") {
      commandeManuelle = false; // Désactive le mode manuel
      fermerBarriere();
       client.publish(topic2, "<font color='red'>Fermee</font>");
    }
  }
}

void setup() {
  Serial.begin(115200);  
  configTime(0, 0, "pool.ntp.org", "time.nist.gov");
  Serial.println(" Synchronisation NTP...");
  delay(2000);  

  // Connexion WiFi
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(1000);
    Serial.print(".");
  }
  Serial.println("\n WiFi Connecté !");

  // Connexion MQTT
  espClient.setInsecure();
  client.setServer(mqtt_server, mqtt_port);
  client.setCallback(callback);

  // Connexion et abonnement après la connexion
  while (!client.connected()) {
    Serial.print(" Connexion à HiveMQ...");
    if (client.connect("ESP8266_Client", mqtt_user, mqtt_password)) {
      Serial.println(" Connecté !");
      client.subscribe(topic_cmd);  
    } else {
      Serial.print(" Erreur : ");
      Serial.println(client.state());
      delay(1000);
    }
  }
  //Initialisation des broches
  pinMode(TRIG, OUTPUT);
  pinMode(ECHO, INPUT);
  pinMode(LED_ROUGE, OUTPUT);
  pinMode(LED_VERTE, OUTPUT);
  barriere.attach(SERVO_PIN);
  fermerBarriere(); // 🔹 Ferme la barrière au démarrage
  // Initialisation du LCD
  lcd.begin(16, 4);
}

void loop() {
  client.loop();  // Vérifie les messages MQTT

  // Si mode manuel activé, on ne mesure pas la distance
  if (commandeManuelle) return;

  // Mesure de la distance
  digitalWrite(TRIG, LOW);
  delayMicroseconds(2);
  digitalWrite(TRIG, HIGH);
  delayMicroseconds(10);
  digitalWrite(TRIG, LOW);
 
  duration = pulseIn(ECHO, HIGH);
  distance = (duration * 0.0343) / 2.0;
  
  Serial.print("Distance: ");
  Serial.println(distance);
  String distanceStr = String(distance);
  client.publish(topic1, distanceStr.c_str());
  
  afficherDateHeure();
  delay(1000); // Mettre à jour la date toutes les secondes

  if (distance > 0 && distance < 10) { // Si un véhicule est détecté
    ouvrirBarriere();
    lcd.setCursor(0, 2);
    lcd.print("Barriere Ouverte");
    lcd.setCursor(3, 3);
    lcd.print("Bienvenue"); 
    client.publish(topic2, "<font color='green'>Ouverte</font>");
    delay(4000);
    fermerBarriere();
  } else {
    lcd.setCursor(0, 2);
    lcd.print("Barriere Fermee");
    lcd.setCursor(3, 3);
    lcd.print("*********");
    client.publish(topic2, "<font color='red'>Fermee</font>");
  }

}
void afficherDateHeure() {
  time_t now;
  struct tm timeinfo;
  time(&now);
  localtime_r(&now, &timeinfo);
  timeinfo.tm_hour += 1;  // Ajoute 1 heure pour UTC+1 (Tunisie)
  if (timeinfo.tm_hour >= 24) {
    timeinfo.tm_hour -= 24;
  }
  
  char buffer[20];
  strftime(buffer, sizeof(buffer), "%d/%m/%Y", &timeinfo);
  char bufferHeure[10];  // Format : HH:MM:SS
  strftime(bufferHeure, sizeof(bufferHeure), "%H:%M:%S", &timeinfo);
  lcd.setCursor(0, 0);
  lcd.print("Date: ");
  lcd.print(buffer);
  lcd.setCursor(0, 1);
  lcd.print("Heure: ");
  lcd.print(bufferHeure);
  String dateHeureMQTT = String(buffer) + " " + String(bufferHeure);
  client.publish(topic3, dateHeureMQTT.c_str());
}

// Fonction pour ouvrir la barrière
void ouvrirBarriere() {
  barriere.write(180); 
  digitalWrite(LED_ROUGE, LOW);
  digitalWrite(LED_VERTE, HIGH);
  Serial.println(" Barrière ouverte ");
}

// Fonction pour fermer la barrière
void fermerBarriere() {
  barriere.write(0); 
  digitalWrite(LED_ROUGE, HIGH);
  digitalWrite(LED_VERTE, LOW);
  Serial.println(" Barrière fermée ");
}
