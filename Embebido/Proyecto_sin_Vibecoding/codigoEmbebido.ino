#include <ESP32Servo.h>
#include "Metrics.h"

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
    str = "****************************"; \
    DebugPrint(str); \
    str = " CURRENT STATE -> [" + states_s[current_state] + "]"; \
    DebugPrint(str); \
    str = "****************************"; \
    DebugPrint(str); \
  }

// Imprime evento detectado con timestamp
#define DebugPrintEvent() \
  { \
    String str; \
    str = "****************************"; \
    DebugPrint(str); \
    str = " EVENT DETECTED -> [" + events_s[current_event] + "]"; \
    DebugPrint(str); \
    str = "****************************"; \
    DebugPrint(str); \
  }
//----------------------------------------------
//----------------------------------------------
#define SERIAL_SPEED               9600
#define SAMPLING_TIME             10000     //Metrics

//Pin map
#define PIN_TRIGGER1                27
#define PIN_ECHO1                   26
#define PIN_TRIGGER2                22
#define PIN_ECHO2                   21
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
#define DISTANCE_THRESHOLD          5     //umbral de distancia

#define SERVO_POS_COLOR1            5
#define SERVO_POS_COLOR2            8
#define MIN_PWM_SERVO_WIDTH        500
#define MAX_PWM_SERVO_WIDTH        2500

#define TIME_TO_OBJECT_0            0
#define LED_TASK_PRIORITY           1
#define LED_QUEUE_SIZE              1
#define STACK_SIZE                1024

#define BLINK_LED_DELAY_MS         200
#define TASK_SENSOR_DELAY          200
#define SENSORS_PRIORITY            1
#define SENSORS_QUEUE_SIZE         10
#define COLOR_THRESHOLD            200
#define COLOR_DELAY                50
#define COLOR_DOMINANCE_FACTOR     0.8


// --- Variables globales
long t_start_moving   = 0;   // Marca de tiempo cuando comienza a moverse
bool waiting_color    = false;
bool waiting_end      = false;
int engine_speed       = 150;
unsigned long redBase = 0;
unsigned long blueBase = 0;
unsigned long greenBase = 0;
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
#define MAX_EVENTS                   9

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
                      EV_CONT } current_event;

String events_s [] = {"EV_OBJECT_DETECTED",
                      "EV_COLOR1_DETECTED",
                      "EV_COLOR2_DETECTED",
                      "EV_COLOR_ERROR",
                      "EV_STOP_BUTTON",
                      "EV_RESTART_BUTTON",
                      "EV_TIMEOUT",
                      "EV_OBJECT_AT_END",
                      "EV_CONT" };

