/**********************************************************************
Project             :esp8266 connect internet and disp weather time by oled 
Program name        : 
Team                :
Author              : zgh
Date                : 20210630
Purpose             : 

***********************************************************************/
#include <ESP8266WiFi.h>          
#include <DNSServer.h>
#include <ESP8266WebServer.h>
#include <WiFiManager.h>         
#include <TimeLib.h>
#include <WiFiUdp.h>
#include <ArduinoJson.h>
#include <Wire.h>  // Only needed for Arduino 1.6.5 and earlier
#include "SSD1306Wire.h" // legacy include: `#include "SSD1306.h"`

// Include the UI lib
#include "OLEDDisplayUi.h"

// Include custom images
#include "images.h"



// NTP Servers:
static const char ntpServerName[] = "ntp1.aliyun.com";
const int timeZone = 8;     // set beijing Time

WiFiUDP Udp;
unsigned int localPort = 8888;  // local port to listen for UDP packets

time_t getNtpTime();
void digitalClockDisplay();
void printDigits(int digits);
void sendNTPpacket(IPAddress &address);

const char* host = "api.seniverse.com";     // 将要连接的服务器地址  
const int httpPort = 80;                    // 将要连接的服务器端口      


// 心知天气HTTP请求所需信息**********************************************
String reqUserKey = "SsrLtEr7KnYnXJZ-o";   // 私钥
String reqLocation = "ip";            // 城市
String reqUnit = "c";                      // 摄氏/华氏
struct weather_back{
  String weather_name;
  int weather_code;
  int temperature;
  }weather_now;

//oled 设置*************************************************************
// Initialize the OLED display using brzo_i2c
// D3 -> SDA
// D5 -> SCL
// SSD1306Brzo display(0x3c, D3, D5);
// or
// SH1106Brzo  display(0x3c, D3, D5);
// Initialize the OLED display using Wire library
//SSD1306Wire  display(0x3c, D2, D1);/
SSD1306Wire  display(0x3c, 0, 2);


OLEDDisplayUi ui ( &display );

int screenW = 128;
int screenH = 64;
int clockCenterX = screenW/2;
int clockCenterY = ((screenH-16)/2)+16;   // top yellow part is 16 px height
int clockRadius = 23;

// utility function for digital clock display: prints leading 0
String twoDigits(int digits);

void clockOverlay(OLEDDisplay *display, OLEDDisplayUiState* state) {
}

void drawProgress(OLEDDisplay *display, int percentage, String label);

void analogClockFrame(OLEDDisplay *display, OLEDDisplayUiState* state, int16_t x, int16_t y);

void digitalClockFrame(OLEDDisplay *display, OLEDDisplayUiState* state, int16_t x, int16_t y);

void drawWeather_picture(OLEDDisplay *display, OLEDDisplayUiState* state, int16_t x, int16_t y);

//计算剩余天数
int clock_date[] = {2021,12,25,8,0};//计时日期{年,月,日,小时,分钟}
int year_alldays(int year);
int year_sumday(int year,int month);
int get_g_allminutes(int year,int month,int day, int hour,int minute);

void draw_Days_Remaining(OLEDDisplay *display, OLEDDisplayUiState* state, int16_t x, int16_t y);

// This array keeps function pointers to all frames
// frames are the single views that slide in
FrameCallback frames[] = { analogClockFrame, digitalClockFrame ,drawWeather_picture,draw_Days_Remaining};

// how many frames are there?
int frameCount = 4;

// Overlays are statically drawn on top of a frame eg. a clock
OverlayCallback overlays[] = { clockOverlay };
int overlaysCount = 1;







