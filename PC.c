
#include <stdio.h>
#include <string.h>
#include <C:\Program Files\mosquitto\devel\mosquitto.h>
#define MAP_SIZE 19
#include <time.h>
 
void delay(int milli_seconds)
{
    // Storing start time
    clock_t start_time = clock();
 
    // looping till required time is not achieved
    while (clock() < start_time + milli_seconds);
}

int x, y;

struct robot{
   int x,y;
};



typedef enum {
      UNKNOWN,       
      DISCOVERED,
      OBSTACLE,
      BLOCKs,
      BLOCKb,
      CURRENT_LOCATION,
      red_block, // type b size 1 color r
      blue_block, // type b size 1 color b
      black_block, //type b size 1 color B
      white_block, //type b size 1 color w
      big_block, ////type b size 2 color 0
      mountain, // type m
      green_block
  } Cell;


    Cell map[MAP_SIZE][MAP_SIZE];


struct mosquitto *mosq;




void initialize_map() {
    for (int i = 0; i < MAP_SIZE; i++) {
        for (int j = 0; j < MAP_SIZE; j++) {
            map[i][j] = UNKNOWN;
        }
    }
    map[0][0] = CURRENT_LOCATION;
}

void mark(int x, int y, int z) {
      static int prev_x = 0;
      static int prev_y = 0;
      static Cell prev_cell = UNKNOWN;

      if (x < 0 || x >= MAP_SIZE || y < 0 || y >= MAP_SIZE) {
          printf("Error: Position out of map bounds\n");
          return;
      }

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
                  case 6:
              map[x][y] = red_block;
              break;
          case 7:
              map[x][y] = blue_block;
              break;
          case 8:
              map[x][y] = black_block;
              break;
          case 9:
              map[x][y] = white_block;
              break;

          case 10:
              map[x][y] = big_block;
              break;
          case 11:
              map[x][y] = mountain;
              break; 

           case 12:
              map[x][y] = green_block;
              break;              
          default:
              map[x][y] = prev_cell; 
              break;
      }

      prev_x = x;
      prev_y = y;
}

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
                    case red_block:
                      cell_char = 'r'; 
                      break;
                      case black_block:
                      cell_char = 'b'; 
                      break;  
                      case green_block:
                      cell_char = 'b'; 
                      break;   
                  default:
                      break;
              }
              printf("%c ", cell_char);
          }
          printf("\n");
      }
      printf("Current position (%d, %d)\n", x, y);
}

int transmit_msg(char robot, int x, int y){
    printf("robot is %c", robot);
    char topic[20];
    if (robot == 'A'){
        strcpy(topic, "/pynqbridge/36");
        printf("sending to A\n");
    }
    if (robot== 'B'){
        strcpy(topic, "/pynqbridge/74");
        printf("sending to B\n");
    }
    else{
    printf("fail intro");
     return 1;
    }

    	mosq = mosquitto_new(NULL, true, NULL);
      if(mosq == NULL){
		fprintf(stderr, "Error: Out of memory.\n");
		return 1;
	}

    mosquitto_username_pw_set(mosq, "Student72", "cheiFot8");
    int rc = mosquitto_connect(mosq, "mqtt.ics.ele.tue.nl", 1883, 60);
    if(rc != MOSQ_ERR_SUCCESS){
		mosquitto_destroy(mosq);
		fprintf(stderr, "Error: %s\n", mosquitto_strerror(rc));
		return 1;
	}
     printf("succesfull connect\n");

    char payload[10]= "Hello";
    printf("%s", payload);


	delay(500); //milli seconds

    rc = mosquitto_publish(mosq, NULL, topic, strlen(payload), payload, 0, false);
	
    if(rc != MOSQ_ERR_SUCCESS){
		fprintf(stderr, "Error publishing: %s\n", mosquitto_strerror(rc));
        return 1;
	}
	else printf("success\n");  
    mosquitto_destroy(mosq);
    return 0;
}

void receive_msg(struct robot *A){
	
	int rc;
	struct mosquitto_message *msg;
	rc = mosquitto_subscribe_simple(
			&msg, 1, true,
			"/pynqbridge/36/#", 0,
			"mqtt.ics.ele.tue.nl", 1883,
			NULL, 60, true,
			"Student71", "jaich4Ya",
			NULL, NULL);
			if(rc){
				printf("Error: %s\n", mosquitto_strerror(rc));
				mosquitto_lib_cleanup();
			}
			if(strcmp(msg->topic, "/pynqbridge/36/")){
			    sscanf(msg->payload,"%d %d", &A->x, &A->y);

                transmit_msg('B', A->x, A->y);
            }
	mosquitto_message_free(&msg);
}


int main(int argc, char *argv[]){
	struct robot robot_A;
	mosquitto_lib_init();
	while (1){
        receive_msg(&robot_A);

        printf("A pos = %d %d", robot_A.x, robot_A.y);
		mark(robot_A.x,robot_A.y, 3);
		print_map();
	}		
	mosquitto_lib_cleanup();
	return 0;
}
