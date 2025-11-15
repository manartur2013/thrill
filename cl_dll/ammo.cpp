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
// Ammo.cpp
//
// implementation of CHudAmmo class
//

#include "hud.h"
#include "cl_util.h"
#include "parsemsg.h"
#include "pm_shared.h"

#include <string.h>
#include <stdio.h>

#include "ammohistory.h"
#include "vgui_TeamFortressViewport.h"

WEAPON *gpActiveSel;	// NULL means off, 1 means just the menu bar, otherwise
						// this points to the active weapon menu item
WEAPON *gpLastSel;		// Last weapon menu selection 

client_sprite_t *GetSpriteList(client_sprite_t *pList, const char *psz, int iRes, int iCount);

WeaponsResource gWR;

int g_weaponselect = 0;

void WeaponsResource :: LoadAllWeaponSprites( void )
{
	for (int i = 0; i < MAX_WEAPONS; i++)
	{
		if ( rgWeapons[i].iId )
			LoadWeaponSprites( &rgWeapons[i] );
	}
}

int WeaponsResource :: CountAmmo( int iId ) 
{ 
	if ( iId < 0 )
		return 0;

	return riAmmo[iId];
}

int WeaponsResource :: HasAmmo( WEAPON *p )
{
	if ( !p )
		return FALSE;

	// weapons with no max ammo can always be selected
	if ( p->iMax1 == -1 )
		return TRUE;

	return (p->iAmmoType == -1) || p->iClip > 0 || CountAmmo(p->iAmmoType) 
		|| CountAmmo(p->iAmmo2Type) || ( p->iFlags & WEAPON_FLAGS_SELECTONEMPTY );
}


void WeaponsResource :: LoadWeaponSprites( WEAPON *pWeapon )
{
	int i, iRes;

	if (ScreenWidth < 640)
		iRes = 320;
	else
		iRes = 640;

	char sz[128];

	if ( !pWeapon )
		return;

	memset( &pWeapon->rcActive, 0, sizeof(wrect_t) );
	memset( &pWeapon->rcInactive, 0, sizeof(wrect_t) );
	memset( &pWeapon->rcAmmo, 0, sizeof(wrect_t) );
	memset( &pWeapon->rcAmmo2, 0, sizeof(wrect_t) );
	pWeapon->hInactive = 0;
	pWeapon->hActive = 0;
	pWeapon->hAmmo = 0;
	pWeapon->hAmmo2 = 0;

	sprintf(sz, "sprites/%s.txt", pWeapon->szName);
	client_sprite_t *pList = SPR_GetList(sz, &i);

	if (!pList)
		return;

	client_sprite_t *p;

	static wrect_t nullrc;
//	pWeapon->hCrosshair = SPR_Load("sprites/crosshairplus.spr");
//	pWeapon->rcCrosshair = nullrc;
	pWeapon->hCrosshair = SPR_Load("sprites/plushair.spr");
//	pWeapon->rcCrosshair = gHUD.GetSpriteRect(pWeapon->hCrosshair);

//  x: 322, y: 250
	
	// manually set this since GetSpriteRect() requires an hud.txt entry

	pWeapon->rcCrosshair.left = 0;
	pWeapon->rcCrosshair.right = 24;
	pWeapon->rcCrosshair.top = 0;
	pWeapon->rcCrosshair.bottom = 48;

	p = GetSpriteList(pList, "weapon", iRes, i);
	if (p)
	{
		sprintf(sz, "sprites/%s.spr", p->szSprite);
		pWeapon->hInactive = SPR_Load(sz);
		pWeapon->rcInactive = p->rc;

		gHR.iHistoryGap = max( gHR.iHistoryGap, pWeapon->rcActive.bottom - pWeapon->rcActive.top );
	}
	else
		pWeapon->hInactive = 0;

	p = GetSpriteList(pList, "weapon_s", iRes, i);
	if (p)
	{
		sprintf(sz, "sprites/%s.spr", p->szSprite);
		pWeapon->hActive = SPR_Load(sz);
		pWeapon->rcActive = p->rc;
	}
	else
		pWeapon->hActive = 0;

	p = GetSpriteList(pList, "ammo", iRes, i);
	if (p)
	{
		sprintf(sz, "sprites/%s.spr", p->szSprite);
		pWeapon->hAmmo = SPR_Load(sz);
		pWeapon->rcAmmo = p->rc;

		gHR.iHistoryGap = max( gHR.iHistoryGap, pWeapon->rcActive.bottom - pWeapon->rcActive.top );
	}
	else
		pWeapon->hAmmo = 0;

	p = GetSpriteList(pList, "ammo2", iRes, i);
	if (p)
	{
		sprintf(sz, "sprites/%s.spr", p->szSprite);
		pWeapon->hAmmo2 = SPR_Load(sz);
		pWeapon->rcAmmo2 = p->rc;

		gHR.iHistoryGap = max( gHR.iHistoryGap, pWeapon->rcActive.bottom - pWeapon->rcActive.top );
	}
	else
		pWeapon->hAmmo2 = 0;

}

// Returns the first weapon for a given slot.
WEAPON *WeaponsResource :: GetFirstPos( int iSlot )
{
	WEAPON *pret = NULL;

	for (int i = 0; i < MAX_WEAPON_POSITIONS; i++)
	{
		if ( rgSlots[iSlot][i] && HasAmmo( rgSlots[iSlot][i] ) )
		{
			pret = rgSlots[iSlot][i];
			break;
		}
	}

	return pret;
}


WEAPON* WeaponsResource :: GetNextActivePos( int iSlot, int iSlotPos )
{
	if ( iSlotPos >= MAX_WEAPON_POSITIONS || iSlot >= MAX_WEAPON_SLOTS )
		return NULL;

	WEAPON *p = gWR.rgSlots[ iSlot ][ iSlotPos+1 ];
	
	if ( !p || !gWR.HasAmmo(p) )
		return GetNextActivePos( iSlot, iSlotPos + 1 );

	return p;
}


