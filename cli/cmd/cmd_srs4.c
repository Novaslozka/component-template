#include "srs4/cmd_cli_srs4.h"

#include "satlab/srs4.h"
#include "prop-client/prop_client_commands.h"
#include "util/string.h"

#include <console/cmddef.h>

#define SRS4_DEFAULT_NODE           (13)
#define SRS4_DEFAULT_TIMEOUT        (3000)

static uint8_t srs4_node = SRS4_DEFAULT_NODE;
static uint32_t srs4_timeout = SRS4_DEFAULT_TIMEOUT;

// forward declarations of the commands that are to be registered
static int cmd_srs4_node(console_ctx_t *ctx, cmd_signature_t *reg);
static int cmd_srs4_timeout(console_ctx_t *ctx, cmd_signature_t *reg);
static int cmd_srs4_gwdt_reset(console_ctx_t *ctx, cmd_signature_t *reg);
static int cmd_srs4_gwdt_counter(console_ctx_t *ctx, cmd_signature_t *reg);
static int cmd_srs4_run(console_ctx_t *ctx, cmd_signature_t *reg);
static int cmd_srs4_tm(console_ctx_t *ctx, cmd_signature_t *reg);
static int cmd_srs4_boot_flash(console_ctx_t *ctx, cmd_signature_t *reg);
static int cmd_srs4_boot_erase(console_ctx_t *ctx, cmd_signature_t *reg);
static int cmd_srs4_boot_checksum(console_ctx_t *ctx, cmd_signature_t *reg);
static int cmd_srs4_boot_verify(console_ctx_t *ctx, cmd_signature_t *reg);
static int cmd_srs4_boot_set(console_ctx_t *ctx, cmd_signature_t *reg);

// the registration function
void register_cli_srs4_cmds(void) {
  console_group_add("srs4", "Satlab SRS-4 Subsystem commands");

  console_cmd_register(cmd_srs4_node, "srs4 node");
  console_cmd_register(cmd_srs4_timeout, "srs4 timeout");

  console_group_add("srs4 gwdt", "SRS-4 Ground watchdog commands");
  console_cmd_register(cmd_srs4_gwdt_reset, "srs4 gwdt reset");
  console_cmd_register(cmd_srs4_gwdt_counter, "srs4 gwdt counter");

  console_cmd_register(cmd_srs4_run, "srs4 run");
  console_cmd_register(cmd_srs4_tm, "srs4 tm");

  console_group_add("srs4 boot", "SRS-4 Bootloader commands");
  console_cmd_register(cmd_srs4_boot_flash, "srs4 boot flash");
  console_cmd_register(cmd_srs4_boot_erase, "srs4 boot erase");
  console_cmd_register(cmd_srs4_boot_checksum, "srs4 boot checksum");
  console_cmd_register(cmd_srs4_boot_verify, "srs4 boot verify");
  console_cmd_register(cmd_srs4_boot_set, "srs4 boot set");
}

// implementations of all commands follow

int cmd_srs4_node(console_ctx_t *ctx, cmd_signature_t *reg) {
  (void)ctx;

  static struct {
    struct arg_int *node;
    struct arg_end *end;
  } args;

  if (reg) {
    args.node = arg_int1(NULL, NULL, "<node>", EXPENDABLE_STRING("SRS-4 CSP address (default " STR(SRS4_DEFAULT_NODE)")"));
    args.end = arg_end(1);

    reg->argtable = &args;
    reg->help = EXPENDABLE_STRING("Set SRS-4 node address");
    return(0);
  }

  srs4_node = (uint8_t)(args.node->ival[0]);
  console_printf("SRS-4 node address was set to %d\n", srs4_node);

  return(CONSOLE_OK);
}

