/*
 * T-Beam con SX1262 - Receptor LoRa (Modo Auto / Manual Conmutable + Botonera)
 */
#include <Arduino.h>
#include <Wire.h>
#include <XPowersLib.h>
#include <SPI.h>
#include <RadioLib.h>
#include <U8g2lib.h>

// === PINES DEL MÓDULO LORA SX1262 ======
#define RADIO_NSS   18   
#define RADIO_DIO1  33   
#define RADIO_RST   23   
#define RADIO_BUSY  32   
#define RADIO_SCK   5    
#define RADIO_MISO  19   
#define RADIO_MOSI  27   

// ==== PINES DE LOS 5 LEDS/RELÉS ====
#define LED1   0   // Corresponde a H1
#define LED2   4   // Corresponde a H2
#define LED3   13  // Corresponde a H3
#define LED4   14  // Corresponde a H4
#define LED5   15  // Corresponde a L2

const int pinesReles[5] = {LED1, LED2, LED3, LED4, LED5};
const char* nombresReles[5] = {"H1", "H2", "H3", "H4", "L2"};

const int configuracionesValidas[] = {0, 17, 6, 26, 23};
const char* textosConfiguracion[] = { 
  "Circuito abierto",   
  "Solo panel 1",       
  "Solo panel 2",       
  "Paneles en serie",   
  "Paneles en paralelo" 
};
const int numConfigs = 5;   
int ultimaConfiguracionValida = 0;

// CONTROL DE MODOS
bool modoAutomatico = true; // Por defecto arranca en automático escuchando la radio

XPowersAXP2101 PMU;  
U8G2_SSD1306_128X64_NONAME_F_HW_I2C u8g2(U8G2_R0, U8X8_PIN_NONE, 22, 21);
SX1262 radio = new Module(RADIO_NSS, RADIO_DIO1, RADIO_RST, RADIO_BUSY);

volatile bool paqueteRecibido = false;

#if defined(ESP8266) || defined(ESP32)
  ICACHE_RAM_ATTR
#endif
void setFlag(void) {
  paqueteRecibido = true;
}

// Comprueba si una cadena es estrictamente un número entero
bool esEnteroValido(String str) {
  str.trim();
  if (str.length() == 0) return false;
  
  int inicio = 0;
  if (str.charAt(0) == '-' || str.charAt(0) == '+') {
    if (str.length() == 1) return false;
    inicio = 1;
  }
  
  for (int i = inicio; i < str.length(); i++) {
    if (!isDigit(str.charAt(i))) {
      return false;
    }
  }
  return true;
}

bool aplicarConfiguracion(int valor) {
  bool esValida = false;
  for (int i = 0; i < numConfigs; i++) {
    if (valor == configuracionesValidas[i]) {
      esValida = true;
      break;
    }
  }

  if (!esValida) return false;

  digitalWrite(LED1, LOW);
  digitalWrite(LED2, LOW);
  digitalWrite(LED3, LOW);
  digitalWrite(LED4, LOW);
  digitalWrite(LED5, LOW);

  if (valor & 0x01) digitalWrite(LED1, HIGH);
  if (valor & 0x02) digitalWrite(LED2, HIGH);   
  if (valor & 0x04) digitalWrite(LED3, HIGH);
  if (valor & 0x08) digitalWrite(LED4, HIGH);   
  if (valor & 0x10) digitalWrite(LED5, HIGH);

  ultimaConfiguracionValida = valor;
  return true;
}

const char* obtenerTextoConfiguracion(int valor) {
  for (int i = 0; i < numConfigs; i++) {
    if (valor == configuracionesValidas[i]) { 
      return textosConfiguracion[i];
    }
  }
  return "Configuracion desconocida";
}

