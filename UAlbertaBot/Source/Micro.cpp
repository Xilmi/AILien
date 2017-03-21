#include "Micro.h"
#include "UnitUtil.h"
#include "MapTools.h"
#include "MapGrid.h"

using namespace UAlbertaBot;

size_t TotalCommands = 0;

const int dotRadius = 2;

void Micro::drawAPM(int x, int y)
{
    int bwapiAPM = BWAPI::Broodwar->getAPM();
    int myAPM = (int)(TotalCommands / ((double)BWAPI::Broodwar->getFrameCount() / (24*60)));
    BWAPI::Broodwar->drawTextScreen(x, y, "%d %d", bwapiAPM, myAPM);
}

void Micro::SmartAttackUnit(BWAPI::Unit attacker, BWAPI::Unit target)
{
    UAB_ASSERT(attacker, "SmartAttackUnit: Attacker not valid");
    UAB_ASSERT(target, "SmartAttackUnit: Target not valid");

    if (!attacker || !target)
    {
        return;
    }

    // if we have issued a command to this unit already this frame, ignore this one
    if (attacker->getLastCommandFrame() >= BWAPI::Broodwar->getFrameCount() || attacker->isAttackFrame())
    {
        return;
    }

    // get the unit's current command
    BWAPI::UnitCommand currentCommand(attacker->getLastCommand());

    // if we've already told this unit to attack this target, ignore this command
    if (currentCommand.getType() == BWAPI::UnitCommandTypes::Attack_Unit &&	currentCommand.getTarget() == target)
    {
        return;
    }

    // if nothing prevents it, attack the target
    attacker->attack(target);
    TotalCommands++;

    if (Config::Debug::DrawUnitTargetInfo) 
    {
        BWAPI::Broodwar->drawCircleMap(attacker->getPosition(), dotRadius, BWAPI::Colors::Red, true);
        BWAPI::Broodwar->drawCircleMap(target->getPosition(), dotRadius, BWAPI::Colors::Red, true);
        BWAPI::Broodwar->drawLineMap( attacker->getPosition(), target->getPosition(), BWAPI::Colors::Red );
    }
}

void Micro::SmartPredictAttack(BWAPI::Unit attacker, BWAPI::Unit target)
{
	if (attacker->isInWeaponRange(target))
	{
		SmartAttackUnit(attacker, target);
	}
	else
	{
		double mySpeed = attacker->getType().topSpeed();
		if (attacker->getType() == BWAPI::UnitTypes::Zerg_Zergling &&  BWAPI::Broodwar->self()->getUpgradeLevel(BWAPI::UpgradeTypes::Metabolic_Boost) > 0)
		{
			mySpeed *= 1.5;
		}
		if (attacker->getType() == BWAPI::UnitTypes::Zerg_Ultralisk &&  BWAPI::Broodwar->self()->getUpgradeLevel(BWAPI::UpgradeTypes::Anabolic_Synthesis) > 0)
		{
			mySpeed *= 1.5;
		}
		if (attacker->getType() == BWAPI::UnitTypes::Zerg_Hydralisk &&  BWAPI::Broodwar->self()->getUpgradeLevel(BWAPI::UpgradeTypes::Muscular_Augments) > 0)
		{
			mySpeed *= 1.5;
		}
		double framesTilEnemyReached = attacker->getPosition().getDistance(target->getPosition()) / mySpeed + BWAPI::Broodwar->getLatencyFrames();

		BWAPI::Position predictedPositon = target->getPosition();
		predictedPositon.x += target->getVelocityX() * framesTilEnemyReached;
		predictedPositon.y += target->getVelocityY() * framesTilEnemyReached;
		if (!predictedPositon.isValid()
			|| (!attacker->isFlying() && !BWAPI::Broodwar->isWalkable(BWAPI::WalkPosition(predictedPositon))))
		{
			SmartAttackUnit(attacker, target);
		}
		else
		{
			SmartMove(attacker, predictedPositon);
			BWAPI::Broodwar->drawLineMap(attacker->getPosition(), predictedPositon, BWAPI::Colors::White);
		}
	}
}

