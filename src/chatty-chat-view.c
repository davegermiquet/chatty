/* -*- mode: c; c-basic-offset: 2; indent-tabs-mode: nil; -*- */
/* chatty-chat-view.c
 *
 * Copyright 2020 Purism SPC
 *
 * Author(s):
 *   Mohammed Sadiq <sadiq@sadiqpk.org>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include <glib/gi18n.h>

#include "chatty-avatar.h"
#include "chatty-chat.h"
#include "chatty-history.h"
#include "chatty-icons.h"
#include "chatty-manager.h"
#include "chatty-utils.h"
#include "users/chatty-contact.h"
#include "users/chatty-pp-buddy.h"
#include "chatty-message-row.h"
#include "chatty-chat-view.h"

struct _ChattyChatView
{
  GtkBox      parent_instance;

  GtkWidget  *message_list;
  GtkWidget  *typing_revealer;
  GtkWidget  *typing_indicator;
  GtkWidget  *chatty_message_list;
  GtkWidget  *input_frame;
  GtkWidget  *scrolled_window;
  GtkWidget  *message_input;
  GtkWidget  *send_file_button;
  GtkWidget  *encrypt_icon;
  GtkWidget  *send_message_button;
  GtkWidget  *empty_view;
  GtkWidget  *empty_label0;
  GtkWidget  *empty_label1;
  GtkWidget  *empty_label2;
  GtkTextBuffer *message_input_buffer;
  GtkAdjustment *vadjustment;

  ChattyChat *chat;
  ChattyConversation *chatty_conv;
  guint       message_type;
  guint       refresh_typing_id;
  gboolean    first_scroll_to_bottom;
};

static GHashTable *ht_sms_id = NULL;
static GHashTable *ht_emoticon = NULL;

#define INDICATOR_WIDTH   60
#define INDICATOR_HEIGHT  40
#define INDICATOR_MARGIN   2
#define MSG_BUBBLE_MAX_RATIO .3

#define LAZY_LOAD_INITIAL_MSGS_LIMIT 20

G_DEFINE_TYPE (ChattyChatView, chatty_chat_view, GTK_TYPE_BOX)


const char *disclaimer_strings[][3] = {
  {
    N_("This is an IM conversation."),
    N_("Your messages are not encrypted,"),
    N_("ask your counterpart to use E2EE."),
  },
  {
    N_("This is an IM conversation."),
    N_("Your messages are secured"),
    N_("by end-to-end encryption."),
  },
  {
    N_("This is an SMS conversation."),
    N_("Your messages are not encrypted,"),
    N_("and carrier rates may apply."),
  },
};

static void
chatty_conv_init_emoticon_translations (void)
{
  ht_emoticon = g_hash_table_new_full (g_str_hash,
                                       g_str_equal,
                                       g_free,
                                       g_free);

  g_hash_table_insert (ht_emoticon, ":)", "🙂");
  g_hash_table_insert (ht_emoticon, ";)", "😉");
  g_hash_table_insert (ht_emoticon, ":(", "🙁");
  g_hash_table_insert (ht_emoticon, ":'(", "😢");
  g_hash_table_insert (ht_emoticon, ":/", "😕");
  g_hash_table_insert (ht_emoticon, ":D", "😀");
  g_hash_table_insert (ht_emoticon, ":'D", "😂");
  g_hash_table_insert (ht_emoticon, ";P", "😜");
  g_hash_table_insert (ht_emoticon, ":P", "😛");
  g_hash_table_insert (ht_emoticon, ";p", "😜");
  g_hash_table_insert (ht_emoticon, ":p", "😛");
  g_hash_table_insert (ht_emoticon, ":o", "😮");
  g_hash_table_insert (ht_emoticon, "B)", "😎 ");
  g_hash_table_insert (ht_emoticon, "SANTA", "🎅");
  g_hash_table_insert (ht_emoticon, "FROSTY", "⛄");
}


static gboolean
chat_view_hash_table_match_item (gpointer key,
                                 gpointer value,
                                 gpointer user_data)
{
  return value == user_data;
}

static gchar *
chatty_msg_list_escape_message (const gchar *message)
{
  g_autofree gchar  *nl_2_br;
  g_autofree gchar  *striped;
  g_autofree gchar  *escaped;
  g_autofree gchar  *linkified;
  gchar *result;

  nl_2_br = purple_strdup_withhtml (message);
  striped = purple_markup_strip_html (nl_2_br);
  escaped = purple_markup_escape_text (striped, -1);
  linkified = purple_markup_linkify (escaped);
  // convert all tags to lowercase for GtkLabel markup parser
  purple_markup_html_to_xhtml (linkified, &result, NULL);

  return result;
}

static void
chatty_draw_typing_indicator (cairo_t *cr)
{
  double dot_pattern [3][3]= {{0.5, 0.9, 0.9},
                              {0.7, 0.5, 0.9},
                              {0.9, 0.7, 0.5}};
  guint  dot_origins [3] = {15, 30, 45};
  double grey_lev,
    x, y,
    width, height,
    rad, deg;

  static guint i;

  deg = G_PI / 180.0;

  rad = INDICATOR_MARGIN * 5;
  x = y = INDICATOR_MARGIN;
  width = INDICATOR_WIDTH - INDICATOR_MARGIN * 2;
  height = INDICATOR_HEIGHT - INDICATOR_MARGIN * 2;

  if (i > 2)
    i = 0;

  cairo_new_sub_path (cr);
  cairo_arc (cr, x + width - rad, y + rad, rad, -90 * deg, 0 * deg);
  cairo_arc (cr, x + width - rad, y + height - rad, rad, 0 * deg, 90 * deg);
  cairo_arc (cr, x + rad, y + height - rad, rad, 90 * deg, 180 * deg);
  cairo_arc (cr, x + rad, y + rad, rad, 180 * deg, 270 * deg);
  cairo_close_path (cr);

  cairo_set_source_rgb (cr, 0.7, 0.7, 0.7);
  cairo_set_line_width (cr, 1.0);
  cairo_stroke (cr);

  for (guint n = 0; n < 3; n++) {
    cairo_arc (cr, dot_origins[n], 20, 5, 0, 2 * G_PI);
    grey_lev = dot_pattern[i][n];
    cairo_set_source_rgb (cr, grey_lev, grey_lev, grey_lev);
    cairo_fill (cr);
  }

  i++;
}


static gboolean
chat_view_typing_indicator_draw_cb (ChattyChatView *self,
                                    cairo_t        *cr)
{
  g_assert (CHATTY_IS_CHAT_VIEW (self));

  if (self->refresh_typing_id > 0)
    chatty_draw_typing_indicator (cr);

  return TRUE;
}

static gboolean
chat_view_indicator_refresh_cb (ChattyChatView *self)
{
  g_assert (CHATTY_IS_CHAT_VIEW (self));

  gtk_widget_queue_draw (self->typing_indicator);

  return G_SOURCE_CONTINUE;
}

static void
chatty_check_for_emoticon (ChattyChatView *self)
{
  GtkTextIter         start, end, position;
  GHashTableIter      iter;
  gpointer            key, value;
  g_autofree char    *text = NULL;

  g_assert (CHATTY_IS_CHAT_VIEW (self));

  gtk_text_buffer_get_bounds (self->message_input_buffer, &start, &end);
  text = gtk_text_buffer_get_text (self->message_input_buffer, &start, &end, FALSE);

  g_hash_table_iter_init (&iter, ht_emoticon);

  while (g_hash_table_iter_next (&iter, &key, &value))
    if (g_str_has_suffix (text, (char *)key)) {
      position = end;

      gtk_text_iter_backward_chars (&position, strlen ((char *)key));
      gtk_text_buffer_delete (self->message_input_buffer, &position, &end);
      gtk_text_buffer_insert (self->message_input_buffer, &position, (char *)value, -1);

      break;
    }
}


static void
chat_view_lurch_status_changed_cb (int      err,
                                   int      status,
                                   gpointer user_data)
{
  ChattyChatView *self = user_data;
  GtkStyleContext *context;
  const char      *icon_name;

  g_assert (CHATTY_IS_CHAT_VIEW (self));

  if (err) {
    g_debug ("Failed to get the OMEMO status.");
    return;
  }

  context = gtk_widget_get_style_context (self->encrypt_icon);

  if (status == LURCH_STATUS_OK) {
    icon_name = "changes-prevent-symbolic";
    self->chatty_conv->omemo.enabled = TRUE;
    gtk_style_context_add_class (context, "encrypt");
  } else {
    icon_name = "changes-allow-symbolic";
    self->chatty_conv->omemo.enabled = FALSE;
    gtk_style_context_add_class (context, "unencrypt");
  }

  gtk_image_set_from_icon_name (GTK_IMAGE (self->encrypt_icon), icon_name, 1);
}


static void
chat_view_update_encrypt_status (ChattyChatView *self)
{
  PurpleConversation *conv;
  PurpleAccount      *account;
  const char         *name;
  g_autofree char    *stripped = NULL;

  g_assert (CHATTY_IS_CHAT_VIEW (self));

  conv = self->chatty_conv->conv;
  name = purple_conversation_get_name (conv);
  account  = purple_conversation_get_account (conv);
  stripped = chatty_utils_jabber_id_strip (name);

  gtk_widget_show (self->encrypt_icon);
  purple_signal_emit (purple_plugins_get_handle(),
                      "lurch-status-im",
                      account,
                      stripped,
                      chat_view_lurch_status_changed_cb,
                      self);
}


static PurpleBlistNode *
chatty_get_conv_blist_node (PurpleConversation *conv)
{
  PurpleBlistNode *node = NULL;

  switch (purple_conversation_get_type (conv)) {
  case PURPLE_CONV_TYPE_IM:
    node = PURPLE_BLIST_NODE (purple_find_buddy (conv->account,
                                                 conv->name));
    break;
  case PURPLE_CONV_TYPE_CHAT:
    node = PURPLE_BLIST_NODE (purple_blist_find_chat (conv->account,
                                                      conv->name));
    break;
  case PURPLE_CONV_TYPE_UNKNOWN:
  case PURPLE_CONV_TYPE_MISC:
  case PURPLE_CONV_TYPE_ANY:
  default:
    g_warning ("Unhandled conversation type %d",
               purple_conversation_get_type (conv));
    break;
  }
  return node;
}

static void
chat_view_setup_file_upload (ChattyChatView *self)
{
  PurplePluginProtocolInfo *prpl_info;
  PurpleConnection         *gc;
  PurpleBlistNode          *node;
  g_autoptr(GList)          list = NULL;

  g_assert (CHATTY_IS_CHAT_VIEW (self));

  gtk_widget_show (self->send_file_button);

  gc = purple_conversation_get_gc (self->chatty_conv->conv);
  node = chatty_get_conv_blist_node (self->chatty_conv->conv);
  prpl_info = PURPLE_PLUGIN_PROTOCOL_INFO (gc->prpl);

  if (prpl_info->blist_node_menu)
    list = prpl_info->blist_node_menu (node);

  for (GList *l = list; l; l = l->next) {
    PurpleMenuAction *act = l->data;

    if (g_strcmp0 (act->label, "HTTP File Upload") == 0) {
      g_object_set_data (G_OBJECT (self->send_file_button),
                         "callback", act->callback);

      g_object_set_data (G_OBJECT (self->send_file_button),
                         "callback-data", act->data);
    }
    purple_menu_action_free (act);
  }
}

static void
chatty_chat_view_update (ChattyChatView *self)
{
  GtkStyleContext *context;
  PurpleAccount *account;
  const char *protocol_id;
  PurpleConversationType conv_type;
  int index = -1;

  g_assert (CHATTY_IS_CHAT_VIEW (self));

  account = purple_conversation_get_account (self->chatty_conv->conv);
  conv_type = purple_conversation_get_type (self->chatty_conv->conv);
  protocol_id = purple_account_get_protocol_id (account);

  if (conv_type == PURPLE_CONV_TYPE_CHAT)
    self->message_type = CHATTY_MSG_TYPE_MUC;
  else if (conv_type == PURPLE_CONV_TYPE_IM &&
           g_strcmp0 (protocol_id, "prpl-mm-sms") == 0)
    self->message_type = CHATTY_MSG_TYPE_SMS;
  else
    self->message_type = CHATTY_MSG_TYPE_IM;

  if (conv_type == PURPLE_CONV_TYPE_IM)
    chat_view_update_encrypt_status (self);

  if (chatty_manager_has_file_upload_plugin (chatty_manager_get_default ()) &&
      g_strcmp0 (protocol_id, "prpl-jabber") == 0)
    chat_view_setup_file_upload (self);

  if (self->message_type == CHATTY_MSG_TYPE_IM)
    index = 0;
  else if (self->message_type == CHATTY_MSG_TYPE_SMS)
    index = 2;

  /* XXX: This is temporary, strings put into different labels isn’t good for translation */
  if (index >= 0) {
    gtk_label_set_label (GTK_LABEL (self->empty_label0), disclaimer_strings[index][0]);
    gtk_label_set_label (GTK_LABEL (self->empty_label1), disclaimer_strings[index][1]);
    gtk_label_set_label (GTK_LABEL (self->empty_label2), disclaimer_strings[index][2]);
  }

  self->chatty_conv->omemo.symbol_encrypt = GTK_IMAGE (self->encrypt_icon);
  context = gtk_widget_get_style_context (self->send_message_button);

  if (self->message_type == CHATTY_MSG_TYPE_SMS)
    gtk_style_context_add_class (context, "button_send_green");
  else if (self->message_type == CHATTY_MSG_TYPE_IM)
    gtk_style_context_add_class (context, "suggested-action");
}

