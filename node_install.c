#include "script.h"
#include "env.h"


//install { ${INSTALL_PKGCONFIG_DIR} libiowow.pc }
//install { ${IW_PUBLIC_HEADER_DESTINATION} ${PUB_HDRS} }

int node_install_setup(struct node *n) {
  if (!g_env.install.enabled) {
    return 0;
  }

  return 0;
}