void Micro::SmartLurker(BWAPI::Unit attacker, BWAPI::Unit target)
{
	double distance = attacker->getPosition().getDistance(target->getPosition());
	if (!attacker->isInWeaponRange(target))
	{
		BWAPI::Unit enemy = attacker->getClosestUnit(BWAPI::Filter::IsEnemy&&!BWAPI::Filter::IsFlying);
		bool enemyInRange = false;
		if (enemy)
		{
			enemyInRange = attacker->isInWeaponRange(enemy);
		}
		if (attacker->isBurrowed() && !enemyInRange)
		{
			attacker->unburrow();
			return;
		}
		else
		{
			SmartMove(attacker, target->getPosition());
		}
	}
	else
	{
		if (!attacker->isBurrowed())
		{
			attacker->burrow();
			return;
		}
		else
		{
			SmartAttackUnit(attacker, target);
		}
	}
}

void Micro::SmartAttackMove(BWAPI::Unit attacker, const BWAPI::Position & targetPosition)
{
    //UAB_ASSERT(attacker, "SmartAttackMove: Attacker not valid");
    //UAB_ASSERT(targetPosition.isValid(), "SmartAttackMove: targetPosition not valid");

	if (attacker->getType() == BWAPI::UnitTypes::Zerg_Lurker)
	{
		BWAPI::Unit enemy = attacker->getClosestUnit(BWAPI::Filter::IsEnemy&&!BWAPI::Filter::IsFlying);
		if (enemy)
		{
			SmartLurker(attacker, enemy);
		}
		else
		{
			SmartMove(attacker, targetPosition);
		}
	}

    if (!attacker || !targetPosition.isValid())
    {
        return;
    }

    // if we have issued a command to this unit already this frame, ignore this one
    if (attacker->getLastCommandFrame() >= BWAPI::Broodwar->getFrameCount() || attacker->isAttackFrame())
    {
        return;
    }

    // get the unit's current command
    BWAPI::UnitCommand currentCommand(attacker->getLastCommand());

    // if we've already told this unit to attack this target, ignore this command
    if (currentCommand.getType() == BWAPI::UnitCommandTypes::Attack_Move &&	currentCommand.getTargetPosition() == targetPosition)
	{
		return;
	}

    // if nothing prevents it, attack the target
    attacker->attack(targetPosition);
    TotalCommands++;

    if (Config::Debug::DrawUnitTargetInfo) 
    {
        BWAPI::Broodwar->drawCircleMap(attacker->getPosition(), dotRadius, BWAPI::Colors::Orange, true);
        BWAPI::Broodwar->drawCircleMap(targetPosition, dotRadius, BWAPI::Colors::Orange, true);
        BWAPI::Broodwar->drawLineMap(attacker->getPosition(), targetPosition, BWAPI::Colors::Orange);
    }
}

void Micro::SmartRegroup(BWAPI::Unit unit, BWAPI::Position regroupPosition, BWAPI::Position concaveFacing, std::map<BWAPI::TilePosition, int> &concaveMap)
{
	BWAPI::TilePosition closestUnoccupiedConcaveTile = BWAPI::TilePosition(0, 0);
	int LowestDistanceFromRegroupPosition = 10000;
	for each(auto tile in concaveMap)
	{
		if (BWAPI::Broodwar->getUnitsOnTile(tile.first).empty() && tile.second < 4)
		{
			int DistanceFromRegroupPosition = BWAPI::Position(tile.first).getDistance(unit->getPosition());
			if (DistanceFromRegroupPosition < LowestDistanceFromRegroupPosition)
			{
				closestUnoccupiedConcaveTile = tile.first;
				LowestDistanceFromRegroupPosition = DistanceFromRegroupPosition;
			}
		}
	}
	if (closestUnoccupiedConcaveTile != BWAPI::TilePosition(0, 0))
	{
		if (unit->getTilePosition() != closestUnoccupiedConcaveTile)
		{
			concaveMap[closestUnoccupiedConcaveTile] += unit->getType().supplyRequired();
			SmartMove(unit, BWAPI::Position(closestUnoccupiedConcaveTile));
		}
	}
	else
	{
		SmartMove(unit, regroupPosition);
	}
}