void actualizarOledManual(bool error = false) {
  u8g2.clearBuffer();
  u8g2.drawStr(0, 12, "=== MODO MANUAL ===");
  u8g2.drawStr(0, 32, "Radio desactivada");
  
  if (error) {
    u8g2.drawStr(0, 48, "ERROR: Valor Invalido");
  } else {
    u8g2.drawStr(0, 48, "Config. forzada:");
  }
  
  u8g2.setCursor(0, 60);
  u8g2.print(obtenerTextoConfiguracion(ultimaConfiguracionValida));
  u8g2.sendBuffer();
}

// Nueva función para procesar la orden enviada desde la Botonera Relés
void procesarComandoRele(String comando) {
  // Formato recibido: RELE:<num_rele>:<estado>
  int primerDosPuntos = comando.indexOf(':');
  int segundoDosPuntos = comando.indexOf(':', primerDosPuntos + 1);

  if (primerDosPuntos != -1 && segundoDosPuntos != -1) {
    int numRele = comando.substring(primerDosPuntos + 1, segundoDosPuntos).toInt();
    int estado = comando.substring(segundoDosPuntos + 1).toInt();

    if (numRele >= 1 && numRele <= 5) {
      int pin = pinesReles[numRele - 1];
      digitalWrite(pin, estado == 1 ? HIGH : LOW);

      // Respuesta al replicador OLED de Python
      String msgOled = "OLED_DATA|=== BOTONERA RELÉS ===|Control Individual|Relé " + 
                       String(nombresReles[numRele - 1]) + " -> " + (estado == 1 ? "ON" : "OFF") + "|Mode: MANUAL";
      Serial.println(msgOled);

      // Refrescar pantalla OLED física
      u8g2.clearBuffer();
      u8g2.drawStr(0, 12, "=== BOTONERA RELÉS ===");
      u8g2.drawStr(0, 32, "Radio desactivada");
      u8g2.setCursor(0, 48);
      u8g2.print("Rele "); u8g2.print(nombresReles[numRele - 1]);
      u8g2.print(estado == 1 ? ": ACTIVADO" : ": DESACTIVADO");
      u8g2.sendBuffer();
    }
  }
}

void setup() {
  Serial.begin(115200);
  delay(1000);

  pinMode(LED1, OUTPUT);
  pinMode(LED2, OUTPUT);
  pinMode(LED3, OUTPUT);
  pinMode(LED4, OUTPUT);
  pinMode(LED5, OUTPUT);

  aplicarConfiguracion(0);
  
  Wire.begin(21, 22);
  if (PMU.begin(Wire, AXP2101_SLAVE_ADDRESS, 21, 22)) {
    PMU.setALDO2Voltage(3300); PMU.enableALDO2();
    PMU.setALDO3Voltage(3300); PMU.enableALDO3(); 
    PMU.setBLDO1Voltage(3300); PMU.enableBLDO1();
    PMU.setDC1Voltage(3300);   PMU.enableDC1();
    delay(100);   
  }

  u8g2.begin();
  u8g2.setFont(u8g2_font_6x10_tf);  
  u8g2.clearBuffer();
  u8g2.drawStr(0, 12, "T-Beam SX1262 OK");
  u8g2.drawStr(0, 30, "Modo: AUTOMATICO");
  u8g2.sendBuffer();

  SPI.begin(RADIO_SCK, RADIO_MISO, RADIO_MOSI, RADIO_NSS);

  pinMode(RADIO_RST, OUTPUT);
  digitalWrite(RADIO_RST, LOW);
  delay(10);
  digitalWrite(RADIO_RST, HIGH);
  delay(10);

  int state = radio.begin(868.0);

  if (state == RADIOLIB_ERR_NONE) {
    radio.setSyncWord(0x12);
    radio.setSpreadingFactor(9);      
    radio.setBandwidth(125.0);
    radio.setCodingRate(7);           
    radio.setCRC(2);

    radio.setPacketReceivedAction(setFlag);
    state = radio.startReceive();
  } else {
    while (true);
  }
}

