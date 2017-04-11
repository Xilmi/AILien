#include "CombatCommander.h"
#include "UnitUtil.h"
#include "InformationManager.h"
#include "Micro.h"
#include "ProductionManager.h"

using namespace UAlbertaBot;

const size_t IdlePriority = 0;
const size_t AttackPriority = 1;
const size_t BaseDefensePriority = 2;
const size_t ScoutDefensePriority = 3;
const size_t DropPriority = 4;

CombatCommander::CombatCommander() 
    : _initialized(false)
{

}

void CombatCommander::initializeSquads()
{
    SquadOrder idleOrder(SquadOrderTypes::Idle, BWAPI::Position(BWAPI::Broodwar->self()->getStartLocation()), 100, "Chill Out");
	_squadData.addSquad("Idle", Squad("Idle", idleOrder, IdlePriority));

    // the main attack squad that will pressure the enemy's closest base location
    SquadOrder mainAttackOrder(SquadOrderTypes::Attack, getMainAttackLocation(), 800, "Attack Enemy Base");
	_squadData.addSquad("MainAttack", Squad("MainAttack", mainAttackOrder, AttackPriority));
	// same as above but for Air-units, will potentially got it's own targets
	_squadData.addSquad("AirAttack", Squad("AirAttack", mainAttackOrder, AttackPriority));

    BWAPI::Position ourBasePosition = BWAPI::Position(BWAPI::Broodwar->self()->getStartLocation());

    // the scout defense squad will handle chasing the enemy worker scout
    SquadOrder enemyScoutDefense(SquadOrderTypes::Defend, ourBasePosition, 900, "Get the scout");
    _squadData.addSquad("ScoutDefense", Squad("ScoutDefense", enemyScoutDefense, ScoutDefensePriority));

    // add a drop squad if we are using a drop strategy
    if (Config::Strategy::StrategyName == "Protoss_Drop")
    {
        SquadOrder zealotDrop(SquadOrderTypes::Drop, ourBasePosition, 900, "Wait for transport");
        _squadData.addSquad("Drop", Squad("Drop", zealotDrop, DropPriority));
    }

    _initialized = true;
}

bool CombatCommander::isSquadUpdateFrame()
{
	return BWAPI::Broodwar->getFrameCount() % 10 == 0;
}

void CombatCommander::update(const BWAPI::Unitset & combatUnits, double myCost, double enemyCost)
{
	myTotalCost = myCost;
	enemyTotalCost = enemyCost;
    if (!Config::Modules::UsingCombatCommander)
    {
        return;
    }
    _combatUnits = combatUnits;

	if (!_combatUnits.empty())
	{
		command();
	}
}