static void
chatty_update_typing_status (ChattyChatView *self)
{
  PurpleConversation     *conv;
  PurpleConvIm           *im;
  GtkTextIter             start, end;
  g_autofree char         *text = NULL;
  PurpleConversationType  type;
  gboolean                empty;

  g_assert (CHATTY_IS_CHAT_VIEW (self));

  conv = self->chatty_conv->conv;
  type = purple_conversation_get_type (conv);

  if (type != PURPLE_CONV_TYPE_IM)
    return;

  gtk_text_buffer_get_bounds (self->message_input_buffer, &start, &end);
  text = gtk_text_buffer_get_text (self->message_input_buffer, &start, &end, FALSE);

  empty = !text || !*text || *text == '/';

  im = PURPLE_CONV_IM (conv);

  if (!empty) {
    gboolean send = (purple_conv_im_get_send_typed_timeout (im) == 0);

    purple_conv_im_stop_send_typed_timeout (im);
    purple_conv_im_start_send_typed_timeout (im);

    if (send || (purple_conv_im_get_type_again (im) != 0 &&
                 time (NULL) > purple_conv_im_get_type_again (im))) {
      unsigned int timeout;

      timeout = serv_send_typing (purple_conversation_get_gc (conv),
                                  purple_conversation_get_name (conv),
                                  PURPLE_TYPING);

      purple_conv_im_set_type_again (im, timeout);
    }
  } else {
    purple_conv_im_stop_send_typed_timeout (im);

    serv_send_typing (purple_conversation_get_gc (conv),
                      purple_conversation_get_name (conv),
                      PURPLE_NOT_TYPING);
  }
}

