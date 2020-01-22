/*
 * Copyright (C) 2020 Purism SPC
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */



#define G_LOG_DOMAIN "chatty-new-chat-dialog"

#include <glib.h>
#include <glib/gi18n.h>
#include <glib-object.h>
#include "chatty-window.h"
#include "chatty-dialogs.h"
#include "chatty-pp-account.h"
#include "chatty-buddy-list.h"
#include "chatty-dbus.h"
#include "chatty-utils.h"
#include "chatty-new-chat-dialog.h"


static void chatty_new_chat_dialog_update (ChattyNewChatDialog *self);
static void chatty_entry_contact_name_check (GtkEntry  *entry, GtkWidget *button);


struct _ChattyNewChatDialog
{
  HdyDialog  parent_instance;

  GtkWidget *pane_view_new_chat;
  GtkWidget *search_entry_contacts;
  GtkWidget *grid_edit_contact;
  GtkWidget *button_add_gnome_contact;
  GtkWidget *stack_panes_new_chat;
  GtkWidget *list_select_chat_account;
  GtkWidget *entry_contact_name;
  GtkWidget *entry_contact_alias;
  GtkWidget *button_add_contact;
  GtkWidget *button_back;
  GtkWidget *button_show_add_contact;
  GtkWidget *dummy_prefix_radio;
};


G_DEFINE_TYPE (ChattyNewChatDialog, chatty_new_chat_dialog, HDY_TYPE_DIALOG)


static void
cb_button_new_chat_back_clicked (ChattyNewChatDialog *self)
{
  g_assert (CHATTY_IS_NEW_CHAT_DIALOG (self));

  gtk_stack_set_visible_child_name (GTK_STACK(self->stack_panes_new_chat),
                                    "view-new-chat");
}


static void
cb_button_show_add_contact_clicked (ChattyNewChatDialog *self)
{
  g_assert (CHATTY_IS_NEW_CHAT_DIALOG (self));

  chatty_new_chat_dialog_update (self);

  gtk_stack_set_visible_child_name (GTK_STACK(self->stack_panes_new_chat),
                                    "view-new-contact");
}


static void
cb_button_add_gnome_contact_clicked (ChattyNewChatDialog *self)
{
  g_assert (CHATTY_IS_NEW_CHAT_DIALOG (self));

  chatty_dbus_gc_write_contact ("", "");
   
  gtk_stack_set_visible_child_name (GTK_STACK(self->stack_panes_new_chat),
                                    "view-new-chat");
}


static void
cb_button_add_contact_clicked (ChattyNewChatDialog *self)
{
  char              *who;
  const char        *alias;
  g_autoptr(GError)  err = NULL;

  chatty_data_t     *chatty = chatty_get_data ();

  g_assert (CHATTY_IS_NEW_CHAT_DIALOG (self));

  who = g_strdup (gtk_entry_get_text (GTK_ENTRY(self->entry_contact_name)));
  alias = gtk_entry_get_text (GTK_ENTRY(self->entry_contact_alias));

  chatty_blist_add_buddy (chatty->selected_account, who, alias);

  chatty_conv_im_with_buddy (chatty->selected_account, g_strdup (who));

  gtk_widget_hide (GTK_WIDGET(self));

  g_free (who);

  gtk_entry_set_text (GTK_ENTRY(self->entry_contact_name), "");
  gtk_entry_set_text (GTK_ENTRY(self->entry_contact_alias), "");

  gtk_stack_set_visible_child_name (GTK_STACK(self->stack_panes_new_chat),
                                    "view-new-chat");
}


static void
cb_contact_name_text_changed (ChattyNewChatDialog *self)
{
  chatty_entry_contact_name_check (GTK_ENTRY(self->entry_contact_name), 
                                   self->button_add_contact);
}


static void
cb_account_list_row_activated (GtkListBox    *box,
                               GtkListBoxRow *row,
                               gpointer       user_data)
{
  ChattyPpAccount *pp_account;
  PurpleAccount   *account;
  GtkWidget       *prefix_radio;

  chatty_data_t *chatty = chatty_get_data ();

  pp_account = g_object_get_data (G_OBJECT (row), "row-account");
  prefix_radio = g_object_get_data (G_OBJECT(row), "row-prefix");
  account = chatty_pp_account_get_account (pp_account);

  g_return_if_fail (account != NULL);

  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON(prefix_radio), TRUE);

  chatty->selected_account = account;

  if (chatty_blist_protocol_is_sms (account)) {
    chatty_new_chat_set_edit_mode (CHATTY_NEW_CHAT_DIALOG(chatty->dialog_new_chat), FALSE);
  } else {
    chatty_new_chat_set_edit_mode (CHATTY_NEW_CHAT_DIALOG(chatty->dialog_new_chat), TRUE);
  }
}


