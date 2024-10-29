

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
    dbus_message_iter_init(res, &ret_args);
    {
      dbus_message_iter_get_basic(&ret_args, &id);
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

static void markup_escape(char *str, size_t str_len, char *dst,
                          size_t dst_len) {
  struct escape_info {
    char *from;
    char *subst;
    size_t from_len;
    size_t subst_len;
  };

  struct escape_info *info = malloc(sizeof(struct escape_info) * 5);
  size_t info_size = 0;
  size_t info_capacity = 5;

  for (size_t i = 0; i < str_len; i++) {
    switch (str[i]) {
    case '&':
      info[info_size].from = &str[i];
      info[info_size].subst = "&amp;";
      info[info_size].from_len = 1;
      info[info_size].subst_len = 5;
      info_size++;
      break;
    case '<':
      info[info_size].from = &str[i];
      info[info_size].subst = "&lt;";
      info[info_size].from_len = 1;
      info[info_size].subst_len = 4;
      info_size++;
      break;
    case '>':
      info[info_size].from = &str[i];
      info[info_size].subst = "&gt;";
      info[info_size].from_len = 1;
      info[info_size].subst_len = 4;
      info_size++;
      break;
    case '\'':
      info[info_size].from = &str[i];
      info[info_size].subst = "&apos;";
      info[info_size].from_len = 1;
      info[info_size].subst_len = 6;
      info_size++;
      break;
    case '"':
      info[info_size].from = &str[i];
      info[info_size].subst = "&quot;";
      info[info_size].from_len = 1;
      info[info_size].subst_len = 6;
      info_size++;
      break;
    default:
      break; // TODO: ref g_markup_escape_text
    }

    if (info_size == info_capacity) {
      size_t new_capacity = info_capacity * 2;
      struct escape_info *new_info =
          realloc(info, sizeof(struct escape_info) * new_capacity);
      if (new_info == NULL) {
        // handle error
        break;
      }
      info = new_info;
      info_capacity = new_capacity;
    }
  } // for

  size_t new_len = str_len;
  for (size_t i = 0; i < info_size; i++) {
    new_len += info[i].subst_len - info[i].from_len;
  }

  // TODO: realloc dst if needed

  char *str_ptr = str;
  size_t dst_pos = 0;
  for (size_t i = 0; i < info_size; i++) {
    size_t copy_len = info[i].from - str_ptr;
    if (dst_pos + copy_len > dst_len) {
      copy_len = dst_len - dst_pos;
    }
    memcpy(dst + dst_pos, str_ptr, copy_len);
    str_ptr += copy_len;
    dst_pos += copy_len;
    if (dst_pos + info[i].subst_len > dst_len) {
      break;
    }
    memcpy(dst + dst_pos, info[i].subst, info[i].subst_len);
    str_ptr += info[i].from_len;
    dst_pos += info[i].subst_len;
  }
  // copy rest
  {
    size_t copy_len = str_len - (str_ptr - str);
    memcpy(dst + dst_pos, str_ptr, copy_len);
    dst_pos += copy_len;
  }
  dst[dst_pos] = '\0';
  free(info); // TODO: make info static
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

  char escape_buf[256] = {};

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
        markup_escape((char *)d + 8, l - 8, escape_buf, sizeof(escape_buf) - 1);
        notify_send(connection, id_buf, escape_buf, urgency_map(priority));
      }
    }
    sd_journal_wait(j, (uint64_t)(100000));
  }
}
