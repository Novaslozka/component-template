#include "srs4/cmd_cli.h"
#include "srs4/cmd_cli_btp.h"
#include "srs4/cmd_cli_prop.h"
#include "srs4/cmd_cli_srs4.h"

#include <string.h>

// the interface for shared libraries
#if defined(SHARED_BUILD)
void register_cli_cmds(void) {
  register_cli_srs4_btp_cmds();
  register_cli_srs4_prop_cmds();
  register_cli_srs4_cmds();
}

void get_version(char* semver, char* gitrev) {
  if(semver) {
    // at most 15 characters and NULL
    strncpy(semver, VERSION, 16);
  }
  if(gitrev) {
    // at most 13 characters and NULL
    strncpy(gitrev, GITREV, 14);
  }
}
#endif
