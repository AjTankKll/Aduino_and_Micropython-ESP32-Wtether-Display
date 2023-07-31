#include <LittleFS.h>
#include <ArduinoZlib.h>
#include <ArduinoJson.h>
#include <SPI.h>
#include <time.h>
#include "TFT_eSPI.h"
// #include "FS.h"
//WLAN类库
#include <WiFi.h>
#include <HTTPClient.h>
#include <WiFiClientSecure.h>
// #include <WiFiClient.h>
// #include <MQTT.h>
// //蓝牙类库
// #include <BLEDevice.h>
// #include <BLEServer.h>
// #include <BLEUtils.h>
// #include <BLE2902.h>
#define api "your api"
#define f3dapi "your api"
#define button 40

//字体
#include "Fonts/bear.h"
#include "Fonts/mano.h"

//128位图
#include "Icons/cloudy.h"
#include "Icons/clearDay.h"
#include "Icons/clearNight.h"
#include "Icons/fog.h"
#include "Icons/haze.h"
#include "Icons/overcast.h"
#include "Icons/overcastDay.h"
#include "Icons/overcastDayRain.h"
#include "Icons/overcastNightRain.h"

//64位图
#include "Icons/cloudy64.h"
#include "Icons/clearDay64.h"
#include "Icons/fog64.h"
#include "Icons/haze64.h"
#include "Icons/overcast64.h"
#include "Icons/overcastRain64.h"

//128Pic
#include "Pic/flowerPic.h"
#include "Pic/womanPic.h"
#include "Pic/beachPic.h"

//图标
//PAGE1
#include "Icons/humidity.h"
#include "Icons/temp.h"
//PAGE 2
#include "Icons/uvindex.h"
#include "Icons/wind.h"
#include "Icons/windsock.h"
#include "Icons/barometer.h"
#include "Icons/compass.h"

//Configue
const char * SSID = "111";
const char * PSW= "12345678";

//重连参数
#define maxReckeck 25
#define maxRetry 10 
#define delayT 4000

TFT_eSPI tft = TFT_eSPI();
TFT_eSprite spr = TFT_eSprite(&tft);

struct f7dweather{
  const char * fd1;
  const char * fd2;
  const char * fd3;
  const char * fd4;
  const char * fd5;
  const char * fd6;
  const char * fd7;
};

struct Weather{
  const char * temp;
  const char * weather;
  const char * windspeed;
  const char * winddir;
  const char * humidity;
  const char * precip;
};

struct f3days{
  const char * fd1weather;
  const char * fd2weather;
  const char * fd3weather;
  const char * uvindex;
  struct f7dweather f7d;
};

//time
struct tm timeinfo;
uint8_t minute;
uint8_t hour;
uint8_t month;
uint16_t year;
uint8_t mday;

unsigned long past;
struct Weather now;
struct Weather *p = &now;
struct f3days f3d;
struct f3days *f = &f3d;

const char* normal[] = {"Sunny","Clear","Overcast","Cloudy","Fog","Haze"};
const char* rain[] = {"Drizzle Rain","Light Rain","Moderate Rain","Heavy Rain","Extreme Rain","Shower Rain","Heavy Shower Rain","Thundershower","Heavy Thunderstorm","Rainstorm","Heavy Rainstorm","Severe Rainstorm","Freezing Rain"};
const char* fog[] = {"Mist","Dense Fog","Strong Fog","Heavy Fog","Extra Heavy Fog"};
const char* haze[] = {"Moderate Haze","Heavy Haze","Severe Haze"};
const char* cloud[] = {"Few Clouds","Partly Cloudy"};

bool checkSSID(){
  WiFi.mode(WIFI_STA);
  int wnum = WiFi.scanNetworks();
  Serial.println("wlan总数" + wnum);
  delay(1000);
  for(int i = 0; i < wnum ; i++){
    if(WiFi.SSID(i) == SSID){
      return true;
    }
  }
  Serial.println("No such wlan");
  return false;
}

