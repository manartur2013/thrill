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
// barnacle - stationary ceiling mounted 'fishing' monster
//=========================================================

#include	"extdll.h"
#include	"util.h"
#include	"cbase.h"
#include	"monsters.h"
#include	"schedule.h"


#define	BARNACLE_BODY_HEIGHT	44 // how 'tall' the barnacle's model is.
#define BARNACLE_PULL_SPEED		8
#define BARNACLE_KILL_VICTIM_DELAY	5 // how many seconds after pulling prey in to gib them. 
#define BARNACLE_SEARCH_RADIUS	104	// radius for finding the prey by sphere under barnacle; good values start from 104

#define	SF_BARNACLE_NO_SPHERE	8

//=========================================================
// Monster's Anim Events Go Here
//=========================================================
#define	BARNACLE_AE_PUKEGIB	2

class CBarnacle : public CBaseMonster
{
public:
	void Spawn( void );
	void Precache( void );
	CBaseEntity *TongueTouchEnt ( float *pflLength );
	int  IRelationship ( CBaseEntity *pTarget ); 
	int  Classify ( void );
	void HandleAnimEvent( MonsterEvent_t *pEvent );
	void EXPORT BarnacleThink ( void );
	void EXPORT WaitTillDead ( void );
	void Killed( entvars_t *pevAttacker, int iGib );
	int TakeDamage( entvars_t *pevInflictor, entvars_t *pevAttacker, float flDamage, int bitsDamageType );
	virtual int		Save( CSave &save );
	virtual int		Restore( CRestore &restore );
	static	TYPEDESCRIPTION m_SaveData[];

	float m_flAltitude;
	float m_flKillVictimTime;
	int	  m_cGibs;// barnacle loads up on gibs each time it kills something.
	BOOL  m_fTongueExtended;
	BOOL  m_fLiftingPrey;
	float m_flTongueAdj;
	float m_flWaitForPrey;
	int	  m_iGibType;
};
LINK_ENTITY_TO_CLASS( monster_barnacle, CBarnacle );

TYPEDESCRIPTION	CBarnacle::m_SaveData[] = 
{
	DEFINE_FIELD( CBarnacle, m_flAltitude, FIELD_FLOAT ),
	DEFINE_FIELD( CBarnacle, m_flKillVictimTime, FIELD_TIME ),
	DEFINE_FIELD( CBarnacle, m_cGibs, FIELD_INTEGER ),// barnacle loads up on gibs each time it kills something.
	DEFINE_FIELD( CBarnacle, m_fTongueExtended, FIELD_BOOLEAN ),
	DEFINE_FIELD( CBarnacle, m_fLiftingPrey, FIELD_BOOLEAN ),
	DEFINE_FIELD( CBarnacle, m_flTongueAdj, FIELD_FLOAT ),
	DEFINE_FIELD( CBarnacle, m_flWaitForPrey, FIELD_TIME ),
	DEFINE_FIELD( CBarnacle, m_iGibType, FIELD_INTEGER ),
};

IMPLEMENT_SAVERESTORE( CBarnacle, CBaseMonster );


//=========================================================
// Classify - indicates this monster's place in the 
// relationship table.
//=========================================================
int	CBarnacle :: Classify ( void )
{
	return	CLASS_BARNACLE;	//CLASS_ALIEN_MONSTER;
}

int  CBarnacle :: IRelationship ( CBaseEntity *pTarget )
{
	return R_DL;
}

//=========================================================
// HandleAnimEvent - catches the monster-specific messages
// that occur when tagged animation frames are played.
//
// Returns number of events handled, 0 if none.
//=========================================================
void CBarnacle :: HandleAnimEvent( MonsterEvent_t *pEvent )
{
	switch( pEvent->event )
	{
	case BARNACLE_AE_PUKEGIB:
		CGib::SpawnRandomGibs( pev, 1, m_iGibType );	
		break;
	default:
		CBaseMonster::HandleAnimEvent( pEvent );
		break;
	}
}

