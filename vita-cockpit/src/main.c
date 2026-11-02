#include <vita2d.h>

#include "app/app.h"

/*
 * Composition root. It constructs the one thing that has a lifetime -- the
 * renderer -- and hands control to the loop. Nothing else constructs.
 *
 * The pool size is explicit rather than vita2d's default. Every vertex a frame
 * submits is allocated from it and must stay valid until the scene is handed to
 * GXM, so an undersized pool is not a cosmetic problem: vita2d does not check
 * the allocation, and a null pool block faults the GPU. The host preview
 * measures the worst-case frame well under 256 KiB; 2 MiB leaves room for more
 * panels and keeps the failure mode impossible rather than unlikely.
 */

#define RENDER_POOL_BYTES (2u * 1024u * 1024u)

int main(void)
{
    if (vita2d_init_advanced(RENDER_POOL_BYTES) < 0)
        return 1;

    int rc = app_run();

    /* Nothing left to degrade to at this point; the process is ending. */
    (void)vita2d_fini();
    return rc;
}
