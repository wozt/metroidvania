/* SPDX-License-Identifier: GPL-3.0-only */
#include "core/native_map.h"
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void err(char *out,size_t n,const char *message)
{if(out&&n)snprintf(out,n,"%s",message);}
static bool room_ok(const char *s)
{
    if(strncmp(s,"mzm:",4) && strncmp(s,"aria:",5))return false;
    for(;*s;s++)if(!((*s>='a'&&*s<='z')||(*s>='0'&&*s<='9')||*s==':'||*s=='_'))return false;
    return true;
}
static bool atlas_ok(const char *s)
{
    if(strncmp(s,"rooms/metroid/tilesets/",strlen("rooms/metroid/tilesets/")) &&
       strncmp(s,"rooms/aria/tilesets/",strlen("rooms/aria/tilesets/")))return false;
    size_t n=strlen(s);
    if(strstr(s,"..")||strchr(s,'\\')||strchr(s,' ')||
       !((n>=10&&!strcmp(s+n-10,"_atlas.bmp")) ||
         (n>=10&&!strcmp(s+n-10,"_atlas.png"))))return false;
    for(;*s;s++)if(!(isalnum((unsigned char)*s)||*s=='/'||*s=='_'||*s=='.'))return false;
    return true;
}
static bool valid(const NativeMap *m)
{
    if(!m||!room_ok(m->room_id)||!atlas_ok(m->atlas)||m->tileset>78||
       !m->tile_count||m->tile_count>NATIVE_MAX_TILES)return false;
    for(unsigned l=0;l<2;l++)if(!m->width[l]||m->width[l]>128||
        !m->height[l]||m->height[l]>128||m->width[l]*m->height[l]>NATIVE_MAX_CELLS)return false;
    return true;
}
static bool parse_header(FILE *f,const char *prefix,char *dst,size_t size)
{
    char line[512];size_t p=strlen(prefix),n;
    if(!fgets(line,sizeof(line),f)||strncmp(line,prefix,p))return false;
    n=strcspn(line+p,"\r\n");
    if(!n||n>=size||line[p+n]!='\n'||line[p+n+1]!='\0')return false;
    memcpy(dst,line+p,n);dst[n]=0;return true;
}
bool native_map_load(NativeMap *out,const char *path,char *error,size_t length)
{
    FILE *f;NativeMap *m;char str[192];char line[512];bool ok=false;
    if(!out||!path){err(error,length,"invalid arguments");return false;}
    f=fopen(path,"r");if(!f){err(error,length,"cannot open native workspace");return false;}
    m=calloc(1,sizeof(*m));if(!m)goto end;
    if(!fgets(line,sizeof(line),f)||strcmp(line,"MVNATIVE 1\n"))goto end;
    if(!parse_header(f,"ROOM ",m->room_id,sizeof(m->room_id))||
       !parse_header(f,"TILESET ",str,sizeof(str)))goto end;
    {char extra;int n;if(sscanf(str,"%d %c",&n,&extra)!=1||n<0||n>78)goto end;
     m->tileset=(unsigned)n;}
    if(!parse_header(f,"TILES ",str,sizeof(str)))goto end;
    {char extra;int n;if(sscanf(str,"%d %c",&n,&extra)!=1||n<=0||n>1024)goto end;
     m->tile_count=(unsigned)n;}
    if(!parse_header(f,"ATLAS ",m->atlas,sizeof(m->atlas)))goto end;
    for(unsigned l=0;l<2;l++){
        unsigned w,h;char name[16],extra;
        if(!fgets(line,sizeof(line),f)||
           sscanf(line,"LAYER %15s %u %u %c",name,&w,&h,&extra)!=3||
           strcmp(name,l?"BG2":"BG1")||!w||w>128||!h||h>128||w*h>NATIVE_MAX_CELLS)goto end;
        m->width[l]=w;m->height[l]=h;
        for(unsigned y=0;y<h;y++){
            char *row=malloc(w*5+4);if(!row)goto end;
            bool good=fgets(row,(int)(w*5+4),f)!=NULL;
            if(good){char *p=row,*ep;
                for(unsigned x=0;x<w;x++){
                    if(strlen(p)<4){good=false;break;}
                    for(unsigned k=0;k<4;k++)if(!isxdigit((unsigned char)p[k])||islower((unsigned char)p[k])){good=false;break;}
                    if(!good)break;
                    unsigned long v=strtoul(p,&ep,16);
                    if(ep!=p+4||v>65535){good=false;break;}
                    m->blocks[l][y*w+x]=(uint16_t)v;p=ep;
                    if(x+1<w){if(*p!=' '){good=false;break;}p++;}
                }
                if(good&&strcmp(p,"\n")&&strcmp(p,"\r\n"))good=false;
            }
            free(row);if(!good)goto end;
        }
    }
    if(!fgets(line,sizeof(line),f)||strcmp(line,"END\n")||fgetc(f)!=EOF||!valid(m))goto end;
    *out=*m;ok=true;
end:
    free(m);fclose(f);err(error,length,ok?"":"invalid MVNATIVE workspace");return ok;
}
bool native_map_save(const NativeMap *m,const char *path,char *error,size_t length)
{
    char tmp[512];FILE *f;int failed=0;
    if(!valid(m)||!path||snprintf(tmp,sizeof(tmp),"%s.tmp",path)>=(int)sizeof(tmp)){
        err(error,length,"invalid save path or native map");return false;}
    f=fopen(tmp,"w");if(!f){err(error,length,"cannot create override");return false;}
    fprintf(f,"MVNATIVE 1\nROOM %s\nTILESET %u\nTILES %u\nATLAS %s\n",
            m->room_id,m->tileset,m->tile_count,m->atlas);
    for(unsigned l=0;l<2;l++){
        fprintf(f,"LAYER BG%u %u %u\n",l+1,m->width[l],m->height[l]);
        for(unsigned y=0;y<m->height[l];y++){
            for(unsigned x=0;x<m->width[l];x++)
                fprintf(f,"%s%04X",x?" ":"",m->blocks[l][y*m->width[l]+x]);
            fputc('\n',f);
        }
    }
    fputs("END\n",f);failed=ferror(f);if(fclose(f))failed=1;
    if(failed||rename(tmp,path)){
        remove(tmp);err(error,length,"native map override write failed");return false;}
    err(error,length,"");return true;
}
bool native_map_edit(NativeMap *m,unsigned layer,int x,int y,
                     unsigned tool,unsigned brush,unsigned *picked)
{
    unsigned w,h;uint16_t *cells;int queue[NATIVE_MAX_CELLS];unsigned head=0,tail=0;
    if(!valid(m)||layer>=2||tool>3||brush>=m->tile_count)return false;
    w=m->width[layer];h=m->height[layer];
    if(x<0||y<0||(unsigned)x>=w||(unsigned)y>=h)return false;
    cells=m->blocks[layer];unsigned idx=(unsigned)y*w+(unsigned)x;
    if(tool==3){if(picked)*picked=cells[idx];return false;}
    if(tool==1)brush=0;
    if(cells[idx]==brush)return false;
    if(tool!=2){cells[idx]=(uint16_t)brush;return true;}
    uint16_t old=cells[idx];cells[idx]=(uint16_t)brush;queue[tail++]=(int)idx;
    while(head<tail){unsigned pos=(unsigned)queue[head++],cx=pos%w,cy=pos/w;
        unsigned neighbors[4],count=0;
        if(cx)neighbors[count++]=pos-1;
        if(cx+1<w)neighbors[count++]=pos+1;
        if(cy)neighbors[count++]=pos-w;
        if(cy+1<h)neighbors[count++]=pos+w;
        for(unsigned i=0;i<count;i++)if(cells[neighbors[i]]==old){
            cells[neighbors[i]]=(uint16_t)brush;
            queue[tail++]=(int)neighbors[i];
        }
    }
    return true;
}
