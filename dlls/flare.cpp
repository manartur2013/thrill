#include "extdll.h"
#include "util.h"
#include "cbase.h"
#include "monsters.h"
#include "weapons.h"
#include "nodes.h"
#include "player.h"
#include "soundent.h"

#ifdef XM_WEAPONS
enum flare_e {
	HANDGRENADE_IDLE = 0,
	HANDGRENADE_FIDGET,
	HANDGRENADE_PINPULL,
	HANDGRENADE_THROW1,	// toss
	HANDGRENADE_THROW2,	// medium
	HANDGRENADE_THROW3,	// hard
	HANDGRENADE_HOLSTER,
	HANDGRENADE_DRAW
};

LINK_ENTITY_TO_CLASS ( weapon_flare, CFlare );

void CFlare::Spawn( )
{
	Precache( );
	m_iId = WEAPON_FLARE;
	SET_MODEL(ENT(pev), "models/w_flare.mdl");

	switch ( pev->sequence )
	{
	case 0:
		UTIL_SetSize(pev, Vector(-1, -1, 0), Vector(1, 1, 10));
//		ALERT( at_console, "Vertical flare spawned!\n" );
		break;
	case 1:
		UTIL_SetSize(pev, Vector(-3, 0, -3), Vector(3, 20, 3));
//		ALERT( at_console, "Horizontal flare spawned!\n" );
		break;
	}

#ifndef CLIENT_DLL
	pev->dmg = 1;
#endif

	m_iDefaultAmmo = 1;

	FallInit();// get ready to fall down.
}

void CFlare::Precache( void )
{
	PRECACHE_MODEL("models/w_flare.mdl");
	PRECACHE_MODEL("models/v_flare.mdl");
	PRECACHE_MODEL("models/p_grenade.mdl");
	PRECACHE_SOUND("ambience/steamjet1.wav");

	PRECACHE_SOUND("weapons/g_bounce1.wav");
	PRECACHE_SOUND("weapons/g_bounce2.wav");
	PRECACHE_SOUND("weapons/g_bounce3.wav");

//	pev->angles.x = 90;
//	pev->angles.y = 270;
}

int CFlare::GetItemInfo(ItemInfo *p)
{
	p->pszName = STRING(pev->classname);
	p->pszAmmo1 = "Flare";
	p->iMaxAmmo1 = FLARE_MAX_CARRY;
	p->pszAmmo2 = NULL;
	p->iMaxAmmo2 = -1;
	p->iMaxClip = WEAPON_NOCLIP;
	p->iSlot = 0;
	p->iPosition = 1;
	p->iId = m_iId = WEAPON_FLARE;
	p->iWeight = HANDGRENADE_WEIGHT;
	p->iFlags = ITEM_FLAG_LIMITINWORLD | ITEM_FLAG_EXHAUSTIBLE;

	return 1;
}

BOOL CFlare::Deploy( )
{
	m_flReleaseThrow = -1;
	return DefaultDeploy( "models/v_flare.mdl", "models/p_grenade.mdl", HANDGRENADE_DRAW, "crowbar" );
}

BOOL CFlare::CanHolster( void )
{
	// can only holster hand grenades when not primed!
	return ( m_flStartThrow == 0 );
}

void CFlare::Holster( int skiplocal /* = 0 */ )
{
	m_pPlayer->m_flNextAttack = UTIL_WeaponTimeBase() + 1.1;

	if ( m_pPlayer->m_rgAmmo[ m_iPrimaryAmmoType ] )
	{
		SendWeaponAnim( HANDGRENADE_HOLSTER );
	}
	else
	{
		m_pPlayer->pev->weapons &= ~(1<<WEAPON_FLARE);
		SetThink( &CFlare::DestroyItem );
		pev->nextthink = gpGlobals->time + 0.1;
	}

	EMIT_SOUND(ENT(m_pPlayer->pev), CHAN_WEAPON, "common/null.wav", 1.0, ATTN_NORM);
}

