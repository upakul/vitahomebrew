#include <psp2kern/types.h>
#include <psp2kern/kernel/modulemgr.h>
#include <psp2kern/kernel/sysmem/data_transfers.h>
#include <taihen.h>

/*
 * _sceIoOpen in SceIofilemgr: the user-facing open export.
 * NID from the VitaSDK 3.60 NID database:
 *   SceIofilemgr / _sceIoOpen = 0xCC67B6FD
 *
 * This is the same NID used in taiHEN's own USAGE.md example for a
 * user-pointer file-open hook.
 */
#define SCE_IO_OPEN_NID 0xCC67B6FD

static tai_hook_ref_t open_hook;
static SceUID open_hook_uid = -1;
static volatile int open_count;
static char last_path[256];

static SceUID hook_sceIoOpen(const char *path, int flags, SceMode mode,
                             void *args)
{
    /*
     * path is a user pointer here. Never dereference it directly in kernel
     * mode: copy it into a bounded kernel buffer first.
     */
    if (path) {
        ksceKernelStrncpyUserToKernel(last_path, path,
                                      sizeof(last_path));
        last_path[sizeof(last_path) - 1] = '\0';
    }

    SceUID fd = TAI_CONTINUE(SceUID, open_hook, path, flags, mode, args);

    if (fd >= 0)
        open_count++;

    return fd;
}

int module_start(SceSize args, void *argp)
{
    (void)args;
    (void)argp;

    open_hook_uid = taiHookFunctionExportForKernel(
        KERNEL_PID,
        &open_hook,
        "SceIofilemgr",
        TAI_ANY_LIBRARY,
        SCE_IO_OPEN_NID,
        hook_sceIoOpen);

    if (open_hook_uid < 0)
        return SCE_KERNEL_START_FAILED;

    return SCE_KERNEL_START_SUCCESS;
}

int module_stop(SceSize args, void *argp)
{
    (void)args;
    (void)argp;

    if (open_hook_uid >= 0) {
        taiHookReleaseForKernel(open_hook_uid, open_hook);
        open_hook_uid = -1;
    }

    return SCE_KERNEL_STOP_SUCCESS;
}

int kdemo_get_open_count(void)
{
    return open_count;
}

int kdemo_get_last_path(char *buf, int size)
{
    if (!buf || size <= 0)
        return -1;

    return ksceKernelStrncpyKernelToUser(buf, last_path, size);
}
