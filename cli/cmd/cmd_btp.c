#include "srs4/cmd_cli_btp.h"

/* Get the GNU version of basename(3) */
#define _GNU_SOURCE

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include "btp/client.h"
#include "btp/error.h"
#include "btp/types.h"

#include "util/util.h"

#include <csp/csp.h>
#include <csp/arch/csp_time.h>

#include <console/cmddef.h>

#define DEFAULT_BACKEND		"file"
#define DEFAULT_TIMEOUT		1000
#define DEFAULT_TIMEOUT_CSUM	10000
#define DEFAULT_BSIZE		186
#define DEFAULT_COUNT		25
#define DEFAULT_ATTEMPTS	3

#define PROGRESS_WIDTH		40U

#define BTP_DEFAULT_NODE            (13)

static uint8_t btp_node = BTP_DEFAULT_NODE;

struct progress_state {
	int sizechars;
	uint32_t last;
	uint32_t lt;
	uint32_t bps;
	uint32_t last_print;
	uint32_t last_print_speed;
	uint32_t remain;
	bool quiet;
	//struct slash *slash;
	console_ctx_t* console;
	bool abort;
};

static char *pretty_bytes(uint32_t bps)
{
	static char buf[25];

	if (bps >= 1048576)
		snprintf(buf, sizeof(buf), "%.1f Mbit", bps/1048576.0);
	else if (bps >= 1024)
		snprintf(buf, sizeof(buf), "%.1f kbit", bps/1024.0);
	else
		snprintf(buf, sizeof(buf), "%"PRIu32" bit", bps);

	return buf;
}

static char *pretty_seconds(uint32_t seconds)
{
	static char buf[25];
	uint32_t hrs = 0, min = 0, sec = 0;

	hrs = seconds / 3600;
	min = (seconds - hrs*3600) / 60;
	sec = seconds - hrs*3600 - min*60;

	if (seconds >= 3600)
		snprintf(buf, sizeof(buf), "%uh %um %us", hrs, min, sec);
	else if (seconds >= 60)
		snprintf(buf, sizeof(buf), "%um %us", min, sec);
	else
		snprintf(buf, sizeof(buf), "%us", sec);

	return buf;
}

static void pretty_progress(struct btp_context *btp, void *arg)
{
	int i, prog, bps;
	struct progress_state *ps = arg;
	uint32_t now;

  // TODO check ^C only
  if(console_can_read_ctx(ps->console) > 0) {
    console_printf("\nInterrupted by user\n");
		ps->abort = true;
  }

	if (ps->quiet || ps->abort)
		return;

	now = csp_get_ms();

	if ((now - ps->last_print) < 250 && btp->next + 1 < btp->blocks)
		return;

	ps->last_print = now;

	console_printf("\r\033[K;");

	if (!ps->sizechars)
		console_printf("%"PRIu32" %n\r", btp->size, &ps->sizechars);

	console_printf("\r%3u%% [", btp->next * 100 / btp->blocks);
	prog = btp->next * PROGRESS_WIDTH / btp->blocks;
	for (i = 0; i < prog; i++)
		console_printf("#");
	for (i = 0; i < PROGRESS_WIDTH - prog; i++)
		console_printf(" ");
	console_printf("]");
	console_printf("%*"PRIu32" ", ps->sizechars,
			 min(btp->next * btp->block_size, btp->size));

	if ((now - ps->last_print_speed) > 1000) {
		bps = ((btp->progress - ps->last) * (uint64_t)8000) / (now - ps->lt);
		ps->bps = ps->bps ? 0.7 * ps->bps + 0.3 * bps : bps;

		ps->remain = (btp->blocks - btp->next) * btp->block_size;
		ps->lt = now;
		ps->last_print_speed = ps->lt;
		ps->last = btp->progress;
	}

	if (!btp->complete && ps->bps) {
		console_printf("%s/s", pretty_bytes(ps->bps));
		if (ps->bps)
			console_printf(" eta %s ", pretty_seconds((ps->remain * 8) / ps->bps));
	}

	fflush(stdout);
}

// forward declarations of the commands that are to be registered
static int cmd_btp_node(console_ctx_t *ctx, cmd_signature_t *reg);
static int cmd_btp_push(console_ctx_t *ctx, cmd_signature_t *reg);
/*static int cmd_btp_pull(console_ctx_t *ctx, cmd_signature_t *reg);
static int cmd_btp_list(console_ctx_t *ctx, cmd_signature_t *reg);
static int cmd_btp_remove(console_ctx_t *ctx, cmd_signature_t *reg);
static int cmd_btp_move(console_ctx_t *ctx, cmd_signature_t *reg);
static int cmd_btp_copy(console_ctx_t *ctx, cmd_signature_t *reg);
static int cmd_btp_shell(console_ctx_t *ctx, cmd_signature_t *reg);*/