void Micro::SmartMove(BWAPI::Unit attacker, const BWAPI::Position & targetPosition)
{
    //UAB_ASSERT(attacker, "SmartAttackMove: Attacker not valid");
    //UAB_ASSERT(targetPosition.isValid(), "SmartAttackMove: targetPosition not valid");

	if (attacker->isBurrowed() && !attacker->isAttacking())
	{
		attacker->unburrow();
		return;
	}

    if (!attacker || !targetPosition.isValid())
    {
        return;
    }

    // if we have issued a command to this unit already this frame, ignore this one
    if (attacker->getLastCommandFrame() >= BWAPI::Broodwar->getFrameCount() || attacker->isAttackFrame())
    {
        return;
    }

    // get the unit's current command
    BWAPI::UnitCommand currentCommand(attacker->getLastCommand());

    // if we've already told this unit to move to this position, ignore this command
    if ((currentCommand.getType() == BWAPI::UnitCommandTypes::Move) && (currentCommand.getTargetPosition() == targetPosition) && attacker->isMoving())
    {
        return;
    }

    // if nothing prevents it, attack the target
	//if (attacker->getClosestUnit())
	//{
	//	if (attacker->getClosestUnit()->getPlayer() == BWAPI::Broodwar->enemy())
	//	{
	//		if ((attacker->isInWeaponRange(attacker->getClosestUnit()) && !attacker->getClosestUnit()->isInWeaponRange(attacker))
	//			|| (attacker->isFlying() && attacker->getClosestUnit()->getType().airWeapon() == BWAPI::WeaponTypes::None)
	//			|| (!attacker->isFlying() && attacker->getClosestUnit()->getType().groundWeapon() == BWAPI::WeaponTypes::None && !attacker->getClosestUnit()->getType().isBuilding()))
	//		{
	//			const int cooldown = attacker->getType().groundWeapon().damageCooldown();
	//			const int currentCooldown = attacker->isStartingAttack() ? cooldown : attacker->getGroundWeaponCooldown();
	//			const int latency = BWAPI::Broodwar->getLatency();
	//			const int framesToAttack = 2 * latency;
	//			if (currentCooldown <= framesToAttack)
	//			{
	//				attacker->attack(attacker->getClosestUnit());
	//				TotalCommands++;
	//				return;
	//			}
	//		}
	//	}
	//}

    attacker->move(targetPosition);
    TotalCommands++;

    if (Config::Debug::DrawUnitTargetInfo) 
    {
        BWAPI::Broodwar->drawCircleMap(attacker->getPosition(), dotRadius, BWAPI::Colors::White, true);
        BWAPI::Broodwar->drawCircleMap(targetPosition, dotRadius, BWAPI::Colors::White, true);
        BWAPI::Broodwar->drawLineMap(attacker->getPosition(), targetPosition, BWAPI::Colors::White);
    }
}

void Micro::SmartRightClick(BWAPI::Unit unit, BWAPI::Unit target)
{
    UAB_ASSERT(unit, "SmartRightClick: Unit not valid");
    UAB_ASSERT(target, "SmartRightClick: Target not valid");

    if (!unit || !target)
    {
        return;
    }

    // if we have issued a command to this unit already this frame, ignore this one
    if (unit->getLastCommandFrame() >= BWAPI::Broodwar->getFrameCount() || unit->isAttackFrame())
    {
        return;
    }

    // get the unit's current command
    BWAPI::UnitCommand currentCommand(unit->getLastCommand());

    // if we've already told this unit to move to this position, ignore this command
    if ((currentCommand.getType() == BWAPI::UnitCommandTypes::Right_Click_Unit) && (currentCommand.getTargetPosition() == target->getPosition()))
    {
        return;
    }

    // if nothing prevents it, attack the target
    unit->rightClick(target);
    TotalCommands++;

    if (Config::Debug::DrawUnitTargetInfo) 
    {
        BWAPI::Broodwar->drawCircleMap(unit->getPosition(), dotRadius, BWAPI::Colors::Cyan, true);
        BWAPI::Broodwar->drawCircleMap(target->getPosition(), dotRadius, BWAPI::Colors::Cyan, true);
        BWAPI::Broodwar->drawLineMap(unit->getPosition(), target->getPosition(), BWAPI::Colors::Cyan);
    }
}