void CombatCommander::command()
{
	BWAPI::Position retreatLocation = MapTools::Instance().getBaseCenter();

	double closestSunkenDistance = 10000;
	BWAPI::Unit closestSunken = nullptr;
	for each (auto sunken in BWAPI::Broodwar->self()->getUnits())
	{
		if (sunken->getType() != BWAPI::UnitTypes::Zerg_Sunken_Colony)
		{
			continue;
		}
		double distance = retreatLocation.getDistance(sunken->getPosition());
		{
			if (distance < closestSunkenDistance)
			{
				closestSunken = sunken;
				closestSunkenDistance = distance;
			}
		}
		UAlbertaBot::UnitInfo enemyToAttack = findBestAttackTarget(sunken, sunken->getPosition(), sunken->getType().groundWeapon().maxRange()); //sunkens should also focus
		if (enemyToAttack.unitID != 0)
		{
			Micro::SmartAttackUnit(sunken, enemyToAttack.unit);
		}
		if (sunken->isAttacking()) //If a sunken is already attacking, then it doesn't matter how close it is, it is the one we want to support and near which we attack
		{
			closestSunken = sunken;
			closestSunkenDistance = distance;
			break;
		}
	}
	if (closestSunken != nullptr)
	{
		retreatLocation = closestSunken->getPosition();
	}
	else
	{
		for each (auto building in BWAPI::Broodwar->self()->getUnits())
		{
			if (!building->getType().isBuilding())
			{
				continue;
			}
			double distance = retreatLocation.getDistance(building->getPosition());
			{
				if (distance < closestSunkenDistance)
				{
					closestSunken = building;
					closestSunkenDistance = distance;
				}
			}
			if (building->isUnderAttack()) //If a building is under attack...
			{
				closestSunken = building;
				closestSunkenDistance = distance;
				break;
			}
		}
	}
	if (closestSunken != nullptr)
	{
		retreatLocation = closestSunken->getPosition();
	}

	int considerationRange = 512;
	
	double closestToEnemy = 10000;
	BWAPI::Unit unitClosestToEnemy = nullptr;

	if (BWAPI::Broodwar->self()->supplyUsed() + BWAPI::Broodwar->self()->minerals() / 25 >= 500 
		&& _broodForce == false)
	{
		_broodForce = true;
		BWAPI::Broodwar->sendText("Broodforce activated!");
	}
	else if (BWAPI::Broodwar->self()->supplyUsed() + BWAPI::Broodwar->self()->minerals() / 25 < 400)
	{
		_broodForce = false;
	}
	int turntakers = BWAPI::Broodwar->getFrameCount() % 3;
	for each (auto ui in InformationManager::Instance().getUnitInfo(BWAPI::Broodwar->self()))
	{
		BWAPI::Unit unit = ui.first;
		if (!UnitUtil::IsCombatUnit(unit))
		{
			continue;
		}
		if (unit->getType().isBuilding() || unit->getType().isWorker() || unit->getType().isDetector())
		{
			continue;
		}
		if (unit->getID() % 3 != turntakers)
		{
			continue;
		}
		bool ignoreFlyers = false;
		bool ignoreNoAntiAir = false;
		bool ignoreAirToAir = true;
		if (unit->getType().airWeapon() == BWAPI::WeaponTypes::None)
		{
			ignoreFlyers = true;
		}
		if (unit->getType().isFlyer())
		{
			ignoreNoAntiAir = true;
		}
		if (!unit->getType().isFlyer() && unit->getType().airWeapon() != BWAPI::WeaponTypes::None)
		{
			ignoreAirToAir = true;
		}
		UAlbertaBot::UnitInfo closestEnemy = findClosestEnemy(unit->getPosition(), ignoreFlyers, ignoreNoAntiAir, true);
		BWAPI::Position attackLocation = getMyAttackLocation(unit);
		//BWAPI::Broodwar->drawLineMap(unit->getPosition(), attackLocation, BWAPI::Colors::Cyan);
		//BWAPI::Broodwar->drawLineMap(unit->getPosition(), retreatLocation, BWAPI::Colors::Yellow);
		if (closestEnemy.unitID != 0)
		{
			//BWAPI::Broodwar->drawLineMap(unit->getPosition(), closestEnemy.lastPosition, BWAPI::Colors::Red);
			double avoidRange = 0;

			if (ignoreNoAntiAir)
			{
				avoidRange = closestEnemy.type.airWeapon().maxRange();
				if (closestEnemy.type == BWAPI::UnitTypes::Terran_Goliath)
				{
					avoidRange += 3 * 32;
				}
			}
			else
			{
				avoidRange = closestEnemy.type.groundWeapon().maxRange();
			}
			if (closestEnemy.type == BWAPI::UnitTypes::Terran_Bunker)
			{
				avoidRange = BWAPI::UnitTypes::Terran_Marine.groundWeapon().maxRange() + 64;
			}
			if (closestEnemy.type == BWAPI::UnitTypes::Terran_Marine
				|| closestEnemy.type == BWAPI::UnitTypes::Zerg_Hydralisk
				|| closestEnemy.type == BWAPI::UnitTypes::Protoss_Dragoon)
			{
				avoidRange += 32;
			}
			if (closestEnemy.type == BWAPI::UnitTypes::Protoss_Reaver)
			{
				avoidRange = 8 * 32;
			}
			if (closestEnemy.type == BWAPI::UnitTypes::Protoss_Carrier)
			{
				avoidRange = 12 * 32;
			}

			bool fleeOnLowHP = true;
			if (unit->getType() == BWAPI::UnitTypes::Zerg_Zergling //Zerglings should not run from ranged-units as they will most likely die anyways
				&& (avoidRange > 32 || closestEnemy.type == BWAPI::UnitTypes::Zerg_Zergling)) //Against Zerglings this has also shown to be bad
			{
				fleeOnLowHP = false;
			}
			if (unit->getType() == BWAPI::UnitTypes::Zerg_Mutalisk)
			{
				fleeOnLowHP = false;
			}

			avoidRange += 64 + closestEnemy.type.topSpeed() * 32;

			double distance = unit->getDistance(closestEnemy.lastPosition);

			if (distance < closestToEnemy)
			{
				closestToEnemy = distance;
				unitClosestToEnemy = unit;
			}
			if (distance < considerationRange)
			{
				std::vector<UnitInfo> ourCombatUnits;
				std::vector<UnitInfo> enemyCombatUnits;

				InformationManager::Instance().getNearbyForce(ourCombatUnits, closestEnemy.lastPosition, BWAPI::Broodwar->self(), considerationRange, int(avoidRange));
				InformationManager::Instance().getNearbyForce(enemyCombatUnits, closestEnemy.lastPosition, BWAPI::Broodwar->enemy(), int(avoidRange));

				double averageHPRatio = 1;

				double NumUnits = 0;
				BWAPI::Position myUnitCenter = unit->getPosition();
				BWAPI::Position positionSum;
				for each (auto ui in ourCombatUnits)
				{
					positionSum += unit->getPosition();
					NumUnits++;
				}
				if (NumUnits > 0)
				{
					myUnitCenter = positionSum / NumUnits;
				}

				BWAPI::Position middle = (closestEnemy.lastPosition + myUnitCenter) / 2;

				double enemyPower = getForcePower(enemyCombatUnits, ourCombatUnits, middle, averageHPRatio);

				ignoreFlyers = false;
				ignoreNoAntiAir = false;
				if (closestEnemy.type.airWeapon() == BWAPI::WeaponTypes::None)
				{
					ignoreFlyers = true;
				}
				if (closestEnemy.type.isFlyer())
				{
					ignoreNoAntiAir = true;
				}
				averageHPRatio = 1;
				double myPower = getForcePower(ourCombatUnits, enemyCombatUnits, middle, averageHPRatio, true);
				BWAPI::Broodwar->drawText(BWAPI::CoordinateType::Map, closestEnemy.lastPosition.x, closestEnemy.lastPosition.y, "%d vs. %d", int(myPower), int(enemyPower));
				double myHPRatio = double(unit->getHitPoints() + unit->getShields()) / double(unit->getType().maxHitPoints() + unit->getType().maxShields()) / averageHPRatio;
				if (fleeOnLowHP)
				{
					myPower *= std::min(1.0, myHPRatio);
				}
				//BWAPI::Broodwar->drawTextMap(unit->getPosition(), "%d%%", int(100 * double(unit->getHitPoints() + unit->getShields()) / double(unit->getType().maxHitPoints() + unit->getType().maxShields()) / averageHPRatio));
				if (myPower > 0 && enemyPower > myPower)
				{
					avoidRange *= enemyPower / myPower;
				}
				avoidRange *= 4.0 / 3.0;
				avoidRange = std::max(avoidRange, 300.0);

				//if (distance < avoidRange)
				//{
				//	if (myPower > enemyPower)
				//	{
				//		BWAPI::Broodwar->drawLineMap(unit->getPosition(), closestEnemy.lastPosition, BWAPI::Colors::Yellow);
				//	}
				//	else
				//	{
				//		BWAPI::Broodwar->drawLineMap(unit->getPosition(), closestEnemy.lastPosition, BWAPI::Colors::Red);
				//	}
				//}
				//else
				//{
				//	BWAPI::Broodwar->drawLineMap(unit->getPosition(), closestEnemy.lastPosition, BWAPI::Colors::Green);
				//}

				if ((myPower >= enemyPower
					|| unit->getType() == BWAPI::UnitTypes::Zerg_Guardian
					|| unit->getType() == BWAPI::UnitTypes::Zerg_Scourge
					|| unit->getType() == BWAPI::UnitTypes::Zerg_Broodling
					|| (unit->getType() == BWAPI::UnitTypes::Zerg_Zergling && unit->isAttacking() && unit->getTarget() && unit->getTarget()->getType() == BWAPI::UnitTypes::Terran_Siege_Tank_Siege_Mode)
					|| (unit->getType() == BWAPI::UnitTypes::Zerg_Ultralisk && unit->isAttacking() && unit->getTarget() && unit->getTarget()->getType() == BWAPI::UnitTypes::Terran_Siege_Tank_Siege_Mode)
					|| _broodForce
					|| unit->isIrradiated()) //I'm gonna die anyways
					&& !unit->isUnderStorm()) //when i'm under a storm, I better get out
				{
					UAlbertaBot::UnitInfo enemyToAttack = findBestAttackTarget(unit, unit->getPosition(), considerationRange);
					if (enemyToAttack.unitID != 0)
					{
						//BWAPI::Broodwar->drawLineMap(unit->getPosition(), enemyToAttack.lastPosition, BWAPI::Colors::Green);
						if (unit->getType() == BWAPI::UnitTypes::Zerg_Scourge)
						{
							Micro::SmartPredictAttack(unit, enemyToAttack.unit);
						}
						else if (unit->getType() == BWAPI::UnitTypes::Zerg_Devourer)
						{
							if (unit->isIrradiated())
							{
								Micro::SmartKiteTarget(unit, enemyToAttack.unit, enemyToAttack.lastPosition);
							}
							else
							{
								Micro::SmartAttackUnit(unit, enemyToAttack.unit); //can't kite caue this cancels attack-animation
							}
						}
						else if (unit->getType() == BWAPI::UnitTypes::Zerg_Mutalisk)
						{
							if (unit->isUnderAttack() || unit->getHitPoints() < unit->getType().maxHitPoints() && enemyPower > 0)
							{
								Micro::MutaDanceTarget(unit, enemyToAttack.unit);
							}
							else
							{
								Micro::SmartKiteTarget(unit, enemyToAttack.unit, enemyToAttack.lastPosition);
							}
						}
						else if (unit->getType() == BWAPI::UnitTypes::Zerg_Hydralisk
							|| unit->getType() == BWAPI::UnitTypes::Zerg_Guardian)
						{
							if (unit->getType() == BWAPI::UnitTypes::Zerg_Hydralisk)
							{
								Micro::SmartKiteTarget(unit, enemyToAttack.unit, enemyToAttack.lastPosition);
							}
							else
							{
								Micro::SmartKiteTarget(unit, enemyToAttack.unit);
							}
						}
						else if (unit->getType() == BWAPI::UnitTypes::Zerg_Lurker)
						{
							Micro::SmartLurker(unit, enemyToAttack.unit);
						}
						else
						{
							Micro::SmartAttackUnit(unit, enemyToAttack.unit);
//							enemyToAttack.powerAssigned += unit->getType().mineralPrice() + unit->getType().gasPrice();
						}
					}
					else
					{
						Micro::SmartAttackMove(unit, attackLocation);
					}
				}
				else
				{
					bool dontSpread = false;
					if (closestEnemy.type == BWAPI::UnitTypes::Zerg_Zergling
						|| closestEnemy.type == BWAPI::UnitTypes::Protoss_Zealot) //only build concaves against units that are stationary, otherwise retreat
					{
						dontSpread = true;
					}
					Micro::SmartAvoid(unit, closestEnemy.lastPosition, retreatLocation, attackLocation, avoidRange, dontSpread);
				}
			}
			else
			{
				Micro::SmartAttackMove(unit, attackLocation);
			}
		}
		else
		{
			if (unitClosestToEnemy == nullptr)
			{
				double distance = attackLocation.getDistance(unit->getPosition());
				if (distance < closestToEnemy)
				{
					closestToEnemy = distance;
					unitClosestToEnemy = unit;
				}
			}
			Micro::SmartAttackMove(unit, attackLocation);
		}
	}
	handleOverlords(unitClosestToEnemy);
}

