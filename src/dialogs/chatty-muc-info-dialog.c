/*
 * Copyright (C) 2020 Purism SPC
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */



#define G_LOG_DOMAIN "chatty-muc-info-dialog"

#include <glib.h>
#include <glib/gi18n.h>
#include <glib-object.h>
#include "chatty-window.h"
#include "chatty-conversation.h"
#include "chatty-icons.h"
#include "purple.h"
#include "chatty-muc-info-dialog.h"


struct _ChattyMucInfoDialog
{
  HdyDialog  parent_instance;

  GtkWidget *button_back;
  GtkWidget *button_invite_contact;
  GtkWidget *button_show_invite_contact;
  GtkWidget *button_edit_topic;
  GtkWidget *label_chat_id;
  GtkWidget *label_num_user;
  GtkWidget *label_topic;
  GtkWidget *label_title;
  GtkWidget *switch_prefs_notifications;
  GtkWidget *switch_prefs_status;
  GtkWidget *list_muc_settings;
  GtkWidget *stack_panes_muc_info;
  GtkWidget *box_topic_frame;
  GtkWidget *box_topic_editor;
  GtkWidget *box_user_list;
  GtkWidget *textview_topic;
  GtkWidget *entry_invite_name;
  GtkWidget *entry_invite_msg;

  GtkTextBuffer *msg_buffer_topic;

  const char *current_topic;
  const char *new_topic;

  ChattyConversation *chatty_conv;
};


enum {
  PROP_0,
  PROP_CHATTY_CONV,
  PROP_LAST
};

static GParamSpec *props[PROP_LAST];

G_DEFINE_TYPE (ChattyMucInfoDialog, chatty_muc_info_dialog, HDY_TYPE_DIALOG)


static void chatty_muc_set_topic (ChattyMucInfoDialog *self);


static void
dialog_delete_cb (ChattyMucInfoDialog *self)
{
  g_assert (CHATTY_IS_MUC_INFO_DIALOG (self));

  g_object_ref (self->chatty_conv->muc.treeview);

  gtk_container_remove (GTK_CONTAINER(self->box_user_list),
                        GTK_WIDGET(self->chatty_conv->muc.treeview));
}


static void
button_back_clicked_cb (ChattyMucInfoDialog *self)
{
  g_assert (CHATTY_IS_MUC_INFO_DIALOG (self));

  gtk_stack_set_visible_child_name (GTK_STACK(self->stack_panes_muc_info), "view-muc-info");
}


static void
button_invite_contact_clicked_cb (ChattyMucInfoDialog *self)
{
  const char *name;
  const char *invite_msg;

  g_assert (CHATTY_IS_MUC_INFO_DIALOG (self));

  name = gtk_entry_get_text (GTK_ENTRY(self->entry_invite_name));
  invite_msg = gtk_entry_get_text (GTK_ENTRY(self->entry_invite_msg));

  if (name != NULL) {
    chatty_conv_invite_muc_user (name, invite_msg);
  }

  gtk_stack_set_visible_child_name (GTK_STACK(self->stack_panes_muc_info), "view-muc-info");
}


static void
button_show_invite_contact_clicked_cb (ChattyMucInfoDialog *self)
{
  g_assert (CHATTY_IS_MUC_INFO_DIALOG (self));

  gtk_widget_grab_focus (GTK_WIDGET(self->entry_invite_name));

  gtk_stack_set_visible_child_name (GTK_STACK(self->stack_panes_muc_info), "view-invite-contact");
}