void Micro::SmartLaySpiderMine(BWAPI::Unit unit, BWAPI::Position pos)
{
    if (!unit)
    {
        return;
    }

    if (!unit->canUseTech(BWAPI::TechTypes::Spider_Mines, pos))
    {
        return;
    }

    BWAPI::UnitCommand currentCommand(unit->getLastCommand());

    // if we've already told this unit to move to this position, ignore this command
    if ((currentCommand.getType() == BWAPI::UnitCommandTypes::Use_Tech_Position) && (currentCommand.getTargetPosition() == pos))
    {
        return;
    }

    unit->canUseTechPosition(BWAPI::TechTypes::Spider_Mines, pos);
}

void Micro::SmartRepair(BWAPI::Unit unit, BWAPI::Unit target)
{
    UAB_ASSERT(unit, "SmartRightClick: Unit not valid");
    UAB_ASSERT(target, "SmartRightClick: Target not valid");

    if (!unit || !target)
    {
        return;
    }

    // if we have issued a command to this unit already this frame, ignore this one
    if (unit->getLastCommandFrame() >= BWAPI::Broodwar->getFrameCount() || unit->isAttackFrame())
    {
        return;
    }

    // get the unit's current command
    BWAPI::UnitCommand currentCommand(unit->getLastCommand());

    // if we've already told this unit to move to this position, ignore this command
    if ((currentCommand.getType() == BWAPI::UnitCommandTypes::Repair) && (currentCommand.getTarget() == target))
    {
        return;
    }

    // if nothing prevents it, attack the target
    unit->repair(target);
    TotalCommands++;

    if (Config::Debug::DrawUnitTargetInfo) 
    {
        BWAPI::Broodwar->drawCircleMap(unit->getPosition(), dotRadius, BWAPI::Colors::Cyan, true);
        BWAPI::Broodwar->drawCircleMap(target->getPosition(), dotRadius, BWAPI::Colors::Cyan, true);
        BWAPI::Broodwar->drawLineMap(unit->getPosition(), target->getPosition(), BWAPI::Colors::Cyan);
    }
}


void Micro::SmartKiteTarget(BWAPI::Unit rangedUnit, BWAPI::Unit target, BWAPI::Position kiteGoal)
{
    UAB_ASSERT(rangedUnit, "SmartKiteTarget: Unit not valid");
	UAB_ASSERT(target, "SmartKiteTarget: Target not valid");
	
	if (target->getType() == BWAPI::UnitTypes::Protoss_Interceptor) //We cannot directly attack Interceptors, we have to a-move-towards them in order to attack them
	{
		SmartAttackMove(rangedUnit, kiteGoal);
		return;
	}

	double range(rangedUnit->getType().groundWeapon().maxRange());
	if (rangedUnit->getType() == BWAPI::UnitTypes::Protoss_Dragoon && BWAPI::Broodwar->self()->getUpgradeLevel(BWAPI::UpgradeTypes::Singularity_Charge))
	{
		range = 6*32;
	}

	// determine whether the target can be kited
    bool kiteLonger = Config::Micro::KiteLongerRangedUnits.find(rangedUnit->getType()) != Config::Micro::KiteLongerRangedUnits.end();
	if (!kiteLonger && range <= target->getType().groundWeapon().maxRange())
	{
		// if we can't kite it, there's no point
		Micro::SmartAttackUnit(rangedUnit, target);
		return;
	}

	bool    kite(true);
	double  dist(rangedUnit->getDistance(target));
	double  speed(rangedUnit->getType().topSpeed());

    
    // if the unit can't attack back don't kite
    if ((rangedUnit->isFlying() && !UnitUtil::CanAttackAir(target)) || (!rangedUnit->isFlying() && !UnitUtil::CanAttackGround(target)))
    {
        kite = false;
    }

	double timeToEnter = std::max(0.0,(dist - range) / speed);
	if ((timeToEnter >= rangedUnit->getGroundWeaponCooldown()))
	{
		kite = false;
	}

	if (target->getType().isBuilding())
	{
		kite = false;
	}
	// if we can't shoot, run away
	if (kite)
	{
		BWAPI::Position fleePosition(rangedUnit->getPosition() - target->getPosition() + rangedUnit->getPosition());
		//BWAPI::Broodwar->drawLineMap(rangedUnit->getPosition(), fleePosition, BWAPI::Colors::Cyan);
		rangedUnit->move(fleePosition);
	}
	// otherwise shoot
	else
	{
		Micro::SmartAttackUnit(rangedUnit, target);
	}
}