void stubbornConnect(){
  int retry = 0;
  Serial.println("WLAN init...");
  Serial.println(SSID);
  Serial.println(PSW);
  Serial.println("开始连接...");
  WiFi.begin(SSID,PSW);
  Serial.println("连接结束.");
  while(WiFi.status() != WL_CONNECTED && retry < maxRetry){
    Serial.println(F("Coonnecting"));
    delay(delayT);
    retry ++;
    Serial.print(WiFi.macAddress());
    Serial.printf(" => WiFi connect - Attempt No. %d\n", retry);
    if(retry == 10){
      if(checkSSID()){
        Serial.println("未扫描到该WLAN SSID,请重新配置网络！");
        return;
      }
    }
    delay(3000);
  }
  if (retry == maxRetry){
    Serial.println(F(" WLAN connection failed, please check the network!"));
    ESP.restart();
  }
  Serial.println("Connected as");
  Serial.println(WiFi.localIP());
}

uint8_t * unpack_buf;

template <typename T>
void api_get(const char * url,T *info){
  WiFiClientSecure *client = new WiFiClientSecure;
  if(client){
    client ->setInsecure();    //设置CA验证为不安全连接
    HTTPClient http;
    if(http.begin(*client,url)){
      int statue = http.GET();
      if(statue == HTTP_CODE_OK){
        WiFiClient *stream = http.getStreamPtr();    //getStreamPtr()返回一个指向HTTP响应数据流的WiFiClient指针,该指针可以直接从服务器读取数据
        Serial.println("初始化api流数据成功!");
        uint8_t gzip_buf[2048] = { 0 } ;   //uint8_t:8位无符号整数类型,跨平台使用，不同的系统字长相同，{ 0 }设置数组填充0
        if(http.connected()){
          size_t size = stream ->available();    //size_t - 无符号整数类型，表示对象大小或者内存的长度，它是sizeof运算符返回的结果类型。它的大小可以根据不同的平台和编译器而变化，
          size_t gzip_size = stream ->readBytes(gzip_buf,size);    //readBytes（储存从数据流对象中读取到的数据的数组，读取数据长度）
          unpack_buf = new uint8_t[sizeof(uint8_t) * 6000];    //outbuf=(uint8_t*)malloc(sizeof(uint8_t)*15000);    //c语言语法:堆区开辟数组内存
          uint32_t unpack_size = 0;
          int result=ArduinoZlib::libmpq__decompress_zlib(gzip_buf, gzip_size, unpack_buf, 6000,unpack_size);    //参数：（指向gzip压缩数据的指针，数据长度，指向储存解压后数据的指针，缓冲区大小，解压数据大小）
          Serial.write(unpack_buf,unpack_size);    //Serial输出buff，需指定buff长度
          Serial.println(" ");
          parseApiJs(info,(char*)unpack_buf);
          // delete[] unpack_buf;    //c语言释放堆区内存关键字：free
        }else{
          Serial.println("http连接断开!");
          return;
        }
      }else{
        Serial.println("api请求发送失败!");
        return;
      }  
    }else{
      Serial.println("HTTPClient init failed!");
      return;
    }
    delete client;
  }else{
    Serial.println("WIFIClient init failed!");
    return;
  }
}

void updateTime(){
  getLocalTime(&timeinfo);
  // year = timeinfo.tm_year + 1900;
  month = timeinfo.tm_mon + 1;
  mday = timeinfo.tm_mday;
  if(timeinfo.tm_hour == 0){
    hour = 24;
  }else{
    hour = timeinfo.tm_hour;
  }
  minute = timeinfo.tm_min;
}
//检测定时器超过20min时，调用parseApiJs函数保存文件

//now JSOON解析
void struct2js(struct Weather *info){
  StaticJsonDocument<600> obj;
  char buf[600];
  JsonObject now = obj.createNestedObject("now");
  now["temp"] = info -> temp;
  now["text"] = info -> weather;
  now["humidity"] = info -> humidity;
  now["windDir"] = info -> winddir;
  now["windSpeed"] = info -> windspeed;
  now["precip"] = info -> precip;
  serializeJson(obj,buf,600);
  // Serial.println(buf);
  if(!LittleFS.begin(true)){
    Serial.println("LittleFS初始化失败！");
    return;
  }else{
    File file = LittleFS.open("/nowJson.js",FILE_WRITE);
    if(!file){ 
      Serial.println("File初始化失败！");
      return;
    }else{
      file.println(buf);
    }
    file.close();
    LittleFS.end();
  }
}