static void
button_edit_topic_clicked_cb (GtkToggleButton     *sender,
                              ChattyMucInfoDialog *self)
{
  GtkStyleContext *sc;
  GtkTextIter      start, end;
  gboolean         edit_mode;

  g_assert (CHATTY_IS_MUC_INFO_DIALOG (self));

  sc = gtk_widget_get_style_context (GTK_WIDGET(self->box_topic_frame));

  gtk_text_buffer_get_bounds (self->msg_buffer_topic,
                              &start,
                              &end);

  if (gtk_toggle_button_get_active (sender)) {
    edit_mode = TRUE;

    self->current_topic = gtk_text_buffer_get_text (self->msg_buffer_topic,
                                                    &start, &end,
                                                    FALSE);

    gtk_widget_grab_focus (GTK_WIDGET(self->textview_topic));

    gtk_style_context_remove_class (sc, "topic_no_edit");
    gtk_style_context_add_class (sc, "topic_edit");
  } else {
    edit_mode = FALSE;

    if (g_strcmp0 (self->current_topic, self->new_topic) != 0) {
      chatty_muc_set_topic (self);
    }

    gtk_style_context_remove_class (sc, "topic_edit");
    gtk_style_context_add_class (sc, "topic_no_edit");
  }

  gtk_text_view_set_editable (GTK_TEXT_VIEW(self->textview_topic), edit_mode);
  gtk_text_view_set_cursor_visible (GTK_TEXT_VIEW(self->textview_topic), edit_mode);
  gtk_widget_set_can_focus (GTK_WIDGET(self->textview_topic), edit_mode);
}


static void
switch_prefs_state_changed_cb (ChattyMucInfoDialog *self)
{
  PurpleBlistNode *node;
  gboolean         active;

  g_assert (CHATTY_IS_MUC_INFO_DIALOG (self));

  active = gtk_switch_get_active (GTK_SWITCH(self->switch_prefs_status));

  node = PURPLE_BLIST_NODE(purple_blist_find_chat (self->chatty_conv->conv->account, 
                                                   self->chatty_conv->conv->name));

  purple_blist_node_set_bool (node, "chatty-status-msg", active);
}


static void
switch_prefs_notify_changed_cb (ChattyMucInfoDialog *self)
{
  PurpleBlistNode *node;
  gboolean         active;

  g_assert (CHATTY_IS_MUC_INFO_DIALOG (self));

  active = gtk_switch_get_active (GTK_SWITCH(self->switch_prefs_notifications));

  node = PURPLE_BLIST_NODE(purple_blist_find_chat (self->chatty_conv->conv->account, 
                                                   self->chatty_conv->conv->name));

  purple_blist_node_set_bool (node, "chatty-notifications", active);
}


static void
invite_name_insert_text_cb (GtkEntry            *entry,
                            const gchar         *text,
                            gint                 length,
                            gint                *position,
                            ChattyMucInfoDialog *self)
{
  g_assert (CHATTY_IS_MUC_INFO_DIALOG (self));

  gtk_widget_set_sensitive (GTK_WIDGET(self->button_invite_contact), 
                            *position ? TRUE : FALSE);
}


static void
invite_name_delete_text_cb (GtkEntry            *entry,
                            gint                 start_pos,
                            gint                 end_pos,
                            ChattyMucInfoDialog *self)
{
  g_assert (CHATTY_IS_MUC_INFO_DIALOG (self));

  gtk_widget_set_sensitive (GTK_WIDGET(self->button_invite_contact), 
                            start_pos ? TRUE : FALSE);
}


static gboolean
textview_key_released_cb (ChattyMucInfoDialog *self)
{
  GtkTextIter start, end;

  g_assert (CHATTY_IS_MUC_INFO_DIALOG (self));

  gtk_text_buffer_get_bounds (self->msg_buffer_topic,
                              &start,
                              &end);

  self->new_topic = gtk_text_buffer_get_text (self->msg_buffer_topic,
                                              &start, &end,
                                              FALSE);

  return TRUE;
}


static void
chatty_muc_set_topic (ChattyMucInfoDialog *self)
{
  PurplePluginProtocolInfo *prpl_info = NULL;
  PurpleConnection         *gc;
  gint                      chat_id;

  g_assert (CHATTY_IS_MUC_INFO_DIALOG (self));

  gc = purple_conversation_get_gc (self->chatty_conv->conv);

  prpl_info = PURPLE_PLUGIN_PROTOCOL_INFO(gc->prpl);

  if (!gc || !prpl_info || !self->new_topic) {
    return;
  }

  if (prpl_info->set_chat_topic == NULL) {
    return;
  }

  chat_id = purple_conv_chat_get_id (PURPLE_CONV_CHAT(self->chatty_conv->conv));

  prpl_info->set_chat_topic (gc, chat_id, self->new_topic);
}


