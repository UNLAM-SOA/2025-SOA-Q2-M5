/*******************************************************
 * Conveyor Classifier – ESP32 + FreeRTOS + MQTT + WiFi
 * Generado por IA (M365 Copilot)
 *******************************************************/

#include <ESP32Servo.h>
#include <WiFi.h>
#include <PubSubClient.h>


#include "Metrics.h"        // <-- NUEVO
#define SAMPLING_TIME 10000 // <-- NUEVO (10 s)


// ====================== LOGGING ======================
#define SERIAL_BAUD 9600
#define LOG_ENABLED 1
#if LOG_ENABLED
  #define LogInfo(msg)  { Serial.printf("[INFO] %s\n", msg); }
  #define LogState()    { Serial.printf("[STATE] %s\n", states_s[current_state].c_str()); }
  #define LogEvent()    { Serial.printf("[EVENT] %s\n", events_s[current_event].c_str()); }
#else
  #define LogInfo(msg)
  #define LogState()
  #define LogEvent()
#endif

// ====================== PINOUT =======================
#define PIN_TRIG_S1   22
#define PIN_ECHO_S1   21
#define PIN_TRIG_S2   27
#define PIN_ECHO_S2   26

#define PIN_SERVO     33

#define PIN_TCS_S0    23
#define PIN_TCS_S1    32
#define PIN_TCS_S2    25
#define PIN_TCS_S3    12
#define PIN_TCS_OUT   4

#define PIN_MOTOR_PWM 19
#define PIN_MOTOR_D1  18
#define PIN_MOTOR_D2  5

#define PIN_BTN_RST   13  
#define PIN_BTN_STOP  14  

#define PIN_LED_RED   2
#define PIN_LED_GRN   15
#define PIN_LED_YEL   3   // RX0 (ajusta si interfiere)

// ====================== TIMINGS ======================
#define US_SETTLE_US      2
#define US_TRIG_US        10
#define SOUND_DIV_2       0.01723f   // cm/us

#define DIST_ON_CM        5
#define DIST_OFF_CM       15

#define TMO_COLOR_MS      5000UL
#define TMO_END_MS        8000UL

#define STK_1K            1024
#define STK_2K            2048
#define STK_4K            4096

#define LED_TASK_PRIO     1
#define SENS_TASK_PRIO    1

#define QUEUE_EVENTS_SZ   12
#define QUEUE_LED_SZ      1

#define LED_BLINK_MS      200         // blink normal
#define LED_FAULT_SHORT   120         // patrón fault: 3 cortos + 1 largo
#define LED_FAULT_LONG    400

#define PERIOD_SENS_MS    100
#define PERIOD_COLOR_MS   30
#define STAB_COLOR_MS     15

// =================== COLOR NORMALIZATION ===============
#define RMIN 70
#define RMAX 935
#define BMIN 70
#define BMAX 800
#define GMIN 70
#define GMAX 960
#define COLOR_DOMINANCE   1.05f
#define COLOR_DIFF        0.10f
#define COLOR_PULSE_TMO   50000UL
#define COLOR_STABLE_READS 2

// =================== SERVO POS =========================
#define SERVO_POS_RED   0
#define SERVO_POS_BLUE  125

// =================== WIFI / MQTT =======================
const char* WIFI_SSID   = "Telecentro-0fd8";
const char* WIFI_PASS   = "";

const char* MQTT_SERVER = "io.adafruit.com";
const int   MQTT_PORT   = 1883;
const char* MQTT_USER   = "prodlineclassifier";
const char* MQTT_TOKEN  = "";
const char* MQTT_CLIENT = "conveyorBelt";


unsigned long initTime = 0;      // <-- NUEVO
unsigned long actualTime = 0;    // <-- NUEVO
bool metricsFinished = false;    // <-- NUEVO