void Micro::MutaDanceTarget(BWAPI::Unit muta, BWAPI::Unit target)
{
    UAB_ASSERT(muta, "MutaDanceTarget: Muta not valid");
    UAB_ASSERT(target, "MutaDanceTarget: Target not valid");

    if (!muta || !target)
    {
        return;
    }
	if (muta->isIrradiated()) //send him to die
	{
		SmartMove(muta, MapGrid::Instance().getLeastExplored());
		return;
	}

    const int cooldown                  = muta->getType().groundWeapon().damageCooldown();
    const int latency                   = BWAPI::Broodwar->getLatency();
    const double speed                  = muta->getType().topSpeed();
    const double range                  = muta->getType().groundWeapon().maxRange();
    double distanceToTarget       = muta->getDistance(target);
	if (target->getType() == BWAPI::UnitTypes::Terran_Bunker)
	{
		distanceToTarget += 64;
	}

	const double distanceToFiringRange  = std::max(distanceToTarget - range,0.0);
	const double timeToEnterFiringRange = distanceToFiringRange / speed;
	const int framesToAttack            = static_cast<int>(timeToEnterFiringRange) + 2*latency;

	// How many frames are left before we can attack?
	const int currentCooldown = muta->isStartingAttack() ? cooldown : muta->getGroundWeaponCooldown();

	BWAPI::Position fleeVector = GetKiteVector(target, muta);
	BWAPI::Position moveToPosition(muta->getPosition() + fleeVector);

	// If we can attack by the time we reach our firing range
	if (currentCooldown <= framesToAttack && muta->getHitPoints() > muta->getType().maxHitPoints() / 3)
	{
		// Move towards and attack the target
		muta->attack(target);
	}
	else // Otherwise we cannot attack and should temporarily back off
	{
		if (muta->getType().isWorker()) //Mineralwalk out of combat
		{
			BWAPI::Unit mineralFiled = muta->getClosestUnit(BWAPI::Filter::IsMineralField);
			if (mineralFiled)
			{
				muta->rightClick(mineralFiled);
			}
		}
		else
		{
			// Determine direction to flee
			// Determine point to flee to
			if (moveToPosition.isValid())
			{
				muta->rightClick(moveToPosition);
			}
			else
			{
				muta->move(moveToPosition);
			}
		}
	}
}

BWAPI::Position Micro::GetKiteVector(BWAPI::Unit unit, BWAPI::Unit target)
{
    BWAPI::Position fleeVec(target->getPosition() - unit->getPosition());
    double fleeAngle = atan2(fleeVec.y, fleeVec.x);
    fleeVec = BWAPI::Position(static_cast<int>(64 * cos(fleeAngle)), static_cast<int>(64 * sin(fleeAngle)));
    return fleeVec;
}

const double PI = 3.14159265;
void Micro::Rotate(double &x, double &y, double angle)
{
	angle = angle*PI/180.0;
	x = (x * cos(angle)) - (y * sin(angle));
	y = (y * cos(angle)) + (x * sin(angle));
}

