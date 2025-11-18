#include <ESP32Servo.h>
#include "Metrics.h"
#include <WiFi.h>
#include <PubSubClient.h> 

// Habilita debug para la impresion por el puerto serial ...
//----------------------------------------------
#define SERIAL_DEBUG_ENABLED 1

#if SERIAL_DEBUG_ENABLED
  #define DebugPrint(str) \
    { \
      Serial.println(str); \
    }
#else
  #define DebugPrint(str)
#endif

// Imprime estado actual con timestamp
#define DebugPrintState() \
  { \
    String str; \
    str = "*****************************************"; \
    DebugPrint(str); \
    str = " CURRENT STATE -> [" + states_s[current_state] + "]"; \
    DebugPrint(str); \
    str = "*****************************************"; \
    DebugPrint(str); \
  }

// Imprime evento detectado con timestamp
#define DebugPrintEvent() \
  { \
    String str; \
    str = "*****************************************"; \
    DebugPrint(str); \
    str = " EVENT DETECTED -> [" + events_s[current_event] + "]"; \
    DebugPrint(str); \
    str = "*****************************************"; \
    DebugPrint(str); \
  }
//----------------------------------------------
//----------------------------------------------
#define SERIAL_SPEED               9600
#define SAMPLING_TIME             10000     //Metrics

//Pin map
#define PIN_TRIGGER2                27
#define PIN_ECHO2                   26
#define PIN_TRIGGER1                22
#define PIN_ECHO1                   21
#define PIN_SERVO                   33
#define PIN_COLOR_SENSOR_S0         23
#define PIN_COLOR_SENSOR_S1         32
#define PIN_COLOR_SENSOR_S2         25
#define PIN_COLOR_SENSOR_S3         12
#define PIN_COLOR_SENSOR_OUT         4  
#define PIN_DCENGINE_PWM            19   
#define PIN_DIR1_DCENGINE           18  
#define PIN_DIR2_DCENGINE            5  
#define PIN_RESTART_BUTTON          13
#define PIN_STOP_BUTTON             14
#define PIN_LED_STOP_ERROR           2
#define PIN_LED_MOVING              15
#define PIN_LED_WAITING              3 //RX0

// Sensor and act configurations
#define SOUND_SPEED_DIV_2           0.01723
#define TIME_ULTRASONIC_2           2
#define TIME_ULTRASONIC_10          10
#define FRECUENCE_PWM              1000
#define RESOLUTION_PWM              8 
#define CHANEL_PWM_ENGINE           4 
#define SPEED_0                     0
#define THRESHOLD_TIME_OUT         500   
#define DISTANCE_SENSOR1            0
#define DISTANCE_SENSOR2            1
#define DISTANCE_SENSORS            2
#define MAX_TIME_TO_COLOR_SENSOR   5000   // ms -> 5 segundos
#define MAX_TIME_TO_END            8000   // ms -> 8 segundos
#define DIST_THRESHOLD_ON           5     //umbral de distancia
#define DIST_THRESHOLD_OFF          15
#define SERVO_POS_COLOR1           0
#define SERVO_POS_COLOR2           125
#define MIN_PWM_SERVO_WIDTH        500
#define MAX_PWM_SERVO_WIDTH        2500

#define TIME_TO_OBJECT_0            0
#define LED_TASK_PRIORITY           1
#define LED_QUEUE_SIZE              1
#define STACK_SIZE_1024            1024
#define STACK_SIZE_2048            2048
#define STACK_SIZE_4096            4096

#define BLINK_LED_DELAY_MS         200
#define TASK_SENSOR_DELAY          100
#define SENSORS_PRIORITY            1
#define SENSORS_QUEUE_SIZE         10
#define COLOR_DIFF_FACTOR          0.1
#define TASK_COLOR_SENSOR_DELAY    30  // EX 100
#define STABILIZE_COLOR_DELAY      15 //EX 50
#define COLOR_DOMINANCE_FACTOR     1.05
#define COLOR_READ_TIMEOUT         50000UL
#define RECONNECT_DELAY            5000
#define DELAY_2000_MS              2000
#define DELAY_500_MS               500
#define DELAY_10_MS                10
#define DELAY_100_MS               100
#define COLOR_REPEATED_READ        2
#define RMIN                       70
#define RMAX                       935
#define BMIN                       70
#define BMAX                       800
#define GMIN                       70
#define GMAX                       960

// --- Variables globales
long t_start_moving   = 0;   // Marca de tiempo cuando comienza a moverse
bool waiting_color    = false;
bool waiting_end      = false;
int engine_speed       = 180;
int redBase = 0;
int blueBase = 0;
int greenBase = 0;
bool color_sensor_calibrated = false;
Servo Servo1;

