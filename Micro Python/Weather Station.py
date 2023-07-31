import network
import urequests as requests
import ujson
import gzip
from machine import RTC,Pin,Timer,SPI,SoftI2C
import time
import gc
from ili9342c import ILI9342C,WHITE,BLACK,FAST,SLOW
import greeks as font
import vga2_8x16 as sfont
import _38size as bfont
import font24 as cfont
import font16
import micropython
import urandom
#weather_list
#https://devapi.qweather.com/v7/air/now?location=101281701&key=80efe4aab624454dbff95e52d6a65c03    空气质量
#https://devapi.qweather.com/v7/air/5d?location=101281701&key=80efe4aab624454dbff95e52d6a65c03    未来空气质量api
#https://devapi.qweather.com/v7/indices/1d?type=1,2&location=101010100&key=80efe4aab624454dbff95e52d6a65c03    天气指数api
weather_dic = {
        'Sunny': 'Normal','Clear':'Normal','Overcast': 'Normal', 'Cloudy': 'Normal','Fog': 'Normal','Haze': 'Normal',
        'Drizzle Rain': 'Rain', 'Light Rain': 'Rain', 'Moderate Rain': 'Rain', 'Heavy Rain': 'Rain', 'Extreme Rain': 'Rain',
        'Shower Rain': 'Rain', 'Heavy Shower Rain': 'Rain', 'Thundershower': 'Rain', 'Heavy Thunderstorm': 'Rain',
        'Rainstorm': 'Rain', 'Heavy Rainstorm': 'Rain', 'Severe Rainstorm': 'Rain', 'Freezing Rain': 'Rain',
        'Mist': 'Fog', 'Dense Fog': 'Fog', 'Strong Fog': 'Fog', 'Heavy Fog': 'Fog', 'Extra Heavy Fog': 'Fog',
        'Moderate Haze': 'Haze', 'Heavy Haze': 'Haze', 'Severe Haze': 'Haze','Few Clouds': 'Cloudy', 'Partly Cloudy': 'Cloudy',
         }
moon = ('New-moon','Waxing crescent','First quarter','Waxing gibbous','Full-moon','Waning gibbous','Last quarter','Waning crescent')
week_list = ('Mon','Tue','Wed','Thr','Fri','Sat','Sun')
buttom = Pin(15,Pin.IN,Pin.PULL_DOWN)

color = WHITE
bcolor = BLACK
tft = ILI9342C(
    SPI(2, baudrate=60000000, sck=Pin(18), mosi=Pin(23)),
    240,
    320,
    reset=Pin(33, Pin.OUT),
    cs=Pin(14, Pin.OUT),
    dc=Pin(27, Pin.OUT),
    backlight=Pin(32, Pin.OUT),
    rotation=4,
    buffer_size=240*120*10*20)
def do_connect():
    wlan = network.WLAN(network.STA_IF)
    wlan.active(True)
    if not wlan.isconnected():
        print('connecting to network...')
        wlan.connect('mz5s', 'czm423004')
        while not wlan.isconnected():
            pass
    else:
        print('network connected')
    print('network config:', wlan.ifconfig())
