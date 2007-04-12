#include "config.h"

#include <stdio.h>
#include <stdlib.h>

#define INFO(str) \
  { printf ("----  %s ...\n", (str)); }

#define WARN(str) \
  { printf ("%s:%d: warning: %s\n", __FILE__, __LINE__, (str)); }

#define FAIL(str) \
  { printf ("%s:%d: %s\n", __FILE__, __LINE__, (str)); exit(1); }

#undef MIN
#define MIN(a,b) ((a)<(b)?(a):(b))