int cmd_srs4_timeout(console_ctx_t *ctx, cmd_signature_t *reg) {
  (void)ctx;

  static struct {
    struct arg_int *timeout;
    struct arg_end *end;
  } args;

  if (reg) {
    args.timeout = arg_int1(NULL, NULL, "<timeout>", EXPENDABLE_STRING("SRS-4 CSP timeout (default " STR(SRS4_DEFAULT_TIMEOUT)" ms)"));
    args.end = arg_end(1);

    reg->argtable = &args;
    reg->help = EXPENDABLE_STRING("Set SRS-4 CSP timeout");
    return(0);
  }

  srs4_timeout = (uint8_t)(args.timeout->ival[0]);
  console_printf("SRS-4 CSP timeout was set to %d ms\n", srs4_timeout);

  return(CONSOLE_OK);
}

int cmd_srs4_gwdt_reset(console_ctx_t *ctx, cmd_signature_t *reg) {
  EMPTY_CMD_SETUP("Reset ground watchdog");

  int ret;

	ret = sl_srs4_gwdt_reset(srs4_node, srs4_timeout, SL_SRS4_GWDT_RESET_KEY);
	if (ret < 0) {
		console_printf("Failed to reset GWDT: %d\n", ret);
		return CONSOLE_ERR_IO;
	}

	return 0;
}

int cmd_srs4_gwdt_counter(console_ctx_t *ctx, cmd_signature_t *reg) {
  EMPTY_CMD_SETUP("Read ground watchdog counter");

  int ret;
	uint32_t counter;

	ret = sl_srs4_gwdt_counter(srs4_node, srs4_timeout, &counter);
	if (ret < 0) {
		console_printf("Failed to read GWDT: %d\n", ret);
		return CONSOLE_ERR_IO;
	}

	console_printf("%"PRIu32" seconds remaining\n", counter);

	return 0;
}

int cmd_srs4_run(console_ctx_t *ctx, cmd_signature_t *reg) {
  (void)ctx;

  static struct {
    struct arg_str *command;
    struct arg_end *end;
  } args;

  if (reg) {
    args.command = arg_str1(NULL, NULL, "<command>", EXPENDABLE_STRING("Command to be executed"));
    args.end = arg_end(1);

    reg->argtable = &args;
    reg->help = EXPENDABLE_STRING("Run command on remote system");
    return(0);
  }

  int ret;

	ret = sl_srs4_shell_run(srs4_node, srs4_timeout, (char*)args.command->sval[0]);
	if (ret < 0) {
		console_printf("Failed to run command: %s\n", strerror(-ret));
		return CONSOLE_ERR_IO;
	}

	return 0;
}

