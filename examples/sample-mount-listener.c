#include <stdio.h>
#include <stdlib.h>

#include <pthread.h>

#include <glib.h>
#include <gio/gio.h>

typedef struct {
    GIOChannel *channel;
    unsigned watch;
    GList *paths;
} MountInfo;

static GMainLoop *loop = NULL;

static MountInfo mounts;

static GList *
parse_mounts (MountInfo *mnts)
{
    GList *paths = NULL;
    GIOStatus status;
    GError *error = NULL;
    status = g_io_channel_seek_position(mnts->channel, 0, G_SEEK_SET, &error);
    if (error) {
        g_print("Couldn't rewind mountinfo channel: %s\n", error->message);
        g_free(error);
        return NULL;
    }

    do {
        gchar *str = NULL;
        gsize len = 0;
        int mount_id, parent_id, major, minor;
        char root[1024], mountpoint[1024];

        status = g_io_channel_read_line(mnts->channel, &str, NULL, &len, &error);
        switch (status) {
        case G_IO_STATUS_AGAIN:
            continue;
        case G_IO_STATUS_NORMAL:
            str[len] = '\0';
            break;
        case G_IO_STATUS_ERROR:
            g_print("Couldn't read line of mountinfo: %s\n", error->message);
            g_free(error);
        case G_IO_STATUS_EOF:
            return g_list_sort(paths, (GCompareFunc)g_strcmp0);
        }

        if (sscanf(str, "%d %d %d:%d %1023s %1023s", &mount_id, &parent_id,
                &major, &minor, root, mountpoint) != 6)
            g_print("Error parsing mountinfo line: %s\n", str);
        else {
            char *mp = g_strcompress(mountpoint);
            //g_print("Got mountpoint: %s", mp);
            paths = g_list_prepend(paths, mp);
        }

        g_free(str);
    } while (TRUE);
}

static gboolean
find_in_list (GList *list, const gchar* data)
{
    return (g_list_find_custom (list, data, (GCompareFunc)g_strcmp0) != NULL);
}

static gboolean
on_mounts_changed(GIOChannel *source, GIOCondition cond, gpointer data)
{
    MountInfo *mnts = data;
    GList *oldpaths = mnts->paths;
    GList *newpaths = parse_mounts (mnts);
    GList *o, *n, *tmp;
    gboolean found = FALSE;

    for (n = newpaths; n != NULL;) {
        if (!find_in_list (oldpaths, n->data)) {
            g_print ("Device mounted at path %s\n", (const gchar *)n->data);
        }
        n = n->next;
    }

    for (o = oldpaths; o != NULL;) {
        if (!find_in_list (newpaths, o->data)) {
            g_print ("Device unmounted at path %s\n", (const gchar *)o->data);
        }
        o = o->next;
    }

    mnts->paths = newpaths;
    g_list_free(oldpaths);

    return TRUE;
}

static gint
monitor_mounts(void)
{
    GError *error = NULL;

    g_print ("Monitors mount info changes. Press Ctrl+C to exit.\n");

    mounts.channel = g_io_channel_new_file("/proc/self/mountinfo", "r", &error);
    if (error) {
        g_print("No /proc/self/mountinfo file: %s. Exiting.\n", error->message);
        g_error_free(error);
    } else {
        mounts.paths = parse_mounts (&mounts);
        mounts.watch = g_io_add_watch(mounts.channel, G_IO_ERR,
                on_mounts_changed, &mounts);
        g_main_loop_run(loop);
    }

	return 0;
}

static void
daemon_cleanup(void)
{
    if (mounts.watch)
        g_source_remove(mounts.watch);

    if (mounts.channel)
        g_io_channel_unref(mounts.channel);

    g_list_free_full(mounts.paths, g_free);

    if (loop)
		g_main_loop_unref(loop);
}

int
main(int argc, char **argv)
{
	GError *error = (GError *)NULL;
	gint ret = 1;
	gint rc;

#if !GLIB_CHECK_VERSION (2, 36, 0)
    g_type_init ();
#endif

	loop = g_main_loop_new(NULL, FALSE);

	ret = monitor_mounts ();

	daemon_cleanup();

	return ret;
}