unsigned long initTime=0;   //Metrics
unsigned long  actualTime=0; //Metrics

//----------------------------------------------
struct stDistanceSensor
{
  int pin_echo;
  int pin_trigger;
  long current_value;
  long prev_value;
};
stDistanceSensor distanceSensors[DISTANCE_SENSORS];


enum DetectedColor {
  NONE,
  RED,
  BLUE,
  UNKNOWN
};


// ======== WiFi & MQTT ========

WiFiClient espClient;
PubSubClient client(espClient);

const char *ssid = "SO Avanzados";
const char *password = "SOA.2025";

const char *mqtt_server = "io.adafruit.com";
const int port = 1883;

const char *user_name = "prodlineclassifier";
const char *user_pass = "INGRESAR KEY ADAFRUIT";
const char *clientId = "conveyorBelt";

// TOPICOS MQTT
const char *topic_conveyor_belt_speed = "prodlineclassifier/feeds/dcengine";
const char *topic_conveyor_belt_state = "prodlineclassifier/feeds/systemstatus";
const char *topic_conveyor_belt_color = "prodlineclassifier/feeds/colorsensor";
const char *topic_conveyor_belt_servo = "prodlineclassifier/feeds/servo";
const char *topic_conveyor_belt_dist_sensor1 = "prodlineclassifier/feeds/distancesensor1";
const char *topic_conveyor_belt_dist_sensor2 = "prodlineclassifier/feeds/distancesensor2";
const char *topic_conveyor_belt_blink_red_led = "prodlineclassifier/feeds/blinkredled";
const char *topic_conveyor_belt_green_led = "prodlineclassifier/feeds/greenled";
const char *topic_conveyor_belt_red_led = "prodlineclassifier/feeds/redled";
const char *topic_conveyor_belt_yellow_led = "prodlineclassifier/feeds/yellowled";




// Functions---------------------------------------------
void turn_on();
void start_moving();
void move_to_color1();
void move_to_color2();
void error_stop();
void manual_stop();
void restart();
void finish();
void none();

/****************************************************** 
                      State Machine 
******************************************************/

#define MAX_STATES                   5
#define MAX_EVENTS                   11

enum states          { ST_IDLE,
                       ST_MOVING,
                       ST_MANUAL_STOP,
                       ST_ERROR,
                       ST_COLOR_DETECTED} current_state;

String states_s [] = { "ST_IDLE",
                       "ST_MOVING",
                       "ST_MANUAL_STOP",
                       "ST_ERROR",
                       "ST_COLOR_DETECTED"};

enum events          {EV_OBJECT_DETECTED,
                      EV_COLOR1_DETECTED,
                      EV_COLOR2_DETECTED,
                      EV_COLOR_ERROR,
                      EV_STOP_BUTTON,
                      EV_RESTART_BUTTON,
                      EV_TIMEOUT,
                      EV_OBJECT_AT_END,
                      EV_CONT,
                      EV_STOP_EXT,
                      EV_RESTART_EXT } current_event;

String events_s [] = {"EV_OBJECT_DETECTED",
                      "EV_COLOR1_DETECTED",
                      "EV_COLOR2_DETECTED",
                      "EV_COLOR_ERROR",
                      "EV_STOP_BUTTON",
                      "EV_RESTART_BUTTON",
                      "EV_TIMEOUT",
                      "EV_OBJECT_AT_END",
                      "EV_CONT",
                      "EV_STOP_EXT",
                      "EV_RESTART_EXT"};

typedef void (*transition)();
transition state_table[MAX_STATES][MAX_EVENTS] =
{
//EV_OBJECT_DETECTED  EV_COLOR1_DETECTED   EV_COLOR2_DETECTED   EV_COLOR_ERROR        EV_STOP_BUTTON       EV_RESTART_BUTTON   EV_TIMEOUT   EV_OBJECT_AT_END  EV_CONT  EV_STOP_EXT     EV_RESTART_EXT
{ start_moving       ,    none            ,    none            ,    none              ,    none            ,    restart        , none       ,   none         , none    ,  none         ,    none    }, // ST_IDLE
{ none               ,    move_to_color1  ,    move_to_color2  ,    error_stop        ,    manual_stop     ,    none           , error_stop ,   none         , none    ,  manual_stop  ,    none    }, // ST_MOVING
{ none               ,    none            ,    none            ,    none              ,    none            ,    restart        , none       ,   none         , none    ,  none         ,   restart  }, // ST_MANUAL_STOP
{ none               ,    none            ,    none            ,    none              ,    none            ,    restart        , none       ,   none         , none    ,  none         ,   restart  }, // ST_ERROR
{ none               ,    none            ,    none            ,    none              ,    manual_stop     ,    none           , error_stop ,   finish       , none    ,  manual_stop  ,   none     }  // ST_COLOR_DETECTED
};