void setup() 
{
    Serial.begin(115200);       
    // The ESP is capable of rendering 60fps in 80Mhz mode
    // but that won't give you much time for anything else
    // run it in 160Mhz mode or just set it to 30 fps
    ui.setTargetFPS(60);
  
    // Customize the active and inactive symbol
    ui.setActiveSymbol(activeSymbol);
    ui.setInactiveSymbol(inactiveSymbol);
  
    // You can change this to
    // TOP, LEFT, BOTTOM, RIGHT
    ui.setIndicatorPosition(TOP);
  
    // Defines where the first frame is located in the bar.
    ui.setIndicatorDirection(LEFT_RIGHT);
  
    // You can change the transition that is used
    // SLIDE_LEFT, SLIDE_RIGHT, SLIDE_UP, SLIDE_DOWN
    ui.setFrameAnimation(SLIDE_LEFT);
  
    // Add frames
    ui.setFrames(frames, frameCount);
  
    // Add overlays
    ui.setOverlays(overlays, overlaysCount);
  
    // Initialising the UI will init the display too.
    ui.init();
  
    display.flipScreenVertically();

    //开机启动
    int process_num;
    for (process_num=0;process_num<=100;process_num++){
      drawProgress(&display,process_num,"starting ···  "+String(process_num)+"%");
      delay(10);
    }
    drawProgress(&display,100,"OK       ···  100%");
    delay(1000);

    
    //显示配置网络信息
    display.clear();
    display.setTextAlignment(TEXT_ALIGN_CENTER);
    display.setFont(ArialMT_Plain_10);
    display.drawString(clockCenterX, clockCenterY-35 , "note:if you use it firstly," );
    display.drawString(clockCenterX, clockCenterY-25 , "configure the " );
    display.drawString(clockCenterX, clockCenterY-15, "internet by AP :" );
    display.drawString(clockCenterX, clockCenterY-5 , "'AutoConnectAP'" );
    display.display();

   
   // 建立WiFiManager对象
   WiFiManager wifiManager;
  
  // 清除ESP8266所存储的WiFi连接信息以便测试WiFiManager工作效果
  // wifiManager.resetSettings();
    //Serial.println("ESP8266 WiFi Settings Cleared");
    wifiManager.autoConnect("'AutoConnectAP'");//自动添加连接wifi
    
    
    // 以上语句中的12345678是连接AutoConnectAP的密码
    
    // WiFi连接成功后将通过串口监视器输出连接成功信息 
    Serial.println(""); 
    Serial.print("ESP8266 Connected to ");
    Serial.println(WiFi.SSID());              // WiFi名称
    Serial.print("IP address:\t");
    Serial.println(WiFi.localIP());           // IP
    //连接成功显示
    for (process_num=0;process_num<=50;process_num++){
      drawProgress(&display,process_num,"connect wifi ···  "+String(process_num)+"%");
      delay(10);
    }
    
    //start udp
    Serial.println("Starting UDP");
    Udp.begin(localPort);
    Serial.print("Local port: ");
    Serial.println(Udp.localPort());
    Serial.println("waiting for sync");
    setSyncProvider(getNtpTime);
    setSyncInterval(300);
    for (process_num=50;process_num<=80;process_num++){
      drawProgress(&display,process_num,"get time ···      "+String(process_num)+"%");
      delay(10);
    }
    for (process_num=80;process_num<=100;process_num++){
      drawProgress(&display,process_num,"get weather ···   "+String(process_num)+"%");
      delay(10);
    }        
    drawProgress(&display,100,"OK               100%");
    delay(2000);  
}

time_t prevDisplay = 0; // when the digital clock was displayed
// 建立心知天气API当前天气请求资源地址
String reqRes = "/v3/weather/now.json?key=" + reqUserKey +
                + "&location=" + reqLocation + 
                "&language=en&unit=" +reqUnit;

void loop() 
{
    // 向心知天气服务器服务器请求信息并对信息进行解析
    httpRequest(reqRes);
    while (1)
    {
        int remainingTimeBudget = ui.update();

        if (remainingTimeBudget > 0) 
        {
          // You can do some work here
          // Don't do stuff if you are below your
          // time budget.
          delay(remainingTimeBudget);
        }
        if ((minute()%2 == 0) &&(second()%60 == 0))
          break;
      }
    delay(1000);
}

