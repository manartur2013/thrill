//========= Copyright © 1996-2002, Valve LLC, All rights reserved. ============
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================

// Triangle rendering, if any

#include "hud.h"
#include "cl_util.h"
#include "com_model.h"

// Triangle rendering apis are in gEngfuncs.pTriAPI

#include "const.h"
#include "entity_state.h"
#include "cl_entity.h"
#include "triangleapi.h"
#include "Exports.h"

#include "particleman.h"
#include "tri.h"
extern IParticleMan *g_pParticleMan;

model_s *TRI_pModel;

#define TRI_HUD

#if defined( TRI_HUD )

/*
================
R_GetSpriteFrame

assume pModel is valid
================
*/
mspriteframe_t *R_GetSpriteFrame( model_t *pModel, int frame, float yaw )
{
	msprite_t		*psprite;
	mspritegroup_t	*pspritegroup;
	mspriteframe_t	*pspriteframe = NULL;
	float		*pintervals, fullinterval;
	int		i, numframes;
	float		targettime;

	psprite = (msprite_t*)pModel->cache.data;

	if ( !psprite )
	{
		gEngfuncs.Con_Printf("Sprite:  no pSprite!!!\n");
		return 0;
	}

	if( frame < 0 )
	{
		frame = 0;
	}
	else if( frame >= psprite->numframes )
	{
		if( frame > psprite->numframes )
			gEngfuncs.Con_Printf( "R_GetSpriteFrame: no such frame %d (%s)\n", frame, pModel->name );
		frame = psprite->numframes - 1;
	}

	if( psprite->frames[frame].type == SPR_SINGLE )
	{
		pspriteframe = psprite->frames[frame].frameptr;
	}
	else if( psprite->frames[frame].type == SPR_GROUP ) 
	{
		pspritegroup = (mspritegroup_t *)psprite->frames[frame].frameptr;
		pintervals = pspritegroup->intervals;
		numframes = pspritegroup->numframes;
		fullinterval = pintervals[numframes-1];

		// when loading in Mod_LoadSpriteGroup, we guaranteed all interval values
		// are positive, so we don't have to worry about division by zero
		targettime = gEngfuncs.GetClientTime() - ((int)( gEngfuncs.GetClientTime() / fullinterval )) * fullinterval;

		for( i = 0; i < (numframes - 1); i++ )
		{
			if( pintervals[i] > targettime )
				break;
		}
		pspriteframe = pspritegroup->frames[i];
	}
	
	return pspriteframe;
}

void TRI_GetSpriteParms( model_t *pSprite, int *frameWidth, int *frameHeight, int *numFrames, int currentFrame )
{
	mspriteframe_t	*pFrame;

	if ( !pSprite || pSprite->type != mod_sprite ) 
		return;

	pFrame = R_GetSpriteFrame( pSprite, currentFrame, 0.0f);

	if( frameWidth ) *frameWidth = pFrame->width;
	if( frameHeight ) *frameHeight = pFrame->height;
	if( numFrames ) *numFrames = pSprite->numframes;
}

void TRI_SprAdjustSize( float *x, float *y, float *w, float *h )
{
	float xscale, yscale;

	if ( !x && !y && !w && !h ) 
	  return;

	// scale for screen sizes
	xscale = ScreenWidth / (float)gHUD.m_iHudScaleWidth;
	yscale = ScreenHeight / (float)gHUD.m_iHudScaleHeight;
	
	if( x ) *x *= xscale;
	if( y ) *y *= yscale;
	if( w ) *w *= xscale;
	if( h ) *h *= yscale;
}