/****************************************************** 
                      End State Machine 
******************************************************/


/****************************************************** 
                  FREERTOS TASKS 
******************************************************/

typedef enum {
  LED_CMD_ON,
  LED_CMD_OFF,
  LED_CMD_BLINK
} LedCommand;

QueueHandle_t xQueueLed;

//-------- LED TASK------------------------------
void led_task(void *parameter) {
  LedCommand cmd;
  while (true) {
    // espera hasta recibir un comando (bloqueado sin uso de CPU)
    if (xQueueReceive(xQueueLed, &cmd, portMAX_DELAY)) {
      switch (cmd) {
        case LED_CMD_ON:
          led_red_on();
          break;
        case LED_CMD_OFF:
          led_red_off();
          break;
        case LED_CMD_BLINK:
          do {
            led_red_on();
            vTaskDelay(pdMS_TO_TICKS(BLINK_LED_DELAY_MS));
            led_red_off();
            vTaskDelay(pdMS_TO_TICKS(BLINK_LED_DELAY_MS));
            // revisa si hay un nuevo comando sin bloquear
          } while (xQueueReceive(xQueueLed, &cmd, 0) == pdFALSE);
          continue;
      }
    }
  }
} 
//--------------------------------------------------------
//--------Sensors task---------------------------------
QueueHandle_t eventQueue;

void sensors_task(void *pv) {
  static int prevRestart = LOW;
  while (true) {
    // Distancia inicio
    if (verifyObjectAtStart()) {
      events ev = EV_OBJECT_DETECTED;
      xQueueSend(eventQueue, &ev, portMAX_DELAY);
    }
    vTaskDelay(pdMS_TO_TICKS(DELAY_10_MS));

    // Distancia fin
    if (verifyObjectAtEnd()) {
      events ev = EV_OBJECT_AT_END;
      xQueueSend(eventQueue, &ev, portMAX_DELAY);
    }
    vTaskDelay(pdMS_TO_TICKS(DELAY_10_MS));

    // Timeout
    if (verifySensorsTimeout()) {
      events ev = EV_TIMEOUT;
      xQueueSend(eventQueue, &ev, portMAX_DELAY);
    }
    vTaskDelay(pdMS_TO_TICKS(DELAY_10_MS));

    // Restart button (polling con flanco)
    int currentRestart = digitalRead(PIN_RESTART_BUTTON);
    if (currentRestart == HIGH && prevRestart == LOW) {
      events ev = EV_RESTART_BUTTON;
      xQueueSend(eventQueue, &ev, portMAX_DELAY);
    }
    prevRestart = currentRestart;

    vTaskDelay(pdMS_TO_TICKS(TASK_SENSOR_DELAY));
  }
}

void color_task(void *pv) {
// Color
  while (true) {
    DetectedColor detected = NONE;
    if (verifyColorSensor(detected)) {
      events ev;
      switch (detected) {
        case RED:      
          ev = EV_COLOR1_DETECTED; 
          publish_color("RED");
          break;
        case BLUE:     
          ev = EV_COLOR2_DETECTED; 
          publish_color("BLUE");
          break;
        case UNKNOWN:  
          ev = EV_COLOR_ERROR;     
          publish_color("UNKNOWN");
          break;          
        default:       
          publish_color("NONE");
          continue; // ignorar NONE
      }
      xQueueSend(eventQueue, &ev, portMAX_DELAY);
    }
    vTaskDelay(pdMS_TO_TICKS(TASK_COLOR_SENSOR_DELAY));
  }
}

TaskHandle_t calibrateColorSensorTaskHandle = NULL;

void calibrate_color_sensor_task(void *pvParameters) {
    while (true) {
        // Espera señal (bloqueada sin consumir CPU)
        ulTaskNotifyTake(pdTRUE, portMAX_DELAY);

        // --- Código de calibración ---
        digitalWrite(PIN_COLOR_SENSOR_S0, HIGH);
        digitalWrite(PIN_COLOR_SENSOR_S1, LOW);

        int r = getRed();
        vTaskDelay(pdMS_TO_TICKS(STABILIZE_COLOR_DELAY));
        int b = getBlue();
        vTaskDelay(pdMS_TO_TICKS(STABILIZE_COLOR_DELAY));
        int g = getGreen();
        vTaskDelay(pdMS_TO_TICKS(STABILIZE_COLOR_DELAY));

        redBase   = map(r, RMIN, RMAX, 255, 0);
        blueBase  = map(b, BMIN, BMAX, 255, 0);
        greenBase = map(g, GMIN, GMAX, 255, 0);

        color_sensor_calibrated = true;
    }
}