// TOPICS (se mantienen iguales)
const char* TOPIC_MOTOR_SPEED    = "prodlineclassifier/feeds/dcengine";      // IN: 1–100
const char* TOPIC_SYSTEM_STATE   = "prodlineclassifier/feeds/systemstatus";  // PUB y también IN (STOP/RESTART)
const char* TOPIC_COLOR_DETECTED = "prodlineclassifier/feeds/colorsensor";   // PUB
const char* TOPIC_SERVO_POSITION = "prodlineclassifier/feeds/servo";         // PUB
const char* TOPIC_DIST_S1        = "prodlineclassifier/feeds/distancesensor1";// PUB
const char* TOPIC_DIST_S2        = "prodlineclassifier/feeds/distancesensor2";// PUB

// ====================== TIPOS =========================
typedef enum { NONE=0, RED, BLUE, UNKNOWN } DetectedColor;

// Estados 
typedef enum {
  ST_WAIT = 0,       // esperando
  ST_RUN,            // moviendo
  ST_PAUSE,          // stop manual
  ST_FAULT,          // error
  ST_SORTED,         // color detectado, esperando fin
  MAX_STATES_ENUM
} states;

// Eventos 
typedef enum {
  EV_START_OBJ = 0,  // objeto detectado al inicio
  EV_SEE_RED,
  EV_SEE_BLUE,
  EV_BAD_COLOR,
  EV_BTN_STOP,       // ISR
  EV_BTN_RST,        // polling
  EV_TMO,            // timeout
  EV_AT_END,         // detectado al final
  EV_TICK,           // continuo
  EV_NET_STOP,       // remoto
  EV_NET_RST,        // remoto
  MAX_EVENTS_ENUM
} events;

String states_s[] = { "ST_WAIT","ST_RUN","ST_PAUSE","ST_FAULT","ST_SORTED" };
String events_s[] = {
  "EV_START_OBJ","EV_SEE_RED","EV_SEE_BLUE","EV_BAD_COLOR","EV_BTN_STOP",
  "EV_BTN_RST","EV_TMO","EV_AT_END","EV_TICK","EV_NET_STOP","EV_NET_RST"
};

// Ultrasonido
struct DistanceSensor {
  int pin_echo;
  int pin_trigger;
  long current_value;
  long prev_value;
};

// ===================== GLOBALES =======================
WiFiClient      wifiClient;
PubSubClient    mqtt(wifiClient);
Servo           Servo1;

QueueHandle_t   eventQueue;
QueueHandle_t   xQueueLed;

TaskHandle_t    tColorCalib = nullptr;

DistanceSensor  dist[2];

volatile states current_state = ST_WAIT;
volatile events current_event = EV_TICK;

volatile bool flagColorPending = false;
volatile bool flagEndPending   = false;

unsigned long tMoveStart = 0;

// PWM (0–255). Se mantiene interfaz del motor/servo.
int beltSpeedPwm = 180;

int redBase=0, blueBase=0, greenBase=0;
bool colorCalibrated = false;

// ===================== TELEMETRÍA (Declaración + Implementación) =====================
// *** CORRECCIÓN: Telemetry debe existir antes de ser usada en t_color, t_color_calib, etc. ***
namespace Telemetry {
  void pubState();
  void pubColor(const String& c);
  void pubServo(int pos);
  void pubDistS1(int v);
  void pubDistS2(int v);
  void pubCalib(bool on);
}

