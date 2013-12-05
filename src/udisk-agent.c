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

/*
 * Daemon main loop handler
 */
static GMainLoop *loop = (GMainLoop *)NULL;

/*
 * Udisk client
 */
static UDisksClient *client = (UDisksClient *)NULL;

/*
 * Callback function to handle "object add" event.
 */
static void automount_on_object_removed(GDBusObjectManager  *manager, GDBusObject *object, gpointer user_data)
{
	UDisksObject *udisk_object = UDISKS_OBJECT(object);
	UDisksBlock *udisk_block_device = udisks_object_peek_block(udisk_object);
	UDisksFilesystem *udisk_filesystem = (UDisksFilesystem *)NULL;
	gchar **mount_points = (gchar **)NULL;
	GVariant *options = (GVariant *)NULL;
	gchar *deviceName = (gchar *)NULL;
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

	/*
	 * Filesystem object test
	 */
	udisk_filesystem = udisks_object_peek_filesystem(udisk_object);
	if (udisk_filesystem == (UDisksFilesystem *)NULL)
		return;

	/*
	 * Test whether block device is mounted.
	 */
	mount_points = udisks_filesystem_get_mount_points(udisk_filesystem);
	if (mount_points == (gchar **)NULL || g_strv_length ((gchar **)mount_points) == 0)
	{
		// Device already mounted
		g_print("Device is not mounted.", deviceName);
		return;
	}

	for (i = 0; mount_points[i] != (gchar *)NULL; i ++)
		g_print("Filesystem to be unmounted => %s\n", mount_points[i]);
	/*
	 * Mount parameters
	 */
	g_variant_builder_init(&builder, G_VARIANT_TYPE_VARDICT);

	/*
	 * User interaction
	 */
	g_variant_builder_add(&builder, "{sv}", "auth.no_user_interaction", g_variant_new_boolean(FALSE));

	options = g_variant_builder_end(&builder);
	g_variant_ref_sink(options);

	rc = udisks_filesystem_call_unmount_sync(udisk_filesystem, options, NULL, &error);
	if (rc)
	{
		g_print("Device %s mounted in %s\n", deviceName, mount_path);
		g_free(mount_path);
	}

	if (error != (GError *)NULL)
	{
		g_error_free(error);
		error = (GError *)NULL;
	}
}

static void automount_on_object_added(GDBusObjectManager  *manager, GDBusObject *object, gpointer user_data)
{
	UDisksObject *udisk_object = UDISKS_OBJECT(object);
	UDisksBlock *udisk_block_device = udisks_object_peek_block(udisk_object);
	UDisksFilesystem *udisk_filesystem = (UDisksFilesystem *)NULL;
	gchar **mount_points = (gchar **)NULL;
	GVariant *options = (GVariant *)NULL;
	gchar *deviceName = (gchar *)NULL;
	GError *error = (GError *)NULL;
	GVariantBuilder builder;
	gchar *mount_path;
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

	/*
	 * Test whether block device is mounted.
	 */
	mount_points = udisks_filesystem_get_mount_points(udisk_filesystem);
	if (mount_points != (gchar **)NULL && g_strv_length ((gchar **) mount_points) > 0)
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
	g_variant_builder_add(&builder, "{sv}", "auth.no_user_interaction", g_variant_new_boolean(FALSE));

	options = g_variant_builder_end(&builder);
	g_variant_ref_sink(options);

	cont = FALSE;

	if (error != (GError *)NULL)
	{
		g_error_free(error);
		error = (GError *)NULL;
	}

	rc = udisks_filesystem_call_mount_sync(udisk_filesystem, options, &mount_path, NULL, &error);
	if (rc)
	{
		g_print("Device %s mounted in %s\n", deviceName, mount_path);
		g_free(mount_path);
	}
	else
	{
		g_printerr( "Mount error, domain = %d, error code = %d, msg = %s\n", error -> domain, error -> code, error->message);
	}

	if (error != (GError *)NULL)
	{
		g_error_free(error);
		error = (GError *)NULL;
	}
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

	name_owner = g_dbus_object_manager_client_get_name_owner(G_DBUS_OBJECT_MANAGER_CLIENT(udisks_client_get_object_manager(client)));

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
	g_signal_connect(manager, "object-added", G_CALLBACK (automount_on_object_added), NULL);

	// Remove signal
	g_signal_connect(manager, "object-removed", G_CALLBACK (automount_on_object_removed), NULL);

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

	/*
	 * Glib init
	 */
	g_type_init();

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
		g_printerr("Error connecting to the udisks daemon: %s\n", error->message);

		/*
		 * Cleanup
		 */
		daemon_cleanup();
		g_error_free (error);

		return ret;
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