//=========================================================
// Spawn
//=========================================================
void CBarnacle :: Spawn()
{
	Precache( );

	SET_MODEL(ENT(pev), "models/barnacle.mdl");
	UTIL_SetSize( pev, Vector(-16, -16, -32), Vector(16, 16, 0) );

	pev->solid			= SOLID_SLIDEBOX;
	pev->movetype		= MOVETYPE_NONE;
	pev->takedamage		= DAMAGE_AIM;
	m_bloodColor		= BLOOD_COLOR_RED;
	pev->effects		= EF_INVLIGHT; // take light from the ceiling 
	pev->health			= 25;
	m_flFieldOfView		= 0.5;// indicates the width of this monster's forward view cone ( as a dotproduct result )
	m_MonsterState		= MONSTERSTATE_NONE;
	m_flKillVictimTime	= 0;
	m_cGibs				= 0;
	m_fLiftingPrey		= FALSE;
	m_flTongueAdj		= -100;
//	m_flTongueAdj		= 0;

	InitBoneControllers();

	SetActivity ( ACT_IDLE );

	SetThink ( &CBarnacle::BarnacleThink );
	pev->nextthink = gpGlobals->time + 0.5;

	UTIL_SetOrigin ( pev, pev->origin );
}

int CBarnacle::TakeDamage( entvars_t *pevInflictor, entvars_t *pevAttacker, float flDamage, int bitsDamageType )
{
	if ( bitsDamageType & DMG_CLUB )
	{
		flDamage = pev->health;
	}

	return CBaseMonster::TakeDamage( pevInflictor, pevAttacker, flDamage, bitsDamageType );
}