//---------------ISR BUtton STOP -------------------
// IRAM_ATTR coloca la ISR en RAM para que se ejecute rápido y seguro
void IRAM_ATTR stopButtonISR() {
  events ev = EV_STOP_BUTTON;
  BaseType_t xHigherPriorityTaskWoken = pdFALSE;
  // Envía el evento a la cola desde contexto de interrupción
  xQueueSendFromISR(eventQueue, &ev, &xHigherPriorityTaskWoken);
  // Si corresponde, fuerza cambio de contexto inmediato
  portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
}
//------------------------------------------

// -------- TASK: WIFI, MQTT y auxiliares-----------------
void wifiTask(void *p)
{
  while (true)
  {
    if (WiFi.status() != WL_CONNECTED)
    {
      wifiConnect();
    }
    
    vTaskDelay(pdMS_TO_TICKS(RECONNECT_DELAY)); // revisa cada 5s
  }
}

void wifiConnect()
{
  //DebugPrint("Intentando conectar WiFi...");
  WiFi.mode(WIFI_STA);
  WiFi.disconnect(true);     // limpia estado anterior
  WiFi.begin(ssid, password);
  //DebugPrint(ssid);
  //DebugPrint(password);
  int intentos = 0;
  const int MAX_INTENTOS = 20; // 20 x 500ms = 10s de espera

  while (WiFi.status() != WL_CONNECTED && intentos < MAX_INTENTOS)
  { 
    vTaskDelay(pdMS_TO_TICKS(DELAY_500_MS)); // wait 500ms
    intentos++;
  }

  if (WiFi.status() == WL_CONNECTED) {
    DebugPrint(" WIFI Conectado.");
  } 
}

void mqtt_task(void *pv) {
  while (true) {
    if (WiFi.status() == WL_CONNECTED) {
      if (!client.connected()) {
        mqttReconnect(); // intenta una vez
      } else {
        client.loop();   // procesa mensajes
      }
    }
    vTaskDelay(pdMS_TO_TICKS(DELAY_100_MS)); // revisa cada 100ms
  }
}

void mqttReconnect() {
  static int intentos = 0;
  const int MAX_INTENTOS = 5;

  DebugPrint("Intentando conexión MQTT...");

  if (client.connect(clientId, user_name, user_pass)) {
    DebugPrint("MQTT CONECTADO");
    client.subscribe(topic_conveyor_belt_speed);
    client.subscribe(topic_conveyor_belt_state);
    intentos = 0;
  } else {
    intentos++;
    if (intentos >= MAX_INTENTOS) {
      vTaskDelay(pdMS_TO_TICKS(RECONNECT_DELAY));
      intentos = 0;
    } else {
      vTaskDelay(pdMS_TO_TICKS(DELAY_2000_MS));
    }
  }
}
// ======== MQTT Callback ========
// Función Callback que recibe los mensajes enviados por los dispositivos
void callback(char* topic, byte* payload, unsigned int length) {
  // Convertir directamente el payload en un String
  String message((char*)payload, length);

  if (strcmp(topic, topic_conveyor_belt_speed) == 0) {
      // Este mensaje pertenece al tópico del motor
      handleConveyorSpeed(message);
      return;
  }
  if (strcmp(topic, topic_conveyor_belt_state) == 0) {
      // Este mensaje pertenece al tópico del motor
      handleConveyorChangeState(message);
      return;
  }
  

  //aca puedo manejar otros topicos

  DebugPrint("Tópico no reconocido");
}

void handleConveyorSpeed(const String &message) {
    // Validar números 1–100
    if (message.length() > 0 && message.toInt() != 0) {
        int newSpeed = message.toInt();
        if (newSpeed >= 1 && newSpeed <= 100) {
            DebugPrint("Mensaje recibido. Velocidad modificada -> " + String(newSpeed));
            engine_speed = map(newSpeed, 0, 100, 100, 255);
        }
    }
}