void parseLocalJs(struct Weather *info){
  //本地获取
  StaticJsonDocument<600> weatherJson; 
  Serial.println("正在读取本地的数据...");
  File file = LittleFS.open("/nowJson.js",FILE_READ);
  if(file && file.available()){
    char* nowData = (char*)file.readStringUntil('\n').c_str();
    // Serial.println(nowData);
    file.close();
    DeserializationError error = deserializeJson(weatherJson,nowData);\
    if(error){
      Serial.println("json解析失败!");
      // api_get(api,p);
      return;
    }else{
      info->temp = weatherJson["now"]["temp"].as<const char *>();
      info->weather = weatherJson["now"]["text"].as<const char *>();
      info->humidity = weatherJson["now"]["humidity"].as<const char *>();
      info->winddir = weatherJson["now"]["windDir"].as<const char *>();
      info->windspeed = weatherJson["now"]["windSpeed"].as<const char *>();
      info->precip = weatherJson["now"]["precip"].as <const char *>();    //降水量
      Serial.println("json parse over");
    }
  }
}

void parseApiJs(struct Weather *info,char* str){
  StaticJsonDocument<600> weatherJson;
  DeserializationError error = deserializeJson(weatherJson,str);    //json解析
  if(error){
    Serial.println("json解析失败!");
    return;
  }
  info->temp = weatherJson["now"]["temp"].as<const char *>();
  info->weather = weatherJson["now"]["text"].as<const char *>();
  info->humidity = weatherJson["now"]["humidity"].as<const char *>();
  info->winddir = weatherJson["now"]["windDir"].as<const char *>();
  info->windspeed = weatherJson["now"]["windSpeed"].as<const char *>();
  info->precip = weatherJson["now"]["precip"].as <const char *>();    //降水量
  Serial.println("json parse over");
  struct2js(info);
  past = millis();

}

//f3d JSOON解析
void struct2js(struct f3days *futrue){
  DynamicJsonDocument obj(600);
  char buf[600];
  JsonArray daily = obj.createNestedArray("daily");
  JsonObject day1 = daily.createNestedObject();
  JsonObject day2 = daily.createNestedObject();
  JsonObject day3 = daily.createNestedObject();
  JsonObject day4 = daily.createNestedObject();
  JsonObject day5 = daily.createNestedObject();
  JsonObject day6 = daily.createNestedObject();
  JsonObject day7 = daily.createNestedObject();
  day1["uvIndex"] = futrue -> uvindex;
  day1["textDay"] = futrue -> fd1weather;
  day2["textDay"] = futrue -> fd2weather;
  day3["textDay"] = futrue -> fd3weather;
  day1["tempMax"] = futrue -> f7d.fd1;
  day2["tempMax"] = futrue -> f7d.fd2;
  day3["tempMax"] = futrue -> f7d.fd3;
  day4["tempMax"] = futrue -> f7d.fd4;
  day5["tempMax"] = futrue -> f7d.fd5;
  day6["tempMax"] = futrue -> f7d.fd6;
  day7["tempMax"] = futrue -> f7d.fd7;
  serializeJson(obj,buf,600);
  // Serial.println(buf);
  if(!LittleFS.begin(true)){
    Serial.println("LittleFS初始化失败！");
    return;
  }else{
    File file = LittleFS.open("/weatherJson.js",FILE_WRITE);
    if(!file){ 
      Serial.println("File初始化失败！");
      return;
    }else{
      file.println(buf);
      file = LittleFS.open("/time.txt",FILE_WRITE);
      updateTime();
      file.println(mday);
      file.println(hour);
      file.println(minute);
      // Serial.println("mday  " + String(mday));
      // Serial.println("hour  " + String(hour));
      // Serial.println("minute  " + String(minute));
    }
    file.close();
    LittleFS.end();
  }
};