void Telemetry::pubState(){
  if(!mqtt.connected()) return;
  String payload = "{\"state\":\"" + states_s[current_state] + "\",\"ts\":" + String(millis()) + "}";
  mqtt.publish(TOPIC_SYSTEM_STATE, payload.c_str());
}
void Telemetry::pubColor(const String& c){
  if(!mqtt.connected()) return;
  String payload = "{\"color\":\"" + c + "\",\"confidence\":0.92}";
  mqtt.publish(TOPIC_COLOR_DETECTED, payload.c_str());
}
void Telemetry::pubServo(int pos){
  if(!mqtt.connected()) return;
  String payload = String("{\"servo\":") + String(pos) + "}";
  mqtt.publish(TOPIC_SERVO_POSITION, payload.c_str());
}
void Telemetry::pubDistS1(int v){
  if(!mqtt.connected()) return;
  String payload = String("{\"dist\":{\"s1\":") + String(v) + "}}";
  mqtt.publish(TOPIC_DIST_S1, payload.c_str());
}
void Telemetry::pubDistS2(int v){
  if(!mqtt.connected()) return;
  String payload = String("{\"dist\":{\"s2\":") + String(v) + "}}";
  mqtt.publish(TOPIC_DIST_S2, payload.c_str());
}
void Telemetry::pubCalib(bool on){
  if(!mqtt.connected()) return;
  String payload = String("{\"calib\":") + (on?"true":"false") + "}";
  mqtt.publish(TOPIC_SYSTEM_STATE, payload.c_str());
}

// ===== WRAPPERS DE COMPATIBILIDAD  =====
// Evitan cambiar llamadas legacy ya presentes en el código.
inline void publish_state() { Telemetry::pubState(); }
inline void publish_color(const String& c) { Telemetry::pubColor(c); }
inline void publish_servo_position(int pos) { Telemetry::pubServo(pos); }
inline void publish_distance_sensor1(int v) { Telemetry::pubDistS1(v); }
inline void publish_distance_sensor2(int v) { Telemetry::pubDistS2(v); }
inline void publish_distance_s1(int v) { Telemetry::pubDistS1(v); }
inline void publish_distance_s2(int v) { Telemetry::pubDistS2(v); }

// ===================== LED ROJO (tarea) =====================
typedef enum { LED_CMD_ON, LED_CMD_OFF, LED_CMD_BLINK } LedCommand;
void led_red_on()  { digitalWrite(PIN_LED_RED, HIGH); }
void led_red_off() { digitalWrite(PIN_LED_RED, LOW);  }

void turn_red_led_on()  { LedCommand c=LED_CMD_ON;   xQueueOverwrite(xQueueLed,&c); }
void turn_red_led_off() { LedCommand c=LED_CMD_OFF;  xQueueOverwrite(xQueueLed,&c); }
void blink_red_led()    { LedCommand c=LED_CMD_BLINK;xQueueOverwrite(xQueueLed,&c); }

void led_task(void* pv) {
  LedCommand cmd;
  for(;;){
    if (xQueueReceive(xQueueLed,&cmd,portMAX_DELAY)) {
      if (cmd==LED_CMD_ON)       { led_red_on();  continue; }
      if (cmd==LED_CMD_OFF)      { led_red_off(); continue; }
      if (cmd==LED_CMD_BLINK) {
        // Patrón especial si estamos en FAULT: 3 cortos + 1 largo
        if (current_state==ST_FAULT) {
          for(;;){
            for(int i=0;i<3;i++){
              led_red_on();  vTaskDelay(pdMS_TO_TICKS(LED_FAULT_SHORT));
              led_red_off(); vTaskDelay(pdMS_TO_TICKS(LED_FAULT_SHORT));
            }
            led_red_on();    vTaskDelay(pdMS_TO_TICKS(LED_FAULT_LONG));
            led_red_off();   vTaskDelay(pdMS_TO_TICKS(LED_FAULT_LONG));
            // cortocircuito si llega nuevo comando
            LedCommand next;
            if (xQueueReceive(xQueueLed,&next,0)==pdTRUE){ cmd=next; break; }
          }
          continue;
        }
        // Blink normal
        do {
          led_red_on();  vTaskDelay(pdMS_TO_TICKS(LED_BLINK_MS));
          led_red_off(); vTaskDelay(pdMS_TO_TICKS(LED_BLINK_MS));
        } while (xQueueReceive(xQueueLed,&cmd,0)==pdFALSE);
      }
    }
  }
}

// Otros LEDs (directos)
void turn_green_led_on()  { digitalWrite(PIN_LED_GRN,HIGH); }
void turn_green_led_off() { digitalWrite(PIN_LED_GRN,LOW);  }
void turn_yellow_led_on() { digitalWrite(PIN_LED_YEL,HIGH); }
void turn_yellow_led_off(){ digitalWrite(PIN_LED_YEL,LOW);  }

// ===================== ACTUADORES =====================
void move_servo(int angle) { Servo1.write(angle); }

void start_engine() {
  digitalWrite(PIN_MOTOR_D1,LOW);
  digitalWrite(PIN_MOTOR_D2,HIGH);
  analogWrite(PIN_MOTOR_PWM,beltSpeedPwm);
}

void stop_engine() {
  digitalWrite(PIN_MOTOR_D1,LOW);
  digitalWrite(PIN_MOTOR_D2,LOW);
  analogWrite(PIN_MOTOR_PWM,0);
}

// ===================== COLOR SENSOR ===================
int readFreq(bool s2,bool s3){
  digitalWrite(PIN_TCS_S2,s2);
  digitalWrite(PIN_TCS_S3,s3);
  return pulseIn(PIN_TCS_OUT,LOW,COLOR_PULSE_TMO);
}
int getRed()  { return readFreq(LOW,LOW);   }
int getBlue() { return readFreq(LOW,HIGH);  }
int getGreen(){ return readFreq(HIGH,HIGH); }

DetectedColor readColor(){
  if(!colorCalibrated) return NONE;

  int rF=getRed();   int r=map(rF,RMIN,RMAX,255,0); vTaskDelay(pdMS_TO_TICKS(STAB_COLOR_MS));
  int gF=getGreen(); int g=map(gF,GMIN,GMAX,255,0); vTaskDelay(pdMS_TO_TICKS(STAB_COLOR_MS));
  int bF=getBlue();  int b=map(bF,BMIN,BMAX,255,0); vTaskDelay(pdMS_TO_TICKS(STAB_COLOR_MS));

  // ausencia de objeto
  if (abs(r-redBase)<redBase*COLOR_DIFF &&
      abs(b-blueBase)<blueBase*COLOR_DIFF &&
      abs(g-greenBase)<greenBase*COLOR_DIFF) {
    return NONE;
  }
  // dominante
  if (r>b*COLOR_DOMINANCE && r>g*COLOR_DOMINANCE) return RED;
  if (b>r*COLOR_DOMINANCE && b>g*COLOR_DOMINANCE) return BLUE;
  return UNKNOWN;
}

bool verifyColor(DetectedColor& out){
  static DetectedColor last=NONE;
  static int repeats=0;
  static bool reported=false;

  DetectedColor cur=readColor();
  if(cur==NONE){ repeats=0; reported=false; last=NONE; return false; }

  if(cur==last) repeats++; else repeats=0;
  last=cur;

  if(repeats>=COLOR_STABLE_READS && !reported){
    out=cur; reported=true; return true;
  }
  return false;
}

void t_color(void* pv){
  for(;;){
    DetectedColor dc=NONE;
    if(verifyColor(dc)){
      events ev;
      switch(dc){
        case RED:     ev=EV_SEE_RED;    Telemetry::pubColor("RED");    break;
        case BLUE:    ev=EV_SEE_BLUE;   Telemetry::pubColor("BLUE");   break;
        case UNKNOWN: ev=EV_BAD_COLOR;  Telemetry::pubColor("UNKNOWN");break;
        default:      Telemetry::pubColor("NONE"); continue;
      }
      xQueueSend(eventQueue,&ev,portMAX_DELAY);
    }
    vTaskDelay(pdMS_TO_TICKS(PERIOD_COLOR_MS));
  }
}

