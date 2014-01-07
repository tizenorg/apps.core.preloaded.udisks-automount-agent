#include <stdio.h>
#include <stdlib.h>

#include <pthread.h>

/*
 * Glib
 */
#include <gio/gunixfdlist.h>
#include <glib/gi18n.h>
#include <glib.h>

/*
 * Udisks includes
 */
#include <udisks/udisks.h>

/*
 * Locale includes
 */
#include <locale.h>

#include <notification.h>

/*
 * Daemon main loop handler
 */
static GMainLoop *loop = (GMainLoop *)NULL;

/*
 * Udisk client
 */
static UDisksClient *client = (UDisksClient *)NULL;


static void
send_notification (
        UDisksObjectInfo *udisk_object_info,
        const gchar *path,
        const gchar *op_type)
{

#if ENABLE_NOTIF
    gint noti_id = 0;
    notification_h noti = NULL;
    notification_error_e ret = NOTIFICATION_ERROR_NONE;
    bundle *content = NULL;
    bundle_raw *enc_content = NULL;
    int bundle_len = 0;
    const gchar *device_name = NULL;

    g_print("Sending notification for event (%s) at path %s\n",
            op_type, path?path:"path_unknown");

    /*
     * This is needed because currently all the events are stored in db by the
     * notification service, and even after those are delivered/broadcasted
     * events are not cleared.
     */
    if (notification_delete_all_by_type (NULL, NOTIFICATION_TYPE_NOTI)
            != NOTIFICATION_ERROR_NONE) {
        g_print("Fail to delete old notifications\n");
    }

    noti = notification_new (NOTIFICATION_TYPE_NOTI, NOTIFICATION_GROUP_ID_NONE,
            NOTIFICATION_PRIV_ID_NONE);
    if (noti == NULL) {
        g_print("Failed to create notification: %d\n", ret);
        goto _finished;
    }

    device_name = udisks_object_info_get_name (udisk_object_info);
    ret = notification_set_text(noti, NOTIFICATION_TEXT_TYPE_TITLE,
            device_name, NULL,
            NOTIFICATION_VARIABLE_TYPE_NONE);
    if (ret != NOTIFICATION_ERROR_NONE) {
        g_print("Fail to notification_set_text\n");
        goto _finished;
    }

    content = bundle_create ();
    bundle_add (content, "event-type", op_type);
    bundle_add (content, "device-name", device_name);
    bundle_add (content, "mount-path", path?path:"");
    bundle_add (content, "device-details",
            udisks_object_info_get_one_liner (udisk_object_info));

    bundle_encode (content, &enc_content, &bundle_len);
    ret = notification_set_text(noti, NOTIFICATION_TEXT_TYPE_CONTENT,
            (const char*) enc_content, NULL, NOTIFICATION_VARIABLE_TYPE_NONE);
    bundle_free_encoded_rawdata (&enc_content); enc_content = NULL;
    if (ret != NOTIFICATION_ERROR_NONE) {
        g_print("Fail to notification_set_text\n");
        goto _finished;
    }

    ret = notification_insert(noti, &noti_id);
    if (ret != NOTIFICATION_ERROR_NONE) {
        g_print("Fail to notification_insert\n");
        goto _finished;
    }

_finished:
    if (content) bundle_free(content);
    if (noti) notification_free (noti);
#endif

    g_object_unref (udisk_object_info);
}

/*
 * Callback function to handle "object add" event.
 */