/*-------- NTP code ----------*/

const int NTP_PACKET_SIZE = 48; // NTP time is in the first 48 bytes of message
byte packetBuffer[NTP_PACKET_SIZE]; //buffer to hold incoming & outgoing packets

time_t getNtpTime()
{
  IPAddress ntpServerIP; // NTP server's ip address

  while (Udp.parsePacket() > 0) ; // discard any previously received packets
  Serial.println("Transmit NTP Request");
  // get a random server from the pool
  WiFi.hostByName(ntpServerName, ntpServerIP);
  Serial.print(ntpServerName);
  Serial.print(": ");
  Serial.println(ntpServerIP);
  sendNTPpacket(ntpServerIP);
  uint32_t beginWait = millis();
  while (millis() - beginWait < 1500) {
    int size = Udp.parsePacket();
    if (size >= NTP_PACKET_SIZE) {
      Serial.println("Receive NTP Response");
      Udp.read(packetBuffer, NTP_PACKET_SIZE);  // read packet into the buffer
      unsigned long secsSince1900;
      // convert four bytes starting at location 40 to a long integer
      secsSince1900 =  (unsigned long)packetBuffer[40] << 24;
      secsSince1900 |= (unsigned long)packetBuffer[41] << 16;
      secsSince1900 |= (unsigned long)packetBuffer[42] << 8;
      secsSince1900 |= (unsigned long)packetBuffer[43];
      return secsSince1900 - 2208988800UL + timeZone * SECS_PER_HOUR;
    }
  }
  Serial.println("No NTP Response :-(");
  return 0; // return 0 if unable to get the time
}

// send an NTP request to the time server at the given address
void sendNTPpacket(IPAddress &address)
{
  // set all bytes in the buffer to 0
  memset(packetBuffer, 0, NTP_PACKET_SIZE);
  // Initialize values needed to form NTP request
  // (see URL above for details on the packets)
  packetBuffer[0] = 0b11100011;   // LI, Version, Mode
  packetBuffer[1] = 0;     // Stratum, or type of clock
  packetBuffer[2] = 6;     // Polling Interval
  packetBuffer[3] = 0xEC;  // Peer Clock Precision
  // 8 bytes of zero for Root Delay & Root Dispersion
  packetBuffer[12] = 49;
  packetBuffer[13] = 0x4E;
  packetBuffer[14] = 49;
  packetBuffer[15] = 52;
  // all NTP fields have been given values, now
  // you can send a packet requesting a timestamp:
  Udp.beginPacket(address, 123); //NTP requests are to port 123
  Udp.write(packetBuffer, NTP_PACKET_SIZE);
  Udp.endPacket();
}

/******************https****************/
// 向心知天气服务器服务器请求信息并对信息进行解析
void httpRequest(String reqRes){
  WiFiClient client;
 
  // 建立http请求信息
  String httpRequest = String("GET ") + reqRes + " HTTP/1.1\r\n" + 
                              "Host: " + host + "\r\n" + 
                              "Connection: close\r\n\r\n";
 
  // 尝试连接服务器
  if (client.connect(host, 80)){
 
    // 向服务器发送http请求信息
    client.print(httpRequest);
 
    // 获取并显示服务器响应状态行 
    String status_response = client.readStringUntil('\n');
 
    // 使用find跳过HTTP响应头
    if (client.find("\r\n\r\n")) {
    }
    
    // 利用ArduinoJson库解析心知天气响应信息
      parseInfo(client); 
  } else {
  }   
  //断开客户端与服务器连接工作
  client.stop(); 
}
 