void CombatCommander::handleOverlords(BWAPI::Unit unitClosestToEnemy)
{
	int detectorUnitsInBattle = 0;
	unsigned int oviCount = UnitUtil::GetAllUnitCount(BWAPI::UnitTypes::Zerg_Overlord, true);
	bool detectorUnitExploring = false;
	bool needDetection = InformationManager::Instance().enemyHasCloakedUnits();
	std::map<unsigned int, BWAPI::Position> bases;
	unsigned int counter = 0;
	for each(auto base in MapTools::Instance().getScoutLocations())
	{
		bases[counter] = base;
		counter++;
	}

	// for each detectorUnit
	counter = 0;

	std::vector<unsigned int> overlordWithJobsIds;

	//finding closest overlord to location that doesn't have a job yet

	while (counter < bases.size() && overlordWithJobsIds.size() < oviCount)
	{
		double closestDistance = 10000;
		BWAPI::Unit bestOviForLocation = nullptr;

		for (auto & detectorUnit : BWAPI::Broodwar->self()->getUnits())
		{
			if (detectorUnit->getType().isBuilding() || !detectorUnit->getType().isDetector())
			{
				continue;
			}
			bool alreadyBusy = false;
			for each (auto oviId in overlordWithJobsIds)
			{
				if (oviId == detectorUnit->getID())
				{
					alreadyBusy = true;
					break;
				}
			}
			if (alreadyBusy)
			{
				continue;
			}
			double distance = detectorUnit->getPosition().getDistance(bases[counter]);
			if (distance < closestDistance)
			{
				closestDistance = distance;
				bestOviForLocation = detectorUnit;
			}
		}
		if (bestOviForLocation != nullptr)
		{
			bool avoidEverything = true;
			if (InformationManager::Instance().getMainBaseLocation(BWAPI::Broodwar->enemy()) == nullptr)
			{
				avoidEverything = false;
			}
			Micro::SmartOviScout(bestOviForLocation, bases[counter], 400, avoidEverything);
			overlordWithJobsIds.push_back(bestOviForLocation->getID());
			counter++;
		}
	}
}

void CombatCommander::updateIdleSquad()
{
}

void CombatCommander::updateAttackSquads()
{
}

void CombatCommander::updateDropSquads()
{
}

void CombatCommander::updateScoutDefenseSquad() 
{
}

void CombatCommander::updateDefenseSquadUnits(Squad & defenseSquad, const size_t & flyingDefendersNeeded, const size_t & groundDefendersNeeded)
{
    const BWAPI::Unitset & squadUnits = defenseSquad.getUnits();
	size_t flyingDefendersInSquad = 0;
	size_t groundDefendersInSquad = 0;
	for (auto & unit : squadUnits)
	{
		if (UnitUtil::CanAttackAir(unit))
		{
			flyingDefendersInSquad += unit->getType().mineralPrice() + unit->getType().gasPrice();
		}
		if (UnitUtil::CanAttackGround(unit))
		{
			if (unit->getType() == BWAPI::UnitTypes::Zerg_Zergling)
			{
				groundDefendersInSquad += (unit->getType().mineralPrice() + unit->getType().gasPrice()) / 2;
			}
			else if (unit->getType() == BWAPI::UnitTypes::Zerg_Drone)
			{
				groundDefendersInSquad += (unit->getType().mineralPrice() + unit->getType().gasPrice()) / 3;
			}
			else
			{
				groundDefendersInSquad += unit->getType().mineralPrice() + unit->getType().gasPrice();
			}
		}
	}

    // if there's nothing left to defend, clear the squad
    if (flyingDefendersNeeded == 0 && groundDefendersNeeded == 0)
    {
        defenseSquad.clear();
        return;
    }
	//BWAPI::Broodwar->printf("I need %d defenders at %d:%d", flyingDefendersNeeded + groundDefendersNeeded, defenseSquad.getSquadOrder().getPosition().x, defenseSquad.getSquadOrder().getPosition().y);

    // add flying defenders if we still need them
    size_t flyingDefendersAdded = 0;
    while (flyingDefendersNeeded > flyingDefendersInSquad + flyingDefendersAdded)
    {
        BWAPI::Unit defenderToAdd = findClosestDefender(defenseSquad, defenseSquad.getSquadOrder().getPosition(), true);

        // if we find a valid flying defender, add it to the squad
        if (defenderToAdd)
        {
			_squadData.assignUnitToSquad(defenderToAdd, defenseSquad);
			flyingDefendersAdded += defenderToAdd->getType().mineralPrice() + defenderToAdd->getType().gasPrice();
        }
        // otherwise we'll never find another one so break out of this loop
        else
        {
            break;
        }
    }

    // add ground defenders if we still need them
    size_t groundDefendersAdded = 0;
    while (groundDefendersNeeded > groundDefendersInSquad + groundDefendersAdded)
    {
        BWAPI::Unit defenderToAdd = findClosestDefender(defenseSquad, defenseSquad.getSquadOrder().getPosition(), false);

        // if we find a valid ground defender add it
        if (defenderToAdd)
        {
			//BWAPI::Broodwar->printf("Added defender: %s", defenderToAdd->getType().getName().c_str());
			_squadData.assignUnitToSquad(defenderToAdd, defenseSquad);
			if (defenderToAdd->getType() == BWAPI::UnitTypes::Zerg_Zergling)
			{
				groundDefendersAdded += (defenderToAdd->getType().mineralPrice() + defenderToAdd->getType().gasPrice()) / 2;
			}
			else if (defenderToAdd->getType() == BWAPI::UnitTypes::Zerg_Drone)
			{
				groundDefendersAdded += (defenderToAdd->getType().mineralPrice() + defenderToAdd->getType().gasPrice()) / 3;
			}
			else
			{
				groundDefendersAdded += defenderToAdd->getType().mineralPrice() + defenderToAdd->getType().gasPrice();
			}
        }
        // otherwise we'll never find another one so break out of this loop
        else
        {
            break;
        }
    }
}

BWAPI::Unit CombatCommander::findClosestDefender(const Squad & defenseSquad, BWAPI::Position pos, bool flyingDefender) 
{
	BWAPI::Unit closestDefender = nullptr;
	double minDistance = std::numeric_limits<double>::max();

    int zerglingsInOurBase = numZerglingsInOurBase();
    bool zerglingRush = zerglingsInOurBase > 0 && BWAPI::Broodwar->getFrameCount() < 5000;

	for (auto & unit : _combatUnits) 
	{
		if ((flyingDefender && !UnitUtil::CanAttackAir(unit)) || (!flyingDefender && !UnitUtil::CanAttackGround(unit)))
        {
            continue;
        }

        if (!_squadData.canAssignUnitToSquad(unit, defenseSquad))
        {
            continue;
        }

        // add workers to the defense squad if we are being rushed very quickly
		if (unit->getType().isWorker())
        {
            continue;
        }

        double dist = unit->getDistance(pos);
        if (!closestDefender || (dist < minDistance))
        {
            closestDefender = unit;
            minDistance = dist;
        }
	}
	return closestDefender;
}