static void
chatty_conv_get_im_messages_cb (const guchar *msg,
                                int           direction,
                                time_t        time_stamp,
                                const guchar *uuid,
                                gpointer      user_data,
                                int           last_message)
{
  ChattyChatView *self = user_data;
  g_autofree gchar *iso_timestamp = NULL;
  guint             msg_dir;

  g_assert (CHATTY_IS_CHAT_VIEW (self));

  iso_timestamp = g_malloc0 (MAX_GMT_ISO_SIZE * sizeof(char));

  // TODO: @LELAND: Check this memory management, don't like it
  free (self->chatty_conv->oldest_message_displayed);
  self->chatty_conv->oldest_message_displayed = g_strdup ((const gchar *)uuid);

  if (direction == 1)
    msg_dir = MSG_IS_INCOMING;
  else if (direction == -1)
    msg_dir = MSG_IS_OUTGOING;
  else
    msg_dir = MSG_IS_SYSTEM; // TODO: LELAND: Do we have this case for IMs?

  strftime (iso_timestamp,
            MAX_GMT_ISO_SIZE * sizeof(char),
            "%b %d",
            localtime (&time_stamp));

  if (msg && *msg) {
    chatty_chat_view_add_message_at (self,
                                     msg_dir,
                                     (const gchar *) msg,
                                     last_message ? iso_timestamp : NULL,
                                     NULL,
                                     ADD_MESSAGE_ON_TOP);
  }
}


