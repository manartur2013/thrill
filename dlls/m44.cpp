#include	"extdll.h"
#include	"util.h"
#include	"cbase.h"
#include	"monsters.h"
#include	"weapons.h"

class CM44 : public CBaseMonster
{
	public:
	void Spawn( void );
	void Precache( void );
	int Classify( void ) { return CLASS_NONE; }
	int	BloodColor( void ) { return DONT_BLEED; }
	int TakeDamage( entvars_t *pevInflictor, entvars_t *pevAttacker, float flDamage, int bitsDamageType );
	void ShowDamage( void );
	void EXPORT TruckThink ( void );

	BOOL IsBarnaclePrey( void ) { return TRUE; }
	void Killed( entvars_t *pevAttacker, int iGib );
	void GibMonster( void );

	int m_truckSound;
	int m_iDoSmokePuff;
};

LINK_ENTITY_TO_CLASS( monster_m44, CM44 );

void CM44 :: Spawn( void )
{
	Precache();
	SET_MODEL(ENT(pev), "models/m44.mdl");
	UTIL_SetSize( pev, Vector(-32,-32,0), Vector(32,32,64));
	pev->solid			= SOLID_BBOX;
	pev->movetype		= MOVETYPE_STEP;
	UTIL_SetOrigin( pev, pev->origin );

	pev->flags |= FL_MONSTER;
	pev->takedamage		= DAMAGE_YES;

	pev->health			= 200;

	m_truckSound = RANDOM_LONG ( 0,1 );
	SetThink( &CM44::TruckThink );

	MonsterInit();
}

void CM44 :: Precache( void )
{
	PRECACHE_MODEL( "models/m44.mdl" );
	PRECACHE_SOUND( "ambience/truck1.wav" );
	PRECACHE_SOUND( "ambience/truck2.wav" );
}

int CM44 :: TakeDamage( entvars_t *pevInflictor, entvars_t *pevAttacker, float flDamage, int bitsDamageType )
{
	if ( bitsDamageType & DMG_CLUB  )
	{	return 0;	}
	else
	{
		return CBaseEntity::TakeDamage(  pevInflictor, pevAttacker, flDamage, bitsDamageType );
		m_iDoSmokePuff = 3 + (flDamage / 5.0);	
	}

	if ( (bitsDamageType & DMG_BULLET) && flDamage > 15)
	{
		flDamage = 15;
	}
}

void CM44 :: ShowDamage( void )
{
	if (m_iDoSmokePuff > 0 || RANDOM_LONG(0,99) > pev->health)
	{
		MESSAGE_BEGIN( MSG_PVS, SVC_TEMPENTITY, pev->origin );
			WRITE_BYTE( TE_SMOKE );
			WRITE_COORD( pev->origin.x + 48 );
			WRITE_COORD( pev->origin.y );
			WRITE_COORD( pev->origin.z + 32 );
			WRITE_SHORT( g_sModelIndexSmoke );
			WRITE_BYTE( RANDOM_LONG(0,9) + 20 ); // scale * 10
			WRITE_BYTE( 12 ); // framerate
		MESSAGE_END();
	}
	if (m_iDoSmokePuff > 0)
		m_iDoSmokePuff--;
}

void CM44 :: TruckThink( void )
{
	pev->nextthink = gpGlobals->time + 0.1;

	ShowDamage( );
	if ( m_truckSound == 0 )
	{ EMIT_SOUND(ENT(pev), CHAN_WEAPON, "ambience/truck1.wav", 1, 0.3); }
	else
	{ EMIT_SOUND(ENT(pev), CHAN_WEAPON, "ambience/truck2.wav", 1, 0.3); }
}

void CM44 :: GibMonster( void )
{
			
}

void CM44 :: Killed( entvars_t *pevAttacker, int iGib )
{
	pev->gravity = 0.3;
	pev->health = 0;
	pev->takedamage = DAMAGE_NO;

	pev->avelocity.z += 16;
	STOP_SOUND(ENT(pev), CHAN_WEAPON, "ambience/truck2.wav");

	MESSAGE_BEGIN( MSG_PVS, SVC_TEMPENTITY, pev->origin );
		WRITE_BYTE( TE_EXPLOSION);		// This just makes a dynamic light now
		WRITE_COORD( pev->origin.x + RANDOM_FLOAT( -50, 50 ));
		WRITE_COORD( pev->origin.y + RANDOM_FLOAT( -50, 50 ));
		WRITE_COORD( pev->origin.z + 56.0 );
		WRITE_SHORT( g_sModelIndexFireball );
		WRITE_BYTE( RANDOM_LONG(0,29) + 30  ); // scale * 10
		WRITE_BYTE( 12  ); // framerate
		WRITE_BYTE( TE_EXPLFLAG_NONE );
	MESSAGE_END();

	UTIL_Remove(this);
}