// the registration function
void register_cli_srs4_btp_cmds(void) {
  console_group_add("btp", "Satlab Blob Transfer Protocol commands");
  
  console_cmd_register(cmd_btp_node, "btp node");
  console_cmd_register(cmd_btp_push, "btp push");
  /*console_cmd_register(cmd_btp_pull, "btp pull");
  console_cmd_register(cmd_btp_list, "btp list");
  console_cmd_register(cmd_btp_remove, "btp remove");
  console_cmd_register(cmd_btp_move, "btp move");
  console_cmd_register(cmd_btp_copy, "btp copy");
  console_cmd_register(cmd_btp_shell, "btp shell");*/
}

// implementations of all commands follow

int cmd_btp_node(console_ctx_t *ctx, cmd_signature_t *reg) {
  (void)ctx;

  static struct {
    struct arg_int *node;
    struct arg_end *end;
  } args;

  if (reg) {
    args.node = arg_int1(NULL, NULL, "<node>", EXPENDABLE_STRING("BTP client CSP address (default " STR(BTP_DEFAULT_NODE)")"));
    args.end = arg_end(1);

    reg->argtable = &args;
    reg->help = EXPENDABLE_STRING("Set BTP client node address");
    return(0);
  }

  btp_node = (uint8_t)(args.node->ival[0]);
  console_printf("BTP client node address was set to %d\n", btp_node);

  return(CONSOLE_OK);
}