int giBucketHeight, giBucketWidth, giABHeight, giABWidth; // Ammo Bar width and height

HSPRITE ghsprBuckets;					// Sprite for top row of weapons menu

DECLARE_MESSAGE(m_Ammo, CurWeapon );	// Current weapon and clip
DECLARE_MESSAGE(m_Ammo, WeaponList);	// new weapon type
DECLARE_MESSAGE(m_Ammo, AmmoX);			// update known ammo type's count
DECLARE_MESSAGE(m_Ammo, AmmoPickup);	// flashes an ammo pickup record
DECLARE_MESSAGE(m_Ammo, WeapPickup);    // flashes a weapon pickup record
DECLARE_MESSAGE(m_Ammo, HideWeapon);	// hides the weapon, ammo, and crosshair displays temporarily
DECLARE_MESSAGE(m_Ammo, ItemPickup);
DECLARE_MESSAGE(m_Ammo, RadPickup);
DECLARE_MESSAGE(m_Ammo, AntPickup);
DECLARE_MESSAGE(m_Ammo, AdrPickup);
DECLARE_MESSAGE(m_Ammo, Airtank);
DECLARE_MESSAGE(m_Ammo, LonJumBat);

DECLARE_COMMAND(m_Ammo, Slot1);
DECLARE_COMMAND(m_Ammo, Slot2);
DECLARE_COMMAND(m_Ammo, Slot3);
DECLARE_COMMAND(m_Ammo, Slot4);
DECLARE_COMMAND(m_Ammo, Slot5);
DECLARE_COMMAND(m_Ammo, Slot6);
DECLARE_COMMAND(m_Ammo, Slot7);
DECLARE_COMMAND(m_Ammo, Slot8);
DECLARE_COMMAND(m_Ammo, Slot9);
DECLARE_COMMAND(m_Ammo, Slot10);
DECLARE_COMMAND(m_Ammo, Close);
DECLARE_COMMAND(m_Ammo, NextWeapon);
DECLARE_COMMAND(m_Ammo, PrevWeapon);

// width of ammo fonts
#define AMMO_SMALL_WIDTH 10
#define AMMO_LARGE_WIDTH 20

#define HISTORY_DRAW_TIME	"5"

int CHudAmmo::Init(void)
{
	gHUD.AddHudElem(this);

	HOOK_MESSAGE(CurWeapon);
	HOOK_MESSAGE(WeaponList);
	HOOK_MESSAGE(AmmoPickup);
	HOOK_MESSAGE(WeapPickup);
	HOOK_MESSAGE(ItemPickup);
	HOOK_MESSAGE(HideWeapon);
	HOOK_MESSAGE(AmmoX);
	HOOK_MESSAGE(RadPickup);
	HOOK_MESSAGE(AntPickup);
	HOOK_MESSAGE(AdrPickup);
	HOOK_MESSAGE(Airtank);
	HOOK_MESSAGE(LonJumBat);

	HOOK_COMMAND("slot1", Slot1);
	HOOK_COMMAND("slot2", Slot2);
	HOOK_COMMAND("slot3", Slot3);
	HOOK_COMMAND("slot4", Slot4);
	HOOK_COMMAND("slot5", Slot5);
	HOOK_COMMAND("slot6", Slot6);
	HOOK_COMMAND("slot7", Slot7);
	HOOK_COMMAND("slot8", Slot8);
	HOOK_COMMAND("slot9", Slot9);
	HOOK_COMMAND("slot10", Slot10);
	HOOK_COMMAND("cancelselect", Close);
	HOOK_COMMAND("invnext", NextWeapon);
	HOOK_COMMAND("invprev", PrevWeapon);

	Reset();

	CVAR_CREATE( "hud_drawhistory_time", HISTORY_DRAW_TIME, 0 );
	CVAR_CREATE( "hud_fastswitch", "0", FCVAR_ARCHIVE );		// controls whether or not weapons can be selected in one keypress

	m_iFlags |= HUD_ACTIVE; //!!!

	gWR.Init();
	gHR.Init();

	return 1;
};

void CHudAmmo::Reset(void)
{
	m_fFade = 0;
	m_iFlags |= HUD_ACTIVE; //!!!

	gpActiveSel = NULL;
	gHUD.m_iHideHUDDisplay = 0;

	gWR.Reset();
	gHR.Reset();
}