// 利用ArduinoJson库解析心知天气响应信息
void parseInfo(WiFiClient client){
  const size_t capacity = JSON_ARRAY_SIZE(1) + JSON_OBJECT_SIZE(1) + 2*JSON_OBJECT_SIZE(3) + JSON_OBJECT_SIZE(6) + 230;
  DynamicJsonDocument doc(capacity);
  
  deserializeJson(doc, client);
  
  JsonObject results_0 = doc["results"][0];
  JsonObject results_0_now = results_0["now"];
  const char* results_0_now_text = results_0_now["text"]; // "Sunny"
  const char* results_0_now_code = results_0_now["code"]; // "0"
  const char* results_0_now_temperature = results_0_now["temperature"]; // "32"
  
  const char* results_0_last_update = results_0["last_update"]; // "2020-06-02T14:40:00+08:00" 
 
  // 赋值结构体weather_now
  String results_0_now_text_str = results_0_now["text"].as<String>();
  int results_0_now_code_int = results_0_now["code"].as<int>(); 
  int results_0_now_temperature_int = results_0_now["temperature"].as<int>(); 
  weather_now.weather_name = results_0_now_text_str;
  if (results_0_now_code_int == 99)
    results_0_now_code_int = 39;
  weather_now.weather_code = results_0_now_code_int;
  weather_now.temperature = results_0_now_temperature_int;
}
/******************************************draw_picture*****************************************/
String twoDigits(int digits){
  if(digits < 10) {
    String i = '0'+String(digits);
    return i;
  }
  else {
    return String(digits);
  }
}


void drawProgress(OLEDDisplay *display, int percentage, String label) {
  display->clear();
  display->setTextAlignment(TEXT_ALIGN_CENTER);
  display->setFont(ArialMT_Plain_10);
  display->drawString(64, 10, label);
  display->drawProgressBar(2, 28, 124, 10, percentage);
  display->display();
}


void analogClockFrame(OLEDDisplay *display, OLEDDisplayUiState* state, int16_t x, int16_t y) {
//  ui.disableIndicator();

  // Draw the clock face
  //display->drawCircle(clockCenterX + x, clockCenterY + y, clockRadius);
  display->drawCircle(clockCenterX + x, clockCenterY + y, 2);
  //
  //hour ticks
  for( int z=0; z < 360;z= z + 30 ){
  //Begin at 0° and stop at 360°
    float angle = z ;
    angle = ( angle / 57.29577951 ) ; //Convert degrees to radians
    int x2 = ( clockCenterX + ( sin(angle) * clockRadius ) );
    int y2 = ( clockCenterY - ( cos(angle) * clockRadius ) );
    int x3 = ( clockCenterX + ( sin(angle) * ( clockRadius - ( clockRadius / 8 ) ) ) );
    int y3 = ( clockCenterY - ( cos(angle) * ( clockRadius - ( clockRadius / 8 ) ) ) );
    display->drawLine( x2 + x , y2 + y , x3 + x , y3 + y);
  }

  // display second hand
  float angle = second() * 6 ;
  angle = ( angle / 57.29577951 ) ; //Convert degrees to radians
  int x3 = ( clockCenterX + ( sin(angle) * ( clockRadius - ( clockRadius / 5 ) ) ) );
  int y3 = ( clockCenterY - ( cos(angle) * ( clockRadius - ( clockRadius / 5 ) ) ) );
  display->drawLine( clockCenterX + x , clockCenterY + y , x3 + x , y3 + y);
  //
  // display minute hand
  angle = minute() * 6 ;
  angle = ( angle / 57.29577951 ) ; //Convert degrees to radians
  x3 = ( clockCenterX + ( sin(angle) * ( clockRadius - ( clockRadius / 4 ) ) ) );
  y3 = ( clockCenterY - ( cos(angle) * ( clockRadius - ( clockRadius / 4 ) ) ) );
  display->drawLine( clockCenterX + x , clockCenterY + y , x3 + x , y3 + y);
  //
  // display hour hand
  angle = hour() * 30 + int( ( minute() / 12 ) * 6 )   ;
  angle = ( angle / 57.29577951 ) ; //Convert degrees to radians
  x3 = ( clockCenterX + ( sin(angle) * ( clockRadius - ( clockRadius / 2 ) ) ) );
  y3 = ( clockCenterY - ( cos(angle) * ( clockRadius - ( clockRadius / 2 ) ) ) );
  display->drawLine( clockCenterX + x , clockCenterY + y , x3 + x , y3 + y);
}

