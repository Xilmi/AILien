#include "Squad.h"
#include "UnitUtil.h"

using namespace UAlbertaBot;

Squad::Squad()
    : _lastRetreatSwitch(0)
    , _lastRetreatSwitchVal(false)
    , _priority(0)
    , _name("Default")
{
    int a = 10;
}

Squad::Squad(const std::string & name, SquadOrder order, size_t priority) 
	: _name(name)
	, _order(order)
    , _lastRetreatSwitch(0)
    , _lastRetreatSwitchVal(false)
    , _priority(priority)
{
}

Squad::~Squad()
{
    clear();
}

void Squad::update()
{
}

bool Squad::isEmpty() const
{
    return _units.empty();
}

size_t Squad::getPriority() const
{
    return _priority;
}

void Squad::setPriority(const size_t & priority)
{
    _priority = priority;
}

void Squad::updateUnits()
{
	setAllUnits();
	setNearEnemyUnits();
	addUnitsToMicroManagers();
}

void Squad::setAllUnits()
{
	// clean up the _units vector just in case one of them died
	BWAPI::Unitset goodUnits;
	for (auto & unit : _units)
	{
		if( unit->isCompleted() && 
			unit->getHitPoints() > 0 && 
			unit->exists() &&
			unit->getPosition().isValid() &&
			unit->getType() != BWAPI::UnitTypes::Unknown)
		{
			goodUnits.insert(unit);
		}
	}
	_units = goodUnits;
}

void Squad::setNearEnemyUnits()
{
	_nearEnemy.clear();
	for (auto & unit : _units)
	{
		int x = unit->getPosition().x;
		int y = unit->getPosition().y;

		int left = unit->getType().dimensionLeft();
		int right = unit->getType().dimensionRight();
		int top = unit->getType().dimensionUp();
		int bottom = unit->getType().dimensionDown();

		_nearEnemy[unit] = unitNearEnemy(unit);
		if (_nearEnemy[unit])
		{
			if (Config::Debug::DrawSquadInfo) BWAPI::Broodwar->drawBoxMap(x-left, y - top, x + right, y + bottom, Config::Debug::ColorUnitNearEnemy);
		}
		else
		{
			if (Config::Debug::DrawSquadInfo) BWAPI::Broodwar->drawBoxMap(x-left, y - top, x + right, y + bottom, Config::Debug::ColorUnitNotNearEnemy);
		}
	}
}

void Squad::addUnitsToMicroManagers()
{
	BWAPI::Unitset meleeUnits;
	BWAPI::Unitset rangedUnits;
	BWAPI::Unitset detectorUnits;
	BWAPI::Unitset transportUnits;
    BWAPI::Unitset tankUnits;
    BWAPI::Unitset medicUnits;

	// add _units to micro managers
	for (auto & unit : _units)
	{
		if(unit->isCompleted() && unit->getHitPoints() > 0 && unit->exists())
		{
			// select dector _units
            if (unit->getType() == BWAPI::UnitTypes::Terran_Medic)
            {
                medicUnits.insert(unit);
            }
            else if (unit->getType() == BWAPI::UnitTypes::Terran_Siege_Tank_Siege_Mode || unit->getType() == BWAPI::UnitTypes::Terran_Siege_Tank_Tank_Mode)
            {
                tankUnits.insert(unit);
            }   
			else if (unit->getType().isDetector() && !unit->getType().isBuilding())
			{
				detectorUnits.insert(unit);
			}
			// select transport _units
			else if (unit->getType() == BWAPI::UnitTypes::Protoss_Shuttle || unit->getType() == BWAPI::UnitTypes::Terran_Dropship)
			{
				transportUnits.insert(unit);
			}
			// select ranged _units
			else if ((unit->getType().groundWeapon().maxRange() > 32) || (unit->getType() == BWAPI::UnitTypes::Protoss_Reaver) || (unit->getType() == BWAPI::UnitTypes::Zerg_Scourge))
			{
				rangedUnits.insert(unit);
			}
			// select melee _units
			else if (unit->getType().groundWeapon().maxRange() <= 32)
			{
				meleeUnits.insert(unit);
			}
		}
	}

	_meleeManager.setUnits(meleeUnits);
	_rangedManager.setUnits(rangedUnits);
	_detectorManager.setUnits(detectorUnits);
	_transportManager.setUnits(transportUnits);
    _tankManager.setUnits(tankUnits);
    _medicManager.setUnits(medicUnits);
}