void Micro::Normalize(double &x, double &y)
{
	double length = sqrt((x * x) + (y * y));
	if (length != 0)
	{
		x = (x / length);
		y = (y / length);
	}
}

BWAPI::Unit Micro::getClosestTarget(BWAPI::Unit rangedUnit, const BWAPI::Unitset & targets)
{
	double closestDist = std::numeric_limits<double>::infinity();
	Logger::LogAppendToFile("crash.log", "assigning closesttarget\n");
	BWAPI::Unit closestTarget = rangedUnit;

	Logger::LogAppendToFile("crash.log", "iterate\n");
	for (const auto & target : targets)
	{
		double distance = rangedUnit->getDistance(target);

		if (distance < closestDist)
		{
			Logger::LogAppendToFile("crash.log", "assigning closesttarget by dist\n");
			closestDist = distance;
			closestTarget = target;
		}
	}
	Logger::LogAppendToFile("crash.log", "retrn closest target\n");
	return closestTarget;
}

void Micro::SmartOviScout(BWAPI::Unit unit, BWAPI::Position scoutLocation, double avoidDistance, bool avoidEverything)
{
	BWAPI::Position closestPositionToAvoid = BWAPI::Position(0,0);
	int distance = 10000;
	for (auto enemyUnit : InformationManager::Instance().getUnitData(BWAPI::Broodwar->enemy()).getUnits())
	{
		if (enemyUnit.second.type.airWeapon() != BWAPI::WeaponTypes::None
			|| enemyUnit.second.type == BWAPI::UnitTypes::Terran_Bunker
			|| (avoidEverything && !enemyUnit.second.unit->isBurrowed() && !enemyUnit.second.unit->isCloaked()))
		{
			if (enemyUnit.second.lastPosition.getDistance(unit->getPosition()) < distance)
			{
				distance = enemyUnit.second.lastPosition.getDistance(unit->getPosition());
				closestPositionToAvoid = enemyUnit.second.lastPosition;
			}
		}
	}
	if (closestPositionToAvoid != BWAPI::Position(0, 0) && distance < avoidDistance)
	{
		BWAPI::Position fleePosition(unit->getPosition() - closestPositionToAvoid + unit->getPosition());
		SmartMove(unit, fleePosition);
	}
	else
	{
		SmartMove(unit, scoutLocation);
	}
}

void Micro::SmartAvoid(BWAPI::Unit unit, BWAPI::Position avoidPosition, BWAPI::Position retreatDirection, BWAPI::Position goalPosition, double avoidRange, bool dontSpread)
{
	if (!unit->isUnderAttack() && unit->isBurrowed())
	{
		return;
	}
	if (UnitUtil::GetAllUnitCount(BWAPI::UnitTypes::Zerg_Sunken_Colony, false) == 0)
	{
		dontSpread = false;
	}
	if (dontSpread && avoidPosition.getDistance(retreatDirection) < unit->getPosition().getDistance(retreatDirection))
	{
		dontSpread = false;
	}
	double distance = unit->getPosition().getDistance(avoidPosition);
	if (distance < avoidRange)
	{
		BWAPI::Position fleePosition(unit->getPosition() - avoidPosition + unit->getPosition());
		if ((fleePosition.getDistance(retreatDirection) < avoidPosition.getDistance(retreatDirection) || unit->getDistance(retreatDirection) < std::max(avoidRange, 300.0))
			&& fleePosition.isValid()
			&& (dontSpread == false || unit->getPosition().getDistance(retreatDirection) < 100)
			&& (BWAPI::Broodwar->isWalkable(BWAPI::WalkPosition(fleePosition)) || unit->isFlying()))
		{
			//BWAPI::Broodwar->drawTextMap(unit->getPosition(), "C");
			SmartMove(unit, fleePosition);
		}
		else
		{
			//BWAPI::Broodwar->drawTextMap(unit->getPosition(), "B");
			SmartMove(unit, retreatDirection);
		}
	}
	else
	{
		//BWAPI::Broodwar->drawTextMap(unit->getPosition(), "T");
		SmartAttackMove(unit, goalPosition);
	}
}