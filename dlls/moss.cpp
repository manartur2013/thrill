/*

===== moss.cpp ========================================================

  growing moss entity

*/

#include "extdll.h"
#include "util.h"
#include "cbase.h"
#include "decals.h"

class CGrowingMoss : public CBaseMonster
{
public:
	void Spawn( void );
	void EXPORT GrowthThink( void );
	void EXPORT AddThink( void );

	float growthBegin;
};

LINK_ENTITY_TO_CLASS( env_moss, CGrowingMoss );

void CGrowingMoss :: Spawn ( void )
{
	SetThink( &CGrowingMoss::AddThink );
	pev->nextthink = gpGlobals->time;
	pev->skin = gDecals[ DECAL_MOSS1 ].index;

	growthBegin = gpGlobals->time + 5.0;
}

void CGrowingMoss :: AddThink( void )
{
	ALERT ( at_console, "AddThink!\n" );

	TraceResult trace;
	int			entityIndex, modelIndex;

	UTIL_TraceLine( pev->origin - Vector(5,5,5), pev->origin + Vector(5,5,5),  ignore_monsters, ENT(pev), &trace );

	MESSAGE_BEGIN( MSG_BROADCAST, SVC_TEMPENTITY);
		WRITE_BYTE( TE_BSPDECAL );
		WRITE_COORD( pev->origin.x );
		WRITE_COORD( pev->origin.y );
		WRITE_COORD( pev->origin.z );
		WRITE_SHORT( (int)pev->skin );
		entityIndex = (short)ENTINDEX(trace.pHit);
		WRITE_SHORT( entityIndex );
		if ( entityIndex )
			WRITE_SHORT( (int)VARS(trace.pHit)->modelindex );
	MESSAGE_END(); 

	SetThink( &CGrowingMoss::SUB_Remove );
	pev->nextthink = gpGlobals->time + 0.1;

	entityIndex = (short)ENTINDEX(trace.pHit);
	if ( entityIndex )
		modelIndex = (int)VARS(trace.pHit)->modelindex;
	else
		modelIndex = 0;

	g_engfuncs.pfnStaticDecal( pev->origin, (int)pev->skin, entityIndex, modelIndex );

//	SetThink( &CGrowingMoss::GrowthThink );
	CGrowingMoss :: GrowthThink();
	SUB_Remove();
}

void CGrowingMoss :: GrowthThink( void )
{
	pev->nextthink = gpGlobals->time + 0.1;

	ALERT ( at_console, "GrowthThink!\n" );

	if ( gpGlobals->time > growthBegin )
	{
		ALERT ( at_console, "GrowthThink: Phase two!\n" );
		pev->skin = gDecals[ DECAL_MOSS2 ].index;
	}
	else if ( gpGlobals->time - growthBegin == 4.0 )
	{
		ALERT ( at_console, "GrowthThink: Phase three!\n" );
		pev->skin = gDecals[ DECAL_MOSS3 ].index;
	}	
	else if ( gpGlobals->time - growthBegin == 6.0 )
	{
		ALERT ( at_console, "GrowthThink: Phase four!\n" );
		pev->skin = gDecals[ DECAL_MOSS4 ].index;
	}
//	else
//	{ CGrowingMoss :: GrowthThink(); }

}