static void
chatty_muc_info_dialog_set_property (GObject       *object,
                                      guint         property_id,
                                      const GValue *value,
                                      GParamSpec   *pspec)
{
  ChattyMucInfoDialog *self = (ChattyMucInfoDialog *)object;

  switch (property_id) {
    case PROP_CHATTY_CONV:
      self->chatty_conv = g_value_get_pointer (value);
      break;
    
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
      break;
  }
}


static void
chatty_muc_info_dialog_get_property (GObject    *object,
                                     guint       property_id,
                                     GValue     *value,
                                     GParamSpec *pspec)
{
  ChattyMucInfoDialog *self = (ChattyMucInfoDialog *)object;

  switch (property_id) {
    case PROP_CHATTY_CONV:
      g_value_set_pointer (value, self->chatty_conv);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
      break;
  }
}


static void
chatty_muc_info_dialog_constructed (GObject *object)
{
  PurpleAccount            *account;
  PurpleConvChat           *chat;
  PurpleConvChatBuddyFlags  flags;
  PurpleBlistNode          *node;
  char                     *user_count_str;
  const char               *chat_name;
  const char               *topic;

  ChattyMucInfoDialog *self = (ChattyMucInfoDialog *)object;

  G_OBJECT_CLASS (chatty_muc_info_dialog_parent_class)->constructed (object);

  self->msg_buffer_topic = gtk_text_buffer_new (NULL);

  gtk_text_view_set_buffer (GTK_TEXT_VIEW(self->textview_topic), 
                            self->msg_buffer_topic);

  account = purple_conversation_get_account (self->chatty_conv->conv);

  chat = PURPLE_CONV_CHAT(self->chatty_conv->conv);
  node = PURPLE_BLIST_NODE(purple_blist_find_chat (account, self->chatty_conv->conv->name));

  chat_name = purple_conversation_get_title (self->chatty_conv->conv);
  user_count_str = g_strdup_printf ("%i %s", self->chatty_conv->muc.user_count, _("members"));

  gtk_label_set_text (GTK_LABEL(self->label_chat_id), chat_name);
  gtk_label_set_text (GTK_LABEL(self->label_num_user), user_count_str);

  topic = purple_conv_chat_get_topic (PURPLE_CONV_CHAT(self->chatty_conv->conv));
  flags = purple_conv_chat_user_get_flags (chat, chat->nick);

  if (flags & PURPLE_CBFLAGS_FOUNDER) {
    gtk_text_buffer_set_text (self->msg_buffer_topic, topic, strlen (topic));

    gtk_widget_show (GTK_WIDGET(self->box_topic_editor));
    gtk_widget_hide (GTK_WIDGET(self->label_topic));
    gtk_widget_show (GTK_WIDGET(self->label_title));
  } else {
    gtk_label_set_text (GTK_LABEL(self->label_topic), topic);

    gtk_widget_show (GTK_WIDGET(self->label_topic));
    gtk_widget_hide (GTK_WIDGET(self->box_topic_editor));
    gtk_widget_hide (GTK_WIDGET(self->label_title));
  }

  gtk_switch_set_state (GTK_SWITCH(self->switch_prefs_notifications),
                        purple_blist_node_get_bool (node, "chatty-notifications"));

  gtk_switch_set_state (GTK_SWITCH(self->switch_prefs_status),
                        purple_blist_node_get_bool (node, "chatty-status-msg"));

  // TODO: will be solved by user_list rework
  gtk_box_pack_start (GTK_BOX(self->box_user_list),
                      GTK_WIDGET(self->chatty_conv->muc.treeview),
                      TRUE, TRUE, 0);

  gtk_widget_show (GTK_WIDGET(self->chatty_conv->muc.treeview));
}