// calculates whether or not to regroup
bool Squad::needsToRegroup()
{
    if (!Config::Micro::UseSparcraftSimulation)
    {
        return false;
    }

	//When our bank + our supply is big enough, we'll attack anything regardless of losses!
	if (BWAPI::Broodwar->self()->supplyUsed() + BWAPI::Broodwar->self()->minerals() / 25 >= 400)
	{
		return false;
	}

	BWAPI::Position center = calcCenter();
	bool regroupWithDefenseSquad = true;

	int closestSunkenDistance = 10000;
	BWAPI::Unit closestSunken = nullptr;
	for each (auto sunken in BWAPI::Broodwar->self()->getUnits())
	{
		if (sunken->getType() != BWAPI::UnitTypes::Zerg_Sunken_Colony)
		{
			continue;
		}
		int distance = calcCenter().getDistance(sunken->getPosition());
		{
			if (distance < closestSunkenDistance)
			{
				closestSunken = sunken;
				closestSunkenDistance = distance;
			}
		}
		if (sunken->isAttacking()) //If a sunken is already attacking, then it doesn't matter how close it is, it is the one we want to support and near which we attack
		{
			closestSunken = sunken;
			closestSunkenDistance = distance;
			break;
		}
	}
	if (closestSunken)
	{
		if (closestSunken->isAttacking())
		{
			regroupWithDefenseSquad = false;
		}
	}

	// if we are not attacking, never regroup
	if (_units.empty() || (_order.getType() != SquadOrderTypes::Attack && !regroupWithDefenseSquad))
	{
		_regroupStatus = std::string("\x04 No combat units available");
		return false;
	}

    BWAPI::Unit unitClosest = unitClosestToEnemy();

	if (!unitClosest)
	{
		_regroupStatus = std::string("\x04 No closest unit");
		return false;
	}

    // if none of our units are in attack range of any enemy units, don't retreat
    std::vector<UnitInfo> enemyCombatUnits;
    const auto & enemyUnitInfo = InformationManager::Instance().getUnitInfo(BWAPI::Broodwar->enemy());

    bool anyInRange = false;
    for (const auto & eui : enemyUnitInfo)
    {
        bool inRange = false;
        for (const auto & u : _units)
        {
            int range = UnitUtil::GetAttackRange(eui.second.type, u->getType());
			if (eui.second.type == BWAPI::UnitTypes::Terran_Siege_Tank_Tank_Mode) //a normal tank could go into siege-mode any time!
			{
				range = UnitUtil::GetAttackRange(BWAPI::UnitTypes::Terran_Siege_Tank_Siege_Mode, u->getType());
			}
			if (eui.second.type == BWAPI::UnitTypes::Terran_Goliath) //cause charon-booster
			{
				range += 64 * 3;
			}
			if (eui.second.type == BWAPI::UnitTypes::Terran_Bunker) //has marines with range-upgrades
			{
				range = UnitUtil::GetAttackRange(BWAPI::UnitTypes::Terran_Marine, u->getType()) + 64 * 1;
			}
			if (eui.second.type == BWAPI::UnitTypes::Protoss_Dragoon  //cause range-upgrade
				|| eui.second.type == BWAPI::UnitTypes::Zerg_Hydralisk
				|| eui.second.type == BWAPI::UnitTypes::Terran_Marine)
			{
				range += 64 * 1;
			}

			range = std::max(range, 300);

            if (range + 128 >= eui.second.lastPosition.getDistance(u->getPosition()))
            {
                inRange = true;
                break;
            }
        }

        if (inRange)
        {
            anyInRange = true;
            break;
        }
    }

    if (!anyInRange)
    {
        _regroupStatus = std::string("\x04 No enemy units in attack range");
        return false;
    }

    SparCraft::ScoreType score = 0;

	//do the SparCraft Simulation!
	CombatSimulation sim;
    
	BWAPI::Position closestEnemyPosition = BWAPI::Position(0,0);
	double cloesestEnemyDistance = 10000;
	for (auto enemy : InformationManager::Instance().getUnitData(BWAPI::Broodwar->enemy()).getUnits())
	{
		if (enemy.second.type.canAttack())
		{
			if (enemy.second.lastPosition.getDistance(unitClosest->getPosition()) < cloesestEnemyDistance)
			{
				cloesestEnemyDistance = enemy.second.lastPosition.getDistance(unitClosest->getPosition());
				closestEnemyPosition = enemy.second.lastPosition;
			}
		}
	}

	sim.setCombatUnits(unitClosest->getPosition(), Config::Micro::CombatRegroupRadius);
	score = sim.simulateCombat();

	// if we are DT rushing and we haven't lost a DT yet, no retreat!
	if (Config::Strategy::StrategyName == "Protoss_DTRush" && (BWAPI::Broodwar->self()->deadUnitCount(BWAPI::UnitTypes::Protoss_Dark_Templar) == 0))
	{
		_regroupStatus = std::string("\x04 DARK TEMPLAR HOOOOO!");
		return false;
	}

    bool retreat = score < 0;
    int switchTime = 100;
    bool waiting = false;

    // we should not attack unless 5 seconds have passed since a retreat
    if (retreat != _lastRetreatSwitchVal)
    {
        if (!retreat && (BWAPI::Broodwar->getFrameCount() - _lastRetreatSwitch < switchTime))
        {
            waiting = true;
            retreat = _lastRetreatSwitchVal;
        }
        else
        {
            waiting = false;
            _lastRetreatSwitch = BWAPI::Broodwar->getFrameCount();
            _lastRetreatSwitchVal = retreat;
        }
    }
	
	if (retreat || waiting)
	{
		_regroupStatus = std::string("\x04 Retreat - simulation predicts defeat or have to wait before attacking again");
	}
	else
	{
		_regroupStatus = std::string("\x04 Attack - simulation predicts success");
	}
	if (_order.getType() != UAlbertaBot::SquadOrderTypes::Idle)
	{
		Logger::LogAppendToFile("retreat.log", "%s Retreat? %d Score was: %d\n", getName().c_str(), retreat, score);
	}
	return retreat;
}