static void
chatty_conv_get_chat_messages_cb (const guchar *msg,
                                  int           time_stamp,
                                  int           direction,
                                  const char   *room,
                                  const guchar *who,
                                  const guchar *uuid,
                                  gpointer      user_data)
{
  ChattyChatView *self = user_data;
  PurpleAccount  *account;

  g_assert (CHATTY_IS_CHAT_VIEW (self));

  /* TODO: @LELAND: Check this memory management, don't like it */
  free (self->chatty_conv->oldest_message_displayed);
  self->chatty_conv->oldest_message_displayed = g_strdup ((const gchar *)uuid);

  if (msg && *msg) {
    if (direction == 1) {
      g_autoptr(GdkPixbuf)  avatar = NULL;
      g_auto(GStrv)         line_split = NULL;
      PurpleBuddy          *buddy;
      g_autofree gchar     *alias = NULL;
      const char           *color;

      account = purple_conversation_get_account (self->chatty_conv->conv);
      buddy = purple_find_buddy (account, room);
      color = chatty_utils_get_color_for_str (room);

      /* Extract the alias from 'who' (full_room_address/alias) */
      line_split = g_strsplit ((const char *)who, "/", -1);
      if (line_split)
        alias = g_strdup (line_split[1]);
      else
        alias = g_strdup ((const char *)who);

      avatar = chatty_icon_get_buddy_icon ((PurpleBlistNode *)buddy,
                                           (const char *)alias,
                                           CHATTY_ICON_SIZE_MEDIUM,
                                           color,
                                           FALSE);

      chatty_chat_view_add_message_at (self,
                                       MSG_IS_INCOMING,
                                       (const char *)msg,
                                       NULL,
                                       avatar,
                                       ADD_MESSAGE_ON_TOP);

    } else if (direction == -1) {
      chatty_chat_view_add_message_at (self,
                                       MSG_IS_OUTGOING,
                                       (const char *)msg,
                                       NULL,
                                       NULL,
                                       ADD_MESSAGE_ON_TOP);
    } else {
      chatty_chat_view_add_message_at (self,
                                       MSG_IS_SYSTEM,
                                       (const char *)msg,
                                       NULL,
                                       NULL,
                                       ADD_MESSAGE_ON_TOP);
    }

    chatty_conv_set_unseen (self->chatty_conv, CHATTY_UNSEEN_NONE);
  }
}


static void
chat_view_edge_overshot_cb (ChattyChatView  *self,
                            GtkPositionType  pos)
{
  g_assert (CHATTY_IS_CHAT_VIEW (self));

  if (pos == GTK_POS_TOP)
    chatty_chat_view_load (self, LAZY_LOAD_INITIAL_MSGS_LIMIT);
}


static gboolean
chat_view_input_focus_in_cb (ChattyChatView *self)
{
  GtkStyleContext *context;

  g_assert (CHATTY_IS_CHAT_VIEW (self));

  context = gtk_widget_get_style_context (self->input_frame);
  gtk_style_context_remove_class (context, "msg_entry_defocused");
  gtk_style_context_add_class (context, "msg_entry_focused");

  return FALSE;
}


static gboolean
chat_view_input_focus_out_cb (ChattyChatView *self)
{
  GtkStyleContext *context;

  g_assert (CHATTY_IS_CHAT_VIEW (self));

  context = gtk_widget_get_style_context (self->input_frame);
  gtk_style_context_remove_class (context, "msg_entry_focused");
  gtk_style_context_add_class (context, "msg_entry_defocused");

  return FALSE;
}



