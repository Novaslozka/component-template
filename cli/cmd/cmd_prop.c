#include "srs4/cmd_cli_prop.h"

#include "prop-client/prop_spec.h"
#include "prop-client/prop_query.h"
#include "prop-client/prop_client_helpers.h"
#include "prop-client/prop_client_commands.h"
#include "prop-client/prop_client.h"

#include <console/cmddef.h>

#define PROP_DEFAULT_NODE           (13)
#define PROP_DEFAULT_TIMEOUT        (3000)

static uint8_t prop_node = PROP_DEFAULT_NODE;
static uint32_t prop_timeout = PROP_DEFAULT_TIMEOUT;

// forward declarations of the commands that are to be registered
static int cmd_prop_client_node(console_ctx_t *ctx, cmd_signature_t *reg);
static int cmd_prop_client_get(console_ctx_t *ctx, cmd_signature_t *reg);
/*static int cmd_prop_client_set(console_ctx_t *ctx, cmd_signature_t *reg);
static int cmd_prop_client_reset(console_ctx_t *ctx, cmd_signature_t *reg);
static int cmd_prop_client_list(console_ctx_t *ctx, cmd_signature_t *reg);
static int cmd_prop_client_save(console_ctx_t *ctx, cmd_signature_t *reg);
static int cmd_prop_client_load(console_ctx_t *ctx, cmd_signature_t *reg);
static int cmd_prop_client_erase(console_ctx_t *ctx, cmd_signature_t *reg);
static int cmd_prop_client_lock(console_ctx_t *ctx, cmd_signature_t *reg);
static int cmd_prop_client_unlock(console_ctx_t *ctx, cmd_signature_t *reg);*/

// the registration function
void register_cli_srs4_prop_cmds(void) {
  console_group_add("prop", "Satlab Properties commands");

  console_cmd_register(cmd_prop_client_node, "prop node");
  console_cmd_register(cmd_prop_client_get, "prop get");
  /*console_cmd_register(cmd_prop_client_set, "prop set");
  console_cmd_register(cmd_prop_client_reset, "prop reset");
  console_cmd_register(cmd_prop_client_list, "prop list");
  console_cmd_register(cmd_prop_client_save, "prop save");
  console_cmd_register(cmd_prop_client_load, "prop load");
  console_cmd_register(cmd_prop_client_erase, "prop erase");
  console_cmd_register(cmd_prop_client_lock, "prop lock");
  console_cmd_register(cmd_prop_client_unlock, "prop unlock");*/
}

// implementations of all commands follow

int cmd_prop_client_node(console_ctx_t *ctx, cmd_signature_t *reg) {
  (void)ctx;

  static struct {
    struct arg_int *node;
    struct arg_end *end;
  } args;

  if (reg) {
    args.node = arg_int1(NULL, NULL, "<node>", EXPENDABLE_STRING("Prop client CSP address (default " STR(PROP_DEFAULT_NODE)")"));
    args.end = arg_end(1);

    reg->argtable = &args;
    reg->help = EXPENDABLE_STRING("Set prop client node address");
    return(0);
  }

  prop_node = (uint8_t)(args.node->ival[0]);
  console_printf("Prop client node address was set to %d\n", prop_node);

  return(CONSOLE_OK);
}

int cmd_prop_client_get(console_ctx_t *ctx, cmd_signature_t *reg) {
  static struct {
    struct arg_str *prop_names;
    struct arg_end *end;
  } args;

  if(reg) {
    args.prop_names = arg_strn(NULL, NULL, "<group.property>", 1, 10, EXPENDABLE_STRING("Names of properties to get"));
    args.end = arg_end(1);

    reg->argtable = &args;
    reg->help = EXPENDABLE_STRING("Get remote properties");
    return 0;
  }

  extern const struct prop_subsys_spec srs4_props;
  
  int i, ret;
	const struct prop_spec *spec;
	char valstr[128];
	struct sl_prop_client_command_context prop_ctx = {
    .node = &prop_node,
    .port = SL_PROP_DEFAULT_PORT, 
    .timeout = &prop_timeout,
    .spec = &srs4_props,
  };

	for (i = 0; i < args.prop_names->count; i++) {
		spec = sl_prop_spec_find_by_name(prop_ctx.spec, args.prop_names->sval[i]);
		if (!spec) {
			console_printf("No such property \"%s\"\n", args.prop_names->sval[i]);
			return CONSOLE_ERR_INVALID_ARG;
		}

		ret = sl_prop_remote_get_to_string(*(prop_ctx.node), prop_ctx.port, *(prop_ctx.timeout), spec, valstr, sizeof(valstr));
		if (ret < 0) {
			console_printf("Failed to read property value: %s\n", strerror(-ret));
			return CONSOLE_ERR_NOT_POSSIBLE;
		}

		console_printf("%s\n", valstr);
	}

  return 0;
}