void parseLocalJs(struct f3days *futrue){
  //本地获取数据
  StaticJsonDocument <1000>weatherJson;
  File file = LittleFS.open("/weatherJson.js",FILE_READ);
  if(file && file.available()){
    char* f3dData = (char*)file.readStringUntil('\n').c_str();
    Serial.println(f3dData);
    file.close();
    LittleFS.end();
    DeserializationError error = deserializeJson(weatherJson,f3dData);
    if(error){
      Serial.println("json解析失败!");
      // api_get(f3dapi,f);
      return;
    }
    futrue->fd1weather = weatherJson["daily"][0]["textDay"].as <const char *>();    //-> 运算符只能用于指针类型
    futrue->fd2weather = weatherJson["daily"][1]["textDay"].as<const char *>();
    futrue->fd3weather = weatherJson["daily"][2]["textDay"].as<const char *>();
    //now
    futrue->uvindex = weatherJson["daily"][0]["uvIndex"].as <const char *>();
    //f7d
    futrue->f7d.fd1 = weatherJson["daily"][0]["tempMax"].as<const char *>();
    futrue->f7d.fd2 = weatherJson["daily"][1]["tempMax"].as<const char *>();
    futrue->f7d.fd3 = weatherJson["daily"][2]["tempMax"].as<const char *>();
    futrue->f7d.fd4 = weatherJson["daily"][3]["tempMax"].as<const char *>();
    futrue->f7d.fd5 = weatherJson["daily"][4]["tempMax"].as<const char *>();
    futrue->f7d.fd6 = weatherJson["daily"][5]["tempMax"].as<const char *>();
    futrue->f7d.fd7 = weatherJson["daily"][6]["tempMax"].as<const char *>();
  }
  file.close();
  past = millis();
}

//3days JSOON解析
void parseApiJs(struct f3days *futrue,char* str){
  {
    DynamicJsonDocument weatherJson(3400);
    DeserializationError error = deserializeJson(weatherJson,str);
    if(error){
      Serial.println("json解析失败!");
      return;
    }
    futrue->fd1weather = weatherJson["daily"][1]["textDay"].as <const char *>();    //-> 运算符只能用于指针类型
    futrue->fd2weather = weatherJson["daily"][2]["textDay"].as<const char *>();
    futrue->fd3weather = weatherJson["daily"][3]["textDay"].as<const char *>();
    //now
    futrue->uvindex = weatherJson["daily"][0]["uvIndex"].as <const char *>();
    //f7d
    futrue->f7d.fd1 = weatherJson["daily"][0]["tempMax"].as <const char *>();
    futrue->f7d.fd2 = weatherJson["daily"][1]["tempMax"].as <const char *>();
    futrue->f7d.fd3 = weatherJson["daily"][2]["tempMax"].as <const char *>();
    futrue->f7d.fd4 = weatherJson["daily"][3]["tempMax"].as <const char *>();
    futrue->f7d.fd5 = weatherJson["daily"][4]["tempMax"].as <const char *>();
    futrue->f7d.fd6 = weatherJson["daily"][5]["tempMax"].as <const char *>();
    futrue->f7d.fd7 = weatherJson["daily"][6]["tempMax"].as <const char *>();
    Serial.println("json parse over");
    delete[] unpack_buf;
  }
  struct2js(futrue);
  past = millis();
}


bool swapCount = 0;

class showWeather{
    public:
    //参数：
    uint16_t skyblue = tft.color565(135,196,237);
    uint16_t blue = tft.color565(135,206,250);
    uint16_t liteblue = tft.color565(178,226,236);
    uint8_t picTurn = 0;
    uint8_t colorTurn = 1;  

    // 函数
    bool loop4str(const char* array[],const char* str,short length){
      for(int i = 0;i < length;i ++){
        if(strcmp(array[i],str) == 0){
          return true;
        }
      }
      return false;
      }

    const char * str4icon(const char* str){
      const char * result = nullptr;
      result = loop4str(normal,str,sizeof(normal)/sizeof(normal[0])) ?str:result;
      result = loop4str(rain,str,sizeof(rain)/sizeof(rain[0])) ?"Rainy":result;
      result = loop4str(fog,str,sizeof(fog)/sizeof(fog[0])) ?"Foggy":result;
      result = loop4str(haze,str,sizeof(haze)/sizeof(haze[0])) ?"Hazy":result;
      result = loop4str(cloud,str,sizeof(cloud)/sizeof(cloud[0])) ?"Cloudy":result;
      return result;
    }

