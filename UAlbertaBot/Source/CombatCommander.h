#pragma once

#include "Common.h"
#include "Squad.h"
#include "SquadData.h"
#include "InformationManager.h"
#include "StrategyManager.h"

namespace UAlbertaBot
{
class CombatCommander
{
	SquadData       _squadData;
    BWAPI::Unitset  _combatUnits;
    bool            _initialized;
	bool			_broodForce; //if set all units will attack regardless of simulation-results

	void			command();
	void			handleOverlords(BWAPI::Unit unitClosestToEnemy);
	void            updateScoutDefenseSquad();
	void            updateAttackSquads();
    void            updateDropSquads();
	void            updateIdleSquad();
	bool            isSquadUpdateFrame();
	int             getNumType(BWAPI::Unitset & units, BWAPI::UnitType type);
	double			getForcePower(std::vector<UnitInfo> units, std::vector<UnitInfo> enemyunits, BWAPI::Position pos, double& averageHPRatio, bool mine = false);

	BWAPI::Unit     findClosestDefender(const Squad & defenseSquad, BWAPI::Position pos, bool flyingDefender);
	UAlbertaBot::UnitInfo findClosestEnemy(BWAPI::Position pos, bool ignoreFlyers = false, bool iAmFlyer = false, bool considerRange = false);
	UAlbertaBot::UnitInfo findBestAttackTarget(BWAPI::Unit unit, BWAPI::Position pos, double radius);
    BWAPI::Unit     findClosestWorkerToTarget(BWAPI::Unitset & unitsToAssign, BWAPI::Unit target);

	BWAPI::Position getDefendLocation();
    BWAPI::Position getMainAttackLocation();
	BWAPI::Position getMyAttackLocation(BWAPI::Unit unit);

    void            initializeSquads();
    void            verifySquadUniqueMembership();
    void            assignFlyingDefender(Squad & squad);
    void            emptySquad(Squad & squad, BWAPI::Unitset & unitsToAssign);
    int             getNumGroundDefendersInSquad(Squad & squad);
    int             getNumAirDefendersInSquad(Squad & squad);

    void            updateDefenseSquadUnits(Squad & defenseSquad, const size_t & flyingDefendersNeeded, const size_t & groundDefendersNeeded);
    int             defendWithWorkers();

    int             numZerglingsInOurBase();
    bool            beingBuildingRushed();

	double			myTotalCost = 0;
	double			enemyTotalCost = 0;
public:

	CombatCommander();

	void update(const BWAPI::Unitset & combatUnits, double myCost = 0, double enemyCost = 0);
    
	void drawSquadInformation(int x, int y);
};
}