int CHudAmmo::VidInit(void)
{
	int HUD_flash_off = gHUD.GetSpriteIndex( "flash_off" );
	int HUD_flash_on = gHUD.GetSpriteIndex( "flash_on" );
	int HUD_flash_off_lit = gHUD.GetSpriteIndex( "flash_off_lit" );
	int HUD_flash_on_lit = gHUD.GetSpriteIndex( "flash_on_lit" );
	int HUD_airtank = gHUD.GetSpriteIndex( "airtank_empty" );
	int HUD_spring = gHUD.GetSpriteIndex( "spring_empty" );
	int HUD_radiation = gHUD.GetSpriteIndex( "syr_radiation" );
	int HUD_radiation_lit = gHUD.GetSpriteIndex( "syr_rad_lit" );
	int HUD_antidote = gHUD.GetSpriteIndex( "syr_antidote" );
	int HUD_antidote_lit = gHUD.GetSpriteIndex( "syr_ant_lit" );
	int HUD_adrenaline = gHUD.GetSpriteIndex( "syr_adrenaline" );
	int HUD_adrenaline_lit = gHUD.GetSpriteIndex( "syr_adr_lit" );
	int HUD_airtank_full = gHUD.GetSpriteIndex( "airtank_full" );
	int HUD_spring_full = gHUD.GetSpriteIndex( "spring_full" );
	int HUD_slash = gHUD.GetSpriteIndex( "slash" );

	m_HUD_small0 = gHUD.GetSpriteIndex( "small_1" );

	m_hSprite1 = gHUD.GetSprite(HUD_flash_off);
	m_hSprite2 = gHUD.GetSprite(HUD_flash_on);
	m_hSprite3 = gHUD.GetSprite(HUD_airtank);
	m_hSprite4 = gHUD.GetSprite(HUD_spring);
	m_hSprite5 = gHUD.GetSprite(HUD_radiation);
	m_hSprite6 = gHUD.GetSprite(HUD_antidote);
	m_hSprite7 = gHUD.GetSprite(HUD_adrenaline);
	m_hSprite8 = gHUD.GetSprite(HUD_airtank_full);
	m_hSprite9 = gHUD.GetSprite(HUD_spring_full);
	m_hSprite10 = gHUD.GetSprite(HUD_slash);
//	m_hSprite11 = gHUD.GetSprite(HUD_flash_off_lit);
//	m_hSprite12 = gHUD.GetSprite(HUD_flash_on_lit);
//	m_hSprite13 = gHUD.GetSprite(HUD_radiation_lit);
//	m_hSprite14 = gHUD.GetSprite(HUD_antidote_lit);
//	m_hSprite15 = gHUD.GetSprite(HUD_adrenaline_lit);

//	m_small0 = gHUD.GetSprite(m_HUD_small0);

	m_prc1 = &gHUD.GetSpriteRect(HUD_flash_off);
	m_prc2 = &gHUD.GetSpriteRect(HUD_flash_on);
	m_prc3 = &gHUD.GetSpriteRect(HUD_airtank);
	m_prc4 = &gHUD.GetSpriteRect(HUD_spring);
	m_prc5 = &gHUD.GetSpriteRect(HUD_radiation);
	m_prc6 = &gHUD.GetSpriteRect(HUD_antidote);
	m_prc7 = &gHUD.GetSpriteRect(HUD_adrenaline);
	m_prc8 = &gHUD.GetSpriteRect(HUD_airtank_full);
	m_prc9 = &gHUD.GetSpriteRect(HUD_spring_full);
	m_prc10 = &gHUD.GetSpriteRect(HUD_slash);
//	m_prc11 = &gHUD.GetSpriteRect(HUD_flash_off_lit);
//	m_prc12 = &gHUD.GetSpriteRect(HUD_flash_on_lit);
//	m_prc13 = &gHUD.GetSpriteRect(HUD_radiation_lit);
//	m_prc14 = &gHUD.GetSpriteRect(HUD_antidote_lit);
//	m_prc15 = &gHUD.GetSpriteRect(HUD_adrenaline_lit);

//	m_prcSmall0 = &gHUD.GetSpriteRect(m_HUD_small0);

	gHUD.plushairindex = gHUD.GetSpriteIndex( "crosshairplus" );
	// Load sprites for buckets (top row of weapon menu)
	m_HUD_bucket0 = gHUD.GetSpriteIndex( "bucket1" );
	m_HUD_selection = gHUD.GetSpriteIndex( "selection" );

	ghsprBuckets = gHUD.GetSprite(m_HUD_bucket0);
	giBucketWidth = gHUD.GetSpriteRect(m_HUD_bucket0).right - gHUD.GetSpriteRect(m_HUD_bucket0).left;
	giBucketHeight = gHUD.GetSpriteRect(m_HUD_bucket0).bottom - gHUD.GetSpriteRect(m_HUD_bucket0).top;

	gHR.iHistoryGap = max( gHR.iHistoryGap, gHUD.GetSpriteRect(m_HUD_bucket0).bottom - gHUD.GetSpriteRect(m_HUD_bucket0).top);

	// If we've already loaded weapons, let's get new sprites
	gWR.LoadAllWeaponSprites();

	if (ScreenWidth >= 640)
	{
		giABWidth = 20;
		giABHeight = 4;
	}
	else
	{
		giABWidth = 10;
		giABHeight = 2;
	}

	return 1;
}

//
// Think:
//  Used for selection of weapon menu item.
//
void CHudAmmo::Think(void)
{
	if ( gHUD.m_fPlayerDead )
		return;

	if ( gHUD.m_iWeaponBits != gWR.iOldWeaponBits )
	{
		gWR.iOldWeaponBits = gHUD.m_iWeaponBits;

		for (int i = MAX_WEAPONS-1; i > 0; i-- )
		{
			WEAPON *p = gWR.GetWeapon(i);

			if ( p )
			{
				if ( gHUD.m_iWeaponBits & ( 1 << p->iId ) )
					gWR.PickupWeapon( p );
				else
					gWR.DropWeapon( p );
			}
		}
	}

	if (!gpActiveSel)
		return;

	// has the player selected one?
	if (gHUD.m_iKeyBits & IN_ATTACK)
	{
		if (gpActiveSel != (WEAPON *)1)
		{
			ServerCmd(gpActiveSel->szName);
			g_weaponselect = gpActiveSel->iId;
		}

		gpLastSel = gpActiveSel;
		gpActiveSel = NULL;
		gHUD.m_iKeyBits &= ~IN_ATTACK;

		PlaySound("common/wpn_select.wav", 1);
	}

}

//
// Helper function to return a Ammo pointer from id
//

HSPRITE* WeaponsResource :: GetAmmoPicFromWeapon( int iAmmoId, wrect_t& rect )
{
	for ( int i = 0; i < MAX_WEAPONS; i++ )
	{
		if ( rgWeapons[i].iAmmoType == iAmmoId )
		{
			rect = rgWeapons[i].rcAmmo;
			return &rgWeapons[i].hAmmo;
		}
		else if ( rgWeapons[i].iAmmo2Type == iAmmoId )
		{
			rect = rgWeapons[i].rcAmmo2;
			return &rgWeapons[i].hAmmo2;
		}
	}

	return NULL;
}


// Menu Selection Code

