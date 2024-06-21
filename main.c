#include <stdio.h>
#include <stdlib.h>
#include <libpynq.h>
#include <platform.h>
#include <stepper.h>
#include <pthread.h>
#include <time.h>
#include <unistd.h>

#define S0 IO_A0 
#define S1 IO_A1
#define S2 IO_A2
#define S3 IO_A3
#define OUT IO_AR4 

#define MAP_SIZE 19
#define INT_MAX 5
unsigned long minFreq = 10000;
unsigned long maxFreq = 200000; 
int obstacle_detected = 0;
int block_detected = 0;
int x = MAP_SIZE/2, y = MAP_SIZE/2;
int front_x = 0, front_y = 1;
int color;
char direction = 'N';
int visit_counts[MAP_SIZE][MAP_SIZE] = {0};
int red, blue, green;

  typedef enum {
      UNKNOWN,       
      DISCOVERED,
      OBSTACLE,s
      BLOCKs,
      BLOCKb,
      CURRENT_LOCATION,
      BOUNDARY
  } Cell;

    Cell map[MAP_SIZE][MAP_SIZE];
int is_within_bounds(int x, int y) {
    return x >= 0 && x < MAP_SIZE && y >= 0 && y < MAP_SIZE;}
int is_explored(int x, int y) {
    return map[x][y] != UNKNOWN;}
void initialize_map() {
    for (int i = 0; i < MAP_SIZE; i++) {
        for (int j = 0; j < MAP_SIZE; j++) {
            map[i][j] = UNKNOWN;
            visit_counts[i][j] = 0;
        }
    }
    map[0][0] = CURRENT_LOCATION;}


int mapFrequencyToRGB(unsigned long frequency, unsigned long minFreq, unsigned long maxFreq) {
    if (frequency < minFreq) frequency = minFreq;
    if (frequency > maxFreq) frequency = maxFreq;
    return (int)(255.0 * (frequency - minFreq) / (maxFreq - minFreq));
}

void setup() {
    pynq_init();
    switchbox_init();
    gpio_init();

    switchbox_set_pin(S0, SWB_GPIO);
    switchbox_set_pin(S1, SWB_GPIO);
    switchbox_set_pin(S2, SWB_GPIO);
    switchbox_set_pin(S3, SWB_GPIO);
    switchbox_set_pin(OUT, SWB_GPIO);

    gpio_set_direction(S0, GPIO_DIR_OUTPUT);
    gpio_set_direction(S1, GPIO_DIR_OUTPUT);
    gpio_set_level(S0, GPIO_LEVEL_HIGH);
    gpio_set_level(S1, GPIO_LEVEL_HIGH); 

    gpio_set_direction(S2, GPIO_DIR_OUTPUT);
    gpio_set_direction(S3, GPIO_DIR_OUTPUT);

    gpio_set_direction(OUT, GPIO_DIR_INPUT);
}

unsigned long micros() {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (unsigned long)(ts.tv_sec * 1000000 + ts.tv_nsec / 1000);
}

unsigned long pulseIn(io_t pin, gpio_level_t level, unsigned long timeout) {
    unsigned long startMicros = micros();
    unsigned long elapsed;

    while (gpio_get_level(pin) == level) {
        elapsed = micros() - startMicros;
        if (elapsed > timeout) {
            return 0;
        }
    }

    while (gpio_get_level(pin) != level) {
        elapsed = micros() - startMicros;
        if (elapsed > timeout) {
            return 0;
        }
    }

    startMicros = micros();
    while (gpio_get_level(pin) == level) {
        elapsed = micros() - startMicros;
        if (elapsed > timeout) {
            return 0;
        }
    }

    return micros() - startMicros;
}

void msleep(int milliseconds) {
    struct timespec ts;
    ts.tv_sec = milliseconds / 1000;
    ts.tv_nsec = (milliseconds % 1000) * 1000000;
    nanosleep(&ts, NULL);
}

unsigned long measureFrequency(io_t pin, gpio_level_t level, unsigned long timeout) {
    int numReadings = 10;
    unsigned long totalDuration = 0;
    int validReadings = 0;

    for (int i = 0; i < numReadings; i++) {
        unsigned long pulseDuration = pulseIn(pin, level, timeout);
        if (pulseDuration > 0) {
            totalDuration += pulseDuration;
            validReadings++;
        }
        msleep(10);
    }

    if (validReadings == 0) {
        return 0;
    }

    unsigned long averageDuration = totalDuration / validReadings;

    return 1000000 / averageDuration;
}

void measureRGBValues(unsigned long* red, unsigned long* green, unsigned long* blue) {

    gpio_set_level(S2, GPIO_LEVEL_LOW);
    gpio_set_level(S3, GPIO_LEVEL_LOW);
    *red = measureFrequency(OUT, GPIO_LEVEL_LOW, 1000000);

    gpio_set_level(S2, GPIO_LEVEL_HIGH);
    gpio_set_level(S3, GPIO_LEVEL_HIGH);
    *green = measureFrequency(OUT, GPIO_LEVEL_LOW, 1000000);

    gpio_set_level(S2, GPIO_LEVEL_LOW);
    gpio_set_level(S3, GPIO_LEVEL_HIGH);
    *blue = measureFrequency(OUT, GPIO_LEVEL_LOW, 1000000);
}