BWAPI::Position CombatCommander::getDefendLocation()
{
	BWAPI::Position defendNear = MapTools::Instance().getBaseCenter();
	double closestDistance = 10000;
	BWAPI::Unit closestUnit = nullptr;
	for (auto & unit : BWAPI::Broodwar->enemy()->getUnits())
	{
		if (unit->getType() == BWAPI::UnitTypes::Zerg_Overlord)
		{
			continue;
		}

		if (UnitUtil::IsValidUnit(unit) && unit->isVisible())
		{
			if (unit->getPosition().getDistance(defendNear) < closestDistance)
			{
				closestDistance = unit->getPosition().getDistance(defendNear);
				closestUnit = unit;
			}
		}
	}
	if (closestUnit)
	{
		defendNear = closestUnit->getPosition();
	}
	return defendNear;
}

void CombatCommander::drawSquadInformation(int x, int y)
{
	_squadData.drawSquadInformation(x, y);
}

BWAPI::Position CombatCommander::getMyAttackLocation(BWAPI::Unit unit)
{
	bool stayAtHome = false;
	if (unit->getType() == BWAPI::UnitTypes::Zerg_Hydralisk)
	{
		int augmentLevel = BWAPI::Broodwar->self()->getUpgradeLevel(BWAPI::UpgradeTypes::Muscular_Augments);
		if (ProductionManager::Instance().sunkenUnlocked && augmentLevel < 1)
		{
			stayAtHome = true;
		}
	}
	//if (unit->getType() == BWAPI::UnitTypes::Zerg_Zergling)
	//{
	//	int speed = BWAPI::Broodwar->self()->getUpgradeLevel(BWAPI::UpgradeTypes::Metabolic_Boost);
	//	if (BWAPI::Broodwar->enemy()->getRace() == BWAPI::Races::Zerg && speed < 1)
	//	{
	//		stayAtHome = true;
	//	}
	//}

	BWAPI::Position pos = unit->getPosition();
	double bestScore = 0;
	BWAPI::Position bestPosition = BWAPI::Positions::None;

	if (!stayAtHome)
	{
		for (auto enemy : InformationManager::Instance().getUnitData(BWAPI::Broodwar->enemy()).getUnits())
		{
			if (!enemy.second.positionValid)
			{
				continue;
			}
			if (unit->getType() == BWAPI::UnitTypes::Zerg_Hydralisk)
			{
				int workerSupply = UnitUtil::GetAllUnitCount(BWAPI::Broodwar->self()->getRace().getWorker()) * BWAPI::Broodwar->self()->getRace().getWorker().supplyRequired();
				int armySupply = BWAPI::Broodwar->self()->supplyUsed() - workerSupply;
			}
			double Score = 0;
			double Defense = 0;
			
			if (unit->getType() == BWAPI::UnitTypes::Zerg_Devourer
				|| unit->getType() == BWAPI::UnitTypes::Zerg_Scourge)
			{
				if (!enemy.second.type.isFlyer())
				{
					continue;
				}
				if (unit->getType() == BWAPI::UnitTypes::Zerg_Scourge) //don't scourge overlords
				{
					if (enemy.second.type.supplyProvided() > 0)
					{
						continue;
					}
				}
				Score += 1;
				Score /= enemy.second.type.topSpeed();
			}
			//Very special treatment for mutalisk... they shall like killing reavers, guardians > shuttles > workers > other non-AA_ground-units before going for buildings
			else if (unit->getType() == BWAPI::UnitTypes::Zerg_Mutalisk)
			{
				if (enemy.second.type == BWAPI::UnitTypes::Zerg_Larva
					|| enemy.second.type == BWAPI::UnitTypes::Zerg_Egg)
				{
					continue;
				}
				else if (enemy.second.unit->isBurrowed()) //should go for mines and burrowed lings
				{
					Score += 4;
				}
				else if (enemy.second.type == BWAPI::UnitTypes::Protoss_Reaver
					|| enemy.second.type == BWAPI::UnitTypes::Zerg_Guardian
					|| enemy.second.type == BWAPI::UnitTypes::Terran_Siege_Tank_Siege_Mode)
				{
					Score += 4;
				}
				else if (enemy.second.type == BWAPI::UnitTypes::Protoss_Shuttle
					|| enemy.second.type == BWAPI::UnitTypes::Terran_Dropship
					|| enemy.second.type == BWAPI::UnitTypes::Zerg_Overlord)
				{
					Score += 4;
				}
				else if ((enemy.second.type.airWeapon() == BWAPI::WeaponTypes::None
					&& !enemy.second.type.isBuilding()) || (enemy.second.type.isBuilding() && enemy.second.type.isFlyer()))
				{
					Score += 3;
				}
				else if (enemy.second.type.isBuilding() && !enemy.second.unit->isFlying())
				{
					Score += 1;
					if (enemy.second.type.isResourceDepot()) //we care more about bases than just random buildings
					{
						Score += 1;
					}
				}
			}
			else
			{
				if (enemy.second.unit->isBurrowed()) //should go for mines and burrowed lings
				{
					Score += 4;
				}
				else if (enemy.second.type.isBuilding() && (!enemy.second.unit->isFlying() || unit->getType().airWeapon() != BWAPI::WeaponTypes::None))
				{
					Score += 1;
					if (enemy.second.type.isResourceDepot()) //we care more about bases than just random buildings
					{
						Score += 1;
					}
				}
			}
			for (const auto & kv : InformationManager::Instance().getUnitInfo(BWAPI::Broodwar->enemy()))
			{
				const UnitInfo & ui = kv.second;
				if (enemy.second.lastPosition.getDistance(ui.lastPosition) < 400)
				{
					if (ui.type.isWorker() && unit->getType() == BWAPI::UnitTypes::Zerg_Mutalisk) //only mutas shall prioritize more workers, so our ground units also snipe unsaturated expos
					{
						Score += 1;
					}
					if (!ui.type.isWorker() && (ui.type.canAttack() || ui.type == BWAPI::UnitTypes::Terran_Bunker))
					{
						if (unit->getType().isFlyer())
						{
							if (ui.type.airWeapon() != BWAPI::WeaponTypes::None)
							{
								Defense += ui.type.mineralPrice() + ui.type.gasPrice();
								if (ui.type == BWAPI::UnitTypes::Zerg_Sunken_Colony || ui.type == BWAPI::UnitTypes::Zerg_Spore_Colony)
								{
									Defense += BWAPI::UnitTypes::Zerg_Creep_Colony.mineralPrice() + BWAPI::UnitTypes::Zerg_Creep_Colony.gasPrice();
									Defense += BWAPI::UnitTypes::Zerg_Drone.mineralPrice() + BWAPI::UnitTypes::Zerg_Drone.gasPrice();
								}
							}
						}
						else
						{
							if (ui.type.groundWeapon() != BWAPI::WeaponTypes::None)
							{
								Defense += ui.type.mineralPrice() + ui.type.gasPrice();
								if (ui.type == BWAPI::UnitTypes::Zerg_Sunken_Colony || ui.type == BWAPI::UnitTypes::Zerg_Spore_Colony)
								{
									Defense += BWAPI::UnitTypes::Zerg_Creep_Colony.mineralPrice() + BWAPI::UnitTypes::Zerg_Creep_Colony.gasPrice();
									Defense += BWAPI::UnitTypes::Zerg_Drone.mineralPrice() + BWAPI::UnitTypes::Zerg_Drone.gasPrice();
								}
							}
						}
					}
				}
			}
			Score /= (Defense + 1);
			Score /= unit->getDistance(enemy.second.lastPosition);
			if (Score > bestScore)
			{
				bestPosition = enemy.second.lastPosition;
				bestScore = Score;
			}
		}
	}
	//I should probably also think about defending once in a while
	for (auto myVulnerable : InformationManager::Instance().getUnitData(BWAPI::Broodwar->self()).getUnits())
	{
		if (!myVulnerable.second.type.isBuilding())
		{
			continue;
		}
		double Score = 0;
		double EnemiesGround = 0;
		double EnemiesAir = 0;
		double DefendersGround = 0;
		double DefendersAir = 0;
		for (const auto & kv : InformationManager::Instance().getUnitInfo(BWAPI::Broodwar->enemy()))
		{
			const UnitInfo & ui = kv.second;
			if (myVulnerable.second.lastPosition.getDistance(ui.lastPosition) < 512)
			{
				if (!ui.type.isWorker() && (ui.type.canAttack() || ui.type == BWAPI::UnitTypes::Terran_Bunker))
				{
					if (ui.type.isFlyer())
					{
						EnemiesAir += ui.type.mineralPrice() + ui.type.gasPrice();
					}
					else
					{
						if (ui.type.isTwoUnitsInOneEgg())
						{
							EnemiesGround += (ui.type.mineralPrice() + ui.type.gasPrice()) / 2;
						}
						else
						{
							EnemiesGround += ui.type.mineralPrice() + ui.type.gasPrice();
						}
					}
				}
			}
		}
		if (EnemiesAir + EnemiesGround > 0)
		{
			for (const auto & kv : InformationManager::Instance().getUnitInfo(BWAPI::Broodwar->self()))
			{
				const UnitInfo & ui = kv.second;
				if (myVulnerable.second.lastPosition.getDistance(ui.lastPosition) < 512)
				{
					if (!ui.type.isBuilding())
					{
						if (ui.type.airWeapon() != BWAPI::WeaponTypes::None)
						{
							DefendersAir += ui.type.mineralPrice() + ui.type.gasPrice();
						}
						if (ui.type.groundWeapon() != BWAPI::WeaponTypes::None)
						{
							if (ui.type.isTwoUnitsInOneEgg())
							{
								DefendersGround += (ui.type.mineralPrice() + ui.type.gasPrice()) / 2;
							}
							else
							{
								DefendersGround += ui.type.mineralPrice() + ui.type.gasPrice();
							}
						}
					}
				}
			}
			if ((EnemiesAir > 2 * DefendersAir || bestPosition == BWAPI::Positions::None) && unit->getType().airWeapon() != BWAPI::WeaponTypes::None)
			{
				Score = 1;
			}
			if ((EnemiesGround > 2 * DefendersGround || bestPosition == BWAPI::Positions::None) && unit->getType().groundWeapon() != BWAPI::WeaponTypes::None)
			{
				Score = 1;
			}
		}
		Score /= unit->getDistance(myVulnerable.second.lastPosition);
		if (Score > bestScore)
		{
			bestPosition = myVulnerable.second.lastPosition;
			bestScore = Score;
		}
	}

	if (bestPosition != BWAPI::Positions::None)
	{
		return bestPosition;
	}
	//home
	if (stayAtHome)
	{
		return MapTools::Instance().getClosestSunkenPosition();
	}
	//Unscouted Main-base-locations:
	double closestDistance = 10000;
	for each(auto baseLocation in BWTA::getStartLocations())
	{
		if (baseLocation->getPosition().getDistance(pos) < closestDistance
			&& baseLocation != BWTA::getStartLocation(BWAPI::Broodwar->self())
			&& !BWAPI::Broodwar->isExplored(baseLocation->getTilePosition()))
		{
			closestDistance = baseLocation->getPosition().getDistance(pos);
			bestPosition = baseLocation->getPosition();
		}
	}
	if (bestPosition != BWAPI::Positions::None)
	{
		return bestPosition;
	}
	// Fourth choice: We can't see anything so explore the map attacking along the way
	return MapGrid::Instance().getLeastExplored();
}