void CFlare::PrimaryAttack()
{
	if ( !m_flStartThrow && m_pPlayer->m_rgAmmo[ m_iPrimaryAmmoType ] > 0 )
	{
		m_flStartThrow = gpGlobals->time;
		m_flReleaseThrow = 0;

		SendWeaponAnim( HANDGRENADE_PINPULL );
		m_flTimeWeaponIdle = UTIL_WeaponTimeBase() + 0.5;
	}
}


void CFlare::WeaponIdle( void )
{
	if ( m_flReleaseThrow == 0 && m_flStartThrow )
		 m_flReleaseThrow = gpGlobals->time;

	if ( m_flTimeWeaponIdle > UTIL_WeaponTimeBase() )
		return;

	if ( m_flStartThrow )
	{
		Vector angThrow = m_pPlayer->pev->v_angle + m_pPlayer->pev->punchangle;

		if ( angThrow.x < 0 )
			angThrow.x = -10 + angThrow.x * ( ( 90 - 10 ) / 90.0 );
		else
			angThrow.x = -10 + angThrow.x * ( ( 90 + 10 ) / 90.0 );

		float flVel = ( 90 - angThrow.x ) * 4;
		if ( flVel > 500 )
			flVel = 500;

		UTIL_MakeVectors( angThrow );

		Vector vecSrc = m_pPlayer->pev->origin + m_pPlayer->pev->view_ofs + gpGlobals->v_forward * 16;

		Vector vecThrow = gpGlobals->v_forward * flVel + m_pPlayer->pev->velocity;

		// alway explode 3 seconds after the pin was pulled
		float time = m_flStartThrow - gpGlobals->time + 3.0;
		if (time < 0)
			time = 0;

		CFlareGrenade::ShootTimed( m_pPlayer->pev, vecSrc, vecThrow, time );

		if ( flVel < 500 )
		{
			SendWeaponAnim( HANDGRENADE_THROW1 );
		}
		else if ( flVel < 1000 )
		{
			SendWeaponAnim( HANDGRENADE_THROW2 );
		}
		else
		{
			SendWeaponAnim( HANDGRENADE_THROW3 );
		}

		// player "shoot" animation
		m_pPlayer->SetAnimation( PLAYER_ATTACK1 );

		m_flReleaseThrow = 0;
		m_flStartThrow = 0;
		m_flNextPrimaryAttack = GetNextAttackDelay(0.5);
		m_flTimeWeaponIdle = UTIL_WeaponTimeBase() + 0.5;

		m_pPlayer->m_rgAmmo[ m_iPrimaryAmmoType ]--;

		if ( !m_pPlayer->m_rgAmmo[ m_iPrimaryAmmoType ] )
		{
			// just threw last grenade
			// set attack times in the future, and weapon idle in the future so we can see the whole throw
			// animation, weapon idle will automatically retire the weapon for us.
			m_flTimeWeaponIdle = m_flNextSecondaryAttack = m_flNextPrimaryAttack = GetNextAttackDelay(0.5);// ensure that the animation can finish playing
		}
		return;
	}
	else if ( m_flReleaseThrow > 0 )
	{
		// we've finished the throw, restart.
		m_flStartThrow = 0;

		if ( m_pPlayer->m_rgAmmo[ m_iPrimaryAmmoType ] )
		{
			SendWeaponAnim( HANDGRENADE_DRAW );
		}
		else
		{
			RetireWeapon();
			return;
		}

		m_flTimeWeaponIdle = UTIL_WeaponTimeBase() + UTIL_SharedRandomFloat( m_pPlayer->random_seed, 10, 15 );
		m_flReleaseThrow = -1;
		return;
	}

	if ( m_pPlayer->m_rgAmmo[m_iPrimaryAmmoType] )
	{
		int iAnim;
		float flRand = UTIL_SharedRandomFloat( m_pPlayer->random_seed, 0, 1 );
		if (flRand <= 0.75)
		{
			iAnim = HANDGRENADE_IDLE;
			m_flTimeWeaponIdle = UTIL_WeaponTimeBase() + UTIL_SharedRandomFloat( m_pPlayer->random_seed, 10, 15 );// how long till we do this again.
		}
		else 
		{
			iAnim = HANDGRENADE_FIDGET;
			m_flTimeWeaponIdle = UTIL_WeaponTimeBase() + 75.0 / 30.0;
		}

		SendWeaponAnim( iAnim );
	}
}

