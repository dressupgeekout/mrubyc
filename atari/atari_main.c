#include <stdio.h>
#include <stdlib.h>

#include "mrubyc.h"

/* XXX 512K maximum */
#define MEMORY_SIZE (1024*512)

static uint8_t MEMORY_POOL[MEMORY_SIZE];

int
main(int argc, char *argv[])
{
	mrbc_init(MEMORY_POOL, MEMORY_SIZE);

#if 0
	uint8_t *mrbbuf = load_mrb_file(argv[1]);
	if (!mrbbuf) {
		return EXIT_FAILURE;
	}
#endif

#if 0
	if (!mrb_create_task(mrbbuf, NULL)) {
		return EXIT_FAILURE;
	}
#endif

	int ret = mrbc_run();

	return (ret == 1) ? EXIT_SUCCESS : EXIT_FAILURE;
}