void WeaponsResource :: SelectSlot( int iSlot, int fAdvance, int iDirection )
{
	if ( gHUD.m_Menu.m_fMenuDisplayed && (fAdvance == FALSE) && (iDirection == 1) )	
	{ // menu is overriding slot use commands
		gHUD.m_Menu.SelectMenuItem( iSlot + 1 );  // slots are one off the key numbers
		return;
	}

	if ( iSlot > MAX_WEAPON_SLOTS )
		return;

	if ( gHUD.m_fPlayerDead || gHUD.m_iHideHUDDisplay & ( HIDEHUD_WEAPONS | HIDEHUD_ALL ) )
		return;

	if (!(gHUD.m_iWeaponBits & (1<<(WEAPON_SUIT)) ))
		return;

	if ( ! ( gHUD.m_iWeaponBits & ~(1<<(WEAPON_SUIT)) ))
		return;

	WEAPON *p = NULL;
	bool fastSwitch = CVAR_GET_FLOAT( "hud_fastswitch" ) != 0;

	if ( (gpActiveSel == NULL) || (gpActiveSel == (WEAPON *)1) || (iSlot != gpActiveSel->iSlot) )
	{
		PlaySound( "common/wpn_hudon.wav", 1 );
		p = GetFirstPos( iSlot );

		if ( p && fastSwitch ) // check for fast weapon switch mode
		{
			// if fast weapon switch is on, then weapons can be selected in a single keypress
			// but only if there is only one item in the bucket
			WEAPON *p2 = GetNextActivePos( p->iSlot, p->iSlotPos );
			if ( !p2 )
			{	// only one active item in bucket, so change directly to weapon
				ServerCmd( p->szName );
				g_weaponselect = p->iId;
				return;
			}
		}
	}
	else
	{
		PlaySound("common/wpn_moveselect.wav", 1);
		if ( gpActiveSel )
			p = GetNextActivePos( gpActiveSel->iSlot, gpActiveSel->iSlotPos );
		if ( !p )
			p = GetFirstPos( iSlot );
	}

	
	if ( !p )  // no selection found
	{
		// just display the weapon list, unless fastswitch is on just ignore it
		if ( !fastSwitch )
			gpActiveSel = (WEAPON *)1;
		else
			gpActiveSel = NULL;
	}
	else 
		gpActiveSel = p;
}

//------------------------------------------------------------------------
// Message Handlers
//------------------------------------------------------------------------

//
// AmmoX  -- Update the count of a known type of ammo
// 
int CHudAmmo::MsgFunc_AmmoX(const char *pszName, int iSize, void *pbuf)
{
	BEGIN_READ( pbuf, iSize );

	int iIndex = READ_BYTE();
	int iCount = READ_BYTE();

	gWR.SetAmmo( iIndex, abs(iCount) );

	return 1;
}

int CHudAmmo::MsgFunc_AmmoPickup( const char *pszName, int iSize, void *pbuf )
{
	BEGIN_READ( pbuf, iSize );
	int iIndex = READ_BYTE();
	int iCount = READ_BYTE();

	// Add ammo to the history
	gHR.AddToHistory( HISTSLOT_AMMO, iIndex, abs(iCount) );

	return 1;
}

int CHudAmmo::MsgFunc_WeapPickup( const char *pszName, int iSize, void *pbuf )
{
	BEGIN_READ( pbuf, iSize );
	int iIndex = READ_BYTE();

	// Add the weapon to the history
	gHR.AddToHistory( HISTSLOT_WEAP, iIndex );

	return 1;
}

int CHudAmmo::MsgFunc_ItemPickup( const char *pszName, int iSize, void *pbuf )
{
	BEGIN_READ( pbuf, iSize );
	const char *szName = READ_STRING();

	// Add the weapon to the history
	gHR.AddToHistory( HISTSLOT_ITEM, szName );

	return 1;
}


int CHudAmmo::MsgFunc_HideWeapon( const char *pszName, int iSize, void *pbuf )
{
	BEGIN_READ( pbuf, iSize );
	
	gHUD.m_iHideHUDDisplay = READ_BYTE();

	if (gEngfuncs.IsSpectateOnly())
		return 1;

	if ( gHUD.m_iHideHUDDisplay & ( HIDEHUD_WEAPONS | HIDEHUD_ALL ) )
	{
		static wrect_t nullrc;
		gpActiveSel = NULL;
		SetCrosshair( 0, nullrc, 0, 0, 0 );
	}
	else
	{
		if ( m_pWeapon )
		
		SetCrosshair( m_pWeapon->hCrosshair, m_pWeapon->rcCrosshair, 255, 255, 255 );
	}

	return 1;
}

// 
//  CurWeapon: Update hud state with the current weapon and clip count. Ammo
//  counts are updated with AmmoX. Server assures that the Weapon ammo type 
//  numbers match a real ammo type.
//
int CHudAmmo::MsgFunc_CurWeapon(const char *pszName, int iSize, void *pbuf )
{
	static wrect_t nullrc;
	int fOnTarget = FALSE;

	BEGIN_READ( pbuf, iSize );

	int iState = READ_BYTE();
	int iId = READ_CHAR();
	int iClip = READ_CHAR();

	// detect if we're also on target
	if ( iState > 1 )
	{
		fOnTarget = TRUE;
	}

	if ( iId < 1 )
	{
		SetCrosshair(0, nullrc, 0, 0, 0);
		return 0;
	}

	if ( g_iUser1 != OBS_IN_EYE )
	{
		// Is player dead???
		if ((iId == -1) && (iClip == -1))
		{
			gHUD.m_fPlayerDead = TRUE;
			gpActiveSel = NULL;
			return 1;
		}
		gHUD.m_fPlayerDead = FALSE;
	}

	WEAPON *pWeapon = gWR.GetWeapon( iId );

	if ( !pWeapon )
		return 0;

	if ( iClip < -1 )
		pWeapon->iClip = abs(iClip);
	else
		pWeapon->iClip = iClip;


	if ( iState == 0 )	// we're not the current weapon, so update no more
		return 1;

	m_pWeapon = pWeapon;

	SetCrosshair(m_pWeapon->hCrosshair, m_pWeapon->rcCrosshair, 255, 255, 255);

	m_fFade = 200.0f; //!!!
	m_iFlags |= HUD_ACTIVE;
	
	return 1;
}