LINK_ENTITY_TO_CLASS ( flare_grenade, CFlareGrenade );

void CFlareGrenade :: Spawn( void )
{
	pev->movetype = MOVETYPE_BOUNCE;
	pev->classname = MAKE_STRING( "grenade" );
	
	pev->solid = SOLID_BBOX;

	SET_MODEL(ENT(pev), "models/w_flare.mdl");
//	UTIL_SetSize(pev, Vector( -2, -10, -2), Vector(2, 10, 2));

	pev->dmg = 1;
	m_fRegisteredSound = FALSE;

//	pev->angles.x = 90;
//	pev->angles.y = 270;
	pev->sequence = 1;
	UTIL_SetSize(pev, Vector(-3, 0, -3), Vector(3, 20, 3));
}

CFlareGrenade *CFlareGrenade::ShootTimed( entvars_t *pevOwner, Vector vecStart, Vector vecVelocity, float time )
{
	CFlareGrenade *pFlare = GetClassPtr( (CFlareGrenade *)NULL );
	pFlare->Spawn();
	UTIL_SetOrigin( pFlare->pev, vecStart );
	pFlare->pev->velocity = vecVelocity;
//	pFlare->pev->angles = UTIL_VecToAngles(pFlare->pev->velocity);
	pFlare->pev->owner = ENT(pevOwner);

	pFlare->SetTouch( &CFlareGrenade::BounceTouch );

	pFlare->pev->dmgtime = gpGlobals->time + time;
	pFlare->SetThink( &CFlareGrenade::TumbleThink );
	pFlare->pev->nextthink = gpGlobals->time + 0.1;
	if (time < 0.1)
	{
		pFlare->pev->nextthink = gpGlobals->time;
		pFlare->pev->velocity = Vector( 0, 0, 0 );
	}
		
	pFlare->pev->framerate = 1.0;

	pFlare->pev->gravity = 0.5;
	pFlare->pev->friction = 0.8;

	return pFlare;
}

void CFlareGrenade :: TumbleThink( void )
{
	if (!IsInWorld())
	{
		UTIL_Remove( this );
		return;
	}

	StudioFrameAdvance( );
	pev->nextthink = gpGlobals->time + 0.1;

	if (pev->dmgtime <= gpGlobals->time)
	{
		EMIT_SOUND_DYN( ENT(pev), CHAN_WEAPON, "ambience/steamjet1.wav", 1, ATTN_NORM, 0, 125 );

		SetThink( &CFlareGrenade::Ignite );
		m_igniteTime = gpGlobals->time + 15;
	}
	if (pev->waterlevel != 0)
	{
		pev->velocity = pev->velocity * 0.5;
		pev->framerate = 0.2;
	}
}

void CFlareGrenade::Ignite( void )
{
	TraceResult tr;
	Vector		vecSpot;// trace starts here!

	vecSpot = pev->origin + Vector ( 0 , 0 , 8 );
	UTIL_TraceLine ( vecSpot, vecSpot + Vector ( 0, 0, -40 ),  ignore_monsters, ENT(pev), & tr);

#ifndef CLIENT_DLL	
	int iContents = UTIL_PointContents ( pev->origin );
	
	if ( iContents == CONTENTS_WATER )
	{
		UTIL_Bubbles( ( pev->origin + Vector ( 18, 0, 2) ) - pev->velocity * 0.1, pev->origin + Vector ( 0, 18, 2), 2 );
	}
	else { UTIL_Sparks( pev->origin + Vector ( 18, 0, 2) ); }
#endif

	MESSAGE_BEGIN( MSG_PVS, SVC_TEMPENTITY, pev->origin );
			WRITE_BYTE( TE_DLIGHT);		
			WRITE_COORD( pev->origin.x );
			WRITE_COORD( pev->origin.y + 18 );
			WRITE_COORD( pev->origin.z );
			WRITE_BYTE( 24 );	// radius * 0.1
			WRITE_BYTE( 255 );
			WRITE_BYTE( 160 );
			WRITE_BYTE( 160 );
//			WRITE_BYTE( RANDOM_LONG ( 75, 125 ) );		// useless and broken?
			WRITE_BYTE( 1 );	// time * 10
			WRITE_BYTE( 1 );	// decay * 0.1
		MESSAGE_END();

	pev->nextthink = gpGlobals->time + .1;

	if ( gpGlobals->time > m_igniteTime )
	{
		MESSAGE_BEGIN( MSG_PVS, SVC_TEMPENTITY, pev->origin );
			WRITE_BYTE( TE_DLIGHT);		
			WRITE_COORD( pev->origin.x );
			WRITE_COORD( pev->origin.y + 18 );
			WRITE_COORD( pev->origin.z );
			WRITE_BYTE( 24 );	// radius * 0.1
			WRITE_BYTE( 255 );
			WRITE_BYTE( 160 );
			WRITE_BYTE( 160 );
			WRITE_BYTE( 10 );	// time * 10
			WRITE_BYTE( 1 );	// decay * 0.1
		MESSAGE_END();
		UTIL_Remove(this);
		STOP_SOUND( ENT(pev), CHAN_WEAPON, "ambience/steamjet1.wav" );
		SetThink ( NULL );
	}
}

