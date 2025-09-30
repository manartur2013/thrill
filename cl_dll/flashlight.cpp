/***
*
*	Copyright (c) 1996-2002, Valve LLC. All rights reserved.
*	
*	This product contains software technology licensed from Id 
*	Software, Inc. ("Id Technology").  Id Technology (c) 1996 Id Software, Inc. 
*	All Rights Reserved.
*
*   Use, distribution, and modification of this source code and/or resulting
*   object code is restricted to non-commercial enhancements to products from
*   Valve LLC.  All other use, distribution, or modification is prohibited
*   without written permission from Valve LLC.
*
****/
//
// flashlight.cpp
//
// implementation of CHudFlashlight class
//

#include "hud.h"
#include "cl_util.h"
#include "parsemsg.h"

#include <string.h>
#include <stdio.h>



DECLARE_MESSAGE(m_Flash, FlashBat)
DECLARE_MESSAGE(m_Flash, Flashlight)

#define BAT_NAME "sprites/%d_Flashlight.spr"

int CHudFlashlight::Init(void)
{
	m_fFade = 0;
	m_fOn = 0;

	HOOK_MESSAGE(Flashlight);
	HOOK_MESSAGE(FlashBat);

	m_iFlags |= HUD_ACTIVE;

	gHUD.AddHudElem(this);

	return 1;
};

void CHudFlashlight::Reset(void)
{
	m_fFade = 0;
	m_fOn = 0;
}

int CHudFlashlight::VidInit(void)
{
	int HUD_flash_on = gHUD.GetSpriteIndex( "flash_on" );
	int HUD_flash_off = gHUD.GetSpriteIndex( "flash_off" );

	m_hSpriteOn = gHUD.GetSprite(HUD_flash_on);
	m_hSpriteOff = gHUD.GetSprite(HUD_flash_off);
	m_prcOn = &gHUD.GetSpriteRect(HUD_flash_on);
	m_prcOff = &gHUD.GetSpriteRect(HUD_flash_off);
	m_iWidth = m_prcOn->right - m_prcOn->left;

	return 1;
};

int CHudFlashlight:: MsgFunc_FlashBat(const char *pszName,  int iSize, void *pbuf )
{

	
	BEGIN_READ( pbuf, iSize );
	int x = READ_BYTE();
	m_iBat = x;
	m_flBat = ((float)x)/100.0;

	return 1;
}

int CHudFlashlight:: MsgFunc_Flashlight(const char *pszName,  int iSize, void *pbuf )
{

	BEGIN_READ( pbuf, iSize );
	m_fOn = READ_BYTE();
	int x = READ_BYTE();
	m_iBat = x;
	m_flBat = ((float)x)/100.0;

	return 1;
}

int CHudFlashlight::Draw(float flTime)
{
	// nothing there

	return 1;
}

int CHudFlashlight::DrawFlashlightIcon(const int& x, const int& y)
{
	int r, g, b, a, iOffset;
	wrect_t rc;

	a = 128;

	UnpackRGB(r,g,b, RGB_GREENISH);

	ScaleColors(r, g, b, a);

	iOffset = 60 * ( 1.0 - m_flBat );

	if( m_fOn)
	{
		if ( m_flBat )
		{
			rc = *m_prcOn;
			rc.top += iOffset;

			SPR_Set( gHUD.GetSprite(gHUD.GetSpriteIndex( "flash_on_lit" )), r, g, b );
			SPR_DrawAdditive( 0, x, y + iOffset, &rc );
		//	SPR_DrawAdditive( 0, x, y, &gHUD.GetSpriteRect(gHUD.GetSpriteIndex( "flash_on_lit" )) );
		}
		SPR_Set(m_hSpriteOn, r, g, b );
		SPR_DrawAdditive( 0,  x, y, m_prcOn);
	}
	else
	{
		if ( m_flBat )
		{
			rc = *m_prcOff;
			rc.top += iOffset;

			SPR_Set( gHUD.GetSprite(gHUD.GetSpriteIndex( "flash_off_lit" )), r, g, b );
			SPR_DrawAdditive( 0, x, y + iOffset, &rc );
		//	SPR_DrawAdditive( 0, x, y, &gHUD.GetSpriteRect(gHUD.GetSpriteIndex( "flash_off_lit" )) );
		}
		SPR_Set(m_hSpriteOff, r, g, b );
		SPR_DrawAdditive( 0,  x, y, m_prcOff);
	}

	return 1;
}

int CHudFlashlight::IsOn( void )
{
	return m_fOn;
}