void Squad::setSquadOrder(const SquadOrder & so)
{
	_order = so;
}

bool Squad::containsUnit(BWAPI::Unit u) const
{
    return _units.contains(u);
}

void Squad::clear()
{
    for (auto & unit : getUnits())
    {
        if (unit->getType().isWorker())
        {
            WorkerManager::Instance().finishedWithWorker(unit);
        }
    }

    _units.clear();
}

bool Squad::unitNearEnemy(BWAPI::Unit unit)
{
	assert(unit);

	BWAPI::Unitset enemyNear;

	MapGrid::Instance().GetUnits(enemyNear, unit->getPosition(), BWAPI::UnitTypes::Terran_Siege_Tank_Siege_Mode.groundWeapon().maxRange() + 128, false, true);

	return enemyNear.size() > 0;
}

BWAPI::Position Squad::calcCenter()
{
    if (_units.empty())
    {
        //if (Config::Debug::DrawSquadInfo)
        //{
        //    BWAPI::Broodwar->printf("Squad::calcCenter() called on empty squad");
        //}
        return BWAPI::Position(0,0);
    }

	BWAPI::Position accum(0,0);
	for (auto & unit : _units)
	{
		accum += unit->getPosition();
	}
	return BWAPI::Position(accum.x / _units.size(), accum.y / _units.size());
}

