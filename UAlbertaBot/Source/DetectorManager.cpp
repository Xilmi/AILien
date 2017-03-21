#include "DetectorManager.h"
#include "BWTA.h"

using namespace UAlbertaBot;

DetectorManager::DetectorManager() : unitClosestToEnemy(nullptr) { }

void DetectorManager::executeMicro(const BWAPI::Unitset & targets) 
{
	//Logger::LogAppendToFile("crash.log", "Entered executeMicro");
	const BWAPI::Unitset & detectorUnits = getUnits();
	if (detectorUnits.empty()) //no need to scout or detect when there's no units left
	{
		return;
	}
	for (size_t i(0); i<targets.size(); ++i)
	{
		// do something here if there's targets
	}

	cloakedUnitMap.clear();
	BWAPI::Unitset cloakedUnits;

	// figure out targets
	for (auto & unit : BWAPI::Broodwar->enemy()->getUnits())
	{
		// conditions for targeting
		if (unit->getType() == BWAPI::UnitTypes::Zerg_Lurker ||
			unit->getType() == BWAPI::UnitTypes::Protoss_Dark_Templar ||
			unit->getType() == BWAPI::UnitTypes::Terran_Wraith) 
		{
			cloakedUnits.insert(unit);
			cloakedUnitMap[unit] = false;
		}
	}

	int detectorUnitsInBattle = 0;
	bool detectorUnitExploring = false;
	std::map<unsigned int, BWAPI::Position> bases;
	unsigned int counter = 0;
	for each(auto base in MapTools::Instance().getScoutLocations())
	{
		bases[counter] = base;
		counter++;
	}

	// for each detectorUnit
	counter = 0;
	for (auto & detectorUnit : detectorUnits)
	{
		//BWAPI::Broodwar->printf("iterate through detectorunits for squad order: %d", order.getType());
		if (order.getType() == SquadOrderTypes::Attack)
		{
			if (detectorUnitsInBattle < double(detectorUnits.size()) && unitClosestToEnemy != nullptr && unitClosestToEnemy->getPosition().isValid() && unitClosestToEnemy->getType() != detectorUnit->getType())
			{
				Micro::SmartMove(detectorUnit, unitClosestToEnemy->getPosition());
				detectorUnitsInBattle++;
			}
		}
		if (counter <= MapTools::Instance().getScoutLocations().size())
		{
			if (order.getType() == SquadOrderTypes::Idle || order.getType() == 0)
			{
				//BWAPI::Broodwar->printf("send detector to least");
				Micro::SmartOviScout(detectorUnit, bases[counter]);
				counter++;
			}
		}
	}
	//Logger::LogAppendToFile("crash.log", "Left executeMicro");
}


BWAPI::Unit DetectorManager::closestCloakedUnit(const BWAPI::Unitset & cloakedUnits, BWAPI::Unit detectorUnit)
{
	BWAPI::Unit closestCloaked = nullptr;
	double closestDist = 100000;

	for (auto & unit : cloakedUnits)
	{
		// if we haven't already assigned an detectorUnit to this cloaked unit
		if (!cloakedUnitMap[unit])
		{
			double dist = unit->getDistance(detectorUnit);

			if (!closestCloaked || (dist < closestDist))
			{
				closestCloaked = unit;
				closestDist = dist;
			}
		}
	}

	return closestCloaked;
}