void TRI_SprDrawStretchPic( model_t* pModel, int frame, float x, float y, float w, float h, float s1, float t1, float s2, float t2 )
{
	gEngfuncs.pTriAPI->SpriteTexture( pModel, frame );

	gEngfuncs.pTriAPI->Begin(TRI_QUADS);

	gEngfuncs.pTriAPI->TexCoord2f(s1, t1);
	gEngfuncs.pTriAPI->Vertex3f(x, y, 0);

	gEngfuncs.pTriAPI->TexCoord2f(s2, t1);
	gEngfuncs.pTriAPI->Vertex3f(x + w, y, 0);

	gEngfuncs.pTriAPI->TexCoord2f(s2, t2);
	gEngfuncs.pTriAPI->Vertex3f(x + w, y + h, 0);

	gEngfuncs.pTriAPI->TexCoord2f(s1, t2);
	gEngfuncs.pTriAPI->Vertex3f(x, y + h, 0);

	gEngfuncs.pTriAPI->End();
}

void TRI_SprDrawGeneric( int frame, float x, float y, wrect_t* prc)
{
	float	s1, s2, t1, t2, flScale;
	float width, height;
	int	w, h;

	// assume we get sizes from image
	TRI_GetSpriteParms( TRI_pModel, &w, &h, NULL, frame );

	width = w;
	height = h;

	if (prc)
	{
		wrect_t	rc = *prc;

		if( rc.left <= 0 || rc.left >= width ) rc.left = 0;
		if( rc.top <= 0 || rc.top >= height ) rc.top = 0;
		if( rc.right <= 0 || rc.right > width ) rc.right = width;
		if( rc.bottom <= 0 || rc.bottom > height ) rc.bottom = height;

		if ( gHUD.m_pCvarScale->value )
			flScale = 0.125;
		else
			flScale = 0.0;

		s1 = ((float)rc.left + flScale) / width;
		t1 = ((float)rc.top + flScale) / height;
		s2 = ((float)rc.right - flScale) / width;
		t2 = ((float)rc.bottom - flScale) / height;

		width = rc.right - rc.left;
		height = rc.bottom - rc.top;
	}
	else
	{
		s1 = t1 = 0.0;
		s2 = t2 = 1.0;
	}

	TRI_SprAdjustSize( &x, &y, &width, &height);

	TRI_SprDrawStretchPic( TRI_pModel, frame, x, y, width, height, s1, t1, s2, t2);
}

void TRI_SprDrawAdditive( int frame, int x, int y, wrect_t *prc)
{
	gEngfuncs.pTriAPI->RenderMode(kRenderTransAdd);

	TRI_SprDrawGeneric(frame, x, y, prc);

	gEngfuncs.pTriAPI->RenderMode(kRenderNormal);
}

void TRI_SprSet( HSPRITE spr, int r, int g, int b)
{
	TRI_pModel = (model_s*)gEngfuncs.GetSpritePointer(spr);

	gEngfuncs.pTriAPI->Color4ub(r, g, b, 255);
}

void TRI_FillRGBA( float x, float y, float width, float height, int r, int g, int b, int a)
{
  TRI_SprAdjustSize( &x, &y, &width, &height);

  gEngfuncs.pfnFillRGBA(x, y, width, height, r, g, b, a);

}

void TRI_SprDrawHoles( int frame, int x, int y, wrect_t *prc)
{
	gEngfuncs.pTriAPI->RenderMode( kRenderTransAlpha );

	TRI_SprDrawGeneric(frame, x, y, prc);

	gEngfuncs.pTriAPI->RenderMode( kRenderNormal );
}

#endif


/*
=================
HUD_DrawNormalTriangles

Non-transparent triangles-- add them here
=================
*/
void CL_DLLEXPORT HUD_DrawNormalTriangles( void )
{
//	RecClDrawNormalTriangles();

	gHUD.m_Spectator.DrawOverview();
}

#if defined( _TFC )
void RunEventList( void );
#endif

/*
=================
HUD_DrawTransparentTriangles

Render any triangles with transparent rendermode needs here
=================
*/
void CL_DLLEXPORT HUD_DrawTransparentTriangles( void )
{
//	RecClDrawTransparentTriangles();

#if defined( _TFC )
	RunEventList();
#endif

	if ( g_pParticleMan )
		 g_pParticleMan->Update();
}