    void pushIcon(const char* str,int32_t x = 56,int32_t y = 30,int32_t width = 128,int32_t height = 128){
      if((strcmp(str4icon(str),"Sunny") == 0 || strcmp(str4icon(str),"Clear") == 0) && (hour >= 6 && hour <= 18) ){ 
        tft.pushImage(x,y,width,height,clearDay);
      }else if((strcmp(str4icon(str),"Sunny") == 0 || strcmp(str4icon(str),"Clear") == 0)&& ((hour >= 18 && hour <= 23) || (hour >= 0 && hour <= 6))){    // 
        tft.pushImage(x,y,width,height,clearNight);
      }else if(strcmp(str4icon(str),"Overcast") == 0 && (hour >= 6 && hour <= 18)){
        tft.pushImage(x,y,width,height,overcastDay);
      }else if((strcmp(str4icon(str),"Overcast") == 0) && ((hour >= 18 && hour <= 23) || (hour >= 0 && hour <= 6))){
        tft.pushImage(x,y,width,height,overcast);
      }else if(strcmp(str4icon(str),"Cloudy") == 0){    
        tft.pushImage(x,y,width,height,cloudy);
      }else if(strcmp(str4icon(str),"Fog") == 0 || strcmp(str4icon(str),"fog") == 0){
        tft.pushImage(x,y,width,height,foggy);
      }else if(strcmp(str4icon(str),"Haze") == 0 || strcmp(str4icon(str),"Hazy") == 0){   
        tft.pushImage(x,y,width,height,hazy);
      }else if((strcmp(str4icon(str),"Rainy") == 0) && ((hour >= 18 && hour <= 23) || (hour >= 0 && hour <= 6))){
        tft.pushImage(x,y,width,height,overcastDayRain);
      }else if((strcmp(str4icon(str),"Rainy") == 0) && ((hour >= 18 && hour <= 23) || (hour >= 0 && hour <= 6))){
        tft.pushImage(x,y,width,height,overcastNightRain);
      }
    }

    void push64Icon(const char* str,int32_t x,int32_t y,int32_t width = 64,int32_t height = 64){
      if(strcmp(str4icon(str),"Sunny") == 0 || strcmp(str4icon(str),"Clear") == 0){ 
        tft.pushImage(x,y,width,height,clearDay64);
      }else if(strcmp(str4icon(str),"Overcast") == 0){
        tft.pushImage(x,y,width,height,overcast64);
      }else if(strcmp(str4icon(str),"Cloudy") == 0){    
        tft.pushImage(x,y,width,height,cloudy64);
      }else if(strcmp(str4icon(str),"Fog") == 0 || strcmp(str4icon(str),"fog") == 0){
        tft.pushImage(x,y,width,height,fog64);
      }else if(strcmp(str4icon(str),"Haze") == 0 || strcmp(str4icon(str),"Hazy") == 0){   
        tft.pushImage(x,y,width,height,haze64);
      }else if(strcmp(str4icon(str),"Rainy") == 0){
        tft.pushImage(x,y,width,height,overcastRain64);
      }
    }

    uint16_t swapColor(){
      switch(colorTurn){
        case 0:
          colorTurn++;
          return skyblue;
        case 1:
          colorTurn++;
          return blue;
        case 2:
          colorTurn = 0;
          return liteblue;
      }
    }

    uint16_t colort = swapColor();

    void drawSpriteCenterString(const char* str,int16_t weigh,int16_t height,int16_t sec = 30,uint8_t font = 4){
      str = str4icon(str);
      int16_t strlength = tft.textWidth(str,font);
      int16_t strheight = tft.fontHeight(font);
      int16_t centerX = weigh - strlength / 2;
      int16_t centerY = height - strheight / 2;
      // Serial.println(strlength);
      // Serial.println(strheight);
      // Serial.println(centerX);
      // Serial.println(centerY);
      spr.createSprite(strlength, strheight + 18);
      spr.setColorDepth(4);
      spr.fillSprite(TFT_TRANSPARENT);
      spr.setTextColor(colort,TFT_TRANSPARENT);
      spr.setTextSize(1);   
      for(int i = 1;i < 19;i++){
        spr.drawString(str,0,i,font);
        spr.pushSprite(centerX,centerY - 18);
        delay(sec);
      }
      // tft.drawCentreString(str,weigh,height,font);
      spr.deleteSprite();
    }

