/* SPDX-License-Identifier: GPL-3.0-only */
#include "native_workspace.h"
#include "core/native_map.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <cairo.h>

struct NativeWorkspace {
    GtkWidget *page,*palette_page,*canvas,*palette,*status,*layer,*tool,*brush,*zoom;
    NativeMap *map,*before,*undo;
    gboolean ready,dirty,dragging,busy,unsaved;
    unsigned layer_id,tool_id,brush_id;
    int last_x,last_y;
    double scale;
    char *override;
    cairo_surface_t *atlas;
    unsigned char *pixels;
};
static void msg(NativeWorkspace *w,const char *s)
{if(w->status)gtk_label_set_text(GTK_LABEL(w->status),s);}
static void discard_atlas(NativeWorkspace *w)
{if(w->atlas)cairo_surface_destroy(w->atlas);w->atlas=NULL;g_free(w->pixels);w->pixels=NULL;}
static gboolean atlas_load(NativeWorkspace *w,const char *name)
{
    GError *e=NULL;GdkPixbuf *p=gdk_pixbuf_new_from_file(name,&e);
    if(!p){if(e)g_error_free(e);return FALSE;}
    int width=gdk_pixbuf_get_width(p),height=gdk_pixbuf_get_height(p),srcstride=gdk_pixbuf_get_rowstride(p);
    int channels=gdk_pixbuf_get_n_channels(p);
    if(width<16||width>1024||height<16||height>4096||channels<3){g_object_unref(p);return FALSE;}
    int stride=cairo_format_stride_for_width(CAIRO_FORMAT_RGB24,width);
    unsigned char *pixels=g_malloc0((size_t)stride*(size_t)height);
    const guchar *src=gdk_pixbuf_read_pixels(p);
    for(int y=0;y<height;y++)for(int x=0;x<width;x++){
        const guchar *q=src+(size_t)y*(size_t)srcstride+(size_t)x*(size_t)channels;
        uint32_t value=0xff000000u|((uint32_t)q[0]<<16)|((uint32_t)q[1]<<8)|q[2];
        memcpy(pixels+(size_t)y*(size_t)stride+x*4,&value,4);
    }
    g_object_unref(p);
    cairo_surface_t *surface=cairo_image_surface_create_for_data(pixels,CAIRO_FORMAT_RGB24,width,height,stride);
    if(cairo_surface_status(surface)!=CAIRO_STATUS_SUCCESS){cairo_surface_destroy(surface);g_free(pixels);return FALSE;}
    discard_atlas(w);w->atlas=surface;w->pixels=pixels;return TRUE;
}
static void stamp(NativeWorkspace *w,cairo_t *cr,unsigned id,double x,double y,double scale)
{
    if(!w->atlas||!w->ready||id>=w->map->tile_count)return;
    cairo_save(cr);cairo_translate(cr,x,y);cairo_scale(cr,scale,scale);
    cairo_rectangle(cr,0,0,16,16);cairo_clip(cr);
    cairo_set_source_surface(cr,w->atlas,-(int)(id%16)*16,-(int)(id/16)*16);
    cairo_pattern_set_filter(cairo_get_source(cr),CAIRO_FILTER_NEAREST);
    cairo_paint(cr);cairo_restore(cr);
}
static void draw_map(GtkDrawingArea *area,cairo_t *cr,int width,int height,gpointer data)
{
    NativeWorkspace *w=data;(void)area;(void)width;(void)height;
    cairo_set_source_rgb(cr,.12,.14,.16);cairo_paint(cr);
    if(!w->ready)return;
    unsigned l=w->layer_id,bw=w->map->width[l],bh=w->map->height[l];double cell=16*w->scale;
    for(unsigned y=0;y<bh;y++)for(unsigned x=0;x<bw;x++){
        unsigned id=w->map->blocks[l][y*bw+x];
        stamp(w,cr,id,x*cell,y*cell,w->scale);
        if(id>=w->map->tile_count){cairo_set_source_rgba(cr,1,0,.2,.5);
            cairo_rectangle(cr,x*cell,y*cell,cell,cell);cairo_fill(cr);}
    }
    cairo_set_source_rgba(cr,1,1,1,.13);cairo_set_line_width(cr,.5);
    for(unsigned x=0;x<=bw;x++){cairo_move_to(cr,x*cell,0);cairo_line_to(cr,x*cell,bh*cell);}
    for(unsigned y=0;y<=bh;y++){cairo_move_to(cr,0,y*cell);cairo_line_to(cr,bw*cell,y*cell);}
    cairo_stroke(cr);
}
static void draw_palette(GtkDrawingArea *area,cairo_t *cr,int width,int height,gpointer data)
{
    NativeWorkspace *w=data;(void)area;(void)width;(void)height;
    cairo_set_source_rgb(cr,.09,.1,.12);cairo_paint(cr);
    if(!w->ready)return;
    for(unsigned i=0;i<w->map->tile_count;i++)stamp(w,cr,i,(i%16)*24,(i/16)*24,1.5);
    cairo_set_source_rgb(cr,1,.8,.24);cairo_set_line_width(cr,2);
    cairo_rectangle(cr,(w->brush_id%16)*24+1,(w->brush_id/16)*24+1,22,22);cairo_stroke(cr);
}
static void resize_canvas(NativeWorkspace *w)
{
    if(!w->ready)return;
    gtk_drawing_area_set_content_width(GTK_DRAWING_AREA(w->canvas),(int)(w->map->width[w->layer_id]*16*w->scale));
    gtk_drawing_area_set_content_height(GTK_DRAWING_AREA(w->canvas),(int)(w->map->height[w->layer_id]*16*w->scale));
    gtk_widget_queue_draw(w->canvas);
}
static void brush_changed(GtkSpinButton *b,gpointer data)
{
    NativeWorkspace *w=data;w->brush_id=(unsigned)gtk_spin_button_get_value_as_int(b);
    gtk_widget_queue_draw(w->palette);
}
static void layer_changed(GObject *obj,GParamSpec *pspec,gpointer data)
{NativeWorkspace *w=data;(void)pspec;w->layer_id=gtk_drop_down_get_selected(GTK_DROP_DOWN(obj));resize_canvas(w);}
static void tool_changed(GObject *obj,GParamSpec *pspec,gpointer data)
{NativeWorkspace *w=data;(void)pspec;w->tool_id=gtk_drop_down_get_selected(GTK_DROP_DOWN(obj));}
static void zoom_changed(GtkSpinButton *b,gpointer data)
{NativeWorkspace *w=data;w->scale=gtk_spin_button_get_value(b)/100.0;resize_canvas(w);}
static void apply_at(NativeWorkspace *w,int x,int y,gboolean continuing)
{
    if(!w->ready||x<0||y<0||(unsigned)x>=w->map->width[w->layer_id]||
       (unsigned)y>=w->map->height[w->layer_id]||(continuing&&w->tool_id>1))return;
    unsigned pick=w->brush_id;
    if(native_map_edit(w->map,w->layer_id,x,y,w->tool_id,w->brush_id,&pick)){
        w->dirty=TRUE;w->unsaved=TRUE;gtk_widget_queue_draw(w->canvas);msg(w,"Unsaved native-room changes");
    }
    if(w->tool_id==3&&pick<w->map->tile_count)
        gtk_spin_button_set_value(GTK_SPIN_BUTTON(w->brush),pick);
}
static void line_to(NativeWorkspace *w,int x,int y)
{
    if(w->last_x<0){apply_at(w,x,y,FALSE);w->last_x=x;w->last_y=y;return;}
    int x0=w->last_x,y0=w->last_y,dx=abs(x-x0),dy=-abs(y-y0),sx=x0<x?1:-1,sy=y0<y?1:-1,error=dx+dy;
    for(;;){apply_at(w,x0,y0,TRUE);if(x0==x&&y0==y)break;
        int e=error*2;if(e>=dy){error+=dy;x0+=sx;}if(e<=dx){error+=dx;y0+=sy;}}
    w->last_x=x;w->last_y=y;
}
static void canvas_down(GtkGestureClick *g,int n,double x,double y,gpointer data)
{
    NativeWorkspace *w=data;(void)g;(void)n;
    if(!w->ready||w->dragging||x<0||y<0)return;
    int bx=(int)(x/(16*w->scale)),by=(int)(y/(16*w->scale));
    if(bx<0||by<0||(unsigned)bx>=w->map->width[w->layer_id]||
       (unsigned)by>=w->map->height[w->layer_id])return;
    *w->before=*w->map;w->dragging=TRUE;w->dirty=FALSE;w->last_x=w->last_y=-1;
    line_to(w,bx,by);
}
static void canvas_motion(GtkEventControllerMotion *g,double x,double y,gpointer data)
{
    NativeWorkspace *w=data;(void)g;
    if(!w->dragging||w->tool_id>1||x<0||y<0)return;
    int bx=(int)(x/(16*w->scale)),by=(int)(y/(16*w->scale));
    if(bx<0||by<0||(unsigned)bx>=w->map->width[w->layer_id]||
       (unsigned)by>=w->map->height[w->layer_id])return;
    if(bx!=w->last_x||by!=w->last_y)line_to(w,bx,by);
}
static void canvas_up(GtkGestureClick *g,int n,double x,double y,gpointer data)
{
    NativeWorkspace *w=data;(void)g;(void)n;(void)x;(void)y;
    if(w->dragging&&w->dirty)*w->undo=*w->before;
    w->dragging=FALSE;
}
static void undo_clicked(GtkButton *b,gpointer data)
{
    NativeWorkspace *w=data;(void)b;
    if(!w->ready||w->dragging||w->undo->tile_count==0)return;
    NativeMap tmp=*w->undo;*w->undo=*w->map;*w->map=tmp;
    w->unsaved=TRUE;gtk_widget_queue_draw(w->canvas);msg(w,"Unsaved changes (undo / redo toggle)");
}
static void palette_click(GtkGestureClick *g,int n,double x,double y,gpointer data)
{
    NativeWorkspace *w=data;(void)g;(void)n;
    if(!w->ready||x<0||y<0)return;
    unsigned tx=(unsigned)(x/24),ty=(unsigned)(y/24),id=ty*16+tx;
    if(tx<16&&id<w->map->tile_count)gtk_spin_button_set_value(GTK_SPIN_BUTTON(w->brush),id);
}
static void save_clicked(GtkButton *b,gpointer data)
{
    NativeWorkspace *w=data;(void)b;char error[160]={0};
    if(!w->ready||!w->override)return;
    if(native_map_save(w->map,w->override,error,sizeof(error))){w->unsaved=FALSE;msg(w,"Saved private override. ROM/source unchanged.");}
    else msg(w,error);
}
static GtkWidget *item(const char *text,GtkWidget *control)
{
    GtkWidget *box=gtk_box_new(GTK_ORIENTATION_HORIZONTAL,4);
    gtk_box_append(GTK_BOX(box),gtk_label_new(text));gtk_box_append(GTK_BOX(box),control);
    return box;
}
NativeWorkspace *native_workspace_new(void)
{
    NativeWorkspace *w=calloc(1,sizeof(*w));if(!w)return NULL;
    w->map=calloc(1,sizeof(NativeMap));w->before=calloc(1,sizeof(NativeMap));w->undo=calloc(1,sizeof(NativeMap));
    if(!w->map||!w->before||!w->undo){native_workspace_free(w);return NULL;}
    w->scale=2.;w->last_x=-1;w->last_y=-1;return w;
}
void native_workspace_free(NativeWorkspace *w)
{
    if(!w)return;discard_atlas(w);g_free(w->override);
    free(w->map);free(w->before);free(w->undo);free(w);
}
void native_workspace_build(NativeWorkspace *w,GtkWidget *center,GtkWidget *right)
{
    static const char *const layers[]={"BG1","BG2",NULL};
    static const char *const tools[]={"Pencil","Eraser","Fill","Picker",NULL};
    GtkWidget *root=gtk_box_new(GTK_ORIENTATION_VERTICAL,5);
    GtkWidget *flow=gtk_flow_box_new();GtkWidget *scroll=gtk_scrolled_window_new();
    GtkWidget *pal_scroll=gtk_scrolled_window_new();
    GtkWidget *undo=gtk_button_new_with_label("Undo / Redo"),*save=gtk_button_new_with_label("Save override");
    w->layer=gtk_drop_down_new_from_strings(layers);w->tool=gtk_drop_down_new_from_strings(tools);
    w->brush=gtk_spin_button_new_with_range(0,1023,1);
    w->zoom=gtk_spin_button_new_with_range(50,400,25);
    gtk_spin_button_set_value(GTK_SPIN_BUTTON(w->zoom),200);
    gtk_flow_box_set_selection_mode(GTK_FLOW_BOX(flow),GTK_SELECTION_NONE);
    gtk_flow_box_set_max_children_per_line(GTK_FLOW_BOX(flow),12);
    gtk_flow_box_set_column_spacing(GTK_FLOW_BOX(flow),5);
    gtk_flow_box_insert(GTK_FLOW_BOX(flow),item("Layer",w->layer),-1);
    gtk_flow_box_insert(GTK_FLOW_BOX(flow),item("Tool",w->tool),-1);
    gtk_flow_box_insert(GTK_FLOW_BOX(flow),item("Brush",w->brush),-1);
    gtk_flow_box_insert(GTK_FLOW_BOX(flow),item("Zoom %",w->zoom),-1);
    gtk_flow_box_insert(GTK_FLOW_BOX(flow),undo,-1);
    gtk_flow_box_insert(GTK_FLOW_BOX(flow),save,-1);
    gtk_box_append(GTK_BOX(root),flow);
    w->canvas=gtk_drawing_area_new();
    gtk_drawing_area_set_content_width(GTK_DRAWING_AREA(w->canvas),480);
    gtk_drawing_area_set_content_height(GTK_DRAWING_AREA(w->canvas),270);
    gtk_drawing_area_set_draw_func(GTK_DRAWING_AREA(w->canvas),draw_map,w,NULL);
    GtkGesture *click=gtk_gesture_click_new();GtkEventController *motion=gtk_event_controller_motion_new();
    gtk_gesture_single_set_button(GTK_GESTURE_SINGLE(click),GDK_BUTTON_PRIMARY);
    gtk_widget_add_controller(w->canvas,GTK_EVENT_CONTROLLER(click));
    gtk_widget_add_controller(w->canvas,motion);
    g_signal_connect(click,"pressed",G_CALLBACK(canvas_down),w);
    g_signal_connect(click,"released",G_CALLBACK(canvas_up),w);
    g_signal_connect(motion,"motion",G_CALLBACK(canvas_motion),w);
    gtk_scrolled_window_set_child(GTK_SCROLLED_WINDOW(scroll),w->canvas);
    gtk_widget_set_hexpand(scroll,TRUE);gtk_widget_set_vexpand(scroll,TRUE);
    gtk_box_append(GTK_BOX(root),scroll);
    w->status=gtk_label_new("Double-click an original MZM room in Native rooms to import an editable copy.");
    gtk_label_set_xalign(GTK_LABEL(w->status),0);
    gtk_label_set_wrap(GTK_LABEL(w->status),TRUE);
    gtk_box_append(GTK_BOX(root),w->status);
    w->page=root;
    gtk_notebook_append_page(GTK_NOTEBOOK(center),root,gtk_label_new("Native tile painter"));
    w->palette=gtk_drawing_area_new();
    gtk_drawing_area_set_content_width(GTK_DRAWING_AREA(w->palette),384);
    gtk_drawing_area_set_content_height(GTK_DRAWING_AREA(w->palette),1536);
    gtk_drawing_area_set_draw_func(GTK_DRAWING_AREA(w->palette),draw_palette,w,NULL);
    GtkGesture *pick=gtk_gesture_click_new();
    gtk_widget_add_controller(w->palette,GTK_EVENT_CONTROLLER(pick));
    g_signal_connect(pick,"pressed",G_CALLBACK(palette_click),w);
    gtk_scrolled_window_set_child(GTK_SCROLLED_WINDOW(pal_scroll),w->palette);
    gtk_widget_set_hexpand(pal_scroll,TRUE);gtk_widget_set_vexpand(pal_scroll,TRUE);
    w->palette_page=pal_scroll;
    gtk_notebook_append_page(GTK_NOTEBOOK(right),pal_scroll,gtk_label_new("Native metatiles"));
    g_signal_connect(w->layer,"notify::selected",G_CALLBACK(layer_changed),w);
    g_signal_connect(w->tool,"notify::selected",G_CALLBACK(tool_changed),w);
    g_signal_connect(w->brush,"value-changed",G_CALLBACK(brush_changed),w);
    g_signal_connect(w->zoom,"value-changed",G_CALLBACK(zoom_changed),w);
    g_signal_connect(undo,"clicked",G_CALLBACK(undo_clicked),w);
    g_signal_connect(save,"clicked",G_CALLBACK(save_clicked),w);
}
static void open_room(NativeWorkspace *w,const char *area,unsigned room)
{
    char area_lower[32],base[300],over[300],atlas[400],error[180]={0};
    size_t n=strlen(area);if(n>=sizeof(area_lower))return;
    for(size_t i=0;i<n;i++)area_lower[i]=(char)g_ascii_tolower(area[i]);area_lower[n]=0;
    snprintf(base,sizeof(base),"assets/extracted/rooms/metroid/workrooms/%s_%03u.mvnative",area_lower,room);
    snprintf(over,sizeof(over),"assets/extracted/overrides/metroid/%s_%03u.mvnative",area_lower,room);
    NativeMap *temporary=calloc(1,sizeof(NativeMap));if(!temporary){msg(w,"Out of memory");return;}
    const char *source=g_file_test(over,G_FILE_TEST_IS_REGULAR)?over:base;
    if(!native_map_load(temporary,source,error,sizeof(error))){msg(w,error);free(temporary);return;}
    snprintf(atlas,sizeof(atlas),"assets/extracted/%s",temporary->atlas);
    if(!atlas_load(w,atlas)){msg(w,"Could not load original metatile atlas");free(temporary);return;}
    *w->map=*temporary;free(temporary);memset(w->undo,0,sizeof(NativeMap));
    g_free(w->override);w->override=g_strdup(over);
    char *folder=g_path_get_dirname(over);g_mkdir_with_parents(folder,0700);g_free(folder);
    w->ready=TRUE;w->unsaved=FALSE;w->layer_id=w->brush_id=0;
    gtk_spin_button_set_range(GTK_SPIN_BUTTON(w->brush),0,w->map->tile_count-1);
    gtk_spin_button_set_value(GTK_SPIN_BUTTON(w->brush),0);
    gtk_drop_down_set_selected(GTK_DROP_DOWN(w->layer),0);
    gtk_drawing_area_set_content_height(GTK_DRAWING_AREA(w->palette),
                                        (int)((w->map->tile_count+15)/16*24));
    resize_canvas(w);gtk_widget_queue_draw(w->palette);
    gchar *text=g_strdup_printf("Editing %s | %u real metatiles | %s. ROM files untouched.",
                               w->map->room_id,w->map->tile_count,
                               source==over?"previous override restored":"new private working copy");
    msg(w,text);g_free(text);
    GtkWidget *parent=gtk_widget_get_parent(w->page);
    if(GTK_IS_NOTEBOOK(parent))gtk_notebook_set_current_page(GTK_NOTEBOOK(parent),
                                   gtk_notebook_page_num(GTK_NOTEBOOK(parent),w->page));
    parent=gtk_widget_get_parent(w->palette_page);
    if(GTK_IS_NOTEBOOK(parent))gtk_notebook_set_current_page(GTK_NOTEBOOK(parent),
                                   gtk_notebook_page_num(GTK_NOTEBOOK(parent),w->palette_page));
}
static void imported(GObject *object,GAsyncResult *result,gpointer data)
{
    NativeWorkspace *w=data;GSubprocess *p=G_SUBPROCESS(object);
    GError *e=NULL;gchar *out=NULL,*err=NULL;
    gboolean ok=g_subprocess_communicate_utf8_finish(p,result,&out,&err,&e);
    w->busy=FALSE;
    if(ok&&g_subprocess_get_successful(p)){
        const char *area=g_object_get_data(G_OBJECT(p),"area");
        unsigned room=GPOINTER_TO_UINT(g_object_get_data(G_OBJECT(p),"room"));
        open_room(w,area,room);
    }else msg(w,e?e->message:err?err:"Room import failed");
    if(e)g_error_free(e);g_free(out);g_free(err);
}
void native_workspace_import_async(NativeWorkspace *w,const char *area,unsigned room)
{
    if(!w||!w->status||w->busy||!area||!area[0]||strlen(area)>23||room>999)return;
    if(w->unsaved){msg(w,"Save the current native override before opening another room.");return;}
    for(const char *p=area;*p;p++)if(!g_ascii_isalnum(*p))return;
    char number[16];snprintf(number,sizeof(number),"%u",room);
    GError *e=NULL;
    GSubprocess *proc=g_subprocess_new(G_SUBPROCESS_FLAGS_STDOUT_PIPE|G_SUBPROCESS_FLAGS_STDERR_PIPE,
                                      &e,"python3","-m","scripts.mzm_native_workspace",
                                      "--area",area,"--room",number,NULL);
    if(!proc){msg(w,e?e->message:"Python importer unavailable");if(e)g_error_free(e);return;}
    w->busy=TRUE;msg(w,"Importing verified MZM metatiles without blocking the UI...");
    g_object_set_data_full(G_OBJECT(proc),"area",g_strdup(area),g_free);
    g_object_set_data(G_OBJECT(proc),"room",GUINT_TO_POINTER(room));
    g_subprocess_communicate_utf8_async(proc,NULL,NULL,imported,w);
    g_object_unref(proc);
}
