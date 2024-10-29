

#include <stdio.h>
#include <stdlib.h>

#include <dbus/dbus.h>
#include <systemd/sd-journal.h>

static int64_t notify_send(DBusConnection *db, const char *summary,
                           const char *body, int urgency) {
  DBusMessage *msg = dbus_message_new_method_call(
      "org.freedesktop.Notifications", "/org/freedesktop/Notifications",
      "org.freedesktop.Notifications", "Notify");

  DBusMessageIter args;
  dbus_message_iter_init_append(msg, &args);
  {
    char *app_name = "my_app";
    dbus_message_iter_append_basic(&args, DBUS_TYPE_STRING, &app_name);
  }
  {
    dbus_uint32_t id = 0;
    dbus_message_iter_append_basic(&args, DBUS_TYPE_UINT32, &id);
  }
  {
    char *icon = "dialog-information";
    dbus_message_iter_append_basic(&args, DBUS_TYPE_STRING, &icon);
  }
  { dbus_message_iter_append_basic(&args, DBUS_TYPE_STRING, &summary); }
  { dbus_message_iter_append_basic(&args, DBUS_TYPE_STRING, &body); }
  {
    DBusMessageIter actions;
    dbus_message_iter_open_container(&args, DBUS_TYPE_ARRAY,
                                     DBUS_TYPE_STRING_AS_STRING, &actions);
    dbus_message_iter_close_container(&args, &actions);
  }
  {
    DBusMessageIter hints;
    dbus_message_iter_open_container(
        &args, DBUS_TYPE_ARRAY,
        DBUS_DICT_ENTRY_BEGIN_CHAR_AS_STRING DBUS_TYPE_STRING_AS_STRING
            DBUS_TYPE_VARIANT_AS_STRING DBUS_DICT_ENTRY_END_CHAR_AS_STRING,
        &hints);
    {
      DBusMessageIter elem;
      dbus_message_iter_open_container(&hints, DBUS_TYPE_DICT_ENTRY, NULL,
                                       &elem);
      {
        char *key = "urgency";
        dbus_message_iter_append_basic(&elem, DBUS_TYPE_STRING, &key);
        DBusMessageIter value;
        dbus_message_iter_open_container(&elem, DBUS_TYPE_VARIANT,
                                         DBUS_TYPE_BYTE_AS_STRING, &value);
        dbus_message_iter_append_basic(&value, DBUS_TYPE_BYTE, &urgency);
        dbus_message_iter_close_container(&elem, &value);
      }
      dbus_message_iter_close_container(&hints, &elem);
    }
    {
      DBusMessageIter elem;
      dbus_message_iter_open_container(&hints, DBUS_TYPE_DICT_ENTRY, NULL,
                                       &elem);
      {
        char *key = "transient";
        dbus_message_iter_append_basic(&elem, DBUS_TYPE_STRING, &key);
        DBusMessageIter value;
        dbus_message_iter_open_container(&elem, DBUS_TYPE_VARIANT,
                                         DBUS_TYPE_BOOLEAN_AS_STRING, &value);
        dbus_bool_t transient = TRUE;
        dbus_message_iter_append_basic(&value, DBUS_TYPE_BOOLEAN, &transient);
        dbus_message_iter_close_container(&elem, &value);
        dbus_message_iter_close_container(&hints, &elem);
      }
    }
    dbus_message_iter_close_container(&args, &hints);
  }
  {
    dbus_int32_t timeout = -1;
    dbus_message_iter_append_basic(&args, DBUS_TYPE_INT32, &timeout);
  }

  DBusError err;
  dbus_error_init(&err);

  DBusMessage *res =
      dbus_connection_send_with_reply_and_block(db, msg, 1000, &err);

  dbus_message_unref(msg);
  if (dbus_error_is_set(&err)) {
    printf("Error: %s\n", err.message);
    dbus_error_free(&err);
    return -1;
  }
  dbus_uint32_t id = 0;
  {
    DBusMessageIter ret_args;
    dbus_message_iter_init(res, &args);
    {
      dbus_message_iter_get_basic(&args, &id);
      printf("Notification ID: %d\n", id);
    }
  }
  dbus_message_unref(res);
  return id;
}

static int urgency_map(int priority) {
  if (priority < 4) {
    return 2;
  }
  if (priority < 6) {
    return 1;
  }
  return 0;
}

int main() {
  sd_journal *j = NULL;
  int r = sd_journal_open(&j, SD_JOURNAL_LOCAL_ONLY);
  if (r < 0) {
    (void)fprintf(stderr, "Failed to open journal: %s\n", strerror(-r));
    return EXIT_FAILURE;
  }

  DBusError err;
  dbus_error_init(&err);
  DBusConnection *connection = dbus_bus_get(DBUS_BUS_SESSION, &err);

  if (dbus_error_is_set(&err)) {
    printf("Error: %s\n", err.message);
    dbus_error_free(&err);
    return -1;
  }

  sd_journal_seek_tail(j);
  sd_journal_previous(j);
  sd_journal_previous(j);
  sd_journal_previous(j);

  while (1) {
    while (sd_journal_next(j) > 0) {
      const void *d = NULL;
      size_t l = 0;

      // SD_JOURNAL_FOREACH_DATA(j, d, l)
      // printf("%.*s\n", (int)l, (const char *)d);

      int priority = 0;
      if (sd_journal_get_data(j, "PRIORITY", &d, &l) == 0 && l - 1 == 9) {
        priority = *((const char *)d + 9) - '0';
      }

      char id_buf[256] = {};
      if (sd_journal_get_data(j, "SYSLOG_IDENTIFIER", &d, &l) == 0) {
        size_t cpy_len = l - 18;
        if (cpy_len > sizeof(id_buf) - 1) {
          cpy_len = sizeof(id_buf) - 1;
        }
        memcpy(id_buf, d + 18, cpy_len);
      }

      if (sd_journal_get_data(j, "MESSAGE", &d, &l) == 0 && l > 8) {
        printf("%.*s\n", (int)l, (const char *)d);
        // TODO: markup escape:
        // like g_markup_escape_text
        // & becomes &amp;
        // < becomes &lt;
        // > becomes &gt;
        // ' becomes &apos;
        // " becomes &quot;
        notify_send(connection, id_buf, d + 8, urgency_map(priority));
      }
    }
    sd_journal_wait(j, (uint64_t)(100000));
  }
}
