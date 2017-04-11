#pragma once

#include "Common.h"
#include "CombatCommander.h"
#include "InformationManager.h"
#include "MapGrid.h"
#include "WorkerManager.h"
#include "ProductionManager.h"
#include "BuildingManager.h"
#include "ScoutManager.h"
#include "StrategyManager.h"
#include "TimerManager.h"

namespace UAlbertaBot
{


class UnitToAssign
{
public:

	BWAPI::Unit unit;
	bool isAssigned;

	UnitToAssign(BWAPI::Unit u)
	{
		unit = u;
		isAssigned = false;
	}
};

class GameCommander 
{
	CombatCommander		    _combatCommander;
	TimerManager		    _timerManager;

	BWAPI::Unitset          _validUnits;
	BWAPI::Unitset          _combatUnits;
	BWAPI::Unitset          _scoutUnits;

    bool                    _initialScoutSet;

    void                    assignUnit(BWAPI::Unit unit, BWAPI::Unitset & set);
	bool                    isAssigned(BWAPI::Unit unit) const;
	std::string				_strategy;
	std::string				_startingstrategy;
	std::string				_sversion;
	std::map<int, std::string> strategyMap;

public:
	GameCommander();
	~GameCommander() {};

	void update();

	void handleUnitAssignments();
	void setValidUnits();
	void setScoutUnits();
	void setCombatUnits();
	void onGameStart();
	void onEnd(bool victory);

	void drawDebugInterface();
    void drawGameInformation(int x, int y);

	BWAPI::Unit getFirstSupplyProvider();
	BWAPI::Unit getClosestUnitToTarget(BWAPI::UnitType type, BWAPI::Position target);
	BWAPI::Unit getClosestWorkerToTarget(BWAPI::Position target);

	void onUnitShow(BWAPI::Unit unit);
	void onUnitHide(BWAPI::Unit unit);
	void onUnitCreate(BWAPI::Unit unit);
	void onUnitComplete(BWAPI::Unit unit);
	void onUnitRenegade(BWAPI::Unit unit);
	void onUnitDestroy(BWAPI::Unit unit);
	void onUnitMorph(BWAPI::Unit unit);

	double macroHeavyness;
	double sunkensPerWorkerSupply;

	double droneScore = 0;
	double lingScore = 0;
	double hydraScore = 0;
	double lurkerScore = 0;
	double mutaScore = 0;
	double ultraScore = 0;
	double guardScore = 0;
	double enemyUnitCost = 0;
	double enemyAirCost = 0;
	double enemyAntiAirCost = 0;
	double scorePerFrame = 0;
	double highestScorePerFrame = 0;
	int highestScoreFrame = 0;
};

}