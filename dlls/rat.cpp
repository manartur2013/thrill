/***
*
*	Copyright (c) 1996-2001, Valve LLC. All rights reserved.
*	
*	This product contains software technology licensed from Id 
*	Software, Inc. ("Id Technology").  Id Technology (c) 1996 Id Software, Inc. 
*	All Rights Reserved.
*
*   This source code contains proprietary and confidential information of
*   Valve LLC and its suppliers.  Access to this code is restricted to
*   persons who have executed a written SDK license with Valve.  Any access,
*   use or distribution of this code by or to any unlicensed person is illegal.
*
****/
//=========================================================
// rat - environmental monster
//=========================================================

#include	"extdll.h"
#include	"util.h"
#include	"cbase.h"
#include	"monsters.h"
#include	"schedule.h"
#include	"soundent.h"

#define		RAT_IDLE				0
#define		RAT_BORED				1
#define		RAT_SCARED_BY_ENT		2
#define		RAT_SCARED_BY_LIGHT		3
#define		RAT_SMELL_FOOD			4
#define		RAT_EAT					5	

//=========================================================
// Monster's Anim Events Go Here
//=========================================================


class CRat : public CBaseMonster
{
public:
	void Spawn( void );
	void Precache( void );
	void SetYawSpeed( void );
	int  Classify ( void );

	void EXPORT MonsterThink ( void );
	void Move ( float flInterval );
	void PickNewDest ( int iCondition );
	void GibMonster( void );

	BOOL	m_fLightHacked;
	int		m_iMode;

	float	m_flLastLightLevel;
	float	m_flNextSmellTime;

};
LINK_ENTITY_TO_CLASS( monster_rat, CRat );

//=========================================================
// Classify - indicates this monster's place in the 
// relationship table.
//=========================================================
int	CRat :: Classify ( void )
{
	return	CLASS_INSECT;
}

//=========================================================
// SetYawSpeed - allows each sequence to have a different
// turn rate associated with it.
//=========================================================
void CRat :: SetYawSpeed ( void )
{
	int ys;

	switch ( m_Activity )
	{
	case ACT_IDLE:
	default:
		ys = 45;
		break;
	}

	pev->yaw_speed = ys;
}

//=========================================================
// Spawn
//=========================================================
void CRat :: Spawn()
{
	Precache( );

	SET_MODEL(ENT(pev), "models/bigrat.mdl");
	UTIL_SetSize(pev, Vector(-8, -4, 0), Vector(4, 4, 8));
	pev->solid			= SOLID_SLIDEBOX;
	pev->movetype		= MOVETYPE_STEP;
	m_bloodColor		= BLOOD_COLOR_RED;
	pev->health			= 8;
	pev->view_ofs		= Vector ( 0, 0, 6 );// position of the eyes relative to monster's origin.
	m_flFieldOfView		= 0.5;// indicates the width of this monster's forward view cone ( as a dotproduct result )
	m_MonsterState		= MONSTERSTATE_NONE;

	MonsterInit();
}

//=========================================================
// Precache - precaches all resources this monster needs
//=========================================================
void CRat :: Precache()
{
	PRECACHE_MODEL("models/bigrat.mdl");
}	

//=========================================================
// AI Schedules Specific to this monster
//=========================================================

//=========================================================
// MonsterThink
//=========================================================
void CRat :: MonsterThink( void  )
{
	CBaseMonster :: MonsterThink();
}

//=========================================================
// PickNewDest
//=========================================================
void CRat :: PickNewDest ( int iCondition )
{
	Vector	vecNewDir;
	Vector	vecDest;
	float	flDist;

	m_iMode = iCondition;

	if ( m_iMode == RAT_SMELL_FOOD )
	{
		// find the food and go there.
		CSound *pSound;

		pSound = CSoundEnt::SoundPointerForIndex( m_iAudibleList );

		if ( pSound )
		{
			m_Route[ 0 ].vecLocation.x = pSound->m_vecOrigin.x + ( 3 - RANDOM_LONG(0,5) );
			m_Route[ 0 ].vecLocation.y = pSound->m_vecOrigin.y + ( 3 - RANDOM_LONG(0,5) );
			m_Route[ 0 ].vecLocation.z = pSound->m_vecOrigin.z;
			m_Route[ 0 ].iType = bits_MF_TO_LOCATION;
			m_movementGoal = RouteClassify( m_Route[ 0 ].iType );
			return;
		}
	}

	do 
	{
		// picks a random spot, requiring that it be at least 128 units away
		// else, the rat will pick a spot too close to itself and run in 
		// circles. this is a hack but buys me time to work on the real monsters.
		vecNewDir.x = RANDOM_FLOAT( -1, 1 );
		vecNewDir.y = RANDOM_FLOAT( -1, 1 );
		flDist		= 256 + ( RANDOM_LONG(0,255) );
		vecDest = pev->origin + vecNewDir * flDist;

	} while ( ( vecDest - pev->origin ).Length2D() < 128 );

	m_Route[ 0 ].vecLocation.x = vecDest.x;
	m_Route[ 0 ].vecLocation.y = vecDest.y;
	m_Route[ 0 ].vecLocation.z = pev->origin.z;
	m_Route[ 0 ].iType = bits_MF_TO_LOCATION;
	m_movementGoal = RouteClassify( m_Route[ 0 ].iType );

	if ( RANDOM_LONG(0,9) == 1 )
	{
		// every once in a while, a rat will play a skitter sound when they decide to run
		EMIT_SOUND_DYN(ENT(pev), CHAN_BODY, "roach/rch_walk.wav", 1, ATTN_NORM, 0, 80 + RANDOM_LONG(0,39) );
	}
}

//=========================================================
// rat's move function
//=========================================================
void CRat :: Move ( float flInterval ) 
{
	float		flWaypointDist;
	Vector		vecApex;

	// local move to waypoint.
	flWaypointDist = ( m_Route[ m_iRouteIndex ].vecLocation - pev->origin ).Length2D();
	MakeIdealYaw ( m_Route[ m_iRouteIndex ].vecLocation );

	ChangeYaw ( pev->yaw_speed );
	UTIL_MakeVectors( pev->angles );

	if ( RANDOM_LONG(0,7) == 1 )
	{
		// randomly check for blocked path.(more random load balancing)
		if ( !WALK_MOVE( ENT(pev), pev->ideal_yaw, 4, WALKMOVE_NORMAL ) )
		{
			// stuck, so just pick a new spot to run off to
			PickNewDest( m_iMode );
		}
	}
	
	WALK_MOVE( ENT(pev), pev->ideal_yaw, m_flGroundSpeed * flInterval, WALKMOVE_NORMAL );

	// if the waypoint is closer than step size, then stop after next step (ok for rat to overshoot)
	if ( flWaypointDist <= m_flGroundSpeed * flInterval )
	{
		// take truncated step and stop

		SetActivity ( ACT_IDLE );
		m_flLastLightLevel = GETENTITYILLUM( ENT ( pev ) );// this is rat's new comfortable light level

		if ( m_iMode == RAT_SMELL_FOOD )
		{
			m_iMode = RAT_EAT;
		}
		else
		{
			m_iMode = RAT_IDLE;
		}
	}

	if ( RANDOM_LONG(0,149) == 1 && m_iMode != RAT_SCARED_BY_LIGHT && m_iMode != RAT_SMELL_FOOD )
	{
		// random skitter while moving as long as not on a b-line to get out of light or going to food
		PickNewDest( FALSE );
	}
}

void CRat :: GibMonster( void )
{
	CGib::SpawnStickyGibs( pev, pev->origin, 4, 0 );
}