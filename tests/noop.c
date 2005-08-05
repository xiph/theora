#include <theora/theora.h>

#include "tests.h"

static int
noop_test_encode ()
{
  theora_info ti;
  theora_state th;

  INFO ("+ Initializing theora_info struct");
  theora_info_init (&ti);

  INFO ("+ Initializing theora_state for encoding");
  if (theora_encode_init (&th, &ti) != OC_DISABLED) {
    INFO ("+ Clearing theora_state");
    theora_clear (&th);
  }

  INFO ("+ Clearing theora_info struct");
  theora_info_clear (&ti);

  return 0;
}

static int
noop_test_decode ()
{
  theora_info ti;
  theora_state th;

  INFO ("+ Initializing theora_info struct");
  theora_info_init (&ti);

  INFO ("+ Initializing theora_state for decoding");
  theora_decode_init (&th, &ti);

  INFO ("+ Clearing theora_state");
  theora_clear (&th);

  INFO ("+ Clearing theora_info struct");
  theora_info_clear (&ti);

  return 0;
}

static int
noop_test_comments ()
{
  theora_comment tc;

  theora_comment_init (&tc);
  theora_comment_clear (&tc);

  return 0;
}

int main(int argc, char *argv[])
{
  /*noop_test_decode ();*/

  noop_test_encode ();

  noop_test_comments ();

  exit (0);
}