void digitalClockFrame(OLEDDisplay *display, OLEDDisplayUiState* state, int16_t x, int16_t y) {
  String timenow = String(hour())+":"+twoDigits(minute())+":"+twoDigits(second());
  String datenow = String(year()) + "-" + twoDigits(month()) + "-" + twoDigits(day());
  String weekday_string[] = {"Sunday", "Monday", "Tuesday", "Wednesday", "Thursday", "Friday", "Saturday" };
  display->setTextAlignment(TEXT_ALIGN_CENTER);
  display->setFont(ArialMT_Plain_16);
  display->drawString(clockCenterX + x  , clockCenterY + y - 30, datenow );
  display->drawString(clockCenterX + x , clockCenterY + y, timenow );
  display->setFont(ArialMT_Plain_10);
  display->drawString(clockCenterX + x  , clockCenterY + y - 13, weekday_string[weekday()-1] );
}

void drawWeather_picture(OLEDDisplay *display, OLEDDisplayUiState* state, int16_t x, int16_t y) {
  // draw an xbm image.
  // Please note that everything that should be transitioned
  // needs to be drawn relative to x and y
  String temperature = "Temp: " + String(weather_now.temperature) + " C"; 
  display->setTextAlignment(TEXT_ALIGN_CENTER);
  display->setFont(ArialMT_Plain_10);
  display->drawXbm(x + 2, y + 8, weather_Logo_width, weather_Logo_height, &picture_code[weather_now.weather_code][0]);
  display->drawString(x + 36+45  , y + 20, weather_now.weather_name);
  display->drawString(x + 36+45  , y + 25+15, temperature);
}

//计算剩余天数
//获取公历年的天数
int year_alldays(int year){
  if((year%4==0 && year%100!=0) || year%400==0) return 366; else return 365;
}

//获取公历年初至某整月的天数
int year_sumday(int year,int month){
  int sum=0;
  int rui[12]={31,29,31,30,31,30,31,31,30,31,30,31};
  int ping[12]={31,28,31,30,31,30,31,31,30,31,30,31};
  int ruiflag=0;
  if((year%4==0 &&year%100!=0) || year%400==0) ruiflag=1;
  for(int index=0;index<month-1;index++)
  {
      if(ruiflag==1) sum+=rui[index];else sum+=ping[index];
  }
  return sum;
}

//获取从公历1800年1月25日至当前日期的总天数
int get_g_allminutes(int year,int month,int day, int hour,int minute)
{
    int total_day,total_min;
    int i=1800,days=-24;
    while(i<year)
    {
        days+=year_alldays(i);
        i++;
    }
    int days2=year_sumday(year,month);

    total_day =  days+days2+day;
    total_min = total_day*24*60 + hour*60 + minute;
    return total_min;
}

void draw_Days_Remaining(OLEDDisplay *display, OLEDDisplayUiState* state, int16_t x, int16_t y){
  int total_minutes = get_g_allminutes(clock_date[0],clock_date[1],clock_date[2],clock_date[3],clock_date[4]) -  get_g_allminutes(year(),month(),day(),hour(),minute());
  int remain_days = total_minutes/(24*60);
  int remain_hours = (total_minutes%(24*60))/60;
  int remain_minutes = (total_minutes%(24*60))%60;
  display->setTextAlignment(TEXT_ALIGN_CENTER);
  display->setFont(ArialMT_Plain_10);
  display->drawString(clockCenterX + x  , clockCenterY + y -20 , "Times Remaining:     ");
  display->drawString(clockCenterX + x  , clockCenterY + y -10 , "              " + String(remain_days)+"days");
  display->drawString(clockCenterX + x  , clockCenterY + y   ,   "              " + String(remain_hours) +"hours");
  display->drawString(clockCenterX + x  , clockCenterY + y +10 , "              " + String(remain_minutes)+"minutes" );
}