void t_color_calib(void* pv){
  for(;;){
    ulTaskNotifyTake(pdTRUE,portMAX_DELAY);
    Telemetry::pubCalib(true);
    digitalWrite(PIN_TCS_S0,HIGH);
    digitalWrite(PIN_TCS_S1,LOW);
    int rF=getRed();  vTaskDelay(pdMS_TO_TICKS(STAB_COLOR_MS));
    int bF=getBlue(); vTaskDelay(pdMS_TO_TICKS(STAB_COLOR_MS));
    int gF=getGreen();vTaskDelay(pdMS_TO_TICKS(STAB_COLOR_MS));
    redBase   = map(rF,RMIN,RMAX,255,0);
    blueBase  = map(bF,BMIN,BMAX,255,0);
    greenBase = map(gF,GMIN,GMAX,255,0);
    colorCalibrated=true;
    Telemetry::pubCalib(false);
  }
}

// ===================== ULTRASONIDOS ===================
static inline int cmFromUs(int us){ return int(SOUND_DIV_2*us); }

int readDistance(const DistanceSensor& ds){
  digitalWrite(ds.pin_trigger,LOW); delayMicroseconds(US_SETTLE_US);
  digitalWrite(ds.pin_trigger,HIGH); delayMicroseconds(US_TRIG_US);
  digitalWrite(ds.pin_trigger,LOW);
  int t=pulseIn(ds.pin_echo,HIGH);
  return cmFromUs(t);
}

bool objectAtStart(){
  dist[0].current_value=readDistance(dist[0]);
  int v=dist[0].current_value; static bool seen=false;
  if(!seen && v>0 && v<=DIST_ON_CM){ seen=true; Telemetry::pubDistS1(v); return true; }
  else if(seen && (v==0 || v>DIST_OFF_CM)){ seen=false; }
  return false;
}

bool objectAtEnd(){
  dist[1].current_value=readDistance(dist[1]);
  int v=dist[1].current_value; static bool seen=false;
  if(!seen && v>0 && v<=DIST_ON_CM){ seen=true; Telemetry::pubDistS2(v); return true; }
  else if(seen && (v==0 || v>DIST_OFF_CM)){ seen=false; }
  return false;
}

// ===================== TIMEOUTS =======================
bool verifyTimeouts(){
  if (current_state!=ST_FAULT && current_state!=ST_PAUSE){
    if(flagColorPending && (millis()-tMoveStart > TMO_COLOR_MS)){
      LogInfo("TIMEOUT: color no detectado a tiempo.");
      return true;
    }
    if(flagEndPending && (millis()-tMoveStart > TMO_END_MS)){
      LogInfo("TIMEOUT: objeto no llegó al sensor final a tiempo.");
      return true;
    }
  }
  return false;
}

// ===================== SENSORS TASK ===================
void t_sensors(void* pv){
  static int prevRST=LOW;
  for(;;){
    if(objectAtStart()){
      events ev=EV_START_OBJ; xQueueSend(eventQueue,&ev,portMAX_DELAY);
    }
    vTaskDelay(pdMS_TO_TICKS(10));

    if(objectAtEnd()){
      vTaskDelay(pdMS_TO_TICKS(500));
      events ev=EV_AT_END; xQueueSend(eventQueue,&ev,portMAX_DELAY);
    }
    vTaskDelay(pdMS_TO_TICKS(10));

    if(verifyTimeouts()){
      events ev=EV_TMO; xQueueSend(eventQueue,&ev,portMAX_DELAY);
    }
    vTaskDelay(pdMS_TO_TICKS(10));

    int curRST=digitalRead(PIN_BTN_RST);
    if(curRST==HIGH && prevRST==LOW){
      events ev=EV_BTN_RST; xQueueSend(eventQueue,&ev,portMAX_DELAY);
    }
    prevRST=curRST;

    vTaskDelay(pdMS_TO_TICKS(PERIOD_SENS_MS));
  }
}

// ===================== ISR STOP =======================
void IRAM_ATTR isrStop(){
  events ev=EV_BTN_STOP;
  BaseType_t hpw=pdFALSE;
  xQueueSendFromISR(eventQueue,&ev,&hpw);
  portYIELD_FROM_ISR(hpw);
}

// ===================== WIFI/MQTT ======================
void wifiConnect(){
  WiFi.mode(WIFI_STA);
  WiFi.disconnect(true);
  WiFi.begin(WIFI_SSID,WIFI_PASS);
  uint8_t tries=0; const uint8_t MAX_TRIES=20;
  while(WiFi.status()!=WL_CONNECTED && tries<MAX_TRIES){ vTaskDelay(pdMS_TO_TICKS(500)); tries++; }
  if(WiFi.status()==WL_CONNECTED) LogInfo("WiFi conectado");
}
void t_wifi(void* pv){
  for(;;){
    if(WiFi.status()!=WL_CONNECTED){ wifiConnect(); }
    vTaskDelay(pdMS_TO_TICKS(5000));
  }
}

void mqttReconnect(){
  if(mqtt.connect(MQTT_CLIENT, MQTT_USER, MQTT_TOKEN)){
    LogInfo("MQTT conectado");
    mqtt.subscribe(TOPIC_MOTOR_SPEED);
    mqtt.subscribe(TOPIC_SYSTEM_STATE);
  }
}
void t_mqtt(void* pv){
  for(;;){
    if(WiFi.status()==WL_CONNECTED){
      if(!mqtt.connected()){ mqttReconnect(); }
      else { mqtt.loop(); }
    }
    vTaskDelay(pdMS_TO_TICKS(100));
  }
}

void mqttCallback(char* topic, byte* payload, unsigned int length){
  String msg((char*)payload,length);

  if(strcmp(topic,TOPIC_MOTOR_SPEED)==0){
    // Mapeo suavizado 1–100 -> ~100–255
    int pct = msg.toInt();
    if(pct>=1 && pct<=100){
      float f = (float)pct/100.0f;
      beltSpeedPwm = 100 + (int)(155.0f * log10f(1.0f + 9.0f * f));
      LogInfo(("Velocidad PWM actualizada: "+String(beltSpeedPwm)).c_str());
    }
    return;
  }

  if(strcmp(topic,TOPIC_SYSTEM_STATE)==0){
    msg.trim();
    events ev;
    // NUEVO formato
    if(msg=="STOP"){         ev=EV_NET_STOP; xQueueSend(eventQueue,&ev,portMAX_DELAY); return; }
    if(msg=="RESTART"){      ev=EV_NET_RST;  xQueueSend(eventQueue,&ev,portMAX_DELAY); return; }
    // COMPAT LEGADO
    if(msg=="ST_MANUAL_STOP"){ ev=EV_NET_STOP; xQueueSend(eventQueue,&ev,portMAX_DELAY); return; }
    if(msg=="ST_IDLE"){        ev=EV_NET_RST;  xQueueSend(eventQueue,&ev,portMAX_DELAY); return; }
  }
}

// ===================== TRANSICIONES ===================
void none() { /* no-op */ }

void turn_on(){
  turn_green_led_off();
  turn_red_led_off();
  turn_yellow_led_on();
  xTaskNotifyGive(tColorCalib);
  stop_engine();

  current_state=ST_WAIT;
  LogState(); Telemetry::pubState();
}

void start_running(){
  LogEvent();
  turn_yellow_led_off();
  turn_green_led_on();
  start_engine();

  tMoveStart = millis();
  flagColorPending = true;
  flagEndPending   = true;

  current_state=ST_RUN;
  LogState(); Telemetry::pubState();
}

void to_red(){
  LogEvent();
  move_servo(SERVO_POS_RED);
  current_state=ST_SORTED;
  flagColorPending=false;
  LogState(); Telemetry::pubState(); Telemetry::pubServo(SERVO_POS_RED);
}

void to_blue(){
  LogEvent();
  move_servo(SERVO_POS_BLUE);
  current_state=ST_SORTED;
  flagColorPending=false;
  LogState(); Telemetry::pubState(); Telemetry::pubServo(SERVO_POS_BLUE);
}

void to_fault(){
  LogEvent();
  stop_engine();
  turn_green_led_off();
  blink_red_led();
  current_state=ST_FAULT;
  LogState(); Telemetry::pubState();
}

void to_pause(){
  LogEvent();
  stop_engine();
  turn_green_led_off();
  turn_red_led_on();
  current_state=ST_PAUSE;
  LogState(); Telemetry::pubState();
}

void do_restart(){
  LogEvent();
  stop_engine();
  xTaskNotifyGive(tColorCalib);

  turn_green_led_off();
  turn_red_led_off();
  turn_yellow_led_on();

  flagColorPending=false;
  flagEndPending=false;

  current_state=ST_WAIT;
  LogState(); Telemetry::pubState();
}

void finish_cycle(){
  LogEvent();
  stop_engine();
  turn_green_led_off();
  turn_red_led_off();
  turn_yellow_led_on();

  flagColorPending=false;
  flagEndPending=false;

  current_state=ST_WAIT;
  LogState(); Telemetry::pubState();
}

// ===================== TABLA FSM ======================
typedef void (*transition)();
transition fsmTable[MAX_STATES_ENUM][MAX_EVENTS_ENUM] = {
/* ST_WAIT */
{
  start_running, /* EV_START_OBJ */
  none,          /* EV_SEE_RED   */
  none,          /* EV_SEE_BLUE  */
  none,          /* EV_BAD_COLOR */
  none,          /* EV_BTN_STOP  */
  do_restart,    /* EV_BTN_RST   */
  none,          /* EV_TMO       */
  none,          /* EV_AT_END    */
  none,          /* EV_TICK      */
  none,          /* EV_NET_STOP  */
  none           /* EV_NET_RST   */
},
/* ST_RUN */
{
  none,          /* EV_START_OBJ */
  to_red,        /* EV_SEE_RED   */
  to_blue,       /* EV_SEE_BLUE  */
  to_fault,      /* EV_BAD_COLOR */
  to_pause,      /* EV_BTN_STOP  */
  none,          /* EV_BTN_RST   */
  to_fault,      /* EV_TMO       */
  none,          /* EV_AT_END    */
  none,          /* EV_TICK      */
  to_pause,      /* EV_NET_STOP  */
  none           /* EV_NET_RST   */
},
/* ST_PAUSE */
{
  none,          /* EV_START_OBJ */
  none,          /* EV_SEE_RED   */
  none,          /* EV_SEE_BLUE  */
  none,          /* EV_BAD_COLOR */
  none,          /* EV_BTN_STOP  */
  do_restart,    /* EV_BTN_RST   */
  none,          /* EV_TMO       */
  none,          /* EV_AT_END    */
  none,          /* EV_TICK      */
  none,          /* EV_NET_STOP  */
  do_restart     /* EV_NET_RST   */
},
/* ST_FAULT */
{
  none,          /* EV_START_OBJ */
  none,          /* EV_SEE_RED   */
  none,          /* EV_SEE_BLUE  */
  none,          /* EV_BAD_COLOR */
  none,          /* EV_BTN_STOP  */
  do_restart,    /* EV_BTN_RST   */
  none,          /* EV_TMO       */
  none,          /* EV_AT_END    */
  none,          /* EV_TICK      */
  none,          /* EV_NET_STOP  */
  do_restart     /* EV_NET_RST   */
},
/* ST_SORTED */
{
  none,          /* EV_START_OBJ */
  none,          /* EV_SEE_RED   */
  none,          /* EV_SEE_BLUE  */
  none,          /* EV_BAD_COLOR */
  to_pause,      /* EV_BTN_STOP  */
  none,          /* EV_BTN_RST   */
  to_fault,      /* EV_TMO       */
  finish_cycle,  /* EV_AT_END    */
  none,          /* EV_TICK      */
  to_pause,      /* EV_NET_STOP  */
  none           /* EV_NET_RST   */
}
};