int cmd_btp_push(console_ctx_t *ctx, cmd_signature_t *reg) {
  (void)ctx;

  static struct {
    struct arg_str *localpath;
    struct arg_str *remotepath;
    struct arg_int *port;
    struct arg_str *backend;
    struct arg_int *bsize;
    struct arg_int *count;
    struct arg_int *timeout;
    struct arg_int *timeout_server;
    struct arg_int *timeout_csum;
    struct arg_int *attempts;
    struct arg_lit *quiet;
    struct arg_lit *rdp;
    struct arg_lit *force;
    struct arg_lit *paranoid;
    struct arg_end *end;
  } args;

  if (reg) {
    args.localpath = arg_str1(NULL, NULL, "<local-path>", EXPENDABLE_STRING("Local path"));
    args.remotepath = arg_str1(NULL, NULL, "<remote-path>", EXPENDABLE_STRING("Remote path"));
    args.port = arg_int0("p", "port", "<port>", EXPENDABLE_STRING("Connect to PORT (default " STR(BTP_DEFAULT_PORT)")"));
    args.backend = arg_str0("b", "backend", "<backend>", EXPENDABLE_STRING("Use BACKEND as backend (default " DEFAULT_BACKEND ")"));
    args.bsize = arg_int0("s", "size", "<size>", EXPENDABLE_STRING("Transfer data in BLOCKSIZE sized blocks (default " STR(DEFAULT_BSIZE)")"));
    args.count = arg_int0("c", "count", "<count>", EXPENDABLE_STRING("Request COUNT blocks at a time (default " STR(DEFAULT_COUNT)")"));
    args.timeout = arg_int0("t", "timeout", "<ms>", EXPENDABLE_STRING("Set timeout to TIMEOUT ms (default " STR(DEFAULT_TIMEOUT)")"));
    args.timeout_server = arg_int0("T", "srvtimeout", "<ms>", EXPENDABLE_STRING("Set server timeout to TIMEOUT ms (default is 10x client timeout)"));
    args.timeout_csum = arg_int0("i", "itimeout", "<ms>", EXPENDABLE_STRING("Set initial checksum timeout to TIMEOUT ms (default " STR(DEFAULT_TIMEOUT_CSUM)")"));
    args.attempts = arg_int0("a", "attempts", "<attempts>", EXPENDABLE_STRING("Retry connection up to ATTEMPTS tries (default " STR(DEFAULT_ATTEMPTS)")"));
    args.quiet = arg_lit0("q", "quiet", EXPENDABLE_STRING("Quiet mode. Do not print progress bar"));
    args.rdp = arg_lit0("r", "rdp", EXPENDABLE_STRING("Enable RDP for transfer"));
    args.force = arg_lit0("f", "force", EXPENDABLE_STRING("Delete data and map file before starting transfer"));
    args.paranoid = arg_lit0("d", "paranoid", EXPENDABLE_STRING("Paranoid mode. Always sync blocks to disk"));
    args.end = arg_end(14);

    reg->argtable = &args;
    reg->help = EXPENDABLE_STRING("Push file to target");
    return(0);
  }

	int ret;

	struct btp_context *btp;
	bool force = (args.force->count > 0);
	uint8_t node = btp_node;
	uint8_t port = (args.port->count > 0) ? (uint8_t)args.port->ival[0] : BTP_DEFAULT_PORT;
  uint32_t timeout = (args.timeout->count > 0) ? (uint32_t)args.timeout->ival[0] : DEFAULT_TIMEOUT;
  uint32_t timeout_csum = (args.timeout_csum->count > 0) ? (uint32_t)args.timeout_csum->ival[0] : DEFAULT_TIMEOUT_CSUM;
	uint32_t timeout_server = (args.timeout_server->count > 0) ? (uint32_t)args.timeout_server->ival[0] : 0;
	uint32_t options = (args.rdp->count > 0) ? CSP_O_RDP : CSP_O_NONE;
  uint32_t bsize = (args.bsize->count > 0) ? (uint32_t)args.bsize->ival[0] : DEFAULT_BSIZE;
  uint32_t count = (args.count->count > 0) ? (uint32_t)args.count->ival[0] : DEFAULT_COUNT;
  unsigned int attempts = (args.attempts->count > 0) ? (unsigned int)args.attempts->ival[0] : DEFAULT_ATTEMPTS;
	uint32_t start, stop;

	char *backend = (args.backend->count > 0) ? (char*)args.backend->sval[0] : (char*)DEFAULT_BACKEND;
	char *remotepath = NULL;
	char *localpath = NULL;
	char pathbuf[BLOB_NAME_SIZE];

	struct progress_state pgstate = {};
  pgstate.console = console_active_ctx;
  pgstate.quiet = (args.quiet->count > 0);

  remotepath = (char*)args.remotepath->sval[0];
	if (remotepath[strlen(remotepath) - 1] == '/') {
		strncpy(pathbuf, remotepath, BLOB_NAME_SIZE - 1);
		pathbuf[BLOB_NAME_SIZE - 1] = '\0';
		strncat(pathbuf, basename(args.remotepath->sval[0]), BLOB_NAME_SIZE - 1);
		pathbuf[BLOB_NAME_SIZE - 1] = '\0';
		remotepath = pathbuf;
	}
	localpath = (char*)args.localpath->sval[0];

	/* Default server side timeout 10x client */
	if (!timeout_server)
		timeout_server = 10 * timeout;

	/* Connect to server */
	btp = btp_client_connect(node, port, timeout, options, attempts);
	if (!btp) {
		console_printf("Failed to connect to BTP server\n");
    return CONSOLE_ERR_IO;
	}

	/* Establish connection to server */
	ret = btp_client_upload(btp, backend, remotepath, localpath, bsize, timeout_csum, timeout_server, force);
	if (ret != BTP_EOK) {
		console_printf("Could not push %s: %s\n", localpath, btp_error(ret));
		goto complete;
	}

	/* Early exit if transfer is already complete */
	if (btp_client_finished(btp)) {
		console_printf("File already complete\n");
		goto complete;
	}

	/* Log start time for statistics */
	start = csp_get_ms();

	/* Get initial status */
	ret = btp_client_request_status(btp);
	if (ret != BTP_EOK) {
		console_printf("\nCould not get status: %s\n", btp_error(ret));
		goto disconnect;
	} else if (pgstate.abort) {
		goto disconnect;
	}

	/* Send blocks until completion or error */
	while (!btp_client_finished(btp)) {
		pretty_progress(btp, &pgstate);

		/* Send blocks */
		ret = btp_client_send_blocks(btp, BTP_OFFSET_NEXT, count, pretty_progress, &pgstate);
		if (ret != BTP_EOK) {
			console_printf("\nCould not send blocks: %s\n", btp_error(ret));
			goto disconnect;
		} else if (pgstate.abort) {
			goto complete;
		}

		/* Send status request */
		ret = btp_client_request_status(btp);
		if (ret != BTP_EOK) {
			console_printf("\nCould not get status: %s\n", btp_error(ret));
			goto disconnect;
		} else if (pgstate.abort) {
			goto complete;
		}
	}

	/* Log transfer speed */
	if (!pgstate.quiet && !pgstate.abort) {
		stop = csp_get_ms();
		pretty_progress(btp, &pgstate);
		console_printf("\n%s/s (%"PRIu32" bytes in %.3fs)\n",
		       pretty_bytes((btp->progress * 8) / ((stop - start) / 1000.0)),
		       btp->progress, (stop - start) / 1000.0);
	}

complete:
	/* Complete transfer */
	ret = btp_client_complete(btp);
	if (ret == BTP_ESTALE)
		console_printf("WARNING: checksum mismatch!\n");

disconnect:
	/* Close connection */
	btp_client_disconnect(btp);

	return CONSOLE_OK;
}