static void
chatty_muc_info_dialog_class_init (ChattyMucInfoDialogClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS(klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS(klass);

  object_class->constructed  = chatty_muc_info_dialog_constructed;

  object_class->set_property = chatty_muc_info_dialog_set_property;
  object_class->get_property = chatty_muc_info_dialog_get_property;

  props[PROP_CHATTY_CONV] =
    g_param_spec_pointer ("chatty-conv",
                          "CHATTY_CONVERSATION",
                          "A Chatty conversation",
                          G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY | G_PARAM_STATIC_STRINGS);

  g_object_class_install_properties (object_class, PROP_LAST, props);

  gtk_widget_class_set_template_from_resource (widget_class,
                                               "/sm/puri/chatty/"
                                               "ui/chatty-dialog-muc-info.ui");

  gtk_widget_class_bind_template_child (widget_class, ChattyMucInfoDialog, button_back);
  gtk_widget_class_bind_template_child (widget_class, ChattyMucInfoDialog, button_show_invite_contact);
  gtk_widget_class_bind_template_child (widget_class, ChattyMucInfoDialog, button_invite_contact);
  gtk_widget_class_bind_template_child (widget_class, ChattyMucInfoDialog, button_edit_topic);
  gtk_widget_class_bind_template_child (widget_class, ChattyMucInfoDialog, label_chat_id);
  gtk_widget_class_bind_template_child (widget_class, ChattyMucInfoDialog, label_num_user);
  gtk_widget_class_bind_template_child (widget_class, ChattyMucInfoDialog, label_topic);
  gtk_widget_class_bind_template_child (widget_class, ChattyMucInfoDialog, label_title);
  gtk_widget_class_bind_template_child (widget_class, ChattyMucInfoDialog, switch_prefs_notifications);
  gtk_widget_class_bind_template_child (widget_class, ChattyMucInfoDialog, switch_prefs_status);
  gtk_widget_class_bind_template_child (widget_class, ChattyMucInfoDialog, entry_invite_name);
  gtk_widget_class_bind_template_child (widget_class, ChattyMucInfoDialog, entry_invite_msg);
  gtk_widget_class_bind_template_child (widget_class, ChattyMucInfoDialog, box_topic_frame);
  gtk_widget_class_bind_template_child (widget_class, ChattyMucInfoDialog, box_topic_editor);
  gtk_widget_class_bind_template_child (widget_class, ChattyMucInfoDialog, box_user_list);
  gtk_widget_class_bind_template_child (widget_class, ChattyMucInfoDialog, textview_topic);
  gtk_widget_class_bind_template_child (widget_class, ChattyMucInfoDialog, list_muc_settings);
  gtk_widget_class_bind_template_child (widget_class, ChattyMucInfoDialog, stack_panes_muc_info);
 
  gtk_widget_class_bind_template_callback (widget_class, dialog_delete_cb);
  gtk_widget_class_bind_template_callback (widget_class, button_edit_topic_clicked_cb);
  gtk_widget_class_bind_template_callback (widget_class, button_back_clicked_cb);
  gtk_widget_class_bind_template_callback (widget_class, button_invite_contact_clicked_cb);
  gtk_widget_class_bind_template_callback (widget_class, button_show_invite_contact_clicked_cb);
  gtk_widget_class_bind_template_callback (widget_class, switch_prefs_state_changed_cb);
  gtk_widget_class_bind_template_callback (widget_class, switch_prefs_notify_changed_cb);
  gtk_widget_class_bind_template_callback (widget_class, textview_key_released_cb);
  gtk_widget_class_bind_template_callback (widget_class, invite_name_insert_text_cb);
  gtk_widget_class_bind_template_callback (widget_class, invite_name_delete_text_cb);
}


static void
chatty_muc_info_dialog_init (ChattyMucInfoDialog *self)
{
  gtk_widget_init_template (GTK_WIDGET(self));

  gtk_list_box_set_header_func (GTK_LIST_BOX(self->list_muc_settings),
                                hdy_list_box_separator_header,
                                NULL, NULL);
}


GtkWidget *
chatty_muc_info_dialog_new (GtkWindow *parent_window,
                            gpointer   chatty_conv)
{
  g_return_val_if_fail (chatty_conv != NULL, NULL);

  return g_object_new (CHATTY_TYPE_MUC_INFO_DIALOG,
                       "transient-for", parent_window,
                       "chatty-conv", chatty_conv,
                       "use-header-bar", 1,
                       NULL);
}