int cmd_srs4_tm(console_ctx_t *ctx, cmd_signature_t *reg) {
  EMPTY_CMD_SETUP("Show telemetry data");

  int ret;
	struct sl_prop_query query;
	uint8_t txbuf[64], rxbuf[256];
	size_t chunksize = 200;

	uint32_t bootcount, gwdt_counter;
	uint16_t mv, mw;
	int16_t ma, temp;
	float pwr, err, ebn0;
	uint32_t frames;
	char gwdt_string[20];
	bool carrier_lock, frame_lock;

	ret = sl_prop_remote_query_create_static(&query, txbuf, sizeof(txbuf), rxbuf, sizeof(rxbuf));
	if (ret < 0) {
		console_printf("Failed to create query\n");
    return CONSOLE_ERR_NO_MEM;
	}

	/* Set chunksize */
	query.chunksize = chunksize;

	/* Fetch entire TM group */
	ret += sl_prop_remote_query_get(&query, SL_SRS4_PROP_TM_GROUP);
	ret += sl_prop_remote_query_get(&query, SL_SRS4_PROP_SYS_BOOTCOUNT);
	ret += sl_prop_remote_query_get(&query, SL_SRS4_PROP_SYS_GWDT_COUNTER);
	ret += sl_prop_remote_query_get(&query, SL_SRS4_PROP_TX_FRAMES);
	ret += sl_prop_remote_query_get(&query, SL_SRS4_PROP_TX_PWR_FWD);
	ret += sl_prop_remote_query_get(&query, SL_SRS4_PROP_RX_FRAMES);
	ret += sl_prop_remote_query_get(&query, SL_SRS4_PROP_RX_RSSI);
	ret += sl_prop_remote_query_get(&query, SL_SRS4_PROP_RX_EBN0);
	ret += sl_prop_remote_query_get(&query, SL_SRS4_PROP_RX_FREQERR);
	ret += sl_prop_remote_query_get(&query, SL_SRS4_PROP_RX_SYMERR);
	ret += sl_prop_remote_query_get(&query, SL_SRS4_PROP_RX_CARRIER_LOCK);
	ret += sl_prop_remote_query_get(&query, SL_SRS4_PROP_RX_FRAME_LOCK);

	/* Check for errors */
	if (ret < 0) {
		console_printf("Failed to add properties to query\n");
    return CONSOLE_ERR_NO_MEM;
	}

	/* Send query message */
	ret = sl_prop_remote_query_send(&query, srs4_node, SL_PROP_DEFAULT_PORT, srs4_timeout);
	if (ret < 0) {
		console_printf("Failed to send query\n");
    return CONSOLE_ERR_NO_MEM;
	}

	/* System info */
	console_color_printf(COLOR_BOLD, "System Info:\n");

	console_printf("%-15s", "Bootcount");
	sl_prop_remote_query_get_reply(&query, SL_SRS4_PROP_SYS_BOOTCOUNT, &bootcount, sizeof(bootcount));
	console_color_printf(COLOR_GREEN, " %7"PRIu32, bootcount);
	console_printf(" boots\n");
	console_printf("%-15s", "GWDT counter");
	sl_prop_remote_query_get_reply(&query, SL_SRS4_PROP_SYS_GWDT_COUNTER, &gwdt_counter, sizeof(gwdt_counter));
	sec_to_time_string(gwdt_counter, gwdt_string, sizeof(gwdt_string));
	console_color_printf(COLOR_GREEN, "%s", gwdt_string);
	console_printf(" (%"PRIu32" seconds)\n", gwdt_counter);

	/* Power consumption */
	console_color_printf(COLOR_BOLD, "\nPower Rails:\n");

	console_printf("%-13s", "VIN");
	sl_prop_remote_query_get_reply(&query, SL_SRS4_PROP_TM_VOLT_VIN, &mv, sizeof(mv));
	console_color_printf(COLOR_GREEN, " %9.2f", (float)mv);
	console_printf(" mV ");
	sl_prop_remote_query_get_reply(&query, SL_SRS4_PROP_TM_CUR_VIN, &ma, sizeof(ma));
	console_color_printf(COLOR_GREEN, " %8.2f", (float)ma);
	console_printf(" mA ");
	sl_prop_remote_query_get_reply(&query, SL_SRS4_PROP_TM_POWER_VIN, &mw, sizeof(mw));
	console_color_printf(COLOR_GREEN, " %8.2f", (float)mw);
	console_printf(" mW\n");

	console_printf("%-13s", "3V3");
	sl_prop_remote_query_get_reply(&query, SL_SRS4_PROP_TM_VOLT_3V3, &mv, sizeof(mv));
	console_color_printf(COLOR_GREEN, " %9.2f", (float)mv);
	console_printf(" mV ");
	sl_prop_remote_query_get_reply(&query, SL_SRS4_PROP_TM_CUR_3V3, &ma, sizeof(ma));
	console_color_printf(COLOR_GREEN, " %8.2f", (float)ma);
	console_printf(" mA ");
	sl_prop_remote_query_get_reply(&query, SL_SRS4_PROP_TM_POWER_3V3, &mw, sizeof(mw));
	console_color_printf(COLOR_GREEN, " %8.2f", (float)mw);
	console_printf(" mW\n");

	console_printf("%-13s", "1V8");
	sl_prop_remote_query_get_reply(&query, SL_SRS4_PROP_TM_VOLT_1V8, &mv, sizeof(mv));
	console_color_printf(COLOR_GREEN, " %9.2f", (float)mv);
	console_printf(" mV ");
	sl_prop_remote_query_get_reply(&query, SL_SRS4_PROP_TM_CUR_1V8, &ma, sizeof(ma));
	console_color_printf(COLOR_GREEN, " %8.2f", (float)ma);
	console_printf(" mA ");
	sl_prop_remote_query_get_reply(&query, SL_SRS4_PROP_TM_POWER_1V8, &mw, sizeof(mw));
	console_color_printf(COLOR_GREEN, " %8.2f", (float)mw);
	console_printf(" mW\n");

	console_printf("%-13s", "1V0");
	sl_prop_remote_query_get_reply(&query, SL_SRS4_PROP_TM_VOLT_1V0, &mv, sizeof(mv));
	console_color_printf(COLOR_GREEN, " %9.2f", (float)mv);
	console_printf(" mV ");
	sl_prop_remote_query_get_reply(&query, SL_SRS4_PROP_TM_CUR_1V0, &ma, sizeof(ma));
	console_color_printf(COLOR_GREEN, " %8.2f", (float)ma);
	console_printf(" mA ");
	sl_prop_remote_query_get_reply(&query, SL_SRS4_PROP_TM_POWER_1V0, &mw, sizeof(mw));
	console_color_printf(COLOR_GREEN, " %8.2f", (float)mw);
	console_printf(" mW\n");

	console_printf("%-13s", "PA");
	sl_prop_remote_query_get_reply(&query, SL_SRS4_PROP_TM_VOLT_PA, &mv, sizeof(mv));
	console_color_printf(COLOR_GREEN, " %9.2f", (float)mv);
	console_printf(" mV ");
	sl_prop_remote_query_get_reply(&query, SL_SRS4_PROP_TM_CUR_PA, &ma, sizeof(ma));
	console_color_printf(COLOR_GREEN, " %8.2f", (float)ma);
	console_printf(" mA ");
	sl_prop_remote_query_get_reply(&query, SL_SRS4_PROP_TM_POWER_PA, &mw, sizeof(mw));
	console_color_printf(COLOR_GREEN, " %8.2f", (float)mw);
	console_printf(" mW\n");

	/* Temperatures */
	console_color_printf(COLOR_BOLD, "\nTemperature Sensors:\n");

	sl_prop_remote_query_get_reply(&query, SL_SRS4_PROP_TM_TEMP_PSU, &temp, sizeof(temp));
	console_printf("%-15s", "PSU");
	console_color_printf(COLOR_GREEN, " %7.2f", temp/100.0);
	console_printf(" C\n");

	sl_prop_remote_query_get_reply(&query, SL_SRS4_PROP_TM_TEMP_MCU, &temp, sizeof(temp));
	console_printf("%-15s", "MCU");
	console_color_printf(COLOR_GREEN, " %7.2f", temp/100.0);
	console_printf(" C\n");

	sl_prop_remote_query_get_reply(&query, SL_SRS4_PROP_TM_TEMP_FPGA, &temp, sizeof(temp));
	console_printf("%-15s", "FPGA");
	console_color_printf(COLOR_GREEN, " %7.2f", temp/100.0);
	console_printf(" C\n");

	sl_prop_remote_query_get_reply(&query, SL_SRS4_PROP_TM_TEMP_XCVR, &temp, sizeof(temp));
	console_printf("%-15s", "XCVR");
	console_color_printf(COLOR_GREEN, " %7.2f", temp/100.0);
	console_printf(" C\n");

	sl_prop_remote_query_get_reply(&query, SL_SRS4_PROP_TM_TEMP_LNA, &temp, sizeof(temp));
	console_printf("%-15s", "LNA");
	console_color_printf(COLOR_GREEN, " %7.2f", temp/100.0);
	console_printf(" C\n");

	sl_prop_remote_query_get_reply(&query, SL_SRS4_PROP_TM_TEMP_PA, &temp, sizeof(temp));
	console_printf("%-15s", "PA");
	console_color_printf(COLOR_GREEN, " %7.2f", temp/100.0);
	console_printf(" C\n");

	/* Transmit */
	console_color_printf(COLOR_BOLD, "\nTransmitter:\n");

	sl_prop_remote_query_get_reply(&query, SL_SRS4_PROP_TX_FRAMES, &frames, sizeof(frames));
	console_printf("%-15s", "Frames");
	console_color_printf(COLOR_GREEN, " %7"PRIu32, frames);
	console_printf(" frames\n");

	sl_prop_remote_query_get_reply(&query, SL_SRS4_PROP_TX_PWR_FWD, &pwr, sizeof(pwr));
	console_printf("%-15s", "Forward power");
	console_color_printf(COLOR_GREEN, " %7.2f", pwr);
	console_printf(" dBm\n");

	/* Receive */
	console_color_printf(COLOR_BOLD, "\nReceiver:\n");

	sl_prop_remote_query_get_reply(&query, SL_SRS4_PROP_RX_FRAMES, &frames, sizeof(frames));
	console_printf("%-15s", "Frames");
	console_color_printf(COLOR_GREEN, " %7"PRIu32, frames);
	console_printf(" frames\n");

	sl_prop_remote_query_get_reply(&query, SL_SRS4_PROP_RX_RSSI, &pwr, sizeof(pwr));
	console_printf("%-15s", "RSSI");
	console_color_printf(COLOR_GREEN, " %7.2f", pwr);
	console_printf(" dBm\n");

	sl_prop_remote_query_get_reply(&query, SL_SRS4_PROP_RX_EBN0, &ebn0, sizeof(ebn0));
	console_printf("%-15s", "Eb/N0");
	console_color_printf(COLOR_GREEN, " %7.2f", ebn0);
	console_printf(" dB\n");

	sl_prop_remote_query_get_reply(&query, SL_SRS4_PROP_RX_FREQERR, &err, sizeof(err));
	console_printf("%-14s", "Freq error");
	console_color_printf(COLOR_GREEN, " %8.2f", err);
	console_printf(" Hz\n");

	sl_prop_remote_query_get_reply(&query, SL_SRS4_PROP_RX_SYMERR, &err, sizeof(err));
	console_printf("%-15s", "Rate error");
	console_color_printf(COLOR_GREEN, " %7.2f", err);
	console_printf(" ppm\n");

	sl_prop_remote_query_get_reply(&query, SL_SRS4_PROP_RX_CARRIER_LOCK, &carrier_lock, sizeof(carrier_lock));
	console_printf("%-15s", "Carrier sync");
	if (carrier_lock)
		console_color_printf(COLOR_GREEN, "%9s", "locked\n");
	else
		//console_color_printf(COLOR_DARK_GRAY, "%9s", "unlocked\n");
		console_color_printf(COLOR_RESET, "%9s", "unlocked\n");

	sl_prop_remote_query_get_reply(&query, SL_SRS4_PROP_RX_FRAME_LOCK, &frame_lock, sizeof(frame_lock));
	console_printf("%-15s", "Frame sync");
	if (frame_lock)
		console_color_printf(COLOR_GREEN, "%9s", "locked\n");
	else
		//console_color_printf(COLOR_DARK_GRAY, "%9s", "unlocked\n");
		console_color_printf(COLOR_RESET, "%9s", "unlocked\n");

	sl_prop_remote_query_destroy(&query);

	return 0;
}