void handleConveyorChangeState(String message) {
    message.trim();   // Limpia \n, \r y espacios
    events ev;   // variable para enviar a la cola
    
    if (message == "ST_MANUAL_STOP") {
        ev = EV_STOP_EXT;
        xQueueSend(eventQueue, &ev, portMAX_DELAY);
        DebugPrint("Evento recibido: EV_STOP_EXT");
        return;
    }

    if (message == "ST_RESTART") {
        ev = EV_RESTART_EXT;
        xQueueSend(eventQueue, &ev, portMAX_DELAY);
        DebugPrint("Evento recibido: EV_RESTART_EXT");
        return;
    }

    DebugPrint("Evento desconocido recibido: " + message);
}

/****************************************************** 
                 END FREERTOS TASKS 
******************************************************/

//------------------MQTT PUBLISH FUNCTIONS----------------
void publish_state() {
  if (client.connected()) {
    String stateStr = states_s[current_state];
    client.publish(topic_conveyor_belt_state, stateStr.c_str());
  }
}

void publish_color(String color){
  if (client.connected()) {
    client.publish(topic_conveyor_belt_color, color.c_str());
  }
}

void publish_servo_position(int pos){
  if (client.connected()) {
    client.publish(topic_conveyor_belt_servo, String(pos).c_str());
  }
}

void publish_distance_sensor1(int value) {
  if (client.connected()) {
    client.publish(topic_conveyor_belt_dist_sensor1, String(value).c_str());
  }
}

void publish_distance_sensor2(int value) {
  if (client.connected()) {
    client.publish(topic_conveyor_belt_dist_sensor2, String(value).c_str());
  }
}

void publish_blink_red_led(bool state) {
  if (client.connected()) {
    client.publish(topic_conveyor_belt_blink_red_led, state ? "1" : "0");
  }
}

void publish_green_led(bool state) {
  if (client.connected()) {
    client.publish(topic_conveyor_belt_green_led, state ? "1" : "0");
  }
}

void publish_red_led(bool state) {
  if (client.connected()) {
    client.publish(topic_conveyor_belt_red_led, state ? "1" : "0");
  }
}

void publish_yellow_led(bool state) {
  if (client.connected()) {
    client.publish(topic_conveyor_belt_yellow_led, state ? "1" : "0");
  }
}
//------------------END MQTT PUBLISH FUNCTIONS----------------

/****************************************************** 
                      Initialization 
******************************************************/

void start(){

  Serial.begin(SERIAL_SPEED);

  /*
  initStats();  //Metrics
  initTime=millis();  //Metrics
  */

  pinMode(PIN_TRIGGER1, OUTPUT);  
  pinMode(PIN_ECHO1, INPUT);
  pinMode(PIN_TRIGGER2, OUTPUT);  
  pinMode(PIN_ECHO2, INPUT);
  pinMode(PIN_DIR1_DCENGINE, OUTPUT);
  pinMode(PIN_DIR2_DCENGINE, OUTPUT);
  pinMode(PIN_LED_STOP_ERROR, OUTPUT);
  pinMode(PIN_LED_MOVING, OUTPUT);
  pinMode(PIN_LED_WAITING, OUTPUT);
  pinMode(PIN_RESTART_BUTTON, INPUT_PULLDOWN);
  pinMode(PIN_STOP_BUTTON, INPUT_PULLDOWN);
   // TCS230
  pinMode(PIN_COLOR_SENSOR_S0, OUTPUT);
  pinMode(PIN_COLOR_SENSOR_S1, OUTPUT);
  pinMode(PIN_COLOR_SENSOR_S2, OUTPUT);
  pinMode(PIN_COLOR_SENSOR_S3, OUTPUT);
  pinMode(PIN_COLOR_SENSOR_OUT, INPUT);
  //SERVO INITIALIZATION
  Servo1.attach(PIN_SERVO, MIN_PWM_SERVO_WIDTH, MAX_PWM_SERVO_WIDTH);
  
  distanceSensors[DISTANCE_SENSOR1].pin_trigger = PIN_TRIGGER1; 
  distanceSensors[DISTANCE_SENSOR1].pin_echo = PIN_ECHO1; 
  distanceSensors[DISTANCE_SENSOR2].pin_trigger = PIN_TRIGGER2; 
  distanceSensors[DISTANCE_SENSOR2].pin_echo = PIN_ECHO2; 

  //PWM DCENGINE 
  ledcAttachChannel(PIN_DCENGINE_PWM, FRECUENCE_PWM, RESOLUTION_PWM, CHANEL_PWM_ENGINE);

  xQueueLed = xQueueCreate(LED_QUEUE_SIZE, sizeof(LedCommand));
  eventQueue = xQueueCreate(SENSORS_QUEUE_SIZE, sizeof(events));
  xTaskCreate(led_task, "led_task", STACK_SIZE_1024, NULL, LED_TASK_PRIORITY, NULL);
  xTaskCreate(sensors_task, "sensors_task", STACK_SIZE_2048, NULL, SENSORS_PRIORITY, NULL);
  xTaskCreate(color_task, "color_task", STACK_SIZE_2048, NULL, SENSORS_PRIORITY, NULL);
  xTaskCreate(calibrate_color_sensor_task, "calibrate_color_sensor_task", 2048, NULL, 1, &calibrateColorSensorTaskHandle);
  xTaskCreate(mqtt_task, "MQTT", STACK_SIZE_4096, NULL, SENSORS_PRIORITY, NULL);
  xTaskCreate(wifiTask, "WiFi", STACK_SIZE_4096, NULL, SENSORS_PRIORITY, NULL);

  client.setServer(mqtt_server, port);
  client.setCallback(callback);

  // ISR Stop button
  attachInterrupt(digitalPinToInterrupt(PIN_STOP_BUTTON), stopButtonISR, RISING);

  turn_on();
  
}
/****************************************************** 
                 End Initialization 
******************************************************/
void none(){
}
void turn_on(){
  turn_green_led_off();
  turn_red_led_off();
  turn_yellow_led_on();
  xTaskNotifyGive(calibrateColorSensorTaskHandle);
  stop_engine();
  current_state = ST_IDLE;
  DebugPrintState();
  publish_state();
}