BWAPI::Position CombatCommander::getMainAttackLocation()
{
	//BWTA::BaseLocation * enemyBaseLocation = InformationManager::Instance().getMainBaseLocation(BWAPI::Broodwar->enemy());

 //   // First choice: Attack an enemy region if we can see units inside it
 //   if (enemyBaseLocation)
 //   {
 //       BWAPI::Position enemyBasePosition = enemyBaseLocation->getPosition();

 //       // get all known enemy units in the area
 //       BWAPI::Unitset enemyUnitsInArea;
	//	MapGrid::Instance().GetUnits(enemyUnitsInArea, enemyBasePosition, 800, false, true);

 //       bool onlyOverlords = true;
 //       for (auto & unit : enemyUnitsInArea)
 //       {
 //           if (unit->getType() != BWAPI::UnitTypes::Zerg_Overlord)
 //           {
 //               onlyOverlords = false;
 //           }
 //       }

 //       if (!BWAPI::Broodwar->isExplored(BWAPI::TilePosition(enemyBasePosition)) || !enemyUnitsInArea.empty())
 //       {
 //           if (!onlyOverlords)
 //           {
 //               return enemyBaseLocation->getPosition();
 //           }
 //       }
 //   }

	//int workerSupply = UnitUtil::GetAllUnitCount(BWAPI::Broodwar->self()->getRace().getWorker()) * BWAPI::Broodwar->self()->getRace().getWorker().supplyRequired();
	//int armySupply = BWAPI::Broodwar->self()->supplyUsed() - workerSupply;

	// Attack closest known enemy expansion
	BWAPI::Position pos = MapTools::Instance().getBaseCenter();

	double bestScore = 0;
	BWAPI::Position bestPosition = BWAPI::Position(0,0);
	for (auto building : InformationManager::Instance().getUnitData(BWAPI::Broodwar->enemy()).getUnits())
	{
		if (!building.second.type.isBuilding())
		{
			continue;
		}
		double baseScore = building.second.type.mineralPrice() + building.second.type.gasPrice();
		double baseDefense = 0;
		for (const auto & kv : InformationManager::Instance().getUnitInfo(BWAPI::Broodwar->enemy()))
		{
			const UnitInfo & ui = kv.second;
			if (building.second.lastPosition.getDistance(ui.lastPosition) < 400)
			{
				if (ui.type.isWorker())
				{
					baseScore += ui.type.mineralPrice() + ui.type.gasPrice();
				}
				if (!ui.type.isWorker() && (ui.type.canAttack() || ui.type == BWAPI::UnitTypes::Terran_Bunker))
				{
					baseDefense += ui.type.mineralPrice() + ui.type.gasPrice();
					if (ui.type == BWAPI::UnitTypes::Zerg_Sunken_Colony || ui.type == BWAPI::UnitTypes::Zerg_Spore_Colony)
					{
						baseDefense += BWAPI::UnitTypes::Zerg_Creep_Colony.mineralPrice() + BWAPI::UnitTypes::Zerg_Creep_Colony.gasPrice();
						baseDefense += BWAPI::UnitTypes::Zerg_Drone.mineralPrice() + BWAPI::UnitTypes::Zerg_Drone.gasPrice();
					}
				}
			}
		}
		baseScore /= (baseDefense + 1);
		baseScore /= (building.second.lastPosition.getDistance(pos) + 1);
		if (baseScore > bestScore)
		{
			bestPosition = building.second.lastPosition;
			bestScore = baseScore;
		}
	}
	if (bestPosition != BWAPI::Position(0, 0))
	{
		return bestPosition;
	}
	//Unscouted Main-base-locations:
	double closestDistance = 10000;
	for each(auto baseLocation in BWTA::getStartLocations())
	{
		if (baseLocation->getPosition().getDistance(pos) < closestDistance 
			&& baseLocation != BWTA::getStartLocation(BWAPI::Broodwar->self())
			&& !BWAPI::Broodwar->isExplored(baseLocation->getTilePosition()))
		{
			closestDistance = baseLocation->getPosition().getDistance(pos);
			bestPosition = baseLocation->getPosition();
		}
	}
	if (bestPosition != BWAPI::Position(0, 0))
	{
		return bestPosition;
	}
    // Fourth choice: We can't see anything so explore the map attacking along the way
    return MapGrid::Instance().getLeastExplored();
}