int ColorDetector2(int red, int green, int blue){
    int redP = red+5;
    //int blueP = blue +10;
    int greenP = green +5; 
     int green_blue = (green+blue)/2;
    int deltaRG = red - green;
    int deltaRB = red - blue;
    int deltaGB = green - blue;
    
   // int deltaGR = green - red;
   // int deltaBR = blue -red;
    //int deltaBG = blue - green;

    
    if (red <= 32 && green <= 32 && blue <= 32) {
        return 0;  // Black
    }
    // White
    else if (abs(deltaRG) <= 12 && abs(deltaRB) <= 25 && abs(deltaGB) <= 15 && green >= 35) {
        return 4;  // White
    }
    // Green
    else if (green > red && green > blue) {
        return 2;  // Green
    }
    // Red
    else if (red > green_blue) {
        return 1;  // Red
    }
    // Blue
    else if (blue > redP && blue >= greenP) {
        return 3;  // Blue
    }

    return 4;  // Default to white
}

void get_front_position() {
    front_x = x;
    front_y = y;
    switch(direction) {
        case 'N': // North should decrease y
            front_y = y + 1;
            break;
        case 'E': // East
            front_x = x + 1;
            break;
        case 'S': // South should increase y
            front_y = y - 1;
            break;
        case 'W': // West
            front_x = x - 1;
            break;
    }
}
void mark(int x, int y, int z) {
      static int prev_x = 0;
      static int prev_y = 0;
      static Cell prev_cell = UNKNOWN;

      if (map[prev_x][prev_y] == CURRENT_LOCATION) {
          map[prev_x][prev_y] = prev_cell;
      }

      prev_cell = (map[x][y] == UNKNOWN) ? DISCOVERED : map[x][y]; 

      switch(z) {
          case 1:
              map[x][y] = BLOCKs;
              break;
          case 2:
              map[x][y] = OBSTACLE;
              break;
          case 3:
              map[x][y] = CURRENT_LOCATION;
              break;
          case 4:
              map[x][y] = BLOCKb;
              break;
          case 5:
              map[x][y] = BOUNDARY;
              break;
          default:
              map[x][y] = prev_cell; 
              break;
      }

      prev_x = x;
      prev_y = y;}
void print_map() {
      printf("Map state:\n");
      for (int y = 0; y < MAP_SIZE; y++) {
          for (int x = 0; x < MAP_SIZE; x++) {
              char cell_char = '?';
              switch(map[x][y]) {
                  case DISCOVERED:
                      cell_char = '.';
                      break;
                  case OBSTACLE:
                      cell_char = 'O';
                      break;
                  case BLOCKb:
                      cell_char = 'b';
                      break;
                  case BLOCKs:
                      cell_char = 's';
                      break;
                  case CURRENT_LOCATION:
                      cell_char = 'R'; 
                      break;
                  case BOUNDARY:
                      cell_char = 'B';
                      break;
                  default:
                      break;
              }
              printf("%c ", cell_char);
          }
          printf("\n");
      }
      printf("Current position (%d, %d)\n", x, y);}
void delay(int number_of_seconds) {
    int milli_seconds = 1000 * number_of_seconds;
    clock_t start_time = clock();
    while (clock() < start_time + milli_seconds);}
void handle_obstacle() {
    if(obstacle_detected){
        stepper_steps(0, 0); 
        delay(5);
        mark(front_x, front_y, 2);
        obstacle_detected = 0;
    }}

void sensor_logic() {
    delay(50);
    get_front_position();
    switch (color) {
        case 1: 
            printf("ON");
            mark(front_x, front_y, 5); // Boundary
            break;
        case 6: 
            mark(front_x, front_y, 1); // BLOCKs
            break; 
        case 2: 
            mark(front_x, front_y, 4); // BLOCKb
            break;
        case 4:
            mark(front_x, front_y, 2); // OBSTACLE
            break;
    }
}

void update_direction(char turn_direction) {
    if (turn_direction == 'L') { 
        switch(direction) {
            case 'N': direction = 'E'; break;
            case 'E': direction = 'S'; break;
            case 'S': direction = 'W'; break;
            case 'W': direction = 'N'; break;
        }
    } else if (turn_direction == 'R') { 
        switch(direction) {
            case 'N': direction = 'W'; break;
            case 'W': direction = 'S'; break;
            case 'S': direction = 'E'; break;
            case 'E': direction = 'N'; break;
        }
    }}