static gboolean
chatty_conv_check_for_command (ChattyChatView *self)
{
  PurpleConversation *conv;
  gchar              *cmd;
  gboolean            retval = FALSE;
  GtkTextIter         start, end;
  PurpleMessageFlags  flags = 0;

  flags |= PURPLE_MESSAGE_NO_LOG | PURPLE_MESSAGE_SYSTEM;
  conv = self->chatty_conv->conv;

  gtk_text_buffer_get_bounds (self->message_input_buffer, &start, &end);

  cmd = gtk_text_buffer_get_text (self->message_input_buffer, &start, &end, FALSE);

  if (cmd && *cmd == '/') {
    PurpleCmdStatus status;
    gchar *error, *cmdline;

    cmdline = cmd + strlen ("/");

    if (purple_strequal (cmdline, "xyzzy")) {
      purple_conversation_write (conv,
                                 "",
                                 "Nothing happens",
                                 flags,
                                 time(NULL));

      g_free (cmd);
      return TRUE;
    }

    purple_conversation_write (conv,
                               "",
                               cmdline,
                               flags,
                               time(NULL));

    status = purple_cmd_do_command (conv, cmdline, cmdline, &error);

    switch (status) {
    case PURPLE_CMD_STATUS_OK:
      retval = TRUE;
      break;
    case PURPLE_CMD_STATUS_NOT_FOUND:
      {
        PurplePluginProtocolInfo *prpl_info = NULL;
        PurpleConnection *gc;

        if ((gc = purple_conversation_get_gc (conv)))
          prpl_info = PURPLE_PLUGIN_PROTOCOL_INFO(gc->prpl);

        if ((prpl_info != NULL) &&
            (prpl_info->options & OPT_PROTO_SLASH_COMMANDS_NATIVE)) {
          gchar *spaceslash;

          /* If the first word in the entered text has a '/' in it, then the user
           * probably didn't mean it as a command. So send the text as message. */
          spaceslash = cmdline;

          while (*spaceslash && *spaceslash != ' ' && *spaceslash != '/') {
            spaceslash++;
          }

          if (*spaceslash != '/') {
            purple_conversation_write (conv,
                                       "",
                                       "Unknown command. Get a list of available commands with '/chatty help'",
                                       flags,
                                       time(NULL));
            retval = TRUE;
          }
        }
        break;
      }
    case PURPLE_CMD_STATUS_WRONG_ARGS:
      purple_conversation_write (conv,
                                 "",
                                 "Wrong number of arguments for the command.",
                                 flags,
                                 time(NULL));
      retval = TRUE;
      break;
    case PURPLE_CMD_STATUS_FAILED:
      purple_conversation_write (conv,
                                 "",
                                 error ? error : "The command failed.",
                                 flags,
                                 time(NULL));
      g_free(error);
      retval = TRUE;
      break;
    case PURPLE_CMD_STATUS_WRONG_TYPE:
      if (purple_conversation_get_type (conv) == PURPLE_CONV_TYPE_IM)
        purple_conversation_write (conv,
                                   "",
                                   "That command only works in chats, not IMs.",
                                   flags,
                                   time(NULL));
      else
        purple_conversation_write (conv,
                                   "",
                                   "That command only works in IMs, not chats.",
                                   flags,
                                   time(NULL));
      retval = TRUE;
      break;
    case PURPLE_CMD_STATUS_WRONG_PRPL:
      purple_conversation_write (conv,
                                 "",
                                 "That command doesn't work on this protocol.",
                                 flags,
                                 time(NULL));
      retval = TRUE;
      break;
    default:
      break;
    }
  }

  g_free (cmd);
  return retval;
}


static void
chat_view_send_file_button_clicked_cb (ChattyChatView *self,
                                       GtkButton      *button)
{
  PurpleBlistNode *node;
  gpointer data;

  void (*callback)(gpointer, gpointer);

  g_assert (CHATTY_IS_CHAT_VIEW (self));
  g_assert (GTK_IS_BUTTON (button));

  callback = g_object_get_data (G_OBJECT (button), "callback");
  data = g_object_get_data (G_OBJECT (button), "callback-data");
  node = chatty_get_conv_blist_node (self->chatty_conv->conv);

  if (callback)
    callback (node, data);
}