static void automount_on_object_removed(GDBusObjectManager  *manager,
        GDBusObject *object, gpointer user_data)
{
	UDisksObject *udisk_object = UDISKS_OBJECT(object);
	UDisksBlock *udisk_block_device = udisks_object_peek_block(udisk_object);
	UDisksFilesystem *udisk_filesystem = (UDisksFilesystem *)NULL;
	const gchar ** mount_points = NULL;
	GVariant *options = (GVariant *)NULL;
	const gchar *deviceName = NULL;
	GError *error = (GError *)NULL;
	GVariantBuilder builder;
	gchar *mount_path;
	gboolean cont;
	gboolean rc;
	int i;

	/*
	 * Test block device
	 */
	if (udisk_block_device == (UDisksBlock *)NULL)
		return;

	deviceName = udisks_block_get_device(udisk_block_device);
	if (deviceName == (gchar *)NULL)
		return;

	g_print("Umount callback for device %s\n", deviceName);
	/*
	 * Filesystem object test
	 */
	udisk_filesystem = udisks_object_peek_filesystem(udisk_object);
	if (udisk_filesystem == (UDisksFilesystem *)NULL)
		return;

	/*
	 * Test whether block device is mounted.
	 */
	mount_points = (const gchar **)
	        udisks_filesystem_get_mount_points(udisk_filesystem);
	if (mount_points == NULL ||
	    g_strv_length ((gchar **)mount_points) == 0)
	{
		// Device already unmounted
		g_print("Device is not mounted.\n", deviceName);
		return;
	}

	for (i = 0; mount_points[i] != (gchar *)NULL; i ++) {
		g_print("Filesystem to be unmounted => %s\n", mount_points[i]);
	}
	/*
	 * Mount parameters
	 */
	g_variant_builder_init(&builder, G_VARIANT_TYPE_VARDICT);

	/*
	 * User interaction
	 */
	g_variant_builder_add(&builder, "{sv}", "auth.no_user_interaction",
	        g_variant_new_boolean(FALSE));

	options = g_variant_builder_end(&builder);
	g_variant_ref_sink(options);

	rc = udisks_filesystem_call_unmount_sync(udisk_filesystem, options,
	        NULL, &error);
	if (rc)
	{
		g_print("Device %s unmounted\n", deviceName);
	}

	if (error != (GError *)NULL)
	{
		g_error_free(error);
		error = (GError *)NULL;
	}

    send_notification (udisks_client_get_object_info (client, udisk_object),
            mount_points[0], "umount");

}

static void automount_on_object_added(GDBusObjectManager  *manager,
        GDBusObject *object, gpointer user_data)
{
	UDisksObject *udisk_object = UDISKS_OBJECT(object);
	UDisksBlock *udisk_block_device = udisks_object_peek_block(udisk_object);
	UDisksFilesystem *udisk_filesystem = (UDisksFilesystem *)NULL;
	const gchar **mount_points = NULL;
	GVariant *options = (GVariant *)NULL;
	const gchar *deviceName = NULL;
	GError *error = (GError *)NULL;
	GVariantBuilder builder;
	gchar *mount_path = NULL;
	gboolean cont;
	gboolean rc;

	/*
	 * Test block device
	 */
	if (udisk_block_device == (UDisksBlock *)NULL)
		return;

	deviceName = udisks_block_get_device(udisk_block_device);
	if (deviceName == (gchar *)NULL)
		return;

	/*
	 * Filesystem object test
	 */
	udisk_filesystem = udisks_object_peek_filesystem(udisk_object);
	if (udisk_filesystem == (UDisksFilesystem *)NULL)
		return;

	g_print("Mount callback for device %s\n", deviceName);
	/*
	 * Test whether block device is mounted.
	 */
	mount_points = (const gchar **)
	        udisks_filesystem_get_mount_points(udisk_filesystem);
	if (mount_points != NULL && g_strv_length ((gchar **) mount_points) > 0)
	{
		// Device already mounted
		g_print("[%s] - device already mounted.", deviceName);
		return;
	}

	/*
	 * Mount parameters
	 */
	g_variant_builder_init(&builder, G_VARIANT_TYPE_VARDICT);

	/*
	 * User interaction
	 */
	g_variant_builder_add(&builder, "{sv}", "auth.no_user_interaction",
	        g_variant_new_boolean(FALSE));

	options = g_variant_builder_end(&builder);
	g_variant_ref_sink(options);

	cont = FALSE;

	if (error != (GError *)NULL)
	{
		g_error_free(error);
		error = (GError *)NULL;
	}

	rc = udisks_filesystem_call_mount_sync(udisk_filesystem, options,
	        &mount_path, NULL, &error);
	if (rc)
	{
		g_print("Device %s mounted in %s\n", deviceName, mount_path);
	}
	else
	{
		g_printerr( "Mount error, domain = %d, error code = %d, msg = %s\n",
		        error -> domain, error -> code, error->message);
	}

	if (error != (GError *)NULL)
	{
		g_error_free(error);
		error = (GError *)NULL;
	}
    else
    {
        send_notification (udisks_client_get_object_info (client, udisk_object),
                mount_path, "mount");
    }
	g_free(mount_path);
}