void start_moving(){
  DebugPrintEvent();  
  turn_yellow_led_off();
  turn_green_led_on();
  start_engine();

  t_start_moving   = millis();
  waiting_color    = true;
  waiting_end      = true;

  current_state = ST_MOVING;
  DebugPrintState();
  publish_state();
}

void move_to_color1(){
  DebugPrintEvent();  
  move_servo(SERVO_POS_COLOR1);  
  current_state = ST_COLOR_DETECTED;
  waiting_color = false;   // Flag color sensado
  DebugPrintState();
  publish_state();
  publish_servo_position(SERVO_POS_COLOR1);
}
void move_to_color2(){
  DebugPrintEvent();  
  move_servo(SERVO_POS_COLOR2);
  current_state = ST_COLOR_DETECTED;
  waiting_color = false;   // Flag color sensado
  DebugPrintState();
  publish_state();
  publish_servo_position(SERVO_POS_COLOR2);
}
void error_stop(){
  DebugPrintEvent();  
  stop_engine();
  turn_green_led_off();
  blink_red_led();
  current_state = ST_ERROR;
  DebugPrintState();
  publish_state();
}

void manual_stop(){
  DebugPrintEvent();  
  stop_engine();
  turn_green_led_off();
  turn_red_led_on();
  current_state = ST_MANUAL_STOP;
  DebugPrintState();
  publish_state();
}

void restart(){
  DebugPrintEvent();  
  stop_engine();
  xTaskNotifyGive(calibrateColorSensorTaskHandle);
  turn_green_led_off();
  turn_red_led_off();
  turn_yellow_led_on();
  waiting_color = false;
  waiting_end   = false;
  current_state = ST_IDLE;
  DebugPrintState();
  publish_state();
}

void finish(){ 
  DebugPrintEvent();  
  stop_engine();
  turn_green_led_off();
  turn_red_led_off();
  turn_yellow_led_on();
  waiting_color = false;
  waiting_end   = false;
  current_state = ST_IDLE;
  DebugPrintState();
  publish_state();
}


//ACTUATORS---------------------------------------------------
void move_servo(int angle){ 
  Servo1.write(angle);
}
void turn_green_led_on(){
  digitalWrite(PIN_LED_MOVING, HIGH);
}
void turn_green_led_off(){
  digitalWrite(PIN_LED_MOVING, LOW);
}

void turn_red_led_on(){
  LedCommand cmd = LED_CMD_ON;
  xQueueOverwrite(xQueueLed, &cmd);
}
void blink_red_led(){
  LedCommand cmd = LED_CMD_BLINK;
  xQueueOverwrite(xQueueLed, &cmd);
}
void turn_red_led_off(){
  LedCommand cmd = LED_CMD_OFF;
  xQueueOverwrite(xQueueLed, &cmd);
  
}

void led_red_on(){
  digitalWrite(PIN_LED_STOP_ERROR, HIGH);
}
void led_red_off(){
  digitalWrite(PIN_LED_STOP_ERROR, LOW);
}