typedef void (*transition)();
transition state_table[MAX_STATES][MAX_EVENTS] =
{
//EV_OBJECT_DETECTED  EV_COLOR1_DETECTED   EV_COLOR2_DETECTED   EV_COLOR_ERROR        EV_STOP_BUTTON       EV_RESTART_BUTTON   EV_TIMEOUT   EV_OBJECT_AT_END  EV_CONT
{ start_moving       ,    none            ,    none            ,    none              ,    none            ,    none           , none       ,   none         , none  }, // ST_IDLE
{ none               ,    move_to_color1  ,    move_to_color2  ,    error_stop        ,    manual_stop     ,    none           , error_stop ,   none         , none  }, // ST_MOVING
{ none               ,    none            ,    none            ,    none              ,    none            ,    restart        , none       ,   none         , none  }, // ST_MANUAL_STOP
{ none               ,    none            ,    none            ,    none              ,    none            ,    restart        , none       ,   none         , none  }, // ST_ERROR
{ none               ,    none            ,    none            ,    none              ,    manual_stop     ,    none           , error_stop ,   finish       , none  }  // ST_COLOR_DETECTED
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

    // Distancia fin
    if (verifyObjectAtEnd()) {
      events ev = EV_OBJECT_AT_END;
      xQueueSend(eventQueue, &ev, portMAX_DELAY);
    }

    // Timeout
    if (verifySensorsTimeout()) {
      events ev = EV_TIMEOUT;
      xQueueSend(eventQueue, &ev, portMAX_DELAY);
    }

    // Color
    DetectedColor detected = NONE;
    if (verifyColorSensor(detected)) {
      events ev;
      switch (detected) {
        case RED:      ev = EV_COLOR1_DETECTED; break;
        case BLUE:     ev = EV_COLOR2_DETECTED; break;
        case UNKNOWN:  ev = EV_COLOR_ERROR;     break;
        default:       return; // ignorar NONE
      }
      xQueueSend(eventQueue, &ev, portMAX_DELAY);
    }

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

//---------------ISR BUtton STOP -------------------
// IRAM_ATTR coloca la ISR en RAM para que se ejecute rápido y seguro
void IRAM_ATTR stopButtonISR() {
  events ev = EV_STOP_BUTTON;
  BaseType_t xHigherPriorityTaskWoken = pdFALSE;
  xQueueSendFromISR(eventQueue, &ev, &xHigherPriorityTaskWoken);
  portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
}
//------------------------------------------
/****************************************************** 
                 END FREERTOS TASKS 
******************************************************/


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
  xTaskCreate(led_task, "led_task", STACK_SIZE, NULL, LED_TASK_PRIORITY, NULL);
  xTaskCreate(sensors_task, "sensors_task", STACK_SIZE, NULL, SENSORS_PRIORITY, NULL);

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
  calibrate_color_sensor();
  stop_engine();
  current_state = ST_IDLE;
  DebugPrintState();
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
}

void move_to_color1(){
  DebugPrintEvent();  
  move_servo(SERVO_POS_COLOR1);  
  current_state = ST_COLOR_DETECTED;
  waiting_color = false;   // Flag color sensado
  DebugPrintState();
}
void move_to_color2(){
  DebugPrintEvent();  
  move_servo(SERVO_POS_COLOR2);
  current_state = ST_COLOR_DETECTED;
  waiting_color = false;   // Flag color sensado
  DebugPrintState();
}
void error_stop(){
  DebugPrintEvent();  
  stop_engine();
  turn_green_led_off();
  blink_red_led();
  current_state = ST_ERROR;
  DebugPrintState();
}

void manual_stop(){
  DebugPrintEvent();  
  stop_engine();
  turn_green_led_off();
  turn_red_led_on();
  current_state = ST_MANUAL_STOP;
  DebugPrintState();
}

void restart(){
  DebugPrintEvent();  
  stop_engine();
  turn_green_led_off();
  turn_red_led_off();
  turn_yellow_led_on();
  waiting_color = false;
  waiting_end   = false;
  current_state = ST_IDLE;
  DebugPrintState();
}

void finish(){ 
  
  restart();

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
  digitalWrite(PIN_DIR1_DCENGINE, HIGH);
  digitalWrite(PIN_DIR2_DCENGINE, LOW);

  analogWrite(PIN_DCENGINE_PWM, engine_speed);
 
}
void stop_engine(){
   
  digitalWrite(PIN_DIR1_DCENGINE, LOW);
  digitalWrite(PIN_DIR2_DCENGINE, LOW);
  analogWrite(PIN_DCENGINE_PWM, SPEED_0);
}
//END ACTUATORS---------------------------------------------------


//COLOR SENSOR------------------------------------------------------
unsigned long getRed() {
  // Rojo: S2=LOW, S3=LOW
  return readFrequency(LOW, LOW);
}

unsigned long getBlue() {
    // Azul: S2=LOW, S3=HIGH
  return readFrequency(LOW, HIGH);
}
  // Green: S2=HIGH, S3=HIGH
unsigned long getGreen() {
  return readFrequency(HIGH, HIGH);
}

