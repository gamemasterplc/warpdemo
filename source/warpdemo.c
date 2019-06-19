#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <malloc.h>
#include <math.h>
#include <gccore.h>
#include "display.h"
#include "scene_tpl.h"
#include "scene.h"

static GXTexObj sceneTexObj;
static GXTexObj warp_tex_obj;
static int tex_width;
static int tex_height;
static char *warp_map;

void GX_SetTevIndWarp(u8 tevstage, u8 indtexid, u8 bias_flag, u8 replace_tex, u8 mtxid);
void InitSceneWarp();
void GenWarpMap(float frequency, float amp, float phase);
void DrawWarpedScene();

int main(int argc, char **argv)
{
	Mtx44 projection;
	Mtx mv;
	Mtx tex;
	float phase = 0.5f;
	
	InitDisplay();
	srand(time(NULL));
	InitSceneWarp();
	while(1)
	{
		GX_SetViewport(0, 0, GetRenderWidth(), GetRenderHeight(), 0, 1);
		GX_SetScissor(0, 0, GetRenderWidth(), GetRenderHeight());
		guOrtho(projection, 0, 256, 0, 512, 0, 300);
		GX_LoadProjectionMtx(projection, GX_ORTHOGRAPHIC);
		guMtxIdentity(tex);
		guMtxTrans(mv, 256, 128, 0);
		GX_LoadPosMtxImm(mv, GX_PNMTX0);
		GX_LoadTexMtxImm(tex, GX_TEXMTX0, GX_MTX3x4);
		GenWarpMap(1.0f, 1.0f, phase);
		DrawWarpedScene();
		EndFrame();
		phase += (1/GetFrameRate());
		if(phase >= 6.28)
		{
			phase -= 6.28;
		}
	}
	return 0;
}

void GX_SetTevIndWarp(u8 tevstage, u8 indtexid, u8 bias_flag, u8 replace_tex, u8 mtxid)
{
	u8 wrap = GX_ITW_OFF;
	u8 bias = GX_ITB_NONE;
	if(replace_tex)
	{
		wrap = GX_ITW_0;
	}
	if(bias_flag)
	{
		bias = GX_ITB_STU;
	}
	GX_SetTevIndirect(tevstage, indtexid, GX_ITF_8, bias, mtxid, wrap, wrap, GX_FALSE, GX_FALSE, GX_ITBA_OFF);
}

void InitSceneWarp()
{
	TPLFile sceneTPL;
	TPL_OpenTPLFromMemory(&sceneTPL, (void *)scene_tpl, scene_tpl_size);
	TPL_GetTexture(&sceneTPL, scene, &sceneTexObj);
	GX_InitTexObjLOD(&sceneTexObj, GX_LINEAR, GX_LINEAR, 0, 0, 0, GX_FALSE, GX_FALSE, GX_ANISO_1);
	tex_width = 64;
	tex_height = 32;
	warp_map = memalign(32, tex_width*tex_height*2*sizeof(char));
	GX_InitTexObj(&warp_tex_obj, warp_map, tex_width, tex_height, GX_TF_IA8, GX_CLAMP, GX_CLAMP, GX_FALSE);
	GX_InitTexObjLOD(&warp_tex_obj, GX_LINEAR, GX_LINEAR, 0, 0, 0, GX_FALSE, GX_FALSE, GX_ANISO_1);
}

void GenWarpMap(float frequency, float amp, float phase)
{
	int i;
	int j;
	u8 x_val;
	u8 y_val;
	float dx;
	float dy;
	float dist;
	float inv_dist;
	
	for(i=0; i<tex_height; i++)
	{
		for(j=0; j<tex_width; j++)
		{
			dx = ((float)j/tex_width);
			dy = ((float)j/tex_height);
			if(dx == 0.0f && dy == 0.0f)
			{
				dist = sqrtf((dx*dx)+(dy*dy));
				inv_dist = 1.0f/dist;
				dx *= inv_dist;
				dy *= inv_dist;
			}
			else
			{
				inv_dist = 0.0f;
				dist = 0.0f;
			}
			dist = sinf((dist*frequency)+phase);
			x_val = (u8)(((dx*dist)*amp*127.0f)+128.0f);
			y_val = (u8)(((dy*dist)*amp*127.0f)+128.0f);
			int block = ((tex_width/4)*(i/4))+(j/4);
			int offset = (block*32)+((j&3)*8)+((i&3)*2);
			warp_map[offset] = x_val;
			warp_map[offset+1] = y_val;
		}
	}
	DCFlushRange(warp_map, tex_width*tex_height*2);
}

void DrawWarpedScene()
{
	f32 ind_mtx[2][3] = {{1, 0, 0}, {0, 1, 0}};
	
	GX_InvalidateTexAll();
	GX_SetIndTexMatrix(GX_ITM_0, ind_mtx, 1);
	GX_SetZMode(GX_TRUE, GX_LEQUAL, GX_TRUE);
    GX_SetCullMode(GX_CULL_BACK);
	GX_SetTexCoordGen(GX_TEXCOORD0, GX_TG_MTX2x4, GX_TG_TEX0, GX_TEXMTX0);
	GX_SetNumTevStages(1);
    GX_SetNumIndStages(1);
    GX_SetNumTexGens(1);
    GX_SetNumChans(0);
	GX_LoadTexObj(&sceneTexObj, GX_TEXMAP0);
	GX_LoadTexObj(&warp_tex_obj, GX_TEXMAP1);
	GX_SetIndTexOrder(GX_INDTEXSTAGE0, GX_TEXCOORD0, GX_TEXMAP1);
	GX_SetIndTexCoordScale(GX_INDTEXSTAGE0, GX_ITS_4, GX_ITS_4);
	GX_SetTevIndWarp(GX_TEVSTAGE0, GX_INDTEXSTAGE0, GX_TRUE, GX_FALSE, GX_ITM_0);
	GX_SetTevOrder(GX_TEVSTAGE0, GX_TEXCOORD0, GX_TEXMAP0, GX_COLORNULL);
	GX_SetTevOp(GX_TEVSTAGE0, GX_REPLACE);
	GX_ClearVtxDesc();
	GX_SetVtxDesc(GX_VA_POS, GX_DIRECT);
	GX_SetVtxDesc(GX_VA_TEX0, GX_DIRECT);
	GX_SetVtxAttrFmt(GX_VTXFMT0, GX_VA_POS, GX_POS_XY, GX_F32, 0);
	GX_SetVtxAttrFmt(GX_VTXFMT0, GX_VA_TEX0, GX_TEX_ST, GX_F32, 0);
	GX_Begin(GX_QUADS, GX_VTXFMT0, 4);
	GX_Position2f32(-256, -128);
	GX_TexCoord2f32(0, 0);
	GX_Position2f32(256, -128);
	GX_TexCoord2f32(1, 0);
	GX_Position2f32(256, 128);
	GX_TexCoord2f32(1, 1);
	GX_Position2f32(-256, 128);
	GX_TexCoord2f32(0, 1);
	GX_End();
}