BWAPI::Unit CombatCommander::findClosestWorkerToTarget(BWAPI::Unitset & unitsToAssign, BWAPI::Unit target)
{
    UAB_ASSERT(target != nullptr, "target was null");

    if (!target)
    {
        return nullptr;
    }

    BWAPI::Unit closestMineralWorker = nullptr;
    double closestDist = 100000;
    
    // for each of our workers
	for (auto & unit : unitsToAssign)
	{
        if (!unit->getType().isWorker())
        {
            continue;
        }

		// if it is a move worker
        if (WorkerManager::Instance().isFree(unit)) 
		{
			double dist = unit->getDistance(target);

            if (!closestMineralWorker || dist < closestDist)
            {
                closestMineralWorker = unit;
                dist = closestDist;
            }
		}
	}

    return closestMineralWorker;
}

// when do we want to defend with our workers?
// this function can only be called if we have no fighters to defend with
int CombatCommander::defendWithWorkers()
{
	// our home nexus position
	BWAPI::Position homePosition = BWTA::getStartLocation(BWAPI::Broodwar->self())->getPosition();;

	// enemy units near our workers
	int enemyUnitsNearWorkers = 0;

	// defense radius of nexus
	int defenseRadius = 300;

	// fill the set with the types of units we're concerned about
	for (auto & unit : BWAPI::Broodwar->enemy()->getUnits())
	{
		// if it's a zergling or a worker we want to defend
		if (unit->getType() == BWAPI::UnitTypes::Zerg_Zergling)
		{
			if (unit->getDistance(homePosition) < defenseRadius)
			{
				enemyUnitsNearWorkers++;
			}
		}
	}

	// if there are enemy units near our workers, we want to defend
	return enemyUnitsNearWorkers;
}

int CombatCommander::numZerglingsInOurBase()
{
    int concernRadius = 600;
    int sunkens = 0;
    BWAPI::Position ourBasePosition = BWAPI::Position(BWAPI::Broodwar->self()->getStartLocation());
    
    // check to see if the enemy has zerglings as the only attackers in our base
    for (auto & unit : BWAPI::Broodwar->self()->getUnits())
    {
        if (unit->getType() != BWAPI::UnitTypes::Zerg_Sunken_Colony)
        {
            continue;
        }

        if (unit->getDistance(ourBasePosition) < concernRadius)
        {
			sunkens++;
        }
    }

	return sunkens;
}

bool CombatCommander::beingBuildingRushed()
{
    int concernRadius = 1200;
    BWAPI::Position ourBasePosition = BWAPI::Position(BWAPI::Broodwar->self()->getStartLocation());
    
    // check to see if the enemy has zerglings as the only attackers in our base
    for (auto & unit : BWAPI::Broodwar->enemy()->getUnits())
    {
        if (unit->getType().isBuilding())
        {
            return true;
        }
    }

    return false;
}

UAlbertaBot::UnitInfo CombatCommander::findBestAttackTarget(BWAPI::Unit unit, BWAPI::Position pos, double range)
{
	double WorstScore = std::numeric_limits<double>::max();
	UAlbertaBot::UnitInfo bestTarget;
	for each(auto enemy in InformationManager::Instance().getUnitInfo(BWAPI::Broodwar->enemy()))
	{
		double distance = unit->getDistance(enemy.second.lastPosition);
		if (distance > range
			|| enemy.second.type == BWAPI::UnitTypes::Zerg_Larva
			|| enemy.second.type == BWAPI::UnitTypes::Zerg_Egg
			|| !enemy.second.positionValid
			|| !enemy.second.unit->isDetected())
		{
			continue;
		}
		if (enemy.second.unit->isFlying() && unit->getType().airWeapon() == BWAPI::WeaponTypes::None)
		{
			continue;
		}
		if (!enemy.second.unit->isFlying() && unit->getType().groundWeapon() == BWAPI::WeaponTypes::None)
		{
			continue;
		}
		if (enemy.second.unit->isVisible() && enemy.second.type.isWorker() && !unit->isFlying() && !unit->isInWeaponRange(enemy.second.unit) && enemy.second.unit->isMoving() && !enemy.second.unit->isBraking())
		{
			continue;
		}
		if (enemy.second.type == BWAPI::UnitTypes::Protoss_Carrier //Ground units shall ignore carriers over cliffs
			&& !BWAPI::Broodwar->isWalkable(BWAPI::WalkPosition(enemy.second.lastPosition))
			&& !unit->isFlying())
		{
			continue;
		}
		if (unit->getType() == BWAPI::UnitTypes::Zerg_Scourge)
		{
			if (enemy.second.type.supplyProvided() > 0)
			{
				continue;
			}
			if (enemy.second.type.isDetector())
			{
				continue;
			}
			if (!enemy.second.unit->isVisible())
			{
				continue;
			}
		}

		double Score = std::numeric_limits<double>::max();
		if (!enemy.second.type.isBuilding()
			|| enemy.second.type.groundWeapon() != BWAPI::WeaponTypes::None
			|| enemy.second.type.airWeapon() != BWAPI::WeaponTypes::None
			|| (enemy.second.unit->isFlying() && unit->isFlying() && unit->getType().airWeapon() != BWAPI::WeaponTypes::None)
			|| enemy.second.type == BWAPI::UnitTypes::Terran_Bunker)
		{
			Score = distance * 1000 + enemy.second.lastHealth + enemy.second.lastShields;
			if (unit->getType() == BWAPI::UnitTypes::Zerg_Scourge)
			{
				Score = distance * 1000;
			}
			if (unit->getType() == BWAPI::UnitTypes::Zerg_Mutalisk)
			{
				if (unit->getPosition().getDistance(enemy.second.lastPosition) < 3 * unit->getType().groundWeapon().maxRange())
				{
					Score = enemy.second.lastHealth + enemy.second.lastShields;
				}
			}
			else if (unit->isInWeaponRange(enemy.second.unit))
			{
				Score = enemy.second.lastHealth + enemy.second.lastShields;
			}
			if (unit->getType() == BWAPI::UnitTypes::Zerg_Hydralisk || unit->getType() == BWAPI::UnitTypes::Zerg_Sunken_Colony)
			{
				if (enemy.second.type.size() == BWAPI::UnitSizeTypes::Medium)
				{
					Score /= 0.75;
				}
				if (enemy.second.type.size() == BWAPI::UnitSizeTypes::Small)
				{
					Score /= 0.5;
				}
			}
			Score *= std::sqrt(std::pow(enemy.second.unit->getVelocityX(), 2) + std::pow(enemy.second.unit->getVelocityX(), 2)) + 1;
			if (enemy.second.type == BWAPI::UnitTypes::Protoss_Interceptor)
			{
				Score = std::numeric_limits<double>::max() - 1; //only target interceptors when there's nothing else available
			}
			if (unit->getType() == BWAPI::UnitTypes::Zerg_Mutalisk
				&& (enemy.second.type.airWeapon() != BWAPI::WeaponTypes::None 
				|| enemy.second.unit->isRepairing() 
				|| enemy.second.type == BWAPI::UnitTypes::Protoss_Reaver
				|| enemy.second.type == BWAPI::UnitTypes::Protoss_High_Templar
				|| enemy.second.type == BWAPI::UnitTypes::Terran_Siege_Tank_Siege_Mode))
			{
				Score /= 1000;
			}
		}
		if (Score < WorstScore)
		{
			WorstScore = Score;
			bestTarget = enemy.second;
		}
	}
	return bestTarget;
}