class weather(object):
    def __init__(self):
        self.weather_futrue_list = []
        self.weather_now = ''
        self.wapi_get = None
        self.fwapi_get = None
        self.page = ''
        self.update = False
        self.update_time = None
        
    weather_api = "https://devapi.qweather.com/v7/weather/now?location=101281701&key=80efe4aab624454dbff95e52d6a65c03&lang=en"
    future_weather_api = "https://devapi.qweather.com/v7/weather/7d?location=101281701&key=80efe4aab624454dbff95e52d6a65c03&lang=en"

    def update_date(self):
        d3 = []
        json_dic = {}
        print('数据更新了')
        self.wapi_get = self.get_weather()
        self.fwapi_get = self.future_weather()
        self.wnow = self.wapi_get[1]
        self.update = True
        for _f in range(1,4):
            d3.append(self.fwapi_get[_f][2])
        if self.wnow in weather_dic and weather_dic[self.wnow] != 'Normal':
            self.weather_now = weather_dic[self.wnow]
        elif self.wnow in weather_dic and weather_dic[self.wnow] == 'Normal':
            self.weather_now = self.wnow
        for _f in range(0,3):
            if d3[_f] in weather_dic:
                if weather_dic[d3[_f]] ==  'Normal':
                    self.weather_futrue_list.append(d3[_f])
                else:
                    self.weather_futrue_list.append(weather_dic[d3[_f]])
        last_time = RTC().datetime()
        json_dict = {'zuptime':last_time,'futrue':self.weather_futrue_list,'now':self.weather_now,'now_api':self.wapi_get,'futrue_api':self.fwapi_get}
        with open('weather_json.json','w',encoding="utf-8") as f:
            ujson.dump(json_dict,f)
        return self.weather_futrue_list,self.weather_now,self.wapi_get,self.fwapi_get
    def read_json(self):
        with open ('weather_json.json','r',encoding="utf-8") as f:
            content = f.read()
            json = ujson.loads(content)
            self.weather_futrue_list = json['futrue']
            self.weather_now = json['now']
            self.wapi_get = json['now_api']
            self.fwapi_get = json['futrue_api']
            self.update_time = json['zuptime']
            self.wnow = self.wapi_get[1]
          
    def vanupdate(self,last,now):
        toupdate = False
        if last[4] == now[4] and last[2] == now[2]:
            time_diff = now[5] - last[5]
            if time_diff > 30:
                toupdate = True
            else:
                toupdate = False
        elif last[4] != now[4]:
            toupdate = True
        return toupdate
    
    def get_time(self,request):
        rtc = RTC()
        if request == 'xxxtime':
            format_list = ['','']
            time = rtc.datetime()
            if time[1] < 10:
                format_list[0] = '0'
            else:
                format_list[0] = ''
            if time[2] < 10:
                format_list[1] = '0'
            else:
                format_list[1] = ''
            month = format_list[0] + str(time[1])
            day = format_list[1] + str(time[2])
            today = '{}-{}-{}'.format(str(time[0]),month,day)
            return today
        if request == 'd7time':
            d7_list = []
            day = rtc.datetime()[3]
            for _d in range(1,7):
                fday = day + 1 + _d
                if fday > 7:
                    fday = fday - 7
                weekday = week_list[fday-1]
                d7_list.append(weekday)
            return d7_list
        if request == 'day_night':
            time = rtc.datetime()[4]
            if 6<time<18 :
                now = 'day'
                return now
            if 18<time<24 or 0<time<6 :
                now = 'night'
                return now

    
    #daily weather
    def get_weather(self,url=weather_api):
        resp=requests.get(self.weather_api)
        if (resp.status_code==200):
            unpack = gzip.decompress(resp.content)
            data = ujson.loads(unpack.decode("utf-8"))
            code = data['code']

            if code == '200':
                    obsTime = data['now']['obsTime']
                    weather = data['now']['text']
                    temp = data['now']['temp']
                    windSpeed = data['now']['windSpeed']
                    windDir = data['now']['windDir']
                    windScale = data['now']['windScale']
                    humidity = data['now']['humidity']    #湿度
                    precip = data['now']['precip']    #降雨量
                    pressure = data['now']['pressure']
                    cloud = data['now']['cloud']
                    return obsTime,weather,temp,windSpeed,windDir,windScale,humidity,precip,pressure,cloud
            else:
                return 'Get data failed.'
            
    def future_weather(self,url=future_weather_api):
        
        resp=requests.get(self.future_weather_api)
        result_list = []
#         
        if (resp.status_code==200):
            unpack = gzip.decompress(resp.content)
            data = ujson.loads(unpack.decode("utf-8"))
            sevend = data['daily']
            code = data['code']
            if code == '200':
                for _c in range(1,8):
                    value_list = locals()['value_list{}'.format(_c)] = []
                    tempMax = locals()['tempMax{}'.format(_c)] = sevend[_c - 1]['tempMax']
                    tempMin = locals()['tempMin{}'.format(_c)] = sevend[_c - 1]['tempMin']
                    textDay = locals()['textDay{}'.format(_c)] = sevend[_c - 1]['textDay']
                    textNight = locals()['textNight{}'.format(_c)] = sevend[_c - 1]['textNight']
                    sunrise = locals()['sunrise{}'.format(_c)] = sevend[_c - 1]['sunrise']
                    sunset = locals()['sunset{}'.format(_c)] = sevend[_c - 1]['sunset']
                    moonrise = locals()['moonrise{}'.format(_c)] = sevend[_c - 1]['moonrise']
                    moonset = locals()['moonset{}'.format(_c)] = sevend[_c - 1]['moonset']
                    precip = locals()['precip{}'.format(_c)] = sevend[_c - 1]['precip']
                    uvIndex = locals()['uvIndex{}'.format(_c)] = sevend[_c - 1]['uvIndex']    #紫外线强度
                    moonPhase = locals()['moonPhase{}'.format(_c)] = sevend[_c - 1]['moonPhase'] 
                    for _h in [tempMax,tempMin,textDay,textNight,sunrise,sunset,moonrise,moonset,precip,uvIndex,moonPhase]:
                        value_list.append(_h)
                    result_list.append(value_list)
            return result_list
    #daily oneword        
    def get_daily_word(self):
        word_api = "https://apiv3.shanbay.com/weapps/dailyquote/quote/?date={}".format(self.get_time('xxxtime'))
        resp = requests.get(word_api)
        json = resp.json()
        word = json['content']
        author = json['author']
        return word,author
    
    def change_page(self):
        if self.page == 'main_page':
            self.digital_page()
        if self.page == 'star_page':
            self.main_page()
        if self.page == 'digital_page':
            self.star_page()
            
    def star_page(self):
        
        self.page = 'star_page'
        tft.fill(WHITE)
        d6 = self.get_time('d7time')
        day = self.get_time('day_night')
        tft.jpg('star/{}.jpg'.format(self.fwapi_get[0][10]),25,245,FAST)
        tft.jpg('star/{}.jpg'.format(self.fwapi_get[1][10]),90,245,FAST)
        tft.jpg('star/{}.jpg'.format(self.fwapi_get[2][10]),155,245,FAST)
        tft.jpg('star/Sunrise.jpg',15,20,FAST)
        tft.jpg('star/Sunset.jpg',15,50,FAST)
        tft.jpg('star/Moonrise.jpg',135,20,FAST)
        tft.jpg('star/Moonset.jpg',135,50,FAST)
        if day == 'day':
            tft.jpg('star/day.jpg',50,89,FAST)
        if day == 'night':
            tft.jpg('star/night.jpg',50,78,FAST)
        tft.write(font16,self.fwapi_get[0][4],65,35,35,color)
        tft.write(font16,self.fwapi_get[0][5],65,65,35,color)
        tft.write(font16,self.fwapi_get[0][6],185,35,35,color)
        tft.write(font16,self.fwapi_get[0][7],185,65,35,color)
        tft.write(font16,d6[0],45,295,35,color)
        tft.write(font16,d6[1],110,295,35,color)
        tft.write(font16,d6[2],175,295,35,color)
        while True:
            time.sleep(0.2)
            if buttom.value() == 1:
                self.change_page()
                break
    def digital_page(self):
        bk_list = ('hill','sand','sea','tree')
        color_list = ('purple','oringe','pink','green')
        title_list = {'Sunny':'Sun','Cloudy':'Clo','Rain':'Rin','Overcast':'Ovc','Clear':'Cle'}
        self.page = 'digital_page'
        
        tft.fill(WHITE)
        _random = urandom.randint(0,3)
        tft.jpg('bk/{}.jpg'.format(bk_list[_random]),52,50,FAST)
        tft.jpg('ui/{}.jpg'.format(color_list[_random]),75,10,FAST)
        tft.jpg('/icon/raindrop.jpg',169,190,FAST)
        tft.jpg('/icon/uv-index.jpg',108,190,FAST)
        tft.jpg('/icon/wind.jpg',45,190,FAST)
        tft.jpg('/icon/barometer.jpg',169,255,FAST)
        tft.jpg('/icon/temp-high.jpg',108,255,FAST)
        tft.jpg('/icon/temp-low.jpg',45,255,FAST)
        tft.write(bfont,title_list[self.wapi_get[1]],10,10,35,color)
        tft.write(font16,self.wapi_get[3],49,220,35,color)
        tft.write(font16,self.fwapi_get[0][9],112,220,35,color)
        tft.write(font16,self.wapi_get[7],173,220,35,color)
        tft.write(font16,self.fwapi_get[0][0],49,285,35,color)
        tft.write(font16,self.fwapi_get[0][1],112,285,35,color)
        tft.write(font16,self.wapi_get[8],165,285,35,color)
        while True:
            time.sleep(0.2)
            if buttom.value() == 1:
                self.change_page()
                break
        
    def main_page(self):
        print('start')
        
        self.page = 'main_page'
        d6 = self.get_time('d7time')
        try:
            with open ('weather_json.json','r',encoding="utf-8") as f:
                content = f.read()
                if len(content) == 0:
                    self.update_date()
                else:
                    json = ujson.loads(content)  
                    last_time = json["zuptime"]
            if self.vanupdate(last_time,RTC().datetime()) == True:
                self.update_date()
            else:
                self.read_json()
        except Exception as ret:
            self.update_date()
        tft.fill(WHITE)
        main_jpg = '/main-icon/{}.jpg'.format(self.weather_now.lower())
        future_1d = '/_64_icon/{}.jpg'.format(self.weather_futrue_list[0].lower())
        future_2d = '/_64_icon/{}.jpg'.format(self.weather_futrue_list[1].lower())
        future_3d = '/_64_icon/{}.jpg'.format(self.weather_futrue_list[2].lower())
        tft.jpg(main_jpg,56,35,FAST)
        tft.write(bfont,self.weather_now,13,13,35,color)
        tft.jpg('/main_other/celsius.jpg',120,155,FAST)
        tft.jpg('/main_other/humidity.jpg',85,193,FAST)
        tft.write(cfont,self.wapi_get[2],90,155,35,color)
        tft.write(cfont,self.wapi_get[6],55,196,35,color)
        if len(self.wapi_get[4]) == 2:
            tft.write(cfont,self.wapi_get[4],135,196,35,color)
            tft.jpg('/main_other/wind.jpg',171,196,FAST)
        elif len(self.wapi_get[4]) == 1:
            tft.write(cfont,self.wapi_get[4],135,196,35,color)
            tft.jpg('/main_other/wind.jpg',153,196,FAST)
        tft.line(119,200,119,213,bcolor)
        if weather_dic[self.wnow] != 'Normal' :
            pass
            tft.text(sfont,self.wnow,15,48,bcolor,color)
        else:
            tft.write(font16,'Day',20,48,35,color)
        tft.jpg(future_1d,10,240,FAST)
        tft.jpg(future_2d,83,240,FAST)
        tft.jpg(future_3d,156,240,FAST)
        tft.write(font16,d6[0],26,236,35,color)
        tft.write(font16,d6[1],105,236,35,color)
        tft.write(font16,d6[2],175,236,35,color)
        tft.write(font16,self.fwapi_get[1][0],26,300,35,color)
        tft.write(font16,self.fwapi_get[2][0],105,300,35,color)
        tft.write(font16,self.fwapi_get[3][0],175,300,35,color)
        d3 = d6[2]
        while True:
            time.sleep(0.2)
            if buttom.value() == 1:
                self.change_page()
                break
if __name__ == "__main__":
    do_connect()
    
    tft.init()
    tft.fill(WHITE)
    weather = weather()
    weather.main_page()
#     try:
#         timer_get = Timer(1)
#         timer_get.deinit()
#         timer_get.init(mode = Timer.PERIODIC ,period = 3000 ,callback=lambda t:update_date())
#     finally:
#         timer_get.deinit()