BWAPI::Position Squad::calcRegroupPosition()
{
	BWAPI::Position regroup(0,0);

	BWAPI::Position closestSunkenPostion = BWAPI::Position(0,0);

	int closestSunkenDistance = 10000;
	BWAPI::Unit closestSunken = nullptr;
	for each (auto sunken in BWAPI::Broodwar->self()->getUnits())
	{
		if (sunken->getType() != BWAPI::UnitTypes::Zerg_Sunken_Colony)
		{
			continue;
		}
		int distance = calcCenter().getDistance(sunken->getPosition());
		{
			if (distance < closestSunkenDistance)
			{
				closestSunken = sunken;
				closestSunkenDistance = distance;
			}
		}
		if (sunken->isAttacking()) //If a sunken is already attacking, then it doesn't matter how close it is, it is the one we want to support and near which we attack
		{
			closestSunken = sunken;
			closestSunkenDistance = distance;
			break;
		}
	}
	if (closestSunken)
	{
		closestSunkenPostion = closestSunken->getPosition();
		if (_order.getType() == SquadOrderTypes::Defend)
		{
			return closestSunkenPostion;
		}
	}

	int minDist = 100000;

	for (auto & unit : _units)
	{
		if (unit->getType() == BWAPI::UnitTypes::Zerg_Overlord) //don't gather near overlords!
		{
			continue;
		}
		if (!_nearEnemy[unit])
		{
			int dist = unit->getDistance(_order.getPosition());
			if (dist < minDist)
			{
				minDist = dist;
				regroup = unit->getPosition();
			}
		}
	}

	if (regroup == BWAPI::Position(0,0))
	{
		if (closestSunkenPostion != BWAPI::Position(0, 0))
		{
			return closestSunkenPostion;
		}
		else
		{
			return BWTA::getRegion(BWTA::getStartLocation(BWAPI::Broodwar->self())->getTilePosition())->getCenter();
		}
	}
	else
	{
		return regroup;
	}
}

BWAPI::Unit Squad::unitClosestToEnemy(BWAPI::Position positionOverride)
{
	BWAPI::Unit closest = nullptr;
	int closestDist = 100000;
	BWAPI::Position closestToWhere = _order.getPosition();
	if (positionOverride != BWAPI::Position(0, 0))
	{
		closestToWhere = positionOverride;
	}

	for (auto & unit : _units)
	{
		if (unit->getType() == BWAPI::UnitTypes::Protoss_Observer || unit->getType() == BWAPI::UnitTypes::Zerg_Overlord)
		{
			continue;
		}

		// the distance to the order position
		int dist = MapTools::Instance().getGroundDistance(unit->getPosition(), closestToWhere);

		if (dist != -1 && (!closest || dist < closestDist))
		{
			closest = unit;
			closestDist = dist;
		}
	}

	if (!closest)
	{
		for (auto & unit : _units)
		{
			if (unit->getType() == BWAPI::UnitTypes::Protoss_Observer || unit->getType() == BWAPI::UnitTypes::Zerg_Overlord)
			{
				continue;
			}

			// the distance to the order position
			int dist = unit->getDistance(BWAPI::Position(BWAPI::Broodwar->enemy()->getStartLocation()));

			if (dist != -1 && (!closest || dist < closestDist))
			{
				closest = unit;
				closestDist = dist;
			}
		}
	}

	return closest;
}

int Squad::squadUnitsNear(BWAPI::Position p)
{
	int numUnits = 0;

	for (auto & unit : _units)
	{
		if (unit->getDistance(p) < 600)
		{
			numUnits++;
		}
	}

	return numUnits;
}

const BWAPI::Unitset & Squad::getUnits() const	
{ 
	return _units; 
} 

const SquadOrder & Squad::getSquadOrder()	const			
{ 
	return _order; 
}

void Squad::addUnit(BWAPI::Unit u)
{
	_units.insert(u);
}

void Squad::removeUnit(BWAPI::Unit u)
{
    _units.erase(u);
}

const std::string & Squad::getName() const
{
    return _name;
}