//
// WeaponList -- Tells the hud about a new weapon type.
//
int CHudAmmo::MsgFunc_WeaponList(const char *pszName, int iSize, void *pbuf )
{
	BEGIN_READ( pbuf, iSize );
	
	WEAPON Weapon;

	strcpy( Weapon.szName, READ_STRING() );
	Weapon.iAmmoType = (int)READ_CHAR();	
	
	Weapon.iMax1 = READ_BYTE();
	if (Weapon.iMax1 == 255)
		Weapon.iMax1 = -1;

	Weapon.iAmmo2Type = READ_CHAR();
	Weapon.iMax2 = READ_BYTE();
	if (Weapon.iMax2 == 255)
		Weapon.iMax2 = -1;

	Weapon.iSlot = READ_CHAR();
	Weapon.iSlotPos = READ_CHAR();
	Weapon.iId = READ_CHAR();
	Weapon.iFlags = READ_BYTE();
	Weapon.iClip = 0;

	gWR.AddWeapon( &Weapon );

	return 1;

}

int CHudAmmo::MsgFunc_RadPickup( const char *pszName, int iSize, void *pbuf )
{
	BEGIN_READ( pbuf, iSize );
	m_radiationCount = READ_SHORT();

	return 1;
}

int CHudAmmo::MsgFunc_AntPickup( const char *pszName, int iSize, void *pbuf )
{
	BEGIN_READ( pbuf, iSize );
	m_antidoteCount = READ_SHORT();

	return 1;
}

int CHudAmmo::MsgFunc_AdrPickup( const char *pszName, int iSize, void *pbuf )
{
	BEGIN_READ( pbuf, iSize );
	m_adrenalineCount = READ_SHORT();

	return 1;
}

int CHudAmmo::MsgFunc_Airtank( const char *pszName, int iSize, void *pbuf )
{
	BEGIN_READ( pbuf, iSize );
	int x = READ_SHORT();

	m_iAirtank = x;

	return 1;
}

int CHudAmmo::MsgFunc_LonJumBat( const char *pszName, int iSize, void *pbuf )
{
	BEGIN_READ( pbuf, iSize );
	int x = READ_SHORT();

	m_iLongJumpBat = x;

	return 1;
}

//------------------------------------------------------------------------
// Command Handlers
//------------------------------------------------------------------------
// Slot button pressed
void CHudAmmo::SlotInput( int iSlot )
{
	if ( gViewPort && gViewPort->SlotInput( iSlot ) )
		return;

	gWR.SelectSlot(iSlot, FALSE, 1);
}

void CHudAmmo::UserCmd_Slot1(void)
{
	SlotInput( 0 );
}

void CHudAmmo::UserCmd_Slot2(void)
{
	SlotInput( 1 );
}

void CHudAmmo::UserCmd_Slot3(void)
{
	SlotInput( 2 );
}

void CHudAmmo::UserCmd_Slot4(void)
{
	SlotInput( 3 );
}

void CHudAmmo::UserCmd_Slot5(void)
{
	SlotInput( 4 );
}

void CHudAmmo::UserCmd_Slot6(void)
{
	SlotInput( 5 );
//	PlaySound("PLAYER/hoot1.wav", 1);
}

void CHudAmmo::UserCmd_Slot7(void)
{
	SlotInput( 6 );
}

void CHudAmmo::UserCmd_Slot8(void)
{
	SlotInput( 7 );
}

void CHudAmmo::UserCmd_Slot9(void)
{
	SlotInput( 8 );
}

void CHudAmmo::UserCmd_Slot10(void)
{
	SlotInput( 9 );
}

void CHudAmmo::UserCmd_Close(void)
{
	if (gpActiveSel)
	{
		gpLastSel = gpActiveSel;
		gpActiveSel = NULL;
		PlaySound("common/wpn_hudoff.wav", 1);
	}
	else
		EngineClientCmd("escape");
}


// Selects the next item in the weapon menu
void CHudAmmo::UserCmd_NextWeapon(void)
{
	if ( gHUD.m_fPlayerDead || (gHUD.m_iHideHUDDisplay & (HIDEHUD_WEAPONS | HIDEHUD_ALL)) )
		return;

	if ( !gpActiveSel || gpActiveSel == (WEAPON*)1 )
		gpActiveSel = m_pWeapon;

	int pos = 0;
	int slot = 0;
	if ( gpActiveSel )
	{
		pos = gpActiveSel->iSlotPos + 1;
		slot = gpActiveSel->iSlot;
	}

	for ( int loop = 0; loop <= 1; loop++ )
	{
		for ( ; slot < MAX_WEAPON_SLOTS; slot++ )
		{
			for ( ; pos < MAX_WEAPON_POSITIONS; pos++ )
			{
				WEAPON *wsp = gWR.GetWeaponSlot( slot, pos );

				if ( wsp && gWR.HasAmmo(wsp) )
				{
					gpActiveSel = wsp;
					return;
				}
			}

			pos = 0;
		}

		slot = 0;  // start looking from the first slot again
	}

	gpActiveSel = NULL;
}

// Selects the previous item in the menu
void CHudAmmo::UserCmd_PrevWeapon(void)
{
	if ( gHUD.m_fPlayerDead || (gHUD.m_iHideHUDDisplay & (HIDEHUD_WEAPONS | HIDEHUD_ALL)) )
		return;

	if ( !gpActiveSel || gpActiveSel == (WEAPON*)1 )
		gpActiveSel = m_pWeapon;

	int pos = MAX_WEAPON_POSITIONS-1;
	int slot = MAX_WEAPON_SLOTS-1;
	if ( gpActiveSel )
	{
		pos = gpActiveSel->iSlotPos - 1;
		slot = gpActiveSel->iSlot;
	}
	
	for ( int loop = 0; loop <= 1; loop++ )
	{
		for ( ; slot >= 0; slot-- )
		{
			for ( ; pos >= 0; pos-- )
			{
				WEAPON *wsp = gWR.GetWeaponSlot( slot, pos );

				if ( wsp && gWR.HasAmmo(wsp) )
				{
					gpActiveSel = wsp;
					return;
				}
			}

			pos = MAX_WEAPON_POSITIONS-1;
		}
		
		slot = MAX_WEAPON_SLOTS-1;
	}

	gpActiveSel = NULL;
}



//-------------------------------------------------------------------------
// Drawing code
//-------------------------------------------------------------------------

int CHudAmmo::Draw(float flTime)
{
	int a, x, y, r, g, b;
	int AmmoWidth, iIconWidth, iFlags;

	if (!(gHUD.m_iWeaponBits & (1<<(WEAPON_SUIT)) ))
		return 1;

	if ( (gHUD.m_iHideHUDDisplay & ( HIDEHUD_WEAPONS | HIDEHUD_ALL )) )
		return 1;

	// Draw Weapon Menu
	DrawWList(flTime);

	// Draw ammo pickup history
	//gHR.DrawAmmoHistory( flTime );

	if (!(m_iFlags & HUD_ACTIVE))
		return 0;

	if (!m_pWeapon)
		return 0;

	// Draw crosshair
	if ( gHUD.m_pCvarCrosshair->value )
	{
		if ( CVAR_GET_FLOAT( "crosshair" ) )
			gEngfuncs.pfnClientCmd( "crosshair 0" );

		if (m_pWeapon)
			DrawCrosshairScalable(m_pWeapon->hCrosshair, m_pWeapon->rcCrosshair, 255, 255, 255);
	}

	WEAPON *pw = m_pWeapon; // shorthand

	AmmoWidth = gHUD.GetSpriteRect(gHUD.m_HUD_number_0).right - gHUD.GetSpriteRect(gHUD.m_HUD_number_0).left;
	iIconWidth = m_pWeapon->rcAmmo.right - m_pWeapon->rcAmmo.left;
	iFlags = DHN_DRAWZERO; // draw 0 values
	
	// set basic alpha for ammo number
	a = 144;

	UnpackRGB(r,g,b, RGB_GREENISH);

	ScaleColors(r, g, b, a );

	y = gHUD.m_iHudScaleHeight - 55;

	// SPR_Draw Ammo
	if ((pw->iAmmoType < 0) && (pw->iAmmo2Type < 0))
	{	
		x = gHUD.m_iHudScaleWidth - 4 * AmmoWidth;
		gHUD.DrawHudNumberAmmo(x, y, iFlags | DHN_3DIGITS, 255, r, g, b);	// draw it "bugged"

		return 0;
	}

	// Does weapon have any ammo at all?
	if (m_pWeapon->iAmmoType >= 0)
	{
		if (pw->iClip >= 0)
		{
			// room for the number and the '|' and the current ammo
			
//			x = ScreenWidth - (8 * AmmoWidth) - iIconWidth;
			x = gHUD.m_iHudScaleWidth - 128;
			x = gHUD.DrawHudNumberAmmo(x, y, iFlags | DHN_3DIGITS, pw->iClip, r, g, b);

			wrect_t rc;
			rc.top = 0;
			rc.left = 0;
			rc.right = AmmoWidth;
			rc.bottom = 100;

//			x += AmmoWidth/2;

//			UnpackRGB(r,g,b, RGB_GREENISH);

			// draw the | bar
//			FillRGBA(x, y, iBarWidth, gHUD.m_iFontHeight, r, g, b, a);
			SPR_Set(m_hSprite10, r, g, b);
			SPR_DrawAdditive(0, x, y, m_prc10);

			x += AmmoWidth/2;

			// GL Seems to need this
		//	ScaleColors(r, g, b, a );
			x = gHUD.DrawHudNumberAmmo(x, y, iFlags | DHN_3DIGITS, gWR.CountAmmo(pw->iAmmoType), r, g, b);		


		}
		else
		{
			// SPR_Draw a bullets only line
			x = gHUD.m_iHudScaleWidth - 4 * AmmoWidth - iIconWidth;
			x = gHUD.DrawHudNumberAmmo(x, y, iFlags | DHN_3DIGITS, gWR.CountAmmo(pw->iAmmoType), r, g, b);
		}

		// Draw the ammo Icon
	/*	int iOffset = (m_pWeapon->rcAmmo.bottom - m_pWeapon->rcAmmo.top)/8;
		SPR_Set(m_pWeapon->hAmmo, r, g, b);
		SPR_DrawAdditive(0, x, y - iOffset, &m_pWeapon->rcAmmo);	*/
	}

	// Does weapon have seconday ammo?
	if (pw->iAmmo2Type > 0) 
	{
		int iIconWidth = m_pWeapon->rcAmmo2.right - m_pWeapon->rcAmmo2.left;

		// Do we have secondary ammo?
		if ((pw->iAmmo2Type != 0) && (gWR.CountAmmo(pw->iAmmo2Type) >= 0))
		{
//			y -= gHUD.m_iFontHeight + gHUD.m_iFontHeight/4;
//			x = ScreenWidth - 4 * AmmoWidth - iIconWidth;
			y = gHUD.m_iHudScaleHeight - 28;
			x = gHUD.m_iHudScaleWidth - 80;
			x = gHUD.DrawHudNumberAmmo(x, y, iFlags|DHN_3DIGITS, gWR.CountAmmo(pw->iAmmo2Type), r, g, b);

			// Draw the ammo Icon
		/*	SPR_Set(m_pWeapon->hAmmo2, r, g, b);
			int iOffset = (m_pWeapon->rcAmmo2.bottom - m_pWeapon->rcAmmo2.top)/8;
			SPR_DrawAdditive(0, x, y - iOffset, &m_pWeapon->rcAmmo2);	*/
		}
	}
	return 1;
}


//
// Draws the ammo bar on the hud
//
int DrawBar(int x, int y, int width, int height, float f)
{
	int r, g, b;

	if (f < 0)
		f = 0;
	if (f > 1)
		f = 1;

	if (f)
	{
		int w = f * width;

		// Always show at least one pixel if we have ammo.
		if (w <= 0)
			w = 1;
		UnpackRGB(r, g, b, RGB_GREENISH);
		FillRGBA(x, y, w, height, r, g, b, 255);
		x += w;
		width -= w;
	}

	UnpackRGB(r, g, b, RGB_GREENISH);

	FillRGBA(x, y, width, height, r, g, b, 128);

	return (x + width);
}



void DrawAmmoBar(WEAPON *p, int x, int y, int width, int height)
{
	if ( !p )
		return;
	
	if (p->iAmmoType != -1)
	{
		if (!gWR.CountAmmo(p->iAmmoType))
			return;

		float f = (float)gWR.CountAmmo(p->iAmmoType)/(float)p->iMax1;
		
		x = DrawBar(x, y, width, height, f);


		// Do we have secondary ammo too?

		if (p->iAmmo2Type != -1)
		{
			f = (float)gWR.CountAmmo(p->iAmmo2Type)/(float)p->iMax2;

			x += 5; //!!!

			DrawBar(x, y, width, height, f);
		}
	}
}

//
// Draw Weapon Menu
//
int CHudAmmo::DrawWList(float flTime)
{
	int r,g,b,x,y,a,i;

	bool shouldDraw, flashDrawn;

	if ( !gpActiveSel && !(gHUD.m_pCvarApparatus->value) )
		shouldDraw = false;
	else
		shouldDraw = true;

	//
	// Draw Apparatus
	//
	UnpackRGB(r,g,b, RGB_GREENISH);

	ScaleColors(r, g, b, 128);

	int apparatusIconWidth = 60;
	x = gHUD.m_iHudScaleWidth - apparatusIconWidth - 22;
	y = 8;

	// Draw Airtank
	// We will cut off 5 pixels for each second being under water (5*12=60)
	// The empty Airtank will always be drawn

	if ( m_iAirtank < 12 || shouldDraw )
	{
		SPR_Set( m_hSprite3, r, g, b );
		SPR_DrawAdditive( 0, x, y, m_prc3 );
		if ( m_iAirtank > 0 )
		{
		int iOffset = 60 - (5 * m_iAirtank);
		wrect_t rc;
		rc = *m_prc8;
		rc.top += iOffset;
		//gEngfuncs.pfnConsolePrint( "Airtank offset\n" );
		SPR_Set(m_hSprite8, r, g, b );
		SPR_DrawAdditive( 0, x , y + iOffset, &rc);
		}

		y += apparatusIconWidth + 8;
	}

	// Draw Spring
	// 12*5

	if ( (m_iLongJumpBat < 5 && m_iLongJumpBat > -1) || shouldDraw )
	{
		SPR_Set( m_hSprite4, r, g, b );
		SPR_DrawAdditive( 0, x, y, m_prc4 );
		if ( m_iLongJumpBat > 0 )
		{
		int iOffset = 60 - (12 * m_iLongJumpBat);
		wrect_t rc;
		rc = *m_prc9;
		rc.top += iOffset;
		//gEngfuncs.pfnConsolePrint( "Spring offset\n" );
		SPR_Set(m_hSprite9, r, g, b );
		SPR_DrawAdditive( 0, x , y + iOffset, &rc);
		}

		y += apparatusIconWidth + 8;
	}
	
	if (gHUD.m_Flash.IsOn() && !shouldDraw)
	{
		gHUD.m_Flash.DrawFlashlightIcon(x,y);
		
		flashDrawn = true;

		y += apparatusIconWidth + 8;
	}
	else flashDrawn = false;

	if ( !shouldDraw )
		return 0;

	// Draw Radiation syringe
	
	if ( m_radiationCount > 0 )
	{
		SPR_Set( gHUD.GetSprite(gHUD.GetSpriteIndex( "syr_rad_lit" )), r, g, b );
		SPR_DrawAdditive( 0, x, y, &gHUD.GetSpriteRect(gHUD.GetSpriteIndex( "syr_rad_lit" )) );
	}
		SPR_Set( m_hSprite5, r, g, b );
		SPR_DrawAdditive( 0, x, y, m_prc5 );
	
	// Draw Antidote syringe
	y += apparatusIconWidth + 8;
	if ( m_antidoteCount > 0 )
	{
		SPR_Set( gHUD.GetSprite(gHUD.GetSpriteIndex( "syr_ant_lit" )), r, g, b );
		SPR_DrawAdditive( 0, x, y, &gHUD.GetSpriteRect(gHUD.GetSpriteIndex( "syr_ant_lit" )) );
	}
		SPR_Set( m_hSprite6, r, g, b );
		SPR_DrawAdditive( 0, x, y, m_prc6 );

	// Draw Adrenaline syringe
	y += apparatusIconWidth + 8;
	if ( m_adrenalineCount > 0 ) 
	{
		SPR_Set( gHUD.GetSprite(gHUD.GetSpriteIndex( "syr_adr_lit" )), r, g, b );
		SPR_DrawAdditive( 0, x, y, &gHUD.GetSpriteRect(gHUD.GetSpriteIndex( "syr_adr_lit" )) );
	}
		SPR_Set( m_hSprite7, r, g, b );
		SPR_DrawAdditive( 0, x, y, m_prc7 );
	
	// Draw Flashlight
	if (flashDrawn == false)
	{
		y += apparatusIconWidth + 8;
		gHUD.m_Flash.DrawFlashlightIcon(x,y);
	}
	// 
	// Draw amount of syringes
	//

	// Amount of Radiation syringes

//	x -= 40;
	a = 255;

	ScaleColors(r, g, b, a);

	y = 8 * 2 + 64 * 2 + 32;

	if ( m_radiationCount > 0 )
	{
		SPR_Set( gHUD.GetSprite(m_HUD_small0 + (m_radiationCount-1)), r, g, b );	
		SPR_DrawAdditive( 0, x, y, &gHUD.GetSpriteRect(m_HUD_small0 + (m_radiationCount-1)) );
	}
	// Amount of Antidote syringes

	y = 8 * 3 + 64 * 3 + 32;

	if ( m_antidoteCount > 0 )
	{
		SPR_Set( gHUD.GetSprite(m_HUD_small0 + (m_antidoteCount-1)), r, g, b );
		SPR_DrawAdditive( 0, x, y, &gHUD.GetSpriteRect(m_HUD_small0 + (m_antidoteCount-1)) );
	}
	
	if ( !gpActiveSel )
		return 0;

	int iActiveSlot;

	if ( gpActiveSel == (WEAPON *)1 )
		iActiveSlot = -1;	// current slot has no weapons
	else 
		iActiveSlot = gpActiveSel->iSlot;

	x = 10; //!!!
	y = 10; //!!!
	
	// Ensure that there are available choices in the active slot
	if ( iActiveSlot > 0 )
	{
		if ( !gWR.GetFirstPos( iActiveSlot ) )
		{
			gpActiveSel = (WEAPON *)1;
			iActiveSlot = -1;
		}
	}
		
	// Draw top line
	for ( i = 0; i < MAX_WEAPON_SLOTS; i++ )
	{
		int iWidth;

		UnpackRGB(r,g,b, RGB_GREENISH);
	
		if ( iActiveSlot == i )
			a = 255;
		else
			a = 192;

		ScaleColors(r, g, b, 255);
		SPR_Set(gHUD.GetSprite(m_HUD_bucket0 + i), r, g, b );

		// make active slot wide enough to accomodate gun pictures
		if ( i == iActiveSlot )
		{
			WEAPON *p = gWR.GetFirstPos(iActiveSlot);
			if ( p )
				iWidth = p->rcActive.right - p->rcActive.left;
			else
				iWidth = giBucketWidth;
		}
		else
			iWidth = giBucketWidth;

		SPR_DrawAdditive(0, x, y, &gHUD.GetSpriteRect(m_HUD_bucket0 + i));
		
		x += iWidth + 5;
	}


	a = 128; //!!!
	x = 10;

	// Draw all of the buckets
	for (i = 0; i < MAX_WEAPON_SLOTS; i++)
	{
		y = giBucketHeight + 10;

		// If this is the active slot, draw the bigger pictures,
		// otherwise just draw boxes
		if ( i == iActiveSlot )
		{
			WEAPON *p = gWR.GetFirstPos( i );
			int iWidth = giBucketWidth;
			if ( p )
				iWidth = p->rcActive.right - p->rcActive.left;

			for ( int iPos = 0; iPos < MAX_WEAPON_POSITIONS; iPos++ )
			{
				p = gWR.GetWeaponSlot( i, iPos );

				if ( !p || !p->iId )
					continue;

				UnpackRGB( r,g,b, RGB_GREENISH );
			
				// if active, then we must have ammo.

				if ( gpActiveSel == p )
				{
					SPR_Set(p->hActive, r, g, b );
					SPR_DrawAdditive(0, x, y, &p->rcActive);

					SPR_Set(gHUD.GetSprite(m_HUD_selection), r, g, b );
					SPR_DrawAdditive(0, x, y, &gHUD.GetSpriteRect(m_HUD_selection));
				}
				else
				{
					// Draw Weapon if Red if no ammo

					if ( gWR.HasAmmo(p) )
						ScaleColors(r, g, b, 192);
					else
					{
						UnpackRGB(r,g,b, RGB_REDISH);
						ScaleColors(r, g, b, 128);
					}

					SPR_Set( p->hInactive, r, g, b );
					SPR_DrawAdditive( 0, x, y, &p->rcInactive );
				}

				// Draw Ammo Bar

				DrawAmmoBar(p, x + giABWidth/2, y, giABWidth, giABHeight);
				
				y += p->rcActive.bottom - p->rcActive.top + 5;
			}

			x += iWidth + 5;

		}
		else
		{
			// Draw Row of weapons.

			UnpackRGB(r,g,b, RGB_GREENISH);

			for ( int iPos = 0; iPos < MAX_WEAPON_POSITIONS; iPos++ )
			{
				WEAPON *p = gWR.GetWeaponSlot( i, iPos );
				
				if ( !p || !p->iId )
					continue;

				if ( gWR.HasAmmo(p) )
				{
					UnpackRGB(r,g,b, RGB_GREENISH);
					a = 128;
				}
				else
				{
					UnpackRGB(r,g,b, RGB_REDISH);
					a = 96;
				}

				FillRGBA( x, y, giBucketWidth, giBucketHeight, r, g, b, a );

				y += giBucketHeight + 5;
			}

			x += giBucketWidth + 5;
		}
	}	
	
	return 1;

}

/* =================================
	DrawCrosshairScalable

This non-engine function is able to draw
a crosshair with different size and placement
on user's request. It is regulated by 
"hud_crosshair" cvar.
================================= */
void CHudAmmo :: DrawCrosshairScalable(HSPRITE spr, wrect_t prc, int r, int g, int b)
{
	int x,y;
	
	if ( gHUD.m_pCvarCrosshair->value == 1 )	// user-friendly variant: crosshair is placed right in the center of the screen
	{
		x = gHUD.m_iHudScaleWidth/2 - prc.right/2;
		y = gHUD.m_iHudScaleHeight/2 - prc.bottom/2 - prc.bottom/3;
	}
	else if ( gHUD.m_pCvarCrosshair->value >= 2 )	// alpha-accurate variant
	{
		x = gHUD.m_iHudScaleWidth/2 - prc.right/2 + 2;
		y = gHUD.m_iHudScaleHeight/2 - prc.bottom/2 + 10;
	}

	SPR_Set( spr, r, g, b );
	SPR_DrawHoles( 0, x, y, &prc );
}

/* =================================
	GetSpriteList

Finds and returns the matching 
sprite name 'psz' and resolution 'iRes'
in the given sprite list 'pList'
iCount is the number of items in the pList
================================= */
client_sprite_t *GetSpriteList(client_sprite_t *pList, const char *psz, int iRes, int iCount)
{
	if (!pList)
		return NULL;

	int i = iCount;
	client_sprite_t *p = pList;

	while(i--)
	{
		if ((!strcmp(psz, p->szName)) && (p->iRes == iRes))
			return p;
		p++;
	}

	return NULL;
}