//=========================================================
//=========================================================
void CBarnacle :: BarnacleThink ( void )
{
	CBaseEntity *pTouchEnt;
	CBaseMonster *pVictim;
	float flLength;

	pev->nextthink = gpGlobals->time + 0.1;

	if ( m_hEnemy != NULL )
	{
// barnacle has prey.

		if ( m_hEnemy->pev->movetype == MOVETYPE_NOCLIP )
		{
			pVictim = m_hEnemy->MyMonsterPointer();
			if ( pVictim )
			{
				pVictim->BarnacleVictimReleased();
				m_fLiftingPrey = FALSE;// indicate that we're not lifting prey.
				m_hEnemy = NULL;
				return;
			}
		}

		if ( !m_hEnemy->IsAlive() )
		{
			// someone (maybe even the barnacle) killed the prey. Reset barnacle.
			m_fLiftingPrey = FALSE;// indicate that we're not lifting prey.
			m_hEnemy = NULL;
			return;
		}

		if ( m_fLiftingPrey )
		{
			if ( m_hEnemy != NULL && m_hEnemy->pev->deadflag != DEAD_NO )
			{
				// crap, someone killed the prey on the way up.
				m_hEnemy = NULL;
				m_fLiftingPrey = FALSE;
				return;
			}

	// still pulling prey.
			Vector vecNewEnemyOrigin = m_hEnemy->pev->origin;
			vecNewEnemyOrigin.x = pev->origin.x;
			vecNewEnemyOrigin.y = pev->origin.y;

			// guess as to where their neck is
			vecNewEnemyOrigin.x -= 6 * cos(m_hEnemy->pev->angles.y * M_PI/180.0);	
			vecNewEnemyOrigin.y -= 6 * sin(m_hEnemy->pev->angles.y * M_PI/180.0);

			m_flAltitude -= BARNACLE_PULL_SPEED;
			vecNewEnemyOrigin.z += BARNACLE_PULL_SPEED;

			if ( fabs( pev->origin.z - ( vecNewEnemyOrigin.z + m_hEnemy->pev->view_ofs.z - 8 ) ) < BARNACLE_BODY_HEIGHT )
			{
		// prey has just been lifted into position ( if the victim origin + eye height + 8 is higher than the bottom of the barnacle, it is assumed that the head is within barnacle's body )
				m_fLiftingPrey = FALSE;

				EMIT_SOUND( ENT(pev), CHAN_WEAPON, "barnacle/bcl_bite3.wav", 1, ATTN_NORM );	

				pVictim = m_hEnemy->MyMonsterPointer();

				m_flKillVictimTime = gpGlobals->time + 10;// now that the victim is in place, the killing bite will be administered in 10 seconds.

				if ( pVictim )
				{
					ALERT( at_console, "the dinner is here\n" );
					pVictim->BarnacleVictimBitten( pev );
					SetActivity ( ACT_EAT );
				}
			}

			UTIL_SetOrigin ( m_hEnemy->pev, vecNewEnemyOrigin );
		}
		else
		{
	// prey is lifted fully into feeding position and is dangling there.

			pVictim = m_hEnemy->MyMonsterPointer();

			if ( m_flKillVictimTime != -1 && gpGlobals->time > m_flKillVictimTime )
			{
				// kill!
				if ( pVictim )
				{
					ALERT( at_console, "CHOMP\n" );
					pVictim->TakeDamage ( pev, pev, pVictim->pev->health, DMG_SLASH | DMG_ALWAYSGIB );
					m_iGibType = pVictim->MainGib();

					if ( pVictim->IsAlive() )
					{
						// somehow, the prey survived it
						ALERT( at_console, "WHAT\n" );
						pVictim->Killed( pev, GIB_ALWAYS );
					}
					m_cGibs = 3;
				}

				return;
			}

			// bite prey every once in a while
			if ( pVictim && ( RANDOM_LONG(0,49) == 0 ) )
			{
				switch ( RANDOM_LONG(0,2) )
				{
				case 0:	EMIT_SOUND( ENT(pev), CHAN_WEAPON, "barnacle/bcl_chew1.wav", 1, ATTN_NORM );	break;
				case 1:	EMIT_SOUND( ENT(pev), CHAN_WEAPON, "barnacle/bcl_chew2.wav", 1, ATTN_NORM );	break;
				case 2:	EMIT_SOUND( ENT(pev), CHAN_WEAPON, "barnacle/bcl_chew3.wav", 1, ATTN_NORM );	break;
				}

				pVictim->BarnacleVictimBitten( pev );
			}

		}
	}
	else
	{
// barnacle has no prey right now, so just idle and check to see if anything is touching the tongue.

		// If idle and no nearby client, don't think so often
		if ( FNullEnt( FIND_CLIENT_IN_PVS( edict() ) ) )
			pev->nextthink = gpGlobals->time + RANDOM_FLOAT(1,1.5);	// Stagger a bit to keep barnacles from thinking on the same frame

		if ( m_fSequenceFinished )
		{// this is done so barnacle will fidget.
			SetActivity ( ACT_IDLE );
			m_flTongueAdj = -100;
		}

		if ( m_cGibs && RANDOM_LONG(0,99) == 1 )
		{
			// cough up a gib.
			CGib::SpawnRandomGibs( pev, 1, m_iGibType );
			m_cGibs--;

			switch ( RANDOM_LONG(0,2) )
			{
			case 0:	EMIT_SOUND( ENT(pev), CHAN_WEAPON, "barnacle/bcl_chew1.wav", 1, ATTN_NORM );	break;
			case 1:	EMIT_SOUND( ENT(pev), CHAN_WEAPON, "barnacle/bcl_chew2.wav", 1, ATTN_NORM );	break;
			case 2:	EMIT_SOUND( ENT(pev), CHAN_WEAPON, "barnacle/bcl_chew3.wav", 1, ATTN_NORM );	break;
			}
		}

		pTouchEnt = TongueTouchEnt( &flLength );

		if ( pTouchEnt != NULL && pev->origin.z - pTouchEnt->EyePosition().z <= m_flAltitude )
			m_fTongueExtended = TRUE;

		if ( pTouchEnt != NULL && m_fTongueExtended && pTouchEnt->pev->movetype != MOVETYPE_NOCLIP )
		{
			// tongue is fully extended, and is touching someone.
			if ( pTouchEnt->FBecomeProne() )
			{
				EMIT_SOUND( ENT(pev), CHAN_WEAPON, "barnacle/bcl_alert2.wav", 1, ATTN_NORM );	

				SetSequenceByName ( "attack1" );
				m_flTongueAdj = -20;

				m_hEnemy = pTouchEnt;

				pTouchEnt->pev->movetype = MOVETYPE_FLY;
				pTouchEnt->pev->velocity = g_vecZero;
				pTouchEnt->pev->basevelocity = g_vecZero;
				pTouchEnt->pev->origin.x = pev->origin.x;
				pTouchEnt->pev->origin.y = pev->origin.y;

				m_fLiftingPrey = TRUE;// indicate that we should be lifting prey.
				m_flKillVictimTime = -1;// set this to a bogus time while the victim is lifted.

				m_flAltitude = (pev->origin.z - pTouchEnt->EyePosition().z);
			}
		}
		else
		{
			if ( !m_fLiftingPrey && !m_cGibs )	// don't bother with traces if pulling a target or already full
			{
				TraceResult tr;
				CBaseMonster *pEntity = NULL;
				bool traceSuccessful = FALSE;

				UTIL_TraceHull(pev->origin, pev->origin - Vector( 0, 0, flLength ), dont_ignore_monsters, human_hull, edict(), &tr);
				if ( tr.flFraction != 1.0 )
				{
					pEntity = (CBaseMonster*)Instance(tr.pHit);
					if ( pEntity && pEntity->IsAlive() && pEntity->Classify() && pEntity->Classify() != CLASS_BARNACLE
						&& pEntity->Classify() != CLASS_INSECT )
					{
						if ( pEntity->pev->origin.z < pev->origin.z )
						{
							ALERT( at_console, "CBarnacle: Found prey via tracehull!\n" );
							traceSuccessful = TRUE;
							m_flWaitForPrey = gpGlobals->time + 3.0;
						}
					}
				}

				if ( !traceSuccessful && ( gpGlobals->time > m_flWaitForPrey + 5.0 || !m_flWaitForPrey ) && !FBitSet( pev->spawnflags, SF_BARNACLE_NO_SPHERE ) ) // don't perform sphere checks too often
				{
					Vector vecCenter;
					float searchRadius;

					if ( flLength >= 1024 )
						searchRadius = 128;
					else if ( flLength >= 512 )
						searchRadius = 104;
					else if ( flLength > 384 )
						searchRadius = 72;
					else if ( flLength > 256 )
						searchRadius = 64;

					vecCenter = pev->origin;
					vecCenter.z -= flLength - searchRadius;

					pEntity = (CBaseMonster*)UTIL_FindEntityInSphere( pEntity, vecCenter, searchRadius );

					if ( pEntity && pEntity->Classify() && pEntity->Classify() != CLASS_BARNACLE 
						&& pEntity->Classify() != CLASS_INSECT )
					{
						ALERT( at_console, "CBarnacle: Found prey via sphere!\n" );
						m_flWaitForPrey = gpGlobals->time + 3.0;
					}
				}

			}

			// calculate a new length for the tongue to be clear of anything else that moves under it. 

			if ( gpGlobals->time <= m_flWaitForPrey )
			{
				if ( m_flAltitude < flLength )
				{
					m_flAltitude += BARNACLE_PULL_SPEED*8;
					pev->nextthink = gpGlobals->time + 0.05;
				}
				else
				{
					m_flAltitude = flLength;
				}
			}
			else
			{
				if ( m_flAltitude > 0 )
				{
					m_flAltitude -= BARNACLE_PULL_SPEED*4;
					m_flWaitForPrey	= gpGlobals->time - 1.0;
				}
				else
				{
					m_fTongueExtended = FALSE;
					m_flAltitude = 0;
				}
			}
			
		}

	}

	// ALERT( at_console, "tounge %f\n", m_flAltitude + m_flTongueAdj );
	SetBoneController( 0, -(m_flAltitude + m_flTongueAdj) );
	StudioFrameAdvance( 0.1 );
}