void turn_yellow_led_on(){
  digitalWrite(PIN_LED_WAITING, HIGH);
}
void turn_yellow_led_off(){
  digitalWrite(PIN_LED_WAITING, LOW);
}
void start_engine(){
  //Gira izq
  digitalWrite(PIN_DIR1_DCENGINE, LOW);
  digitalWrite(PIN_DIR2_DCENGINE, HIGH);

  analogWrite(PIN_DCENGINE_PWM, engine_speed);
 
}
void stop_engine(){
   
  digitalWrite(PIN_DIR1_DCENGINE, LOW);
  digitalWrite(PIN_DIR2_DCENGINE, LOW);
  analogWrite(PIN_DCENGINE_PWM, SPEED_0);
}
//END ACTUATORS---------------------------------------------------


//COLOR SENSOR------------------------------------------------------
int getRed() {
  // Rojo: S2=LOW, S3=LOW
  return readFrequency(LOW, LOW);
}

int getBlue() {
    // Azul: S2=LOW, S3=HIGH
  return readFrequency(LOW, HIGH);
}
  // Green: S2=HIGH, S3=HIGH
int getGreen() {
  return readFrequency(HIGH, HIGH);
}

int readFrequency(bool s2, bool s3) {
  
  digitalWrite(PIN_COLOR_SENSOR_S2, s2);
  digitalWrite(PIN_COLOR_SENSOR_S3, s3);
  
  return pulseIn(PIN_COLOR_SENSOR_OUT, LOW, COLOR_READ_TIMEOUT);
}

DetectedColor readColorSensor() {
  if (!color_sensor_calibrated) 
    return NONE;

  // --- Lectura y normalización ---
  int Red = getRed();
  int redValue = map(Red, RMIN, RMAX, 255, 0);   // Normaliza a 0–255
  vTaskDelay(pdMS_TO_TICKS(STABILIZE_COLOR_DELAY));

  int Green = getGreen();
  int greenValue = map(Green, GMIN, GMAX, 255, 0);
  vTaskDelay(pdMS_TO_TICKS(STABILIZE_COLOR_DELAY));

  int Blue = getBlue();
  int blueValue = map(Blue, BMIN, BMAX, 255, 0);
  vTaskDelay(pdMS_TO_TICKS(STABILIZE_COLOR_DELAY));

  /*
  Serial.printf("R: %d | RB: %d\n", redValue, redBase);
  Serial.printf("G: %d | GBase: %d\n", greenValue, greenBase);
  Serial.printf("B: %d | BBase: %d\n", blueValue, blueBase);
  */
  // --- Detección de ausencia de objeto ---
  if (abs(redValue - redBase)   < redBase   * COLOR_DIFF_FACTOR &&
      abs(blueValue - blueBase) < blueBase  * COLOR_DIFF_FACTOR &&
      abs(greenValue - greenBase) < greenBase * COLOR_DIFF_FACTOR) {
    return NONE;
  }

  // --- Determinación de color dominante ---
  if (redValue > blueValue * COLOR_DOMINANCE_FACTOR && redValue > greenValue * COLOR_DOMINANCE_FACTOR){
    return RED;
  }
  else if (blueValue > redValue * COLOR_DOMINANCE_FACTOR && blueValue > greenValue * COLOR_DOMINANCE_FACTOR){
    return BLUE;
  }
  // --- Objeto presente pero color indefinido ---
  else{
    return UNKNOWN;
  }
}

bool verifyColorSensor(DetectedColor &detected) {
  static DetectedColor lastColor = NONE;
  static int repeatCount = 0;
  static bool alreadyReported = false;
  
  DetectedColor current = readColorSensor();
  //Serial.printf("Current: %d \n", current);
  
  // RESET cuando no hay objeto
  if (current == NONE) {
    repeatCount = 0;
    alreadyReported = false;
    lastColor = NONE;
    return false;
  }

  // Si el color es igual al anterior → acumula estabilidad
  if (current == lastColor) {
    repeatCount++;
  } else {
    repeatCount = 0;  // primera aparición del nuevo color
  }

  lastColor = current;

  // Verificación final
  if (repeatCount >= COLOR_REPEATED_READ && !alreadyReported) {
    detected = current;
    alreadyReported = true;
    return true;
  }

  return false;
}

//END COLOR SENSOR------------------------------------------------------