void CFlareGrenade::BounceTouch( CBaseEntity *pOther )
{
	// don't hit the guy that launched this grenade
	if ( pOther->edict() == pev->owner )
		return;

#ifndef CLIENT_DLL
	// only do damage if we're moving fairly fast
	if (m_flNextAttack < gpGlobals->time && pev->velocity.Length() > 100)
	{
		entvars_t *pevOwner = VARS( pev->owner );
		if (pevOwner)
		{
			TraceResult tr = UTIL_GetGlobalTrace( );
			ClearMultiDamage( );
			pOther->TraceAttack(pevOwner, 1, gpGlobals->v_forward, &tr, DMG_CLUB ); 
			ApplyMultiDamage( pev, pevOwner);
		}
		m_flNextAttack = gpGlobals->time + 1.0; // debounce
	}
#endif

	Vector vecTestVelocity;
	// pev->avelocity = Vector (300, 300, 300);

	// this is my heuristic for modulating the grenade velocity because grenades dropped purely vertical
	// or thrown very far tend to slow down too quickly for me to always catch just by testing velocity. 
	// trimming the Z velocity a bit seems to help quite a bit.
	vecTestVelocity = pev->velocity; 
	vecTestVelocity.z *= 0.45;

	if ( !m_fRegisteredSound && vecTestVelocity.Length() <= 60 )
	{
		//ALERT( at_console, "Grenade Registered!: %f\n", vecTestVelocity.Length() );

		// grenade is moving really slow. It's probably very close to where it will ultimately stop moving. 
		// go ahead and emit the danger sound.
		
		// register a radius louder than the explosion, so we make sure everyone gets out of the way
		CSoundEnt::InsertSound ( bits_SOUND_DANGER, pev->origin, pev->dmg / 0.4, 0.3 );
		m_fRegisteredSound = TRUE;
	}

	if (pev->flags & FL_ONGROUND)
	{
		// add a bit of static friction
		pev->velocity = pev->velocity * 0.8;

	}
	else
	{
		// play bounce sound
		BounceSound();
	}
	pev->framerate = pev->velocity.Length() / 200.0;
	if (pev->framerate > 1.0)
		pev->framerate = 1;
	else if (pev->framerate < 0.5)
		pev->framerate = 0;

}

void CFlareGrenade :: BounceSound( void )
{
	switch ( RANDOM_LONG( 0, 2 ) )
	{
	case 0:	EMIT_SOUND(ENT(pev), CHAN_VOICE, "weapons/g_bounce1.wav", 1, ATTN_NORM);	break;
	case 1:	EMIT_SOUND(ENT(pev), CHAN_VOICE, "weapons/g_bounce2.wav", 1, ATTN_NORM);	break;
	case 2:	EMIT_SOUND(ENT(pev), CHAN_VOICE, "weapons/g_bounce3.wav", 1, ATTN_NORM);	break;
	}
}
#endif