//=========================================================
// Killed.
//=========================================================
void CBarnacle :: Killed( entvars_t *pevAttacker, int iGib )
{
	CBaseMonster *pVictim;

	pev->solid = SOLID_NOT;
	pev->takedamage = DAMAGE_NO;

	if ( m_hEnemy != NULL )
	{
		pVictim = m_hEnemy->MyMonsterPointer();

		if ( pVictim )
		{
			pVictim->BarnacleVictimReleased();
		}
	}

//	CGib::SpawnRandomGibs( pev, 4, 1 );

	switch ( RANDOM_LONG ( 0, 1 ) )
	{
	case 0:	EMIT_SOUND( ENT(pev), CHAN_WEAPON, "barnacle/bcl_die1.wav", 1, ATTN_NORM );	break;
	case 1:	EMIT_SOUND( ENT(pev), CHAN_WEAPON, "barnacle/bcl_die3.wav", 1, ATTN_NORM );	break;
	}
	
	SetActivity ( ACT_DIESIMPLE );
	SetBoneController( 0, 0 );

	StudioFrameAdvance( 0.1 );

	pev->nextthink = gpGlobals->time + 0.1;
	SetThink ( &CBarnacle::WaitTillDead );
}

//=========================================================
//=========================================================
void CBarnacle :: WaitTillDead ( void )
{
	pev->nextthink = gpGlobals->time + 0.1;

	float flInterval = StudioFrameAdvance( 0.1 );
	DispatchAnimEvents ( flInterval );

	if ( m_fSequenceFinished )
	{
		// death anim finished. 
		StopAnimation();
		SetThink ( NULL );
	}
}

