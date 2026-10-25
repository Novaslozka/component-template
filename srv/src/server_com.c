// app headers
#include "template/server_com.h"

// standard libraries
#include <string.h>
#include <stdio.h>

// SALLY libraries
#include <service/service.h>
#include <service/service_filesystem.h>
#include <service/service_com.h>

// headers shared for both client and server
#include "packets.h"

// SALLY backends provided by the library user
static service_fs_t* server_fs = NULL;
static service_os_t* server_os = NULL;
static service_com_t* server_com = NULL;

void server_set_backends(service_fs_t* fs, service_os_t* os, service_com_t* com) {
  server_fs = fs;
  server_os = os;
  server_com = com;
}


// Proměnné:

typedef struct {
    int x;
    int y;
} point_t;

static point_t snake[64];
static int snake_len = 0;
static point_t apple = {0, 0};
static int game_initialized = 0;

// Funkce pro reset hry
static void reset_snake_game(void) {
    snake_len = 3; // Počáteční délka hada
    // První setup hada
    snake[0] = (point_t){4, 4}; // Hlava
    snake[1] = (point_t){3, 4}; // Tělo
    snake[2] = (point_t){2, 4}; // Ocas
    
    // Počáteční pozice jablka
    apple = (point_t){6, 4};
}



int server_process_cmd(service_com_handle_t hcom, uint8_t* buff, size_t len) {
  // an example of a very simple processing
  switch(buff[0]) {
    case DRIVER_TEMPLATE_CMD_PING: {
      // on ping, just send the defined reply
      const char* rpl = DRIVER_TEMPLATE_CMD_PING_RPL;
      server_com->write(hcom, (void*)rpl, strlen(rpl));
    } break;

    case DRIVER_TEMPLATE_CMD_ECHO: {
      // on echo, send everything back, except the command byte
      uint8_t* pos = (uint8_t*)strstr((char*)buff, DRIVER_TEMPLATE_CMD_LINE_END);
      if(pos != NULL) {
        size_t echo_len = pos-buff-1;
        server_com->write(hcom, (void*)&buff[1], echo_len);
        server_com->write(hcom, (void*)DRIVER_TEMPLATE_CMD_LINE_END, strlen(DRIVER_TEMPLATE_CMD_LINE_END));
      }
    } break;




    case DRIVER_TEMPLATE_CMD_TEST: {
      const char* rpl = "test\r\n";
      server_com->write(hcom, (void*)rpl, strlen(rpl));
    } break;



    case DRIVER_TEMPLATE_SNAKE: {
      uint8_t direction = buff[1];

      if (!game_initialized) {
        reset_snake_game();
        game_initialized = 1;
      }

      point_t new_head = snake[0];
      switch (direction) {
        case SNAKE_DIR_UP:    new_head.y--; break;
        case SNAKE_DIR_DOWN:  new_head.y++; break;
        case SNAKE_DIR_LEFT:  new_head.x--; break;
        case SNAKE_DIR_RIGHT: new_head.x++; break;
        default: break;
      }

      // Kontrola kolize:
      int collision = 0;
      if (new_head.x < 0 || new_head.x >= 8 || new_head.y < 0 || new_head.y >= 8) {
        collision = 1;
      } else {
        for (int i = 0; i < snake_len; i++) {
          if (snake[i].x == new_head.x && snake[i].y == new_head.y) {
            collision = 1;
            break;
          }
        }
      }

      if (collision) {
        reset_snake_game();
      } else {
        // Kontrola, zda snědl jablko:
        int ate_apple = (new_head.x == apple.x && new_head.y == apple.y);

        // Posunutí těla hada:
        for (int i = snake_len; i > 0; i--) {
          snake[i] = snake[i - 1];
        }
        snake[0] = new_head;

        // Nové jablko a posunutí:
        if (ate_apple) {
          if (snake_len >= 64) {
            reset_snake_game();
          } else {
            snake_len++;

            int placed = 0;
            while (!placed) {
              apple.x = (apple.x + 3) % 8;
              apple.y = (apple.y + 2) % 8;

              int collision_with_snake = 0;
              for (int i = 0; i < snake_len; i++) {
                if (snake[i].x == apple.x && snake[i].y == apple.y) {
                  collision_with_snake = 1;
                  break;
                }
              }

              if (!collision_with_snake) {
                  placed = 1;
              }
            }
          }
        }
      }

      // Vykreslení mřížky:
      char rpl[70];
      memset(rpl, '.', 64);

      // Vykreslení jablka:
      if (apple.y >= 0 && apple.y < 8 && apple.x >= 0 && apple.x < 8) {
          rpl[apple.y * 8 + apple.x] = 'a';
      }

      // Vykreslení hada:
      for (int i = 0; i < snake_len; i++) {
        if (snake[i].y >= 0 && snake[i].y < 8 && snake[i].x >= 0 && snake[i].x < 8) {
          rpl[snake[i].y * 8 + snake[i].x] = (i == 0) ? 'O' : 'o';
        }
      }

      // Odeslání aktuální mřížky:
      server_com->write(hcom, (void*)rpl, 64);
    } break;







    case DRIVER_TEMPLATE_CMD_KILL:
      // on kill, stop processing 
      return(DRIVER_TEMPLATE_CMD_KILL);
    
    default:
      break;
  }

  return(0);
}