static void
chat_view_send_message_button_clicked_cb (ChattyChatView *self)
{
  PurpleConversation  *conv;
  PurpleAccount *account;
  GtkTextIter    start, end;
  GDateTime     *time;
  gchar         *message = NULL;
  gchar         *footer_str = NULL;
  const gchar   *protocol_id;
  gchar         *sms_id_str;
  guint          sms_id;

  g_assert (CHATTY_IS_CHAT_VIEW (self));

  conv = self->chatty_conv->conv;
  account = purple_conversation_get_account (conv);

  gtk_text_buffer_get_bounds (self->message_input_buffer, &start, &end);

  if (chatty_conv_check_for_command (self)) {
    gtk_widget_hide (self->send_message_button);
    gtk_text_buffer_delete (self->message_input_buffer, &start, &end);

    return;
  }

  if (!purple_account_is_connected (account))
    return;

  protocol_id = purple_account_get_protocol_id (account);

  gtk_widget_grab_focus (self->message_input);
  purple_idle_touch ();

  message = gtk_text_buffer_get_text (self->message_input_buffer, &start, &end, FALSE);

  time = g_date_time_new_now_local ();
  footer_str = g_date_time_format (time, "%R");
  g_date_time_unref (time);

  footer_str = g_strconcat ("<small>",
                            "<span color='grey'>",
                            footer_str,
                            "</span>",
                            "<span color='grey'>",
                            " ✓",
                            "</span></small>",
                            NULL);

  self->chatty_conv->msg_bubble_footer = gtk_label_new (NULL);
  gtk_widget_show (self->chatty_conv->msg_bubble_footer);
  gtk_label_set_markup (GTK_LABEL (self->chatty_conv->msg_bubble_footer), footer_str);
  gtk_label_set_xalign (GTK_LABEL (self->chatty_conv->msg_bubble_footer), 1);

  if (gtk_text_buffer_get_char_count (self->message_input_buffer)) {
    /* provide a msg-id to the sms-plugin for send-receipts */
    if (g_strcmp0 (protocol_id, "prpl-mm-sms") == 0) {
      sms_id = g_random_int ();

      sms_id_str = g_strdup_printf ("%i", sms_id);

      g_hash_table_insert (ht_sms_id,
                           sms_id_str, self->chatty_conv->msg_bubble_footer);

      g_debug ("hash table insert sms_id_str: %s  ht_size: %i\n",
               sms_id_str, g_hash_table_size (ht_sms_id));

      purple_conv_im_send_with_flags (PURPLE_CONV_IM (conv),
                                      sms_id_str,
                                      PURPLE_MESSAGE_NO_LOG |
                                      PURPLE_MESSAGE_NOTIFY |
                                      PURPLE_MESSAGE_INVISIBLE);
    }

    if (purple_conversation_get_type (conv) == PURPLE_CONV_TYPE_IM) {
      purple_conv_im_send (PURPLE_CONV_IM(conv), message);
    } else if (purple_conversation_get_type (conv) == PURPLE_CONV_TYPE_CHAT) {
      purple_conv_chat_send(PURPLE_CONV_CHAT(conv), message);
    }

    gtk_widget_hide (self->send_message_button);
  }

  gtk_text_buffer_delete (self->message_input_buffer, &start, &end);

  g_free (message);
  g_free (footer_str);
}

static gboolean
chat_view_input_key_pressed_cb (ChattyChatView *self,
                                GdkEventKey    *event_key)
{
  g_assert (CHATTY_IS_CHAT_VIEW (self));

  if (!chatty_settings_get_return_sends_message (chatty_settings_get_default ()) ||
      gtk_text_buffer_get_char_count (self->message_input_buffer) == 0)
    return FALSE;

  if (!(event_key->state & GDK_SHIFT_MASK) && event_key->keyval == GDK_KEY_Return) {
    chat_view_send_message_button_clicked_cb (self);

    return TRUE;
  }

  return FALSE;
}


static void
chat_view_message_input_changed_cb (ChattyChatView *self)
{
  PurpleAccount *account;
  const gchar   *protocol;
  gboolean       has_text;

  g_assert (CHATTY_IS_CHAT_VIEW (self));

  account  = purple_conversation_get_account (self->chatty_conv->conv);
  protocol = purple_account_get_protocol_id (account);
  has_text = gtk_text_buffer_get_char_count (self->message_input_buffer) > 0;
  gtk_widget_set_visible (self->send_message_button, has_text);

  if (chatty_settings_get_send_typing (chatty_settings_get_default ()))
    chatty_update_typing_status (self);

  if (chatty_settings_get_convert_emoticons (chatty_settings_get_default ()) &&
      (g_strcmp0 (protocol, "prpl-mm-sms") != 0))
    chatty_check_for_emoticon (self);
}

static void
list_page_size_changed_cb (ChattyChatView *self)
{
  gdouble size, upper, value;

  g_assert (CHATTY_IS_CHAT_VIEW (self));

  size  = gtk_adjustment_get_page_size (self->vadjustment);
  value = gtk_adjustment_get_value (self->vadjustment);
  upper = gtk_adjustment_get_upper (self->vadjustment);

  if (upper - size <= DBL_EPSILON)
    return;

  /* If close to bottom, scroll to bottom */
  if (!self->first_scroll_to_bottom || upper - value < (size * 1.75))
    gtk_adjustment_set_value (self->vadjustment, upper);

  self->first_scroll_to_bottom = TRUE;
}

static void
chat_view_adjustment_changed_cb (GtkAdjustment  *adjustment,
                                 GParamSpec     *pspec,
                                 ChattyChatView *self)
{
  GtkAdjustment *vadjust;
  GtkWidget     *vscroll;
  gdouble        upper;
  gdouble        page_size;
  gint           max_height;

  g_assert (CHATTY_IS_CHAT_VIEW (self));

  vadjust = gtk_scrolled_window_get_vadjustment (GTK_SCROLLED_WINDOW (self->scrolled_window));
  vscroll = gtk_scrolled_window_get_vscrollbar (GTK_SCROLLED_WINDOW (self->scrolled_window));
  upper = gtk_adjustment_get_upper (GTK_ADJUSTMENT (vadjust));
  page_size = gtk_adjustment_get_page_size (GTK_ADJUSTMENT (vadjust));
  max_height = gtk_scrolled_window_get_max_content_height (GTK_SCROLLED_WINDOW (self->scrolled_window));

  gtk_adjustment_set_value (vadjust, upper - page_size);

  if (upper > (gdouble)max_height) {
    gtk_widget_set_visible (vscroll, TRUE);
    gtk_widget_hide (self->encrypt_icon);
  } else {
    gtk_widget_set_visible (vscroll, FALSE);
    gtk_widget_show (self->encrypt_icon);
  }
}