static void print_timestamp(void)
{
	GTimeVal now;
	time_t now_time;
	struct tm *now_tm;
	gchar time_buf[128];

	g_get_current_time(&now);
	now_time = (time_t)now.tv_sec;
	now_tm = localtime(&now_time);
	strftime (time_buf, sizeof time_buf, "%H:%M:%S", now_tm);

	g_print ("%s.%03d ", time_buf, (gint)now.tv_usec/1000);
}

static void print_name_owner(void)
{
	gchar *name_owner = (gchar *)NULL;

	name_owner = g_dbus_object_manager_client_get_name_owner(
	        G_DBUS_OBJECT_MANAGER_CLIENT(udisks_client_get_object_manager(
	                client)));

	print_timestamp();

	if (name_owner != (gchar *)NULL)
		g_print("The udisks-daemon is running (name-owner %s).\n", name_owner);
	else
		g_print("The udisks-daemon is not running.\n");

	g_free(name_owner);
}

static gint handle_command_automount(void)
{
	GDBusObjectManager *manager;

	g_print ("Moniors udisks and mount block devices. Press Ctrl+C to exit.\n");

	// Get connection manager
	manager = udisks_client_get_object_manager(client);

	// Add signal
	g_signal_connect(manager, "object-added",
	        G_CALLBACK (automount_on_object_added), NULL);

	// Remove signal
	g_signal_connect(manager, "object-removed",
	        G_CALLBACK (automount_on_object_removed), NULL);

	// Print daemon info
	print_name_owner();

	// Main loop start
	g_main_loop_run(loop);

	return 0;
}

static void daemon_cleanup(void)
{
	if (loop != (GMainLoop *)NULL)
		g_main_loop_unref(loop);

	if (client != (UDisksClient *)NULL)
		g_object_unref (client);
}

int main(int argc, char **argv)
{
	GError *error = (GError *)NULL;
	gint ret = 1;
	gint rc;

#if !GLIB_CHECK_VERSION (2, 36, 0)
    g_type_init ();
#endif

	/*
	 * Set locale
	 */
	setlocale(LC_ALL, "");

	/*
	 * Main loop handler
	 */
	loop = g_main_loop_new(NULL, FALSE);

	/*
	 * Udisk cleint
	 */
	client = udisks_client_new_sync(NULL, &error);
	if (client == NULL)
	{
		g_printerr("Error connecting to the udisks daemon: %s\n",
		        error->message);

		/*
		 * Cleanup
		 */
		daemon_cleanup();
		g_error_free (error);

		return ret;
	}

    /*
     * clear old notifications
     */
    if (notification_delete_all_by_type (NULL, NOTIFICATION_TYPE_NOTI)
            != NOTIFICATION_ERROR_NONE) {
        g_print("Fail to delete old notifications\n");
    }

	/*
	 * Run daemon
	 */
	ret = handle_command_automount();

	/*
	 * Cleanup
	 */
	daemon_cleanup();

	/*
	 * Return
	 */
	return ret;
}
