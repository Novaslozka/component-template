#include "cli/cmd_cli.h"
#include "cli/client.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <console/cmddef.h>

// SALLY backends - set during build
extern service_com_t COM_BACKEND;
service_com_t* com = &COM_BACKEND;

// communnication port handle
service_com_handle_t hcom = NULL;

// forward declarations of the commands that are to be registered
static int cli_ping(console_ctx_t *ctx, cmd_signature_t *reg);
static int cli_echo(console_ctx_t *ctx, cmd_signature_t *reg);
static int cli_kill(console_ctx_t *ctx, cmd_signature_t *reg);
static int cli_port(console_ctx_t *ctx, cmd_signature_t *reg);

// the registration function
void register_cli_cmds(void) {
  console_group_add("cli", "Client example");

  console_cmd_register(cli_ping, "cli ping");
  console_cmd_register(cli_echo, "cli echo");
  console_cmd_register(cli_kill, "cli kill");
  console_cmd_register(cli_port, "cli port");
}

// implementations of all commands follow

static int cli_ping(console_ctx_t *ctx, cmd_signature_t *reg) {
  EMPTY_CMD_SETUP("Ping the server")

  if(!hcom) {
    console_printf("No port set!\n");
    return CONSOLE_ERR_NOT_POSSIBLE;
  }
  
  uint8_t ping_buff[16] = { 0 };
  int16_t ret = client_ping(hcom, ping_buff);
  if(ret != 0) {
    console_printf("Failed, code %d\n", ret);
    return CONSOLE_ERR_IO;
  }

  console_printf("Server replied with: %s\n", ping_buff);
  return CONSOLE_OK;
}

static int cli_echo(console_ctx_t *ctx, cmd_signature_t *reg) {
  static struct {
    struct arg_str *msg;
    struct arg_end *end;
  } args;

  if(reg) {
    args.msg = arg_str1(NULL, NULL, "<msg>", EXPENDABLE_STRING("Message to echo"));
    args.end = arg_end(1);

    reg->argtable = &args;
    reg->help = EXPENDABLE_STRING("Echo a message");
    return 0;
  }

  if(!hcom) {
    console_printf("No port set!\n");
    return CONSOLE_ERR_NOT_POSSIBLE;
  }
  
  char echo_in_buff[16] = { 0 };
  int16_t ret = client_echo(hcom, echo_in_buff, args.msg->sval[0]);
  if(ret != 0) {
    console_printf("Failed, code %d\n", ret);
    return CONSOLE_ERR_IO;
  }

  console_printf("Got echo: %s\n", echo_in_buff);
  return CONSOLE_OK;
}

static int cli_kill(console_ctx_t *ctx, cmd_signature_t *reg) {
  EMPTY_CMD_SETUP("Kill the server")

  if(!hcom) {
    console_printf("No port set!\n");
    return CONSOLE_ERR_NOT_POSSIBLE;
  }
  
  int16_t ret = client_kill_server(hcom);
  if(ret != 0) {
    console_printf("Failed, code %d\n");
    return CONSOLE_ERR_IO;
  }

  console_printf("Server terminated\n");
  return CONSOLE_OK;
}

static int cli_port(console_ctx_t *ctx, cmd_signature_t *reg) {
  static struct {
    struct arg_str *port;
    struct arg_end *end;
  } args;

  if(reg) {
    args.port = arg_str1(NULL, NULL, "<port>", EXPENDABLE_STRING("Port to use for communication with the server"));
    args.end = arg_end(1);

    reg->argtable = &args;
    reg->help = EXPENDABLE_STRING("Set the communication port");
    return 0;
  }

  if(!hcom) {
    hcom = com->com_handle_malloc();
  }

  client_set_backends(NULL, NULL, com);

  service_com_conf_t conf = { .speed = 0 };
  com->open(hcom, (service_com_device_t)args.port->sval[0], &conf);
  return CONSOLE_OK;
}