static void
chat_view_sms_sent_cb (const char *sms_id,
                       int         status)
{
  GtkWidget   *bubble_footer;
  GDateTime   *time;
  gchar       *footer_str = NULL;
  const gchar *color;
  const gchar *symbol;

  if (sms_id == NULL)
    return;

  switch (status) {
    case CHATTY_SMS_RECEIPT_NONE:
      color = "<span color='red'>";
      symbol = " x";
      break;
    case CHATTY_SMS_RECEIPT_MM_ACKN:
      color = "<span color='grey'>";
      symbol = " ✓";
      break;
    case CHATTY_SMS_RECEIPT_SMSC_ACKN:
      color = "<span color='#6cba3d'>";
      symbol = " ✓";
      break;
    default:
      return;
  }

  time = g_date_time_new_now_local ();
  footer_str = g_date_time_format (time, "%R");
  g_date_time_unref (time);

  bubble_footer = (GtkWidget*) g_hash_table_lookup (ht_sms_id, sms_id);

  footer_str = g_strconcat ("<small>",
                            "<span color='grey'>",
                            footer_str,
                            "</span>",
                            color,
                            symbol,
                            "</span></small>",
                            NULL);

  if (bubble_footer != NULL) {
    gtk_label_set_markup (GTK_LABEL(bubble_footer), footer_str);

    g_hash_table_remove (ht_sms_id, sms_id);
  }

  g_free (footer_str);
}

static void
chatty_chat_view_finalize (GObject *object)
{
  ChattyChatView *self = (ChattyChatView *)object;

  g_clear_object (&self->chat);

  G_OBJECT_CLASS (chatty_chat_view_parent_class)->finalize (object);
}

static void
chatty_chat_view_class_init (ChattyChatViewClass *klass)
{
  GObjectClass   *object_class = G_OBJECT_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  object_class->finalize = chatty_chat_view_finalize;

  gtk_widget_class_set_template_from_resource (widget_class,
                                               "/sm/puri/chatty/"
                                               "ui/chatty-chat-view.ui");

  gtk_widget_class_bind_template_child (widget_class, ChattyChatView, message_list);
  gtk_widget_class_bind_template_child (widget_class, ChattyChatView, typing_revealer);
  gtk_widget_class_bind_template_child (widget_class, ChattyChatView, typing_indicator);
  gtk_widget_class_bind_template_child (widget_class, ChattyChatView, input_frame);
  gtk_widget_class_bind_template_child (widget_class, ChattyChatView, scrolled_window);
  gtk_widget_class_bind_template_child (widget_class, ChattyChatView, message_input);
  gtk_widget_class_bind_template_child (widget_class, ChattyChatView, send_file_button);
  gtk_widget_class_bind_template_child (widget_class, ChattyChatView, encrypt_icon);
  gtk_widget_class_bind_template_child (widget_class, ChattyChatView, send_message_button);
  gtk_widget_class_bind_template_child (widget_class, ChattyChatView, empty_view);
  gtk_widget_class_bind_template_child (widget_class, ChattyChatView, empty_label0);
  gtk_widget_class_bind_template_child (widget_class, ChattyChatView, empty_label1);
  gtk_widget_class_bind_template_child (widget_class, ChattyChatView, empty_label2);
  gtk_widget_class_bind_template_child (widget_class, ChattyChatView, message_input_buffer);
  gtk_widget_class_bind_template_child (widget_class, ChattyChatView, vadjustment);

  gtk_widget_class_bind_template_callback (widget_class, chat_view_edge_overshot_cb);
  gtk_widget_class_bind_template_callback (widget_class, chat_view_typing_indicator_draw_cb);
  gtk_widget_class_bind_template_callback (widget_class, chat_view_input_focus_in_cb);
  gtk_widget_class_bind_template_callback (widget_class, chat_view_input_focus_out_cb);
  gtk_widget_class_bind_template_callback (widget_class, chat_view_send_file_button_clicked_cb);
  gtk_widget_class_bind_template_callback (widget_class, chat_view_send_message_button_clicked_cb);
  gtk_widget_class_bind_template_callback (widget_class, chat_view_input_key_pressed_cb);
  gtk_widget_class_bind_template_callback (widget_class, chat_view_message_input_changed_cb);
  gtk_widget_class_bind_template_callback (widget_class, list_page_size_changed_cb);
}

static void
chatty_chat_view_init (ChattyChatView *self)
{
  GtkAdjustment *vadjustment;

  gtk_widget_init_template (GTK_WIDGET (self));
  gtk_list_box_set_placeholder(GTK_LIST_BOX (self->message_list), self->empty_view);

  vadjustment = gtk_scrolled_window_get_vadjustment (GTK_SCROLLED_WINDOW (self->scrolled_window));
  g_signal_connect_after (G_OBJECT (vadjustment), "notify::upper",
                          G_CALLBACK (chat_view_adjustment_changed_cb),
                          self);
}

GtkWidget *
chatty_chat_view_new (void)
{
  return g_object_new (CHATTY_TYPE_CHAT_VIEW, NULL);
}


void
chatty_chat_view_purple_init (void)
{
  ht_sms_id = g_hash_table_new_full (g_str_hash, g_str_equal, g_free, NULL);
  chatty_conv_init_emoticon_translations ();

  purple_signal_connect (purple_conversations_get_handle (),
                         "sms-sent", ht_sms_id,
                         PURPLE_CALLBACK (chat_view_sms_sent_cb), NULL);
}