void loop() {
  // ---- 1. LEER COMANDOS DESDE LA INTERFAZ PYTHON (SERIAL) ----
  if (Serial.available() > 0) {
    String comando = Serial.readStringUntil('\n');
    comando.trim();

    if (comando == "SET_AUTO") {
      modoAutomatico = true;
      paqueteRecibido = false; 
      radio.startReceive();   
      
      Serial.println("OLED_DATA|MODO AUTOMATICO|Escuchando radio...|-|-");
      
      u8g2.clearBuffer();
      u8g2.drawStr(0, 12, "T-Beam SX1262 OK");
      u8g2.drawStr(0, 30, "Modo: AUTOMATICO");
      u8g2.drawStr(0, 50, "Escuchando...");
      u8g2.sendBuffer();
    } 
    else if (comando == "SET_MANUAL") {
      modoAutomatico = false;
      
      String datos = "OLED_DATA|=== MODO MANUAL ===|Radio desactivada|Config. forzada:|" + String(obtenerTextoConfiguracion(ultimaConfiguracionValida));
      Serial.println(datos);
      
      actualizarOledManual();
    } 
    else if (comando.startsWith("MANUAL:")) {
      if (!modoAutomatico) {
        String strValor = comando.substring(7);
        bool exito = false;

        if (esEnteroValido(strValor)) {
          int valor = strValor.toInt();
          exito = aplicarConfiguracion(valor);
        }

        if (exito) {
          String datos = "OLED_DATA|=== MODO MANUAL ===|Radio desactivada|Config. forzada:|" + String(obtenerTextoConfiguracion(ultimaConfiguracionValida));
          Serial.println(datos);
          actualizarOledManual(false);
        } else {
          String datos = "OLED_DATA|=== MODO MANUAL ===|Radio desactivada|ERROR: Valor Invalido|Actual: " + String(obtenerTextoConfiguracion(ultimaConfiguracionValida));
          Serial.println(datos);
          actualizarOledManual(true);
        }
      }
    }
    else if (comando.startsWith("RELE:")) {
      if (!modoAutomatico) {
        procesarComandoRele(comando);
      }
    }
  }

  // ---- 2. PROCESAR RADIO LORA (SOLO SI EL MODO AUTOMÁTICO ESTÁ ACTIVO) ----
  if (modoAutomatico && paqueteRecibido) {
    paqueteRecibido = false;
    String recibido = "";
    int state = radio.readData(recibido);

    if (state == RADIOLIB_ERR_NONE) {
      bool exito = false;
      int valorRecibido = -1;

      if (recibido.startsWith("VAL:")) {
        int indiceEspacio = recibido.indexOf(' ');
        if (indiceEspacio != -1) {
          String numeroStr = recibido.substring(4, indiceEspacio);
          
          if (esEnteroValido(numeroStr)) {
            valorRecibido = numeroStr.toInt();
            exito = aplicarConfiguracion(valorRecibido);
          }
        }
      }

      u8g2.clearBuffer();
      u8g2.drawStr(0, 12, "LORA RECIBIDO:");
      u8g2.drawStr(0, 30, recibido.c_str());
      u8g2.setCursor(0, 50);
      if (exito) {
        u8g2.print("OK: "); u8g2.print(obtenerTextoConfiguracion(valorRecibido));
      } else {
        u8g2.drawStr(0, 50, "ERROR: Config. no valida");
      }
      u8g2.setCursor(0, 60);
      u8g2.print("Actual: "); u8g2.print(obtenerTextoConfiguracion(ultimaConfiguracionValida));
      u8g2.sendBuffer(); 

      Serial.print("OLED_DATA|LORA RECIBIDO:|");
      Serial.print(recibido); Serial.print("|");
      if (exito) {
        Serial.print("OK: "); Serial.print(obtenerTextoConfiguracion(valorRecibido));
      } else {
        Serial.print("ERROR: Config. no valida");
      }
      Serial.print("|Actual: "); Serial.println(obtenerTextoConfiguracion(ultimaConfiguracionValida));
    }
    
    radio.startReceive();
  }
  
  delay(10); 
}