UAlbertaBot::UnitInfo CombatCommander::findClosestEnemy(BWAPI::Position pos, bool ignoreFlyers, bool iAmFlyer, bool considerRange)
{
	double shortestDistance = 10000;
	UAlbertaBot::UnitInfo closestUnit;
	for each(auto unit in InformationManager::Instance().getUnitInfo(BWAPI::Broodwar->enemy()))
	{
		if ((unit.second.type.isBuilding() && !unit.second.type.canAttack() && unit.second.type != BWAPI::UnitTypes::Terran_Bunker)
			|| unit.second.type == BWAPI::UnitTypes::Zerg_Larva
			|| unit.second.type == BWAPI::UnitTypes::Zerg_Egg
			|| unit.second.type == BWAPI::UnitTypes::Terran_Vulture_Spider_Mine
			|| unit.second.type == BWAPI::UnitTypes::Protoss_Scarab
			|| !unit.second.positionValid)
		{
			continue;
		}
		if (ignoreFlyers && unit.second.type.isFlyer())
		{
			continue;
		}
		double currentDistance = pos.getDistance(unit.second.lastPosition);

		double relevantRange = 0;

		if (iAmFlyer)
		{
			relevantRange = unit.second.type.airWeapon().maxRange();
			if (unit.second.type == BWAPI::UnitTypes::Terran_Goliath)
			{
				relevantRange += 3 * 32;
			}
		}
		else
		{
			relevantRange = unit.second.type.groundWeapon().maxRange();
		}
		if (unit.second.type == BWAPI::UnitTypes::Terran_Bunker)
		{
			relevantRange = BWAPI::UnitTypes::Terran_Marine.groundWeapon().maxRange() + 64;
		}
		if (unit.second.type == BWAPI::UnitTypes::Terran_Marine
			|| unit.second.type == BWAPI::UnitTypes::Zerg_Hydralisk
			|| unit.second.type == BWAPI::UnitTypes::Protoss_Dragoon)
		{
			relevantRange += 32;
		}
		if (unit.second.type == BWAPI::UnitTypes::Protoss_Reaver)
		{
			relevantRange = 8 * 32;
		}
		if (unit.second.type == BWAPI::UnitTypes::Protoss_Carrier)
		{
			relevantRange = 12 * 32;
		}

		if (considerRange)
		{
			currentDistance -= relevantRange;
		}

		if (currentDistance < shortestDistance)
		{
			closestUnit = unit.second;
			shortestDistance = currentDistance;
		}
	}
	return closestUnit;
}