//=========================================================
// Precache - precaches all resources this monster needs
//=========================================================
void CBarnacle :: Precache()
{
	PRECACHE_MODEL("models/barnacle.mdl");

	PRECACHE_SOUND("barnacle/bcl_alert2.wav");//happy, lifting food up
	PRECACHE_SOUND("barnacle/bcl_bite3.wav");//just got food to mouth
	PRECACHE_SOUND("barnacle/bcl_chew1.wav");
	PRECACHE_SOUND("barnacle/bcl_chew2.wav");
	PRECACHE_SOUND("barnacle/bcl_chew3.wav");
	PRECACHE_SOUND("barnacle/bcl_die1.wav" );
	PRECACHE_SOUND("barnacle/bcl_die3.wav" );
}	

//=========================================================
// TongueTouchEnt - does a trace along the barnacle's tongue
// to see if any entity is touching it. Also stores the length
// of the trace in the int pointer provided.
//=========================================================
#define BARNACLE_CHECK_SPACING	8
CBaseEntity *CBarnacle :: TongueTouchEnt ( float *pflLength )
{
	TraceResult	tr;
	float		length;

	// trace once to hit architecture and see if the tongue needs to change position.
	UTIL_TraceLine ( pev->origin, pev->origin - Vector ( 0 , 0 , 2048 ), ignore_monsters, ENT(pev), &tr );
	length = fabs( pev->origin.z - tr.vecEndPos.z );
	if ( pflLength )
	{
		*pflLength = length;
	}

	Vector delta = Vector( BARNACLE_CHECK_SPACING, BARNACLE_CHECK_SPACING, 0 );
	Vector mins = pev->origin - delta;
	Vector maxs = pev->origin + delta;
	maxs.z = pev->origin.z;
	mins.z -= length;

	CBaseEntity *pList[10];
	int count = UTIL_EntitiesInBox( pList, 10, mins, maxs, (FL_CLIENT|FL_MONSTER) );
	if ( count )
	{
		for ( int i = 0; i < count; i++ )
		{
			// only clients and monsters
		//	if ( pList[i] != this && IRelationship( pList[i] ) > R_NO && pList[i]->MyMonsterPointer()->IsBarnaclePrey() && pList[ i ]->pev->deadflag == DEAD_NO )	// this ent is one of our enemies. Barnacle tries to eat it.
			if ( pList[i] != this && pList[i]->MyMonsterPointer()->IsBarnaclePrey() && pList[ i ]->pev->deadflag == DEAD_NO ) // the relationship check is excessive now that we have the IsBarnaclePrey function
			{
				ALERT (at_console, "MA FOAH\n");
				return pList[i];
			}
		}
	}

	return NULL;
}