int cmd_srs4_boot_flash(console_ctx_t *ctx, cmd_signature_t *reg) {
  (void)ctx;

  static struct {
    struct arg_str *filename;
    struct arg_end *end;
  } args;

  if (reg) {
    args.filename = arg_str1(NULL, NULL, "<filename>", EXPENDABLE_STRING("Filename to flash"));
    args.end = arg_end(1);

    reg->argtable = &args;
    reg->help = EXPENDABLE_STRING("Flash image from NOR-flash to alternate partition");
    return(0);
  }

  int ret;
	const char *filename;
	uint32_t crc;

	filename = args.filename->sval[0];

	console_printf("Flashing \"%s\" to alternate boot partition\n", filename);

	ret = sl_srs4_bootloader_flash_alternate(srs4_node, srs4_timeout, filename, &crc);
	if (ret < 0) {
		console_printf("Failed to flash image: %s\n", strerror(-ret));
		return CONSOLE_ERR_IO;
	}

	console_printf("Flashed image, checksum = %08"PRIx32"\n", crc);

	return 0;
}

int cmd_srs4_boot_erase(console_ctx_t *ctx, cmd_signature_t *reg) {
  EMPTY_CMD_SETUP("Erase alternate partition");

  int ret;

	ret = sl_srs4_bootloader_erase_alternate(srs4_node, srs4_timeout);
	if (ret < 0) {
		console_printf("Failed to erase image: %s\n", strerror(-ret));
		return CONSOLE_ERR_IO;
	}

	console_printf("Success - alternate image was erased\n");

	return 0;;
}