void
chatty_chat_view_purple_uninit (void)
{
  purple_signals_disconnect_by_handle (ht_sms_id);

  g_hash_table_destroy (ht_sms_id);
  g_hash_table_destroy (ht_emoticon);
}

void
chatty_chat_view_set_chat (ChattyChatView *self,
                           ChattyChat     *chat)
{
  g_return_if_fail (CHATTY_IS_CHAT_VIEW (self));
  g_return_if_fail (CHATTY_IS_CHAT (chat));

  /* TODO */
  g_return_if_reached ();

  if (!g_set_object (&self->chat, chat))
    return;

  chatty_chat_view_update (self);
}

ChattyChat *
chatty_chat_view_get_chat (ChattyChatView *self)
{
  g_return_val_if_fail (CHATTY_IS_CHAT_VIEW (self), NULL);

  return self->chat;
}


void
chatty_chat_view_load (ChattyChatView *self,
                       guint           limit)
{
  PurpleAccount *account;
  const gchar   *conv_name;
  gboolean       im;

  g_return_if_fail (CHATTY_IS_CHAT_VIEW (self));
  g_return_if_fail (self->chatty_conv);

  im = (self->chatty_conv->conv->type == PURPLE_CONV_TYPE_IM);

  conv_name = purple_conversation_get_name (self->chatty_conv->conv);
  account = purple_conversation_get_account (self->chatty_conv->conv);

  if (im) {
    g_autofree char *who = NULL;

    /* Remove resource (user could be connecting from different devices/applications) */
    who = chatty_utils_jabber_id_strip (conv_name);

    chatty_history_get_im_messages (account->username,
                                    who,
                                    chatty_conv_get_im_messages_cb,
                                    self,
                                    limit,
                                    self->chatty_conv->oldest_message_displayed);
  } else {
    chatty_history_get_chat_messages (account->username,
                                      conv_name,
                                      chatty_conv_get_chat_messages_cb,
                                      self,
                                      limit,
                                      self->chatty_conv->oldest_message_displayed);
  }
}


void
chatty_chat_view_remove_footer (ChattyChatView *self)
{
  g_return_if_fail (CHATTY_IS_CHAT_VIEW (self));

  g_hash_table_foreach_remove (ht_sms_id,
                               chat_view_hash_table_match_item,
                               self->chatty_conv->msg_bubble_footer);
}

void
chatty_chat_view_focus_entry (ChattyChatView *self)
{
  g_return_if_fail (CHATTY_IS_CHAT_VIEW (self));

  gtk_widget_grab_focus (self->message_input);
}


void
chatty_chat_view_set_conv (ChattyChatView     *self,
                           ChattyConversation *chatty_conv)
{
  g_return_if_fail (CHATTY_IS_CHAT_VIEW (self));

  self->chatty_conv = chatty_conv;

  chatty_chat_view_update (self);
}

ChattyConversation *
chatty_chat_view_get_conv (ChattyChatView *self)
{
  g_return_val_if_fail (CHATTY_IS_CHAT_VIEW (self), NULL);

  return self->chatty_conv;
}

void
chatty_chat_view_show_typing_indicator (ChattyChatView *self)
{
  g_return_if_fail (CHATTY_IS_CHAT_VIEW (self));

  gtk_revealer_set_reveal_child (GTK_REVEALER (self->typing_revealer), TRUE);

  self->refresh_typing_id = g_timeout_add (300,
                                           (GSourceFunc)chat_view_indicator_refresh_cb,
                                           self);
}

void
chatty_chat_view_hide_typing_indicator (ChattyChatView *self)
{
  g_return_if_fail (CHATTY_IS_CHAT_VIEW (self));

  gtk_revealer_set_reveal_child (GTK_REVEALER (self->typing_revealer), FALSE);
  g_clear_handle_id (&self->refresh_typing_id, g_source_remove);
}

void
chatty_chat_view_add_message_at (ChattyChatView *self,
                                 guint           message_dir,
                                 const gchar    *html_message,
                                 const gchar    *footer,
                                 GdkPixbuf      *avatar,
                                 guint           position)
{
  GtkWidget *row;
  g_autofree char *message = NULL;

  /* Don’t set avatar for IM chats */
  if (self->message_type == CHATTY_MSG_TYPE_IM ||
      self->message_type == CHATTY_MSG_TYPE_SMS)
    avatar = NULL;

  message = chatty_msg_list_escape_message (html_message);
  row = chatty_message_row_new ();

  if (position == ADD_MESSAGE_ON_BOTTOM)
    gtk_container_add (GTK_CONTAINER (self->message_list), row);
  else
    gtk_list_box_prepend (GTK_LIST_BOX (self->message_list), row);

  chatty_message_row_set_item (CHATTY_MESSAGE_ROW (row), message_dir,
                               self->message_type, message,
                               footer, avatar);

  if (message_dir == MSG_IS_OUTGOING && self->chatty_conv->msg_bubble_footer)
    chatty_message_row_set_footer (CHATTY_MESSAGE_ROW (row),
                                   self->chatty_conv->msg_bubble_footer);
}