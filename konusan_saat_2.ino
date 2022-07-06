/*******************************************************************************
  P1016*32 Led Matrix ile internetten zaman bilgisi çekilerek yapılan saat
  ve saat bilgisine göre dfplayer ile hatırlatma anonsları

  Internet Clock for P10 Led Matrix 16x32 & reminder announcements according to time information with dfplayer

  TR/izmir/ Haziran/2022/ by Dr.TRonik YouTube

    P10 MonoColor Hardware Connections:
            ------IDC16 IN------
  CS/GPIO15/D8  |1|   |2|  D0/GPIO16
            Gnd |3|   |4|  D6/GPIO12/MISO
            Gnd |5|   |6|  X
            Gnd |7|   |8|  D5/GPIO14/CLK
            Gnd |9|   |10| D3/GPIO0
            Gnd |11|  |12| D7/GPIO13/MOSI
            Gnd |13|  |14| X
            Gnd |15|  |16| X
            -------------------

  GPIO5/D1 - Rx (SoftSerial to comunicate with the DFPlayer mini Pin no:3)
  GPIO4/D2 - Tx (SoftSerial to comunicate with the DFPlayer mini Pin no:2 + 1K Resistor?)
*******************************************************************************/

/********************************************************************
  GLOBALS___GLOBALS___GLOBALS___GLOBALS___GLOBALS___GLOBALS___
 ********************************************************************/
//ESP8266 ile ilgili
#include <ESP8266WiFi.h>
#include <ESP8266mDNS.h>
#include <WiFiUdp.h>
#include <ArduinoOTA.h>
#include <ESP8266httpUpdate.h>

//P10 ile ilgili
#include <DMDESP.h>
#include <fonts/angka6x13.h> // Saat ve dakika için
#include <fonts/SystemFont5x7.h> // Saniye efekti için

//SETUP DMD
#define DISPLAYS_WIDE 1 //Yatayda bir panel
#define DISPLAYS_HIGH 1 // Dikeyde bir panel
DMDESP Disp(DISPLAYS_WIDE, DISPLAYS_HIGH); //Bir panel kullanıldı...

//İnternet üzerinden zamanı alabilme
#include <NTPClient.h>
#include <Time.h>
#include <TimeLib.h>
#include "Timezone.h"

//Mini player ile ilgili
#include <DFPlayerMini_Fast.h>
#include <SoftwareSerial.h>
#define SOUND_SERIAL_RX 5
#define SOUND_SERIAL_TX 4
SoftwareSerial sound_serial(SOUND_SERIAL_RX, SOUND_SERIAL_TX); // RX, TX

DFPlayerMini_Fast sound; //"sound" adında player objesi oluşturuldu...

//SSID ve Şifre
#define STA_SSID "Dr.TRonik"
#define STA_PASSWORD  "Dr.TRonik"

//SOFT AP
#define SOFTAP_SSID "Saat"
#define SOFTAP_PASSWORD "87654321"
#define SERVER_PORT 2000

//Zaman intervalleri
#define NTP_OFFSET   60 * 60      // In seconds
#define NTP_INTERVAL 300 * 1000    // In miliseconds, 5dk da bir güncelleme
#define NTP_ADDRESS  "tr.pool.ntp.org"  // en yakın ntp pool  (ntp.org)

WiFiServer server(SERVER_PORT);

WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP, NTP_ADDRESS, NTP_OFFSET, NTP_INTERVAL);
time_t local, utc;

bool connected = false;
unsigned long last_second;

int  p10_Brightness, saat, dakika, saniye ;
boolean flag = 1;
int s = 14;


/********************************************************************
  SETUP___SETUP___SETUP___SETUP___SETUP___SETUP___SETUP___SETUP___
 ********************************************************************/
void setup() {

  sound_serial.begin(9600);
  delay(100);
  sound.begin(sound_serial);
  delay(1000);
  Serial.begin(115200);
  sound.volume(s); //Ses ayarı 0-30 arası...
  delay(1000);

  Disp.start();

  Disp.setFont(SystemFont5x7);
  Disp.setBrightness(1);

  last_second = millis();

  bool r;
  WiFi.mode(WIFI_AP_STA);
  WiFi.softAPConfig(IPAddress(192, 168, 35, 35), IPAddress(192, 168, 35, 35), IPAddress(255, 255, 255, 0));
  r = WiFi.softAP(SOFTAP_SSID, SOFTAP_PASSWORD, 6, 0);
  server.begin();

  timeClient.begin();   // Start the NTP UDP client

  if (r)
    Serial.println("SoftAP started!");
  else
    Serial.println("ERROR: Starting SoftAP");

  Serial.print("Trying WiFi connection to ");
  Serial.println(STA_SSID);

  WiFi.setAutoReconnect(true);
  WiFi.begin(STA_SSID, STA_PASSWORD);

  ArduinoOTA.begin();
}

/********************************************************************
  LOOP__LOOP__LOOP__LOOP__LOOP__LOOP__LOOP__LOOP__LOOP__LOOP__LOOP__
 ********************************************************************/
void loop() {
  Disp.loop();

  set_bright();

  Disp.setBrightness(p10_Brightness);

  Disp.setFont(SystemFont5x7);

  //Saniye efekti___
  if (millis() / 1000 % 2 == 0) // her 1 saniye için
  {
    Disp.drawChar(14, 5, ':'); //iki noktayı göster
  }
  else
  {
    Disp.drawChar(14, 5, ' '); // gösterme
  }
  //___Saniye efekti___ sonu

  //Handle OTA
  ArduinoOTA.handle();

  //Handle Connection to Access Point
  if (WiFi.status() == WL_CONNECTED)
  {
    if (!connected)
    {
      connected = true;
      Serial.println("");
      Serial.print("Connected to ");
      Serial.println(STA_SSID);
      Serial.println("WiFi connected");
      Serial.print("IP address: ");
      Serial.println(WiFi.localIP());
    }
  }
  else
  {
    if (connected)
    {
      connected = false;
      Serial.print("Disonnected from ");
      Serial.println(STA_SSID);
    }
  }

  if (millis() - last_second > 1000)
  {
    last_second = millis();

    timeClient.update();
    unsigned long epochTime = timeClient.getEpochTime();

    // convert received time stamp to time_t object

    utc = epochTime;

    // Then convert the UTC UNIX timestamp to local time
    TimeChangeRule usEDT = { "EDT", Second, Sun, Mar, 2, +120 };  //Eastern Daylight Time (EDT). Türkiye: UTC+3 nedeni ile +120dk ayarlanmalı...
    TimeChangeRule usEST = { "EST", First, Sun, Nov, 2, -60 };   //Eastern Time Zone. -60 ile zaman sunucusuna bağlanana kadar 0'dan başlamasını sağlıyor...
    Timezone usEastern(usEDT, usEST);
    local = usEastern.toLocal(utc);

    saat = hour(local); //Eğer 24 değil de 12 li saat istenirse :hourFormat12(local); olmalı
    dakika = minute(local);
    saniye =  second(local);
  }
  // End of the millis...

  //__P10 panele dmdesp kütüphanesi ile yazdırma...
  Disp.setFont(angka6x13); //Yazdırılacak font seçimi.

  char saat_   [3]; //saat verisini biçimlendirilmiş string formatına çevirmek için değişken...
  char dakika_ [3]; //dakika verisini biçimlendirilmiş string formatına çevirmek için değişken...

  sprintf(saat_ , "%02d", saat); // "saat" verisini, 10'dan küçükse başına 0 koyarak, iki basamaklı,  "saat_" char dizisine desimal çevir "%02d"...
  sprintf(dakika_ , "%02d", dakika);// "dakika" verisini, 10'dan küçükse başına 0 koyarak, iki basamaklı,  "dakika_" char dizisine desimal çevir "%02d"...

  Disp.drawText(0,  1, saat_); //Biçimlendirilmiş char dizisini koordinatlara yazdır (x ekseni 0, y ekseni 1)...
  Disp.drawText(19, 1, dakika_);

  //__Saate göre hatırlatma anonsları=========================================
  if ((saat == 7 && dakika == 0) && (saniye == 0) ) {

    sound.play(2); //Saat 07:00:00 ise 2. sesi  çal
  }

  if ((saat == 8 && dakika == 0) && (saniye == 0) ) {

    sound.play(3); //Saat 08:00:00 ise 3. sesi  çal
  }

  if ((saat == 9 && dakika == 0) && (saniye == 0) ) {

    sound.play(4); 
  }

  if ((saat == 10 && dakika == 0) && (saniye == 0) ) {

    sound.play(5); 
  }

  if ((saat == 11 && dakika == 0) && (saniye == 0) ) {

    sound.play(6); 
  }

  if ((saat == 12 && dakika == 0) && (saniye == 0) ) {

    sound.play(7); 
  }

  if ((saat == 13 && dakika == 0) && (saniye == 0) ) {

    sound.play(8); 
  }

  if ((saat == 14 && dakika == 0) && (saniye == 0) ) {

    sound.play(9); 
  }

  if ((saat == 15 && dakika == 0) && (saniye == 0) ) {

    sound.play(10); 
  }

  if ((saat == 16 && dakika == 0) && (saniye == 0) ) {

    sound.play(11);
  }

  if ((saat == 17 && dakika == 0) && (saniye == 0) ) {

    sound.play(12); 
  }

  if ((saat == 18 && dakika == 0) && (saniye == 0) ) {

    sound.play(13);
  }

  //__Saate göre hatırlatma anonsları__Sonu================================


  // loop içerisinde bir defa çalışacak açılış anonsu...
  if (flag) {
    sound.volume(s);
    sound.play(1);
  }
  flag = 0;

}
//End of the loop...

/********************************************************************
  VOIDs__VOIDs__VOIDs__VOIDs__VOIDs__VOIDs__VOIDs__VOIDs__VOIDs__VOIDs
********************************************************************/
//Saate göre parlaklık  ayarlama
int set_bright () {

  if (saat >= 7 && saat < 21)  {
    p10_Brightness = 50;
  }
  else
  {
    p10_Brightness = 1;
  }
  return p10_Brightness;
}

/* email:bilgi@ronaer.com
   insta: https://www.instagram.com/dr.tronik2022/
   YouTube: Dr.TRonik: www.youtube.com/c/DrTRonik
*/