void calibrate_color_sensor(){
  // Set Pulse Width scaling to 20%
	digitalWrite(PIN_COLOR_SENSOR_S0,HIGH);
	digitalWrite(PIN_COLOR_SENSOR_S1,LOW);

  redBase   = getRed();
  blueBase  = getBlue();
  greenBase = getGreen();
  color_sensor_calibrated = true;
  /*
  Serial.println("Calibracion completada:");
  Serial.print("RED base: ");   Serial.println(redBase);
  Serial.print("BLUE base: ");  Serial.println(blueBase);
  Serial.print("GREEN base: "); Serial.println(greenBase);
  */
}

unsigned long readFrequency(bool s2, bool s3) {
  
  digitalWrite(PIN_COLOR_SENSOR_S2, s2);
  digitalWrite(PIN_COLOR_SENSOR_S3, s3);
  
  return pulseIn(PIN_COLOR_SENSOR_OUT, LOW);
}

DetectedColor readColorSensor() {
  if (!color_sensor_calibrated) 
    return NONE;

  unsigned long red = getRed();
  vTaskDelay(pdMS_TO_TICKS(COLOR_DELAY));

  unsigned long blue = getBlue();
  vTaskDelay(pdMS_TO_TICKS(COLOR_DELAY));

  unsigned long green = getGreen();
  vTaskDelay(pdMS_TO_TICKS(COLOR_DELAY));

  // No hay objeto, valores similares a la base
  if (abs((long)red - (long)redBase) < COLOR_THRESHOLD &&
      abs((long)blue - (long)blueBase) < COLOR_THRESHOLD &&
      abs((long)green - (long)greenBase) < COLOR_THRESHOLD) {
    return NONE;
  }
  /*
  Serial.print("RED  y base: ");   Serial.print(red); Serial.println(redBase);
  Serial.print("BLUE y base: ");  Serial.print(blue); Serial.println(blueBase);
  Serial.print("GREEN y base: "); Serial.print(green); Serial.println(greenBase);
  */

  // HAY OBJETO - determinar de qué color es
  if (red < blue * COLOR_DOMINANCE_FACTOR && red < green * COLOR_DOMINANCE_FACTOR)
    return RED;

if (blue < red * COLOR_DOMINANCE_FACTOR && blue < green * COLOR_DOMINANCE_FACTOR)
    return BLUE;

  // Objeto presente pero no es rojo ni azul
  return UNKNOWN;
  
}

bool verifyColorSensor(DetectedColor &detected) {
  static bool colorAlreadyReported = false;
  DetectedColor current = readColorSensor();
  //DebugPrint(current); //imprime color detectado para debug
  if ((current == RED || current == BLUE || current == UNKNOWN) && !colorAlreadyReported) {
    detected = current;
    colorAlreadyReported = true;   // No repetir hasta que salga
    return true;
  }
  
  if (current == NONE) {
    colorAlreadyReported = false;  // Reset cuando ya no hay objeto
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
  distanceSensors[DISTANCE_SENSOR1].current_value=readDistanceSensor(distanceSensors[DISTANCE_SENSOR1]);

  int current_value = distanceSensors[DISTANCE_SENSOR1].current_value;
  int prev_value = distanceSensors[DISTANCE_SENSOR1].prev_value;
  if(current_value != prev_value){
    distanceSensors[DISTANCE_SENSOR1].prev_value = current_value;
    if(current_value <= DISTANCE_THRESHOLD){

      return true;
    }
  }

  return false;
}


bool verifyObjectAtEnd(){  
  distanceSensors[DISTANCE_SENSOR2].current_value=readDistanceSensor(distanceSensors[DISTANCE_SENSOR2]);
  
  int current_value = distanceSensors[DISTANCE_SENSOR2].current_value;
  int prev_value = distanceSensors[DISTANCE_SENSOR2].prev_value;
  
  if(current_value != prev_value){
    distanceSensors[DISTANCE_SENSOR2].prev_value = current_value;
    if(current_value <= DISTANCE_THRESHOLD){
      return true;
    }
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






