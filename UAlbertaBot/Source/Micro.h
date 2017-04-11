#pragma once

#include <Common.h>
#include <BWAPI.h>

namespace UAlbertaBot
{
namespace Micro
{      
    void SmartAttackUnit(BWAPI::Unit attacker, BWAPI::Unit target);
	void SmartPredictAttack(BWAPI::Unit attacker, BWAPI::Unit target);
	void SmartLurker(BWAPI::Unit attacker, BWAPI::Unit target);
    void SmartAttackMove(BWAPI::Unit attacker, const BWAPI::Position & targetPosition);
    void SmartMove(BWAPI::Unit attacker, const BWAPI::Position & targetPosition);
    void SmartRightClick(BWAPI::Unit unit, BWAPI::Unit target);
    void SmartLaySpiderMine(BWAPI::Unit unit, BWAPI::Position pos);
    void SmartRepair(BWAPI::Unit unit, BWAPI::Unit target);
	void SmartKiteTarget(BWAPI::Unit rangedUnit, BWAPI::Unit target, BWAPI::Position kiteGoal = BWAPI::Positions::None);
    void MutaDanceTarget(BWAPI::Unit muta, BWAPI::Unit target, BWAPI::Position fleeto = BWAPI::Positions::None);
	void SmartRegroup(BWAPI::Unit unit, BWAPI::Position regroupPosition, BWAPI::Position concaveFacing, std::map<BWAPI::TilePosition, int> &concaveMap);
	void SmartOviScout(BWAPI::Unit unit, BWAPI::Position scoutLocation, double avoidDistance = 0, bool avoidEverything = true);
	void SmartAvoid(BWAPI::Unit unit, BWAPI::Position avoidPositionm, BWAPI::Position retreatDirection, BWAPI::Position goalPosition, double distance = 400, bool dontSpread = false);
    BWAPI::Position GetKiteVector(BWAPI::Unit unit, BWAPI::Unit target);
	BWAPI::Unit getClosestTarget(BWAPI::Unit rangedUnit, const BWAPI::Unitset & targets);

    void Rotate(double &x, double &y, double angle);
    void Normalize(double &x, double &y);

    void drawAPM(int x, int y);
};
}