//DISTANCE SENSORS------------------------------------------------------
int readDistanceSensor(stDistanceSensor distance_sensor)
{  
  int time_to_object = TIME_TO_OBJECT_0 ;

  int pin_echo = distance_sensor.pin_echo;
  int pin_trigger = distance_sensor.pin_trigger;
  //Desactiva el trigger
  digitalWrite(pin_trigger, LOW);
  delayMicroseconds(TIME_ULTRASONIC_2);
  
  //activa el Trigger por 10 microsegundos
  digitalWrite(pin_trigger, HIGH);
  delayMicroseconds(TIME_ULTRASONIC_10);
  
   //Desactiva el trigger
  digitalWrite(pin_trigger, LOW);
  
  //Leo el pin de Echo, y obtengo el tiempo al objeto en microsegundos
  time_to_object = pulseIn(pin_echo, HIGH);
  return calcDistance(time_to_object);
}

bool verifyObjectAtStart(){  
    distanceSensors[DISTANCE_SENSOR1].current_value = readDistanceSensor(distanceSensors[DISTANCE_SENSOR1]);
    //DebugPrint(distanceSensors[DISTANCE_SENSOR1].current_value);
    int current_value = distanceSensors[DISTANCE_SENSOR1].current_value;
    static bool objectDetected = false;

    if(!objectDetected && current_value > 0 && current_value <= DIST_THRESHOLD_ON){
        objectDetected = true;
        publish_distance_sensor1(current_value);
        return true;
    }
    else if(objectDetected && (current_value == 0 || current_value > DIST_THRESHOLD_OFF)){
        objectDetected = false;
    }

    return false;
}


bool verifyObjectAtEnd(){  
    distanceSensors[DISTANCE_SENSOR2].current_value = readDistanceSensor(distanceSensors[DISTANCE_SENSOR2]);
    //DebugPrint(distanceSensors[DISTANCE_SENSOR2].current_value);

    int current_value = distanceSensors[DISTANCE_SENSOR2].current_value;
    static bool objectDetected = false;

    // Solo disparar cuando el objeto aparece bajo el umbral
    if(!objectDetected && current_value > 0 && current_value <= DIST_THRESHOLD_ON){
        objectDetected = true;
        publish_distance_sensor2(current_value);
        return true;
    }
    else if(objectDetected && (current_value == 0 || current_value > DIST_THRESHOLD_OFF)){
        objectDetected = false;
    }

    return false;
}

int calcDistance(int time_to_object) 
{
  //calculo la distancia al objeto aplicando la formula
  int distanceToObject=SOUND_SPEED_DIV_2*time_to_object;
  return distanceToObject;
}
//END DISTANCE SENSORS------------------------------------------------------


// TIMEOUT VERIFICATION ------------------------------------------------------------
bool verifySensorsTimeout(){
  
  if(current_state != ST_ERROR && current_state != ST_MANUAL_STOP)
  {
    // Chequeo de timeout de color
    if (waiting_color && (millis() - t_start_moving > MAX_TIME_TO_COLOR_SENSOR)) {
      DebugPrint("TIMEOUT -> No se detectó color a tiempo!");
      return true;
    }
    // Chequeo de timeout de final
    if (waiting_end && (millis() - t_start_moving > MAX_TIME_TO_END)) {
      DebugPrint("TIMEOUT -> El objeto no llegó al sensor final a tiempo!");
      return true;
    }
  }
  return false;
}

//END TIMEOUT VERIFICATION ------------------------------------------------------------



/****************************************************** 
                    Get new EVENT 
******************************************************/
void get_new_event( )
{
  events new_event;
  //se bloquea en espera de un nuevo evento
  if( xQueueReceive(eventQueue, &new_event, portMAX_DELAY) == pdPASS )
  {
    //Serial.print("DEBUG: Evento recibido de la cola -> ");
    //Serial.println(events_s[new_event]);
     if (new_event != current_event) {
      current_event = new_event;
    }  
  }else {
    current_event = EV_CONT; // seguridad
  }

}
/****************************************************** 
                END Get new EVENT 
******************************************************/



/****************************************************** 
                State Machine Function
******************************************************/

void fsm( )
{
  get_new_event();

  if( (current_event >= 0) && (current_event < MAX_EVENTS) && (current_state >= 0) && (current_state < MAX_STATES) )
  {
    state_table[current_state][current_event]();     
  }
  else
  {
    current_state = ST_ERROR;
  }

}

/****************************************************** 
                END State Machine Function
******************************************************/


void setup()
{
  start();    
}


void loop()
{
  /***********Metrics **************
  actualTime=millis();
  
  //cantidad de tiempo que se va a tomar las muestras 
  if(actualTime-initTime>SAMPLING_TIME){
    initTime=actualTime;
    finishStats();
   }
   vTaskDelay(1); // se cede CPU
  
  ***********End Metrics ***************/

  fsm();
}