// ===================== EVENT LOOP =====================
void pull_event(){
  events ev;
  if (xQueueReceive(eventQueue,&ev,portMAX_DELAY)==pdPASS){
    if(ev!=current_event) current_event=ev;
  } else {
    current_event=EV_TICK;
  }
}
void fsm_run(){
  pull_event();
  if (current_event>=0 && current_event<MAX_EVENTS_ENUM &&
      current_state>=0 && current_state<MAX_STATES_ENUM) {
    // *** CORRECCIÓN: ejecutar la transición correcta ***
    fsmTable[current_state][current_event]();
  } else {
    current_state=ST_FAULT;
  }
}

// ===================== SETUP / LOOP ===================
void setup(){
  Serial.begin(SERIAL_BAUD);

  // Pines
  pinMode(PIN_TRIG_S1,OUTPUT); pinMode(PIN_ECHO_S1,INPUT);
  pinMode(PIN_TRIG_S2,OUTPUT); pinMode(PIN_ECHO_S2,INPUT);

  pinMode(PIN_MOTOR_D1,OUTPUT);
  pinMode(PIN_MOTOR_D2,OUTPUT);
  pinMode(PIN_MOTOR_PWM,OUTPUT);

  pinMode(PIN_LED_RED,OUTPUT);
  pinMode(PIN_LED_GRN,OUTPUT);
  pinMode(PIN_LED_YEL,OUTPUT);

  pinMode(PIN_BTN_RST,INPUT_PULLDOWN);
  pinMode(PIN_BTN_STOP,INPUT_PULLDOWN);

  pinMode(PIN_TCS_S0,OUTPUT);
  pinMode(PIN_TCS_S1,OUTPUT);
  pinMode(PIN_TCS_S2,OUTPUT);
  pinMode(PIN_TCS_S3,OUTPUT);
  pinMode(PIN_TCS_OUT,INPUT);

  // Servo
  Servo1.attach(PIN_SERVO, 500, 2500);

  // Ultrasónicos
  dist[0].pin_trigger=PIN_TRIG_S1; dist[0].pin_echo=PIN_ECHO_S1;
  dist[1].pin_trigger=PIN_TRIG_S2; dist[1].pin_echo=PIN_ECHO_S2;

  // Queues
  xQueueLed  = xQueueCreate(QUEUE_LED_SZ, sizeof(LedCommand));
  eventQueue = xQueueCreate(QUEUE_EVENTS_SZ, sizeof(events));

  // Tasks
  xTaskCreate(led_task,       "t_led_red",     STK_1K, NULL, LED_TASK_PRIO, NULL);
  xTaskCreate(t_sensors,      "t_ultra_io",    STK_2K, NULL, SENS_TASK_PRIO, NULL);
  xTaskCreate(t_color,        "t_color_tcs",   STK_2K, NULL, SENS_TASK_PRIO, NULL);
  xTaskCreate(t_color_calib,  "t_color_calib", STK_2K, NULL, 1, &tColorCalib);
  xTaskCreate(t_mqtt,         "t_net_mqtt",    STK_4K, NULL, SENS_TASK_PRIO, NULL);
  xTaskCreate(t_wifi,         "t_net_wifi",    STK_4K, NULL, SENS_TASK_PRIO, NULL);

  // MQTT
  mqtt.setServer(MQTT_SERVER, MQTT_PORT);
  mqtt.setCallback(mqttCallback);

  // ISR STOP
  attachInterrupt(digitalPinToInterrupt(PIN_BTN_STOP), isrStop, RISING);

  // Estado inicial
  turn_on();

  
  initStats();             // <-- NUEVO
  initTime = millis();     // <-- NUEVO
  LogInfo("Metrics: muestreo iniciado por 10s (SAMPLING_TIME=10000)")

}

void loop(){
  
 if (!metricsFinished) {
    actualTime = millis();
    if (actualTime - initTime > SAMPLING_TIME) {
      metricsFinished = true;
      finishStats();  // <-- NUEVO
    }
 }

  fsm_run();
  vTaskDelay(1); // cede CPU
}