#ifndef FUSION_STORY_WORKSPACE_H
#define FUSION_STORY_WORKSPACE_H
#include <gtk/gtk.h>
/* Shared story data editor. No native cutscene runtime exists yet. */
void story_workspace_build(GtkWidget *center,GtkWidget **events_page,GtkWidget **cutscene_page);
#endif