static void
chatty_entry_contact_name_check (GtkEntry  *entry,
                                 GtkWidget *button)
{
  PurpleBuddy *buddy;
  const char  *name;

  chatty_data_t *chatty = chatty_get_data ();

  name = gtk_entry_get_text (entry);

  if ((*name != '\0') && chatty->selected_account) {
    buddy = purple_find_buddy (chatty->selected_account, name);
  }

  if ((*name != '\0') && !buddy) {
    gtk_widget_set_sensitive (button, TRUE);
  } else {
    gtk_widget_set_sensitive (button, FALSE);
  }
}


void
chatty_new_chat_set_edit_mode (ChattyNewChatDialog *self,
                               gboolean             edit)
{
  g_return_if_fail (CHATTY_IS_NEW_CHAT_DIALOG (self));
  
  if (edit) {
    gtk_widget_show (GTK_WIDGET(self->grid_edit_contact));
    gtk_widget_show (GTK_WIDGET(self->button_add_contact));
    gtk_widget_hide (GTK_WIDGET(self->button_add_gnome_contact));
  } else {
    gtk_widget_hide (GTK_WIDGET(self->grid_edit_contact));
    gtk_widget_hide (GTK_WIDGET(self->button_add_contact));
    gtk_widget_show (GTK_WIDGET(self->button_add_gnome_contact));
  }
}



static void
chatty_new_chat_add_account_to_list (ChattyNewChatDialog *self,
                                     ChattyPpAccount     *account)
{
  HdyActionRow   *row;
  const gchar    *protocol_id;
  GtkWidget      *prefix_radio_button;

  row = hdy_action_row_new ();
  g_object_set_data (G_OBJECT(row),
                     "row-account",
                     (gpointer) account);

  protocol_id = chatty_pp_account_get_protocol_id (account);

  // TODO list supported protocols here
  if ((g_strcmp0 (protocol_id, "prpl-jabber")) != 0 &&
      (g_strcmp0 (protocol_id, "prpl-matrix")) != 0 &&
      (g_strcmp0 (protocol_id, "prpl-telegram")) != 0 &&
      (g_strcmp0 (protocol_id, "prpl-delta")) != 0 &&
      (g_strcmp0 (protocol_id, "prpl-threepl")) != 0 &&
      (g_strcmp0 (protocol_id, "prpl-mm-sms")) != 0) {
    return;
  }

  if (chatty_pp_account_get_status (account) == CHATTY_DISCONNECTED) {
    return;
  }

  prefix_radio_button = gtk_radio_button_new_from_widget (GTK_RADIO_BUTTON(self->dummy_prefix_radio));
  gtk_widget_show (GTK_WIDGET(prefix_radio_button));

  gtk_widget_set_sensitive (prefix_radio_button, FALSE);

  g_object_set_data (G_OBJECT(row),
                     "row-prefix",
                     (gpointer)prefix_radio_button);

  hdy_action_row_add_prefix (row, GTK_WIDGET(prefix_radio_button ));
  hdy_action_row_set_title (row, chatty_pp_account_get_username (account));

  gtk_container_add (GTK_CONTAINER(self->list_select_chat_account), GTK_WIDGET(row));

  gtk_widget_show (GTK_WIDGET(row));
}


static void
chatty_new_chat_account_list_clear (GtkWidget *list)
{
  GList *children;
  GList *iter;

  children = gtk_container_get_children (GTK_CONTAINER(list));

  for (iter = children; iter != NULL; iter = g_list_next (iter)) {
    gtk_container_remove (GTK_CONTAINER(list), GTK_WIDGET(iter->data));
  }

  g_list_free (children);
}


static gboolean
chatty_new_chat_populate_account_list (ChattyNewChatDialog *self)
{
  GList         *l;
  gboolean       ret = FALSE;
  HdyActionRow  *row;

  chatty_new_chat_account_list_clear (self->list_select_chat_account);

  for (l = purple_accounts_get_all (); l != NULL; l = l->next) {
    ChattyPpAccount *pp_account;
    ret = TRUE;

    pp_account = chatty_pp_account_find (l->data);

    chatty_new_chat_add_account_to_list (self, pp_account);
  }

  row = HDY_ACTION_ROW(gtk_list_box_get_row_at_index (GTK_LIST_BOX(self->list_select_chat_account), 0));

  if (row) {
    cb_account_list_row_activated (GTK_LIST_BOX(self->list_select_chat_account), 
                                   GTK_LIST_BOX_ROW(row), 
                                   NULL);
  }

  return ret;
}


