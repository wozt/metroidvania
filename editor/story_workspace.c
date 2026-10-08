/* SPDX-License-Identifier: GPL-3.0-only */
/* Shared world-neutral story/cutscene source editor and three-track event browser.
 * Runtime scene adapters do not exist yet; never imply live engine playback. */
#include "story_workspace.h"
#include <stdio.h>
#include <glib/gstdio.h>
#include <string.h>
#include <unistd.h>

typedef struct {
    GtkWidget *page, *view, *status, *tracks[3], *scene_select;
    const char *source;
    const char *kind;
    gboolean modified, scene_switching;
    guint active_scene;
} StoryTab;

static void set_text(StoryTab *tab, const gchar *value)
{
    GtkTextBuffer *buffer=gtk_text_view_get_buffer(GTK_TEXT_VIEW(tab->view));
    gtk_text_buffer_set_text(buffer,value,-1);
    gtk_text_buffer_set_modified(buffer,FALSE);
    tab->modified=FALSE;
}
static gchar *get_text(StoryTab *tab)
{
    GtkTextBuffer *buffer=gtk_text_view_get_buffer(GTK_TEXT_VIEW(tab->view));
    GtkTextIter first,last;
    gtk_text_buffer_get_bounds(buffer,&first,&last);
    return gtk_text_buffer_get_text(buffer,&first,&last,FALSE);
}
static void source_modified(GtkTextBuffer *buffer,gpointer userdata)
{
    StoryTab *tab=userdata;
    tab->modified=gtk_text_buffer_get_modified(buffer);
    if(tab->modified)gtk_label_set_text(GTK_LABEL(tab->status),
        "Unsaved project-owned source changes. Validate and save before changing tabs.");
}
static char *extract_value(const char *line, const char *name)
{
    const char *p=line;
    while(*p==' '||*p=='\t')++p;
    size_t n=strlen(name);
    if(strncmp(p,name,n)!=0)return NULL;
    p+=n;
    while(*p==' '||*p=='\t')++p;
    if(*p++!='=')return NULL;
    while(*p==' '||*p=='\t')++p;
    if(*p++!='"')return NULL;
    const char *end=strchr(p,'"');
    if(!end || end-p>180)return NULL;
    return g_strndup(p,(gsize)(end-p));
}
static void event_selected(GtkButton *button,gpointer userdata)
{
    StoryTab *tab=userdata;
    const char *id=g_object_get_data(G_OBJECT(button),"mv-event-id");
    GtkTextBuffer *buffer=gtk_text_view_get_buffer(GTK_TEXT_VIEW(tab->view));
    GtkTextIter first,found,end;
    gtk_text_buffer_get_start_iter(buffer,&first);
    gchar *needle=g_strdup_printf("id = \"%s\"",id);
    if(gtk_text_iter_forward_search(&first,needle,GTK_TEXT_SEARCH_TEXT_ONLY,&found,&end,NULL)){
        gtk_text_buffer_select_range(buffer,&found,&end);
        gtk_text_view_scroll_to_iter(GTK_TEXT_VIEW(tab->view),&found,0.12,FALSE,0,0);
    }
    g_free(needle);
}
static void event_card(StoryTab *tab,const char *id,const char *title,const char *track)
{
    guint column=2;
    if(g_strcmp0(track,"metroid")==0)column=0;
    else if(g_strcmp0(track,"castlevania")==0)column=1;
    GtkWidget *button=gtk_button_new_with_label(title&&*title?title:id);
    gtk_widget_set_tooltip_text(button,id);
    gtk_widget_set_hexpand(button,TRUE);
    gtk_widget_add_css_class(button,"flat");
    g_object_set_data_full(G_OBJECT(button),"mv-event-id",g_strdup(id),g_free);
    g_signal_connect(button,"clicked",G_CALLBACK(event_selected),tab);
    gtk_box_append(GTK_BOX(tab->tracks[column]),button);
}
static void rebuild_events(StoryTab *tab)
{
    if(!tab->tracks[0])return;
    for(unsigned i=0;i<3;++i){
        GtkWidget *child;
        while((child=gtk_widget_get_first_child(tab->tracks[i])))
            gtk_box_remove(GTK_BOX(tab->tracks[i]),child);
    }
    gchar *source=get_text(tab);
    gchar **lines=g_strsplit(source,"\n",-1);
    char *id=NULL,*title=NULL,*track=NULL;
    for(guint i=0;lines[i];++i){
        if(g_strcmp0(lines[i],"[[events]]")==0){
            if(id)event_card(tab,id,title,track);
            g_clear_pointer(&id,g_free);
            g_clear_pointer(&title,g_free);
            g_clear_pointer(&track,g_free);
        }else if(!id){id=extract_value(lines[i],"id");}
        else if(!title){title=extract_value(lines[i],"title");}
        if(!track)track=extract_value(lines[i],"track");
    }
    if(id)event_card(tab,id,title,track);
    g_free(id);g_free(title);g_free(track);
    g_strfreev(lines);g_free(source);
}
static void reload_clicked(GtkButton *button,gpointer userdata)
{
    StoryTab *tab=userdata;
    (void)button;
    if(tab->modified){
        gtk_label_set_text(GTK_LABEL(tab->status),
            "Source is modified; save before reloading to avoid losing edits.");
        return;
    }
    gchar *text=NULL;
    if(!g_file_get_contents(tab->source,&text,NULL,NULL)){
        gtk_label_set_text(GTK_LABEL(tab->status),"Project source not found.");return;
    }
    set_text(tab,text);g_free(text);
    rebuild_events(tab);
    gtk_label_set_text(GTK_LABEL(tab->status),"Project source reloaded.");
}
static void scene_changed(GObject *object,GParamSpec *pspec,gpointer userdata)
{
    static const char *const paths[]={
        "data/cutscenes/interzone_first_meeting.toml",
        "data/cutscenes/aria_creaking_skull_portal.toml",
        "data/cutscenes/mzm_deorem_soul_portal.toml"
    };
    StoryTab *tab=userdata;(void)pspec;
    if(tab->scene_switching)return;
    guint selection=gtk_drop_down_get_selected(GTK_DROP_DOWN(object));
    if(selection>=G_N_ELEMENTS(paths))return;
    if(tab->modified){
        tab->scene_switching=TRUE;
        gtk_drop_down_set_selected(GTK_DROP_DOWN(tab->scene_select),tab->active_scene);
        tab->scene_switching=FALSE;
        gtk_label_set_text(GTK_LABEL(tab->status),
            "Unsaved changes: validate and save before switching cutscenes.");
        return;
    }
    tab->active_scene=selection;
    tab->source=paths[selection];
    reload_clicked(NULL,tab);
}
static void validate_save(GtkButton *button,gpointer userdata)
{
    StoryTab *tab=userdata;(void)button;
    gchar *text=get_text(tab);
    gchar *temporary=NULL;
    GError *error=NULL;
    gint fd=g_file_open_tmp("metroidvania-story-XXXXXX",&temporary,&error);
    if(fd<0){gtk_label_set_text(GTK_LABEL(tab->status),error?error->message:"Temp file failed");
        g_clear_error(&error);g_free(text);return;}
    close(fd);
    gboolean valid=g_file_set_contents(temporary,text,-1,&error);
    gchar *stdout_data=NULL,*stderr_data=NULL;
    gint wait=0;
    if(valid){
        gchar *argv[]={"python3","-m","scripts.validate_story_assets","--kind",
            (gchar *)tab->kind,temporary,NULL};
        valid=g_spawn_sync(NULL,argv,NULL,G_SPAWN_SEARCH_PATH,NULL,NULL,
                           &stdout_data,&stderr_data,&wait,&error);
        if(valid)valid=g_spawn_check_wait_status(wait,&error);
    }
    if(valid)valid=g_file_set_contents(tab->source,text,-1,&error);
    if(valid){
        gtk_text_buffer_set_modified(gtk_text_view_get_buffer(GTK_TEXT_VIEW(tab->view)),FALSE);
        tab->modified=FALSE;
        rebuild_events(tab);
        gtk_label_set_text(GTK_LABEL(tab->status),"Validated and saved project data (not a gameplay implementation).");
    }else{
        gchar *message=g_strdup_printf("Validation/save failed: %s%s%s",
            error?error->message:"",stderr_data?" | ":"",stderr_data?stderr_data:"");
        gtk_label_set_text(GTK_LABEL(tab->status),message);
        g_free(message);
    }
    g_free(stdout_data);g_free(stderr_data);g_clear_error(&error);
    g_unlink(temporary);g_free(temporary);g_free(text);
}
static void story_tab_build(GtkWidget *center,const char *name,const char *source,
                            const char *kind,gboolean timeline)
{
    StoryTab *tab=g_new0(StoryTab,1);
    tab->source=source;tab->kind=kind;
    GtkWidget *page=gtk_box_new(GTK_ORIENTATION_VERTICAL,6);
    GtkWidget *bar=gtk_box_new(GTK_ORIENTATION_HORIZONTAL,7);
    GtkWidget *save=gtk_button_new_from_icon_name("document-save-symbolic");
    GtkWidget *reload=gtk_button_new_from_icon_name("view-refresh-symbolic");
    GtkWidget *source_scroll=gtk_scrolled_window_new();
    GtkWidget *source_view=gtk_text_view_new();
    tab->page=page;tab->view=source_view;
    tab->status=gtk_label_new(timeline?
        "Three-track canonical event timeline. Edit the TOML source below; changes are validated before saving.":
        "Shared cutscene commands for both worlds. Engine-specific runtime translation is still pending.");
    gtk_widget_set_tooltip_text(save,"Validate and save project data");
    gtk_widget_set_tooltip_text(reload,"Reload unmodified project file");
    gtk_box_append(GTK_BOX(bar),gtk_label_new(name));
    gtk_box_append(GTK_BOX(bar),save);
    gtk_box_append(GTK_BOX(bar),reload);
    if(!timeline){
        static const char *const names[]={
            "Interzone / first meeting", "Aria / Creaking Skull portal",
            "Metroid / Deorem soul portal", NULL};
        tab->scene_select=gtk_drop_down_new_from_strings(names);
        gtk_box_append(GTK_BOX(bar),tab->scene_select);
        g_signal_connect(tab->scene_select,"notify::selected",G_CALLBACK(scene_changed),tab);
    }
    gtk_box_append(GTK_BOX(page),bar);
    if(timeline){
        GtkWidget *tracks=gtk_box_new(GTK_ORIENTATION_HORIZONTAL,8);
        const char *names[]={"METROID / ZEBES", "CASTLEVANIA / CASTLE", "SHARED / INTERZONE"};
        for(unsigned i=0;i<3;++i){
            GtkWidget *pane=gtk_box_new(GTK_ORIENTATION_VERTICAL,4);
            GtkWidget *scroll=gtk_scrolled_window_new();
            GtkWidget *list=gtk_box_new(GTK_ORIENTATION_VERTICAL,3);
            gtk_box_append(GTK_BOX(pane),gtk_label_new(names[i]));
            gtk_scrolled_window_set_child(GTK_SCROLLED_WINDOW(scroll),list);
            gtk_widget_set_vexpand(scroll,TRUE);
            gtk_widget_set_hexpand(scroll,TRUE);
            gtk_widget_set_size_request(scroll,-1,130);
            gtk_box_append(GTK_BOX(pane),scroll);
            gtk_widget_set_hexpand(pane,TRUE);
            gtk_box_append(GTK_BOX(tracks),pane);
            tab->tracks[i]=list;
        }
        gtk_widget_set_size_request(tracks,-1,170);
        gtk_box_append(GTK_BOX(page),tracks);
    }else{
        GtkWidget *help=gtk_label_new(
            "Common actions: move, animation, dialogue, wait, sound, music, portal, camera, choice, set_flag. "
            "Set world = zero_mission / aria / interzone; adapters will map these to the target native engine later.");
        gtk_label_set_wrap(GTK_LABEL(help),TRUE);
        gtk_label_set_xalign(GTK_LABEL(help),0);
        gtk_box_append(GTK_BOX(page),help);
    }
    gtk_text_view_set_monospace(GTK_TEXT_VIEW(source_view),TRUE);
    gtk_text_view_set_wrap_mode(GTK_TEXT_VIEW(source_view),GTK_WRAP_NONE);
    gtk_scrolled_window_set_child(GTK_SCROLLED_WINDOW(source_scroll),source_view);
    gtk_widget_set_vexpand(source_scroll,TRUE);
    gtk_widget_set_hexpand(source_scroll,TRUE);
    gtk_box_append(GTK_BOX(page),source_scroll);
    gtk_label_set_wrap(GTK_LABEL(tab->status),TRUE);
    gtk_label_set_xalign(GTK_LABEL(tab->status),0);
    gtk_box_append(GTK_BOX(page),tab->status);
    g_signal_connect(save,"clicked",G_CALLBACK(validate_save),tab);
    g_signal_connect(reload,"clicked",G_CALLBACK(reload_clicked),tab);
    g_signal_connect(gtk_text_view_get_buffer(GTK_TEXT_VIEW(source_view)),"modified-changed",
                     G_CALLBACK(source_modified),tab);
    gchar *contents=NULL;
    if(g_file_get_contents(source,&contents,NULL,NULL)){
        set_text(tab,contents);g_free(contents);rebuild_events(tab);
    }else gtk_label_set_text(GTK_LABEL(tab->status),"Missing project story source.");
    g_object_set_data_full(G_OBJECT(page),"mv-story-tab",tab,g_free);
    g_object_set_data(G_OBJECT(page),"mv-world-mode",GINT_TO_POINTER(3));
    gtk_notebook_append_page(GTK_NOTEBOOK(center),page,gtk_label_new(name));
    gtk_notebook_set_tab_reorderable(GTK_NOTEBOOK(center),page,TRUE);
    gtk_notebook_set_tab_detachable(GTK_NOTEBOOK(center),page,TRUE);
}
void story_workspace_build(GtkWidget *center,GtkWidget **events_page,GtkWidget **cutscene_page)
{
    story_tab_build(center,"Event orchestration","data/story/timeline.toml","timeline",TRUE);
    if(events_page)*events_page=gtk_notebook_get_nth_page(GTK_NOTEBOOK(center),
        gtk_notebook_get_n_pages(GTK_NOTEBOOK(center))-1);
    story_tab_build(center,"Cutscene editor","data/cutscenes/interzone_first_meeting.toml","cutscene",FALSE);
    if(cutscene_page)*cutscene_page=gtk_notebook_get_nth_page(GTK_NOTEBOOK(center),
        gtk_notebook_get_n_pages(GTK_NOTEBOOK(center))-1);
}
