/*
 * Privilege escalation through libjbc.
 *
 * initialize_jbc() elevates the process credentials once at startup so the
 * application can read /system_data and write under /data; terminate_jbc()
 * restores the original credentials at exit.
 */
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <orbis/libkernel.h>
#include <libjbc.h>

#include "util.h"
#include "notifi.h"

// Variables for (un)jailbreaking
jbc_cred g_Cred;
jbc_cred g_RootCreds;
static bool g_elevated = false;   // true only when jailbreak() changed the creds

// Verify jailbreak
static int is_jailbroken(void)
{
    FILE *s_FilePointer = fopen("/user/.jailbreak", "w");

    if (!s_FilePointer)
        return 0;

    fclose(s_FilePointer);
    remove("/user/.jailbreak");
    return 1;
}

// Jailbreaks creds
static int jailbreak(void)
{
    int rc;

    if (is_jailbroken())
    {
        LOG("jbc: /user already writable, skipping credential escalation");
        return 1;
    }

    rc = jbc_get_cred(&g_Cred);
    LOG("jbc: get_cred rc=%d uid=%u prison=%lx cdir=%lx rdir=%lx jdir=%lx", rc, g_Cred.uid,
        (unsigned long)g_Cred.prison, (unsigned long)g_Cred.cdir, (unsigned long)g_Cred.rdir, (unsigned long)g_Cred.jdir);
    g_RootCreds = g_Cred;
    rc = jbc_jailbreak_cred(&g_RootCreds);
    LOG("jbc: jailbreak_cred rc=%d prison0=%lx rootvnode=%lx", rc,
        (unsigned long)g_RootCreds.prison, (unsigned long)g_RootCreds.rdir);
    rc = jbc_set_cred(&g_RootCreds);
    LOG("jbc: set_cred rc=%d", rc);
    g_elevated = true;

    return (is_jailbroken());
}

// Restores the original creds, only if this process elevated them
static void unjailbreak(void)
{
    if (!g_elevated)
        return;

    jbc_set_cred(&g_Cred);
    g_elevated = false;
}

// Initialize jailbreak
int initialize_jbc(void)
{
    // Pop notification depending on jailbreak result
    if (!jailbreak())
    {
        LOG("Jailbreak failed!");
        notifi(NULL, "Jailbreak failed!");
        return 0;
    }

    LOG("Jailbreak succeeded!");
    return 1;
}

// Unload libjbc libraries
void terminate_jbc(void)
{
	unjailbreak();
    LOG("Jailbreak removed!");
}