void turn_robot(char turn_direction) {
     update_direction(turn_direction);
      int turn_speed = 3072*6;
      int step_count = 640;

      stepper_set_speed(turn_speed, turn_speed);
      if (turn_direction == 'L') {
          stepper_steps(-step_count, step_count);
          }
       else if (turn_direction == 'R') {
          stepper_steps(step_count, -step_count);
      }
      while (!stepper_steps_done());
      delay(100);}
void choose_next_move() {
    int directions[4][2] = {{0, 1}, {1, 0}, {0, -1}, {-1, 0}}; // Directions corresponding to N, E, S, W
    char turn_directions[4] = {'N', 'E', 'S', 'W'}; // Corresponding turn directions
    int least_visited_x = x, least_visited_y = y, min_visits = INT_MAX;
    int found = 0;
    int best_dir_index = -1;

    for (int i = 0; i < 4; i++) {
        int new_x = x + directions[i][0];
        int new_y = y + directions[i][1];

        if (is_within_bounds(new_x, new_y)) {
            if (!is_explored(new_x, new_y) && map[new_x][new_y] != OBSTACLE && map[new_x][new_y] != BLOCKs && map[new_x][new_y] != BLOCKb) {
                best_dir_index = i;
                found = 1;
                break;
            } else if (visit_counts[new_x][new_y] < min_visits && map[new_x][new_y] != OBSTACLE && map[new_x][new_y] != BLOCKs && map[new_x][new_y] != BLOCKb) {
                least_visited_x = new_x;
                least_visited_y = new_y;
                min_visits = visit_counts[new_x][new_y];
                best_dir_index = i;
            }
        }
    }

    if (!found) {
        if (min_visits != INT_MAX) {
            x = least_visited_x;
            y = least_visited_y;
        } else {
            printf("All reachable cells have been explored or are blocked.\n");
            return;
        }
    } else {
        x += directions[best_dir_index][0];
        y += directions[best_dir_index][1];
    }

    // Determine the direction to turn based on the best direction index
    char target_direction = turn_directions[best_dir_index];
    if (direction != target_direction) {
        // Calculate whether to turn left or right
        int current_index = (strchr(turn_directions, direction) - turn_directions);
        int turns = (best_dir_index - current_index + 4) % 4;
        if (turns == 1 || turns == 3) { // If 1 turn right, if 3 turns left (more efficient)
            char turn = (turns == 1) ? 'R' : 'L';
            turn_robot(turn);
        }
    }

    visit_counts[x][y]++;}

int transmit_msg(int x, int y){ //optimal delay =200
   char msg[6];
    sprintf(msg, "%d %d", x, y);
    //printf("%s", msg);//debug

  unsigned long msg_length = strlen(msg);
 //sending message length 4 bytes
  uart_send(UART0, (uint8_t)msg_length>>0);
  delay(200);
  uart_send(UART0, (uint8_t)msg_length>>8);
  delay(200);
  uart_send(UART0, (uint8_t)msg_length>>16);
  delay(200);
  uart_send(UART0, (uint8_t)msg_length>>24);
  delay(200);
//sending message 
  for(unsigned long i=0; i<msg_length;i++){
    uart_send(UART0, msg[i]);
    delay(200);
  }
  return 1;// sent succesfully
}


void step_forward() {
    choose_next_move();
    stepper_set_speed(3070*5,3070*5);
    stepper_steps(1280/2,1280/2);
    while (!stepper_steps_done());
    mark(x, y, 3);}

void *runRGBControl() {
    // Continuously measure and map RGB values
    while(1){
        unsigned long redFreq, greenFreq, blueFreq;
        measureRGBValues(&redFreq, &greenFreq, &blueFreq);

        int red = mapFrequencyToRGB(redFreq, minFreq, maxFreq)-25;
        int green = mapFrequencyToRGB(greenFreq, minFreq, maxFreq);
        int blue = mapFrequencyToRGB(blueFreq, minFreq, maxFreq);
            if (blue <  60) {
                 blue -= 5;
                         }


    //printf("Mapped RGB - R: %d, G: %d, B: %d\n", red, green, blue);
    //printf("%d\n", ColorDetector2(red, green, blue));
    color = ColorDetector2(red, green, blue);
    msleep(1000);
    }
    return NULL;
}

int main(void) {
    pynq_init();
    stepper_init();  
    switchbox_set_pin(IO_AR0, SWB_UART0_RX);
    switchbox_set_pin(IO_AR1, SWB_UART0_TX);
    setup();
    uart_init(UART0);
    uart_reset_fifos(UART0);
    stepper_enable();
    initialize_map();
    mark(x, y, 3);
    transmit_msg(x,y);
    print_map();
    pthread_t rgbThread;
    pthread_create(&rgbThread, NULL, runRGBControl, NULL);
    while(1) {
        printf("direction: %c", direction);
        printf("(%d, %d)\n", front_x, front_y);
        sensor_logic();
        delay(5000);
        handle_obstacle();
        step_forward();
        transmit_msg(x,y);
        print_map();
    }

    stepper_destroy();
    pynq_destroy();
    return 0;
}