double	CombatCommander::getForcePower(std::vector<UnitInfo> units, std::vector<UnitInfo> enemyunits, BWAPI::Position pos, double& averageHPRatio, bool mine)
{
	double totalPower = 0;
	bool threatenedByFliers = false;
	//our strength depends on their composition
	double enemyLargeValue = 0;
	double enemyMediumValue = 0;
	double enemySmallValue = 0;
	double enemyExplosiveValue = 0;
	double enemyConcussiveValue = 0;
	double enemyNormalValue = 0;
	double enemyFlyerValue = 0;
	double enemyGroundValue = 0;
	double enemyAntiGroundValue = 0;
	double enemyAntiAirValue = 0;
	bool enemyHasDetector = false;
	double enemyTotalValue = 0;

	for each (auto enemyunit in enemyunits)
	{
		if (enemyunit.type.isDetector())
		{
			enemyHasDetector = true;
		}
		if ((enemyunit.type.isWorker() && !enemyunit.unit->isAttacking() && !enemyunit.unit->isRepairing())
			|| (enemyunit.type.isBuilding() && !enemyunit.type.canAttack() && enemyunit.type != BWAPI::UnitTypes::Terran_Bunker)
			|| enemyunit.type == BWAPI::UnitTypes::Zerg_Overlord
			|| enemyunit.type == BWAPI::UnitTypes::Zerg_Larva
			|| enemyunit.type == BWAPI::UnitTypes::Zerg_Egg
			|| enemyunit.type == BWAPI::UnitTypes::Protoss_Observer
			|| !enemyunit.positionValid)
		{
			continue;
		}
		double currentValue = (enemyunit.type.mineralPrice() + enemyunit.type.gasPrice()) / (enemyunit.type.isTwoUnitsInOneEgg() ? 2 : 1);
		currentValue *= double(enemyunit.lastHealth + enemyunit.lastShields) / double(enemyunit.type.maxHitPoints() + enemyunit.type.maxShields());
		if (enemyunit.unit->isStimmed())
		{
			currentValue *= 2;
		}
		//double distance = enemyunit.lastPosition.getDistance(pos);
		//double range = 0;
		//if (enemyunit.type.groundWeapon() != BWAPI::WeaponTypes::None)
		//{
		//	range = enemyunit.type.groundWeapon().maxRange();
		//}
		//if (enemyunit.type.airWeapon() != BWAPI::WeaponTypes::None)
		//{
		//	range = std::max((double)enemyunit.type.airWeapon().maxRange(), range);
		//}
		//if (enemyunit.type == BWAPI::UnitTypes::Terran_Bunker)
		//{
		//	range = BWAPI::UnitTypes::Terran_Marine.groundWeapon().maxRange() + 64;
		//}
		//if (enemyunit.type == BWAPI::UnitTypes::Terran_Marine
		//	|| enemyunit.type == BWAPI::UnitTypes::Zerg_Hydralisk
		//	|| enemyunit.type == BWAPI::UnitTypes::Protoss_Dragoon)
		//{
		//	range += 32;
		//}
		//if (enemyunit.type == BWAPI::UnitTypes::Protoss_Reaver)
		//{
		//	range = 8 * 32;
		//}
		//if (enemyunit.type == BWAPI::UnitTypes::Protoss_Carrier)
		//{
		//	range = 12 * 32;
		//}
		//if (distance <= range + enemyunit.type.topSpeed() * 24)
		//{
		//	currentValue *= 1;
		//}
		//else
		//{
		//	currentValue *= std::max(0.0, 1 - (distance - range + enemyunit.type.topSpeed() * 24) / (range + enemyunit.type.topSpeed() * 24 * 5));
		//}
		//by size-type
		if (enemyunit.type.size() == BWAPI::UnitSizeTypes::Large)
		{
			enemyLargeValue += currentValue;
		}
		else if (enemyunit.type.size() == BWAPI::UnitSizeTypes::Medium)
		{
			enemyMediumValue += currentValue;
		}
		else if (enemyunit.type.size() == BWAPI::UnitSizeTypes::Small)
		{
			enemySmallValue += currentValue;
		}
		//by armor-type
		if (enemyunit.type.groundWeapon() != BWAPI::WeaponTypes::None && enemyunit.type.groundWeapon().damageType() == BWAPI::DamageTypes::Concussive)
		{
			enemyConcussiveValue += currentValue;
		}
		else if (enemyunit.type.groundWeapon() != BWAPI::WeaponTypes::None && enemyunit.type.groundWeapon().damageType() == BWAPI::DamageTypes::Explosive && !enemyunit.type.isBuilding()) //workaround for sunken-underestimate
		{
			enemyExplosiveValue += currentValue;
		}
		else
		{
			enemyNormalValue += currentValue;
		}
		//by flying
		if (enemyunit.type.isFlyer())
		{
			enemyFlyerValue += currentValue;
		}
		else
		{
			enemyGroundValue += currentValue;
		}
		if (enemyunit.type.groundWeapon() != BWAPI::WeaponTypes::None)
		{
			enemyAntiGroundValue += currentValue * 1.0 / 10.0;
		}
		if (enemyunit.type.airWeapon() != BWAPI::WeaponTypes::None || enemyunit.type == BWAPI::UnitTypes::Terran_Bunker)
		{
			enemyAntiAirValue += currentValue * 1.0 / 3.0;
			if (enemyunit.type.isBuilding() || enemyunit.type == BWAPI::UnitTypes::Terran_Goliath)
			{
				enemyAntiAirValue += currentValue * 1.0 / 3.0;
			}
		}
		enemyTotalValue += currentValue;
	}

	double explosiveScoreMod = 1;
	double concussiveScoreMod = 1;
	double largeScoreMod = 1;
	double mediumScoreMod = 1;
	double smallScoreMod = 1;
	double noAirAttackScoreMod = 1;
	double noGroundAttackScoreMod = 1;
	double flyerMod = 1;
	double groundMod = 1;

	if (enemyTotalValue > 0)
	{
		if (mine) //only consider that for myself, as it otherwise would be doubled
		{
			explosiveScoreMod = (enemyLargeValue + enemyMediumValue * 0.75 + enemySmallValue * 0.5) / enemyTotalValue;
			concussiveScoreMod = (enemyLargeValue * 0.25 + enemyMediumValue * 0.5 + enemySmallValue) / enemyTotalValue;
			largeScoreMod = (enemyConcussiveValue / 0.25 + enemyExplosiveValue + enemyNormalValue) / enemyTotalValue;
			mediumScoreMod = (enemyConcussiveValue / 0.5 + enemyExplosiveValue / 0.75 + enemyNormalValue) / enemyTotalValue;
			smallScoreMod = (enemyConcussiveValue + enemyExplosiveValue / 0.5 + enemyNormalValue) / enemyTotalValue;
		}
		noAirAttackScoreMod = enemyGroundValue / enemyTotalValue;
		noGroundAttackScoreMod = enemyFlyerValue / enemyTotalValue;
		flyerMod = (enemyTotalValue - enemyAntiAirValue) / enemyTotalValue;
		groundMod = (enemyTotalValue - enemyAntiGroundValue) / enemyTotalValue;
	}

	averageHPRatio = 1;
	double unitsCounted = 0;
	double averageDamage = 0;
	for each (auto unit in units)
	{
		if (unit.player == BWAPI::Broodwar->self())
		{
			if (unit.type.isBuilding())
			{
				if (unit.type.canAttack() || UnitUtil::GetAllUnitCount(BWAPI::UnitTypes::Zerg_Sunken_Colony) == 0)
				{
					totalPower += 10000;
				}
			}
		}
		if ((unit.type.isWorker() && !unit.unit->isAttacking() && !unit.unit->isRepairing())
			|| (unit.type.isBuilding() && !unit.type.canAttack() && unit.type != BWAPI::UnitTypes::Terran_Bunker)
			|| unit.type == BWAPI::UnitTypes::Zerg_Overlord
			|| unit.type == BWAPI::UnitTypes::Terran_Dropship
			|| unit.type == BWAPI::UnitTypes::Protoss_Shuttle
			|| unit.type == BWAPI::UnitTypes::Zerg_Larva
			|| unit.type == BWAPI::UnitTypes::Zerg_Egg
			|| unit.type == BWAPI::UnitTypes::Protoss_Observer
			|| unit.type == BWAPI::UnitTypes::Terran_Vulture_Spider_Mine //we want to snipe it, not run from it
			|| !unit.positionValid)
		{
			continue;
		}
		if (unit.type == BWAPI::UnitTypes::Protoss_High_Templar && unit.unit->getEnergy() < 70) //don't fear reavers or templars who won't storm
		{
			continue;
		}
		if (unit.player == BWAPI::Broodwar->enemy()) //my air-units shouldn't fear enemy ground units that can't shoot up, but my ground units should ignore air-units
		{
			if (unit.type.isFlyer() && enemyAntiAirValue == 0)
			{
				continue;
			}
		}
		double value = (unit.type.mineralPrice() + unit.type.gasPrice()) / (unit.type.isTwoUnitsInOneEgg() ? 2 : 1);
		if (unit.unit->isStimmed())
		{
			value *= 2;
		}
		if (unit.type == BWAPI::UnitTypes::Zerg_Lurker && !enemyHasDetector)
		{
			value *= 4;
		}
		double currentHPRatio = double(unit.lastHealth + unit.lastShields) / double(unit.type.maxHitPoints() + unit.type.maxShields());
		averageHPRatio += currentHPRatio;
		++unitsCounted;
		if (unit.type == BWAPI::UnitTypes::Zerg_Sunken_Colony
			|| unit.type == BWAPI::UnitTypes::Zerg_Spore_Colony)
		{
			value += BWAPI::UnitTypes::Zerg_Creep_Colony.mineralPrice();
			value += BWAPI::UnitTypes::Zerg_Drone.mineralPrice();
		}
		if (unit.type == BWAPI::UnitTypes::Terran_Bunker)
		{
			value += 4 * BWAPI::UnitTypes::Terran_Marine.mineralPrice();
		}
		if (unit.type.groundWeapon() != BWAPI::WeaponTypes::None) //Goons suck against our zerglings
		{
			if (unit.type.groundWeapon().damageType() == BWAPI::DamageTypes::Explosive)
			{
				value *= explosiveScoreMod;
			}
			else if (unit.type.groundWeapon().damageType() == BWAPI::DamageTypes::Concussive)
			{
				value *= concussiveScoreMod;
			}
		}
		if (!unit.type.isBuilding())
		{
			if (unit.type.size() == BWAPI::UnitSizeTypes::Large)
			{
				value *= largeScoreMod;
			}
			else if (unit.type.size() == BWAPI::UnitSizeTypes::Medium)
			{
				value *= mediumScoreMod;
			}
			else if (unit.type.size() == BWAPI::UnitSizeTypes::Small)
			{
				value *= smallScoreMod;
			}
		}
		if (unit.type.groundWeapon() == BWAPI::WeaponTypes::None && unit.type.airWeapon() != BWAPI::WeaponTypes::None)
		{
			value *= noGroundAttackScoreMod;
		}
		if (unit.type.airWeapon() == BWAPI::WeaponTypes::None && unit.type.groundWeapon() != BWAPI::WeaponTypes::None)
		{
			value *= noAirAttackScoreMod;
		}
		if (unit.type == BWAPI::UnitTypes::Protoss_Reaver)
		{
			value *= noAirAttackScoreMod;
		}
		if (unit.type.isFlyer())
		{
			value *= flyerMod;
		}
		else
		{
			value *= groundMod;
		}
		value *= double(unit.lastHealth + unit.lastShields) / double(unit.type.maxHitPoints() + unit.type.maxShields());
		//double distance = unit.lastPosition.getDistance(pos);
		//double range = 0;
		//if (unit.type.groundWeapon() != BWAPI::WeaponTypes::None)
		//{
		//	range = unit.type.groundWeapon().maxRange();
		//}
		//if (unit.type.airWeapon() != BWAPI::WeaponTypes::None)
		//{
		//	range = std::max((double)unit.type.airWeapon().maxRange(), range);
		//}
		//if (unit.type == BWAPI::UnitTypes::Terran_Bunker)
		//{
		//	range = BWAPI::UnitTypes::Terran_Marine.groundWeapon().maxRange() + 64;
		//}
		//if (unit.type == BWAPI::UnitTypes::Terran_Marine
		//	|| unit.type == BWAPI::UnitTypes::Zerg_Hydralisk
		//	|| unit.type == BWAPI::UnitTypes::Protoss_Dragoon)
		//{
		//	range += 32;
		//}
		//if (unit.type == BWAPI::UnitTypes::Protoss_Reaver)
		//{
		//	range = 8 * 32;
		//}
		//if (unit.type == BWAPI::UnitTypes::Protoss_Carrier)
		//{
		//	range = 12 * 32;
		//}
		//if (distance <= range + unit.type.topSpeed() * 24)
		//{
		//	value *= 1;
		//}
		//else
		//{
		//	value *= std::max(0.0, 1 - (distance - range + unit.type.topSpeed() * 24) / (range + unit.type.topSpeed() * 24 * 5));
		//}
		totalPower += value;
	}
	if (unitsCounted > 0)
	{
		averageHPRatio /= unitsCounted;
	}
	return totalPower;
}