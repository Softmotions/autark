#include "script.h"
#include "utils.h"
#include "config.h"

struct scriptx {
  struct node *root;
};



int script_from_file(const char *path, struct node **out) {
  struct value buf = utils_file_as_buf(path, CFG_SCRIPT_MAX_SIZE_BYTES);
  if (buf.error) {
    return value_destroy(&buf);
  }
  int ret = script_from_buf(&buf, out);
  value_destroy(&buf);
  return ret;
}

int script_from_buf(const struct value *buf, struct node **out) {
  //  TODO:
  return 0;
}