int cmd_srs4_boot_checksum(console_ctx_t *ctx, cmd_signature_t *reg) {
  (void)ctx;

  static struct {
    struct arg_int *size;
    struct arg_end *end;
  } args;

  if (reg) {
    args.size = arg_int1(NULL, NULL, "<size>", EXPENDABLE_STRING("Size to checksum"));
    args.end = arg_end(1);

    reg->argtable = &args;
    reg->help = EXPENDABLE_STRING("Calculate checksum of alternate partition");
    return(0);
  }

  int ret;
	uint32_t size, crc;

	size = (uint32_t)args.size->ival[0];

	ret = sl_srs4_bootloader_checksum_alternate(srs4_node, srs4_timeout, size, &crc);
	if (ret < 0) {
		console_printf("Failed to checksum image: %s\n", strerror(-ret));
		return CONSOLE_ERR_IO;
	}

	console_printf("Image checksum = %08"PRIx32"\n", crc);

	return 0;
}

int cmd_srs4_boot_verify(console_ctx_t *ctx, cmd_signature_t *reg) {
  (void)ctx;

  static struct {
    struct arg_str *partition;
    struct arg_end *end;
  } args;

  if (reg) {
    args.partition = arg_str1(NULL, NULL, "<partition>", EXPENDABLE_STRING("Partition to verify, either \"primary\" or \"alternate\""));
    args.end = arg_end(1);

    reg->argtable = &args;
    reg->help = EXPENDABLE_STRING("Verify image in partition");
    return(0);
  }

  int ret;
	uint8_t partition = 1;
	uint32_t size, checksum;

  if (!strcmp(args.partition->sval[0], "primary")) {
    partition = 0;
  } else if (!strcmp(args.partition->sval[0], "alternate")) {
    partition = 1;
  } else {
    return CONSOLE_ERR_INVALID_ARG;
  }

	ret = sl_srs4_bootloader_verify(srs4_node, srs4_timeout, partition, &size, &checksum);
	if (ret < 0) {
		console_printf("Failed to verify image: %s\n", strerror(-ret));
		return CONSOLE_ERR_IO;
	}

	console_printf("%s image is %s! (size %u, checksum %08X)\n",
	       partition == 0 ? "Primary" : "Alternate",
	       ret == 0 ? "OK" : "corrupt",
	       size, checksum);

	return ret == 0 ? 0 : CONSOLE_ERR_IO;
}

int cmd_srs4_boot_set(console_ctx_t *ctx, cmd_signature_t *reg) {
  (void)ctx;

  static struct {
    struct arg_int *boots;
    struct arg_end *end;
  } args;

  if (reg) {
    args.boots = arg_int1(NULL, NULL, "<boots>", EXPENDABLE_STRING("Number of boots, maximum of 255"));
    args.end = arg_end(1);

    reg->argtable = &args;
    reg->help = EXPENDABLE_STRING("Set number of boots on alternate partition");
    return(0);
  }

  int ret;
	uint8_t boots = (uint8_t)args.boots->ival[0];

  if(boots > 255) {
    console_printf("Maximum number of boots is 255\n");
    return CONSOLE_ERR_INVALID_ARG;
  }

	printf("Setting alternate firmware boot for %hhu boots\n", boots);

	ret = sl_srs4_bootloader_set_alternate(srs4_node, srs4_timeout, boots);
	if (ret < 0) {
		console_printf("Failed to set boot image: %s\n", strerror(-ret));
		return CONSOLE_ERR_IO;
	}

	console_printf("Success - Reboot to boot alternate image\n");

	return 0;
}