    void mainPage(){
      //now icon
      tft.fillScreen(TFT_BLACK);
      pushIcon(now.weather,56,39);
      tft.drawCentreString(str4icon(now.weather),120,140,0);
      //top UI
      tft.fillRoundRect(50,15,175,28,14,colort);
      tft.fillCircle(25,29,14,colort);
      //f3d info
      tft.loadFont(mano15);
      push64Icon(f3d.fd1weather,8,220);
      tft.drawCentreString(str4icon(f3d.fd1weather),40,280,0);
      push64Icon(f3d.fd2weather,88,220);
      tft.drawCentreString(str4icon(f3d.fd2weather),120,280,0);
      push64Icon(f3d.fd3weather,168,220);
      tft.drawCentreString(str4icon(f3d.fd3weather),200,280,0);
      //temp & humidity
      tft.pushImage(35,180,45,45,humidity);
      tft.pushImage(140,180,45,45,temp);
      tft.drawCentreString(now.humidity,85,195,0);
      tft.drawCentreString(now.temp,190,195,0);
      tft.drawFastVLine(125,192,17,colort);
      tft.unloadFont();
      //main info
      drawSpriteCenterString(now.weather,120,158);
    }
    uint16_t axis = 300;

    uint16_t arrayJ[2]; 
    uint16_t arrayO[2]; 
    // uint8_t (*p)[2] = array;

    void drawChartPoint(const char * str,uint8_t pointNum,uint16_t x,uint16_t y,uint16_t color = TFT_TRANSPARENT){
      int8_t temp = atoi(str);
      if(temp > 40)temp = 40;
      if(temp < 0)temp = 0;
      uint16_t chartX = (pointNum - 1)*25 + 14;    
      uint16_t chartY = 40 - temp;
      if (temp >= 0){
        for(uint8_t i = 1;i < 11;i ++){
          spr.fillCircle(chartX,chartY + 10 - i,2,colort);
          spr.pushSprite(x,y,colort);
          delay(10);
          spr.fillCircle(chartX,chartY + 10 - i + 4,2,TFT_BLACK);
          spr.pushSprite(x,y,TFT_BLACK);
        }
      }

      if((pointNum) % 2 == 0){
        arrayJ[0] = chartX;
        arrayJ[1] = chartY;
      }else{
        arrayO[0] = chartX;
        arrayO[1] = chartY;
      }
    }

    void chartLine(uint16_t a1[2],uint16_t a2[2]){
      spr.drawLine(a1[0],a1[1],a2[0],a2[1],colort);
    }

    void chartSprite(){
      //7d weather
      spr.createSprite(179, 40);    //左：40  右：200  上：270  下：310  坐标轴：300  间隔：25
      spr.setColorDepth(4);
      spr.fillSprite(TFT_TRANSPARENT);\
      drawChartPoint(f3d.f7d.fd1,1,30,270);
      drawChartPoint(f3d.f7d.fd2,2,30,270);
      chartLine(arrayJ,arrayO);
      drawChartPoint(f3d.f7d.fd3,3,30,270);
      chartLine(arrayO,arrayJ);
      drawChartPoint(f3d.f7d.fd4,4,30,270);
      chartLine(arrayJ,arrayO);
      drawChartPoint(f3d.f7d.fd5,5,30,270);
      chartLine(arrayO,arrayJ);
      drawChartPoint(f3d.f7d.fd6,6,30,270);
      chartLine(arrayJ,arrayO);
      drawChartPoint(f3d.f7d.fd7,7,30,270);
      chartLine(arrayO,arrayJ);
      drawChartPoint(f3d.f7d.fd7,8,30,270);    //多画一点8，前面代码逻辑有点问题,最后两个点之间的线连接不上   
      spr.deleteSprite();
    }

    int8_t maxTemp;
    int8_t minTemp;

    void sortTemp(){
      const char * temp[] = {f3d.f7d.fd1,f3d.f7d.fd2,f3d.f7d.fd3,f3d.f7d.fd4,f3d.f7d.fd5,f3d.f7d.fd6,f3d.f7d.fd7};
      maxTemp = atoi(temp[0]);
      minTemp = atoi(temp[0]);
      for (int8_t i = 1; i < 7; i++) {

          int8_t toint = atoi(temp[i]);
          if (toint > maxTemp) {
            maxTemp = toint;
          }

          if (toint < minTemp) {
            minTemp = toint;
          }
      }
    }