static void
chatty_new_chat_dialog_update (ChattyNewChatDialog *self)
{
  gtk_entry_set_text (GTK_ENTRY(self->entry_contact_name), "");
  gtk_entry_set_text (GTK_ENTRY(self->entry_contact_alias), "");

  chatty_new_chat_populate_account_list (self);
}


GtkWidget *
chatty_new_chat_get_list_container (ChattyNewChatDialog *self)
{
  g_return_val_if_fail (CHATTY_IS_NEW_CHAT_DIALOG (self), NULL);
  
  return self->pane_view_new_chat;
}


GtkWidget *
chatty_new_chat_get_search_entry (ChattyNewChatDialog *self)
{
  g_return_val_if_fail (CHATTY_IS_NEW_CHAT_DIALOG (self), NULL);

  return self->search_entry_contacts;
}


static void
chatty_new_chat_dialog_constructed (GObject *object)
{
  G_OBJECT_CLASS (chatty_new_chat_dialog_parent_class)->constructed (object);
}

static void
chatty_new_chat_dialog_finalize (GObject *object)
{
  G_OBJECT_CLASS (chatty_new_chat_dialog_parent_class)->finalize (object);
}

static void
chatty_new_chat_dialog_class_init (ChattyNewChatDialogClass *klass)
{
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);
  GObjectClass   *object_class = G_OBJECT_CLASS (klass);

  object_class->constructed  = chatty_new_chat_dialog_constructed;
  object_class->finalize = chatty_new_chat_dialog_finalize;

  gtk_widget_class_set_template_from_resource (widget_class,
                                               "/sm/puri/chatty/"
                                               "ui/chatty-dialog-new-chat.ui");

  gtk_widget_class_bind_template_child (widget_class, ChattyNewChatDialog, pane_view_new_chat);
  gtk_widget_class_bind_template_child (widget_class, ChattyNewChatDialog, search_entry_contacts);
  gtk_widget_class_bind_template_child (widget_class, ChattyNewChatDialog, grid_edit_contact);
  gtk_widget_class_bind_template_child (widget_class, ChattyNewChatDialog, button_add_gnome_contact);
  gtk_widget_class_bind_template_child (widget_class, ChattyNewChatDialog, stack_panes_new_chat);
  gtk_widget_class_bind_template_child (widget_class, ChattyNewChatDialog, list_select_chat_account);
  gtk_widget_class_bind_template_child (widget_class, ChattyNewChatDialog, entry_contact_name);
  gtk_widget_class_bind_template_child (widget_class, ChattyNewChatDialog, entry_contact_alias);
  gtk_widget_class_bind_template_child (widget_class, ChattyNewChatDialog, button_add_contact);
  gtk_widget_class_bind_template_child (widget_class, ChattyNewChatDialog, button_back);
  gtk_widget_class_bind_template_child (widget_class, ChattyNewChatDialog, button_show_add_contact);

  gtk_widget_class_bind_template_callback (widget_class, cb_button_new_chat_back_clicked);
  gtk_widget_class_bind_template_callback (widget_class, cb_button_show_add_contact_clicked);
  gtk_widget_class_bind_template_callback (widget_class, cb_button_add_contact_clicked);
  gtk_widget_class_bind_template_callback (widget_class, cb_button_add_gnome_contact_clicked);
  gtk_widget_class_bind_template_callback (widget_class, cb_contact_name_text_changed);
  gtk_widget_class_bind_template_callback (widget_class, cb_account_list_row_activated);
}


static void
chatty_new_chat_dialog_init (ChattyNewChatDialog *self)
{
  gtk_widget_init_template (GTK_WIDGET (self));

  gtk_list_box_set_header_func (GTK_LIST_BOX(self->list_select_chat_account),
                                hdy_list_box_separator_header,
                                NULL, NULL);

  self->dummy_prefix_radio = gtk_radio_button_new_from_widget (GTK_RADIO_BUTTON (NULL));
}


GtkWidget *
chatty_new_chat_dialog_new (GtkWindow *parent_window)
{
  g_return_val_if_fail (GTK_IS_WINDOW (parent_window), NULL);

  return g_object_new (CHATTY_TYPE_NEW_CHAT_DIALOG,
                       "transient-for", parent_window,
                       "use-header-bar", 1,
                       NULL);
}