    void f3dPage(){
      //Pic
      tft.fillScreen(TFT_BLACK);
      switch(picTurn){
        case 0:
          tft.pushImage(42,42,150,148,beachPic);
          picTurn++;
          break;
        case 1:
          tft.pushImage(42,42,150,146,womanPic);
          picTurn++;
          break;
        case 2:
          tft.pushImage(42,42,150,148,flowerPic);
          picTurn = 0;
          break;
      }

      //top UI
      tft.fillRoundRect(50,15,175,28,14,colort);
      tft.fillCircle(25,29,14,colort);

      //configue
      tft.pushImage(25,180,47,47,barometer);
      tft.pushImage(135,180,47,47,compass);
      tft.pushImage(25,220,47,47,uvindex);
      tft.pushImage(135,220,47,47,wind);
      tft.loadFont(mano15);
      tft.drawCentreString(now.precip,80,197,0);
      tft.drawCentreString(now.winddir,190,197,0);
      tft.drawCentreString(f3d.uvindex,80,237,0);
      tft.drawCentreString(now.windspeed,190,237,0);
      tft.drawRoundRect(26,266,187,48,10,TFT_WHITE);
      sortTemp();
      tft.drawCentreString(String(minTemp),10,270,colort);
      tft.drawCentreString(String(maxTemp),230,270,colort);
      chartSprite();
      tft.unloadFont();
    }

    void swapPageAnime(){    //对角线长度433
      for(int i = 0;i < 440;i += 6){
        tft.fillCircle(240,360,i,skyblue);
      }
    }
};

showWeather tftShow = showWeather();
const char* jsonData;
bool loadLoocalJson = true;

void setup(){
  Serial.begin(115200);
  pinMode(button, INPUT_PULLDOWN);
  uint8_t lastTimeDay;
  uint8_t lastTimeHour; 
  uint8_t lastTimeMin; 
  tft.init();
  tft.fillScreen(TFT_BLACK);
  tft.setSwapBytes(true);
  stubbornConnect();
  configTime(8 * 3600, 0, "ntp1.aliyun.com", "ntp2.aliyun.com", "ntp3.aliyun.com");    //同步时间
  updateTime();
  tft.println("WLAN connected...");
  tft.println("Update time...");
  tft.println("Local info founded...");
  tft.setCursor(0,5);
  tft.setTextColor(TFT_WHITE);
  tft.setTextSize(1);
  if(!LittleFS.begin(true)){
    Serial.println("LittleFS初始化失败！");
    tft.println("LittleFS init failed");
  }else{
    File file = LittleFS.open("/time.txt",FILE_READ);
    if(file){
      lastTimeDay = file.readStringUntil('\n').toInt();
      lastTimeHour = file.readStringUntil('\n').toInt();
      lastTimeMin = file.readStringUntil('\n').toInt();
    }
    file.close();
    if(lastTimeDay != mday){
      loadLoocalJson = false;
    }else if(lastTimeHour != hour){
      loadLoocalJson = false;
    }else if(minute - lastTimeMin > 30){
      Serial.println(lastTimeMin - minute);
      loadLoocalJson = false;
    }
    if(LittleFS.exists("/nowJson.js") && LittleFS.exists("/weatherJson.js") && loadLoocalJson == true){    //启动时间大于20min更新
      File file = LittleFS.open("/nowJson.js",FILE_READ);
      if(file.available()){
        Serial.println("检测到本地文件...");
        tft.println("Local info founded...");
        parseLocalJs(p);
        // parseLocalJs(f);
      }
      file = LittleFS.open("/weatherJson.js",FILE_READ);
      if(file.available()){
        if(file.available()){
        Serial.println("检测到本地文件...");
        tft.println("Local info founded...");
        parseLocalJs(f);
        }
      }
      // LittleFS.end();
    }else{
      Serial.println("从api获取数据...");
      // tft.println("Get info from API...");
      api_get(api,p);
      api_get(f3dapi,f);
    }
  }
  Serial.println("show start!");
  tftShow.mainPage();
}

void loop(){
  unsigned long now = millis();
  if(digitalRead(button) == HIGH && swapCount == 0){
    tftShow.swapPageAnime();
    tftShow.f3dPage();
    swapCount = 1;
    delay(70);
  }else if(digitalRead(button) == HIGH && swapCount == 1){
    tftShow.swapPageAnime();
    tftShow.mainPage();
    delay(70);
    swapCount = 0;
  }
  if(now - past == 1200000 ){   //20分钟：1200000    
    Serial.println("更新");
    api_get(api,p);
    api_get(f3dapi,f);
    getLocalTime(&timeinfo);
  }
  if(WiFi.status() != WL_CONNECTED){
    stubbornConnect();
  }
} 
