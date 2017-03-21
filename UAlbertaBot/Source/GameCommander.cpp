#include "Common.h"
#include "GameCommander.h"
#include "UnitUtil.h"
#include <random>

using namespace UAlbertaBot;

GameCommander::GameCommander() 
    : _initialScoutSet(false)
{
}

void GameCommander::update()
{
	_timerManager.startTimer(TimerManager::All);

	if (BWAPI::Broodwar->self()->supplyUsed() < 1 && BWAPI::Broodwar->self()->minerals() < 50)
	{
		BWAPI::Broodwar->sendText("Good Game, well played, %s", BWAPI::Broodwar->enemy()->getName().c_str());
		BWAPI::Broodwar->leaveGame();
		return;
	}

	// populate the unit vectors we will pass into various managers
	handleUnitAssignments();

	// utility managers
	_timerManager.startTimer(TimerManager::InformationManager);
	InformationManager::Instance().update();
	_timerManager.stopTimer(TimerManager::InformationManager);

	enemyUnitCost = 0;
	enemyAirCost = 0;
	double enemyGroundCost = 0;
	enemyAntiAirCost = 0;
	bool lurkersStillGood = true;
	bool allowMutaInMutaLessBuild = false;
	double enemyMobileDetectorCost = 0;
	double enemyAntiGroundCost = 0;
	double enemyDefenseBuildingCost = 0;
	double enemyBuildingCost = 0;
	double enemyFlyingAntiAir = 0;
	double enemyFlyingBuildingCost = 0;
	double enemySmallCost = 0;
	double enemyMediumCost = 0;
	double enemyLargeCost = 0;
	double enemyGroundAoE = 0;
	double enemyAirAoE = 0;
	double enemyAntiMeleeAoE = 0;
	double enemyMarineGoonCost = 0;

	double myTotalCost = 0;
	double myHydraCost = 0;
	double myLurkerCost = 0;
	double myMutaCost = 0;
	double myLingCost = 0;
	double myUltraCost = 0;
	double myGuardCost = 0;
	double myUnitCost = 0;
	for (auto enemy : InformationManager::Instance().getUnitData(BWAPI::Broodwar->enemy()).getUnits())
	{
		if (enemy.second.type == BWAPI::UnitTypes::Terran_Marine
			|| enemy.second.type == BWAPI::UnitTypes::Protoss_Dragoon)
		{
			enemyMarineGoonCost += enemy.second.type.mineralPrice() + enemy.second.type.gasPrice();
		}
		if (enemy.second.type.size() == BWAPI::UnitSizeTypes::Small)
		{
			enemySmallCost += enemy.second.type.mineralPrice() + enemy.second.type.gasPrice();
		}
		if (enemy.second.type.size() == BWAPI::UnitSizeTypes::Medium)
		{
			enemyMediumCost += enemy.second.type.mineralPrice() + enemy.second.type.gasPrice();
		}
		if (enemy.second.type.size() == BWAPI::UnitSizeTypes::Medium)
		{
			enemyLargeCost += enemy.second.type.mineralPrice() + enemy.second.type.gasPrice();
		}
		if (enemy.second.type == BWAPI::UnitTypes::Protoss_High_Templar
			|| enemy.second.type == BWAPI::UnitTypes::Zerg_Defiler)
		{
			enemyGroundAoE += enemy.second.type.mineralPrice() + enemy.second.type.gasPrice();
			enemyAirAoE += enemy.second.type.mineralPrice() + enemy.second.type.gasPrice();
		}
		if (enemy.second.type == BWAPI::UnitTypes::Terran_Siege_Tank_Siege_Mode
			|| enemy.second.type == BWAPI::UnitTypes::Terran_Siege_Tank_Tank_Mode
			|| enemy.second.type == BWAPI::UnitTypes::Terran_Vulture
			|| enemy.second.type == BWAPI::UnitTypes::Terran_Vulture_Spider_Mine
			|| enemy.second.type == BWAPI::UnitTypes::Protoss_Reaver)
		{
			enemyGroundAoE += enemy.second.type.mineralPrice() + enemy.second.type.gasPrice();
			lurkersStillGood = false;
		}
		if (enemy.second.type == BWAPI::UnitTypes::Terran_Firebat
			|| enemy.second.type == BWAPI::UnitTypes::Protoss_Archon
			|| enemy.second.type == BWAPI::UnitTypes::Zerg_Lurker
			|| enemy.second.type == BWAPI::UnitTypes::Terran_Vulture)
		{
			enemyAntiMeleeAoE += enemy.second.type.mineralPrice() + enemy.second.type.gasPrice();
		}
		if (enemy.second.type == BWAPI::UnitTypes::Protoss_Corsair
			|| enemy.second.type == BWAPI::UnitTypes::Terran_Valkyrie
			|| enemy.second.type == BWAPI::UnitTypes::Protoss_Archon)
		{
			enemyAirAoE += enemy.second.type.mineralPrice() + enemy.second.type.gasPrice();
		}
		if (enemy.second.type.isFlyer() && enemy.second.type != BWAPI::UnitTypes::Zerg_Overlord)
		{
			enemyAirCost += enemy.second.type.mineralPrice() + enemy.second.type.gasPrice();
		}
		if (enemy.second.type.airWeapon() != BWAPI::WeaponTypes::Enum::None)
		{
			enemyAntiAirCost += enemy.second.type.mineralPrice() + enemy.second.type.gasPrice();
			if (enemy.second.type.isFlyer())
			{
				enemyFlyingAntiAir += enemy.second.type.mineralPrice() + enemy.second.type.gasPrice();
			}
		}
		if (enemy.second.type.groundWeapon() != BWAPI::WeaponTypes::Enum::None)
		{
			if (enemy.second.type == BWAPI::UnitTypes::Protoss_Scout
				|| enemy.second.type == BWAPI::UnitTypes::Terran_Wraith
				|| enemy.second.type == BWAPI::UnitTypes::Terran_Goliath)
			{
				enemyAntiGroundCost += (enemy.second.type.mineralPrice() + enemy.second.type.gasPrice()) / 2;
			}
			else
			{
				enemyAntiGroundCost += enemy.second.type.mineralPrice() + enemy.second.type.gasPrice();
			}
		}
		if (!enemy.second.type.isWorker() && !enemy.second.type.isBuilding() && enemy.second.type != BWAPI::UnitTypes::Zerg_Overlord)
		{
			enemyUnitCost += enemy.second.type.mineralPrice() + enemy.second.type.gasPrice();
			if (!enemy.second.type.isFlyer())
			{
				enemyGroundCost += enemy.second.type.mineralPrice() + enemy.second.type.gasPrice();
			}
		}
		if (enemy.second.type.isDetector())
		{
			lurkersStillGood = false;
			if (!enemy.second.type.isBuilding())
			{
				enemyMobileDetectorCost += enemy.second.type.mineralPrice() + enemy.second.type.gasPrice();
			}
		}
		if (enemy.second.type == BWAPI::UnitTypes::Protoss_Reaver
			|| enemy.second.type == BWAPI::UnitTypes::Protoss_Shuttle
			|| enemy.second.type == BWAPI::UnitTypes::Terran_Dropship
			|| enemy.second.type == BWAPI::UnitTypes::Terran_Siege_Tank_Siege_Mode)
		{
			allowMutaInMutaLessBuild = true;
		}
		if (enemy.second.type.isBuilding())
		{
			enemyBuildingCost += enemy.second.type.mineralPrice() + enemy.second.type.gasPrice();
			if (enemy.second.type == BWAPI::UnitTypes::Terran_Barracks
				|| enemy.second.type == BWAPI::UnitTypes::Terran_Factory
				|| enemy.second.type == BWAPI::UnitTypes::Protoss_Gateway
				|| enemy.second.type == BWAPI::UnitTypes::Protoss_Robotics_Facility
				|| enemy.second.type == BWAPI::UnitTypes::Zerg_Spawning_Pool
				|| enemy.second.type == BWAPI::UnitTypes::Zerg_Hydralisk_Den
				|| enemy.second.type == BWAPI::UnitTypes::Zerg_Ultralisk_Cavern
				|| enemy.second.type == BWAPI::UnitTypes::Zerg_Defiler_Mound)
			{
				enemyGroundCost += enemy.second.type.mineralPrice() + enemy.second.type.gasPrice();
			}
			if (enemy.second.type == BWAPI::UnitTypes::Terran_Starport
				|| enemy.second.type == BWAPI::UnitTypes::Protoss_Stargate
				|| enemy.second.type == BWAPI::UnitTypes::Zerg_Spire
				|| enemy.second.type == BWAPI::UnitTypes::Zerg_Greater_Spire)
			{
				enemyAirCost += enemy.second.type.mineralPrice() + enemy.second.type.gasPrice();
			}
			if (enemy.second.type.canAttack())
			{
				enemyDefenseBuildingCost += enemy.second.type.mineralPrice() + enemy.second.type.gasPrice();
				if (enemy.second.type == BWAPI::UnitTypes::Zerg_Sunken_Colony || enemy.second.type == BWAPI::UnitTypes::Zerg_Spore_Colony)
				{
					enemyDefenseBuildingCost += BWAPI::UnitTypes::Zerg_Creep_Colony.mineralPrice() + BWAPI::UnitTypes::Zerg_Sunken_Colony.gasPrice();
					enemyDefenseBuildingCost += BWAPI::UnitTypes::Zerg_Drone.mineralPrice() + BWAPI::UnitTypes::Zerg_Drone.gasPrice();
				}
			}
			if (enemy.second.type == BWAPI::UnitTypes::Terran_Bunker)
			{
				enemyDefenseBuildingCost += enemy.second.type.mineralPrice() + enemy.second.type.gasPrice();
			}
			if (enemy.second.type.isFlyingBuilding() && enemy.second.unit->isFlying())
			{
				enemyFlyingBuildingCost += enemy.second.type.mineralPrice() + enemy.second.type.gasPrice();
			}
		}
	}
	for (auto myUnits : InformationManager::Instance().getUnitData(BWAPI::Broodwar->self()).getUnits())
	{
		if (!myUnits.second.type.isWorker() && !myUnits.second.type.isBuilding() && myUnits.second.type != BWAPI::UnitTypes::Zerg_Overlord)
		{
			if (myUnits.second.type == BWAPI::UnitTypes::Zerg_Zergling)
			{
				myLingCost += (myUnits.second.type.mineralPrice() + myUnits.second.type.gasPrice()) / 2;
				myTotalCost += (myUnits.second.type.mineralPrice() + myUnits.second.type.gasPrice()) / 2;
				myUnitCost += (myUnits.second.type.mineralPrice() + myUnits.second.type.gasPrice()) / 2;
			}
			myUnitCost += myUnits.second.type.mineralPrice() + myUnits.second.type.gasPrice();
		}
		else
		{
			myTotalCost += myUnits.second.type.mineralPrice() + myUnits.second.type.gasPrice();
		}
		if (myUnits.second.type == BWAPI::UnitTypes::Zerg_Hydralisk)
		{
			myHydraCost += myUnits.second.type.mineralPrice() + myUnits.second.type.gasPrice();
		}
		if (myUnits.second.type == BWAPI::UnitTypes::Zerg_Lurker)
		{
			myLurkerCost += myUnits.second.type.mineralPrice() + myUnits.second.type.gasPrice();
			myLurkerCost += BWAPI::UnitTypes::Zerg_Hydralisk.mineralPrice() + BWAPI::UnitTypes::Zerg_Hydralisk.gasPrice();
		}
		if (myUnits.second.type == BWAPI::UnitTypes::Zerg_Mutalisk)
		{
			myMutaCost += myUnits.second.type.mineralPrice() + myUnits.second.type.gasPrice();
		}
		if (myUnits.second.type == BWAPI::UnitTypes::Zerg_Ultralisk)
		{
			myUltraCost += myUnits.second.type.mineralPrice() + myUnits.second.type.gasPrice();
		}
		if (myUnits.second.type == BWAPI::UnitTypes::Zerg_Guardian)
		{
			myGuardCost += myUnits.second.type.mineralPrice() + myUnits.second.type.gasPrice();
		}
	}
	double remainingLingTechCost = BWAPI::UnitTypes::Zerg_Hatchery.mineralPrice() + BWAPI::UnitTypes::Zerg_Hatchery.gasPrice()
		+ BWAPI::UnitTypes::Zerg_Spawning_Pool.mineralPrice() + BWAPI::UnitTypes::Zerg_Spawning_Pool.gasPrice();
	double remainingHydraTechCost = BWAPI::UnitTypes::Zerg_Hatchery.mineralPrice() + BWAPI::UnitTypes::Zerg_Hatchery.gasPrice()
		+ BWAPI::UnitTypes::Zerg_Spawning_Pool.mineralPrice() + BWAPI::UnitTypes::Zerg_Spawning_Pool.gasPrice()
		+ BWAPI::UnitTypes::Zerg_Extractor.mineralPrice() + BWAPI::UnitTypes::Zerg_Extractor.gasPrice()
		+ BWAPI::UnitTypes::Zerg_Hydralisk_Den.mineralPrice() + BWAPI::UnitTypes::Zerg_Hydralisk_Den.gasPrice();
	double remainingLurkerTechCost = BWAPI::UnitTypes::Zerg_Hatchery.mineralPrice() + BWAPI::UnitTypes::Zerg_Hatchery.gasPrice()
		+ BWAPI::UnitTypes::Zerg_Spawning_Pool.mineralPrice() + BWAPI::UnitTypes::Zerg_Spawning_Pool.gasPrice()
		+ BWAPI::UnitTypes::Zerg_Extractor.mineralPrice() + BWAPI::UnitTypes::Zerg_Extractor.gasPrice()
		+ BWAPI::UnitTypes::Zerg_Hydralisk_Den.mineralPrice() + BWAPI::UnitTypes::Zerg_Hydralisk_Den.gasPrice()
		+ BWAPI::UnitTypes::Zerg_Lair.mineralPrice() + BWAPI::UnitTypes::Zerg_Lair.gasPrice()
		+ BWAPI::TechTypes::Lurker_Aspect.mineralPrice() + BWAPI::TechTypes::Lurker_Aspect.gasPrice();
	double remainingMutaTechCost = BWAPI::UnitTypes::Zerg_Hatchery.mineralPrice() + BWAPI::UnitTypes::Zerg_Hatchery.gasPrice()
		+ BWAPI::UnitTypes::Zerg_Spawning_Pool.mineralPrice() + BWAPI::UnitTypes::Zerg_Spawning_Pool.gasPrice()
		+ BWAPI::UnitTypes::Zerg_Extractor.mineralPrice() + BWAPI::UnitTypes::Zerg_Extractor.gasPrice()
		+ BWAPI::UnitTypes::Zerg_Lair.mineralPrice() + BWAPI::UnitTypes::Zerg_Lair.gasPrice()
		+ BWAPI::UnitTypes::Zerg_Spire.mineralPrice() + BWAPI::UnitTypes::Zerg_Spire.gasPrice();
	double remainingUltraTechCost = BWAPI::UnitTypes::Zerg_Hatchery.mineralPrice() + BWAPI::UnitTypes::Zerg_Hatchery.gasPrice()
		+ BWAPI::UnitTypes::Zerg_Spawning_Pool.mineralPrice() + BWAPI::UnitTypes::Zerg_Spawning_Pool.gasPrice()
		+ BWAPI::UnitTypes::Zerg_Extractor.mineralPrice() + BWAPI::UnitTypes::Zerg_Extractor.gasPrice()
		+ BWAPI::UnitTypes::Zerg_Lair.mineralPrice() + BWAPI::UnitTypes::Zerg_Lair.gasPrice()
		+ BWAPI::UnitTypes::Zerg_Queens_Nest.mineralPrice() + BWAPI::UnitTypes::Zerg_Queens_Nest.gasPrice()
		+ BWAPI::UnitTypes::Zerg_Hive.mineralPrice() + BWAPI::UnitTypes::Zerg_Hive.gasPrice()
		+ BWAPI::UnitTypes::Zerg_Ultralisk_Cavern.mineralPrice() + BWAPI::UnitTypes::Zerg_Ultralisk_Cavern.gasPrice();
	double remainingGuardTechCost = BWAPI::UnitTypes::Zerg_Hatchery.mineralPrice() + BWAPI::UnitTypes::Zerg_Hatchery.gasPrice()
		+ BWAPI::UnitTypes::Zerg_Spawning_Pool.mineralPrice() + BWAPI::UnitTypes::Zerg_Spawning_Pool.gasPrice()
		+ BWAPI::UnitTypes::Zerg_Extractor.mineralPrice() + BWAPI::UnitTypes::Zerg_Extractor.gasPrice()
		+ BWAPI::UnitTypes::Zerg_Lair.mineralPrice() + BWAPI::UnitTypes::Zerg_Lair.gasPrice()
		+ BWAPI::UnitTypes::Zerg_Spire.mineralPrice() + BWAPI::UnitTypes::Zerg_Spire.gasPrice()
		+ BWAPI::UnitTypes::Zerg_Queens_Nest.mineralPrice() + BWAPI::UnitTypes::Zerg_Queens_Nest.gasPrice()
		+ BWAPI::UnitTypes::Zerg_Hive.mineralPrice() + BWAPI::UnitTypes::Zerg_Hive.gasPrice()
		+ BWAPI::UnitTypes::Zerg_Spire.mineralPrice() + BWAPI::UnitTypes::Zerg_Greater_Spire.gasPrice();

	if (UnitUtil::GetAllUnitCount(BWAPI::UnitTypes::Zerg_Hatchery) > 0 || UnitUtil::GetAllUnitCount(BWAPI::UnitTypes::Zerg_Lair) > 0 || UnitUtil::GetAllUnitCount(BWAPI::UnitTypes::Zerg_Hive) > 0)
	{
		remainingLingTechCost -= BWAPI::UnitTypes::Zerg_Hatchery.mineralPrice() + BWAPI::UnitTypes::Zerg_Hatchery.gasPrice();
		remainingHydraTechCost -= BWAPI::UnitTypes::Zerg_Hatchery.mineralPrice() + BWAPI::UnitTypes::Zerg_Hatchery.gasPrice();
		remainingLurkerTechCost -= BWAPI::UnitTypes::Zerg_Hatchery.mineralPrice() + BWAPI::UnitTypes::Zerg_Hatchery.gasPrice();
		remainingMutaTechCost -= BWAPI::UnitTypes::Zerg_Hatchery.mineralPrice() + BWAPI::UnitTypes::Zerg_Hatchery.gasPrice();
		remainingUltraTechCost -= BWAPI::UnitTypes::Zerg_Hatchery.mineralPrice() + BWAPI::UnitTypes::Zerg_Hatchery.gasPrice();
		remainingGuardTechCost -= BWAPI::UnitTypes::Zerg_Hatchery.mineralPrice() + BWAPI::UnitTypes::Zerg_Hatchery.gasPrice();
	}
	if (UnitUtil::GetAllUnitCount(BWAPI::UnitTypes::Zerg_Spawning_Pool) > 0)
	{
		remainingLingTechCost -= BWAPI::UnitTypes::Zerg_Spawning_Pool.mineralPrice() + BWAPI::UnitTypes::Zerg_Spawning_Pool.gasPrice();
		remainingHydraTechCost -= BWAPI::UnitTypes::Zerg_Spawning_Pool.mineralPrice() + BWAPI::UnitTypes::Zerg_Spawning_Pool.gasPrice();
		remainingLurkerTechCost -= BWAPI::UnitTypes::Zerg_Spawning_Pool.mineralPrice() + BWAPI::UnitTypes::Zerg_Spawning_Pool.gasPrice();
		remainingMutaTechCost -= BWAPI::UnitTypes::Zerg_Spawning_Pool.mineralPrice() + BWAPI::UnitTypes::Zerg_Spawning_Pool.gasPrice();
		remainingUltraTechCost -= BWAPI::UnitTypes::Zerg_Spawning_Pool.mineralPrice() + BWAPI::UnitTypes::Zerg_Spawning_Pool.gasPrice();
		remainingGuardTechCost -= BWAPI::UnitTypes::Zerg_Spawning_Pool.mineralPrice() + BWAPI::UnitTypes::Zerg_Spawning_Pool.gasPrice();
	}
	if (UnitUtil::GetAllUnitCount(BWAPI::UnitTypes::Zerg_Hydralisk_Den) > 0)
	{
		remainingHydraTechCost -= BWAPI::UnitTypes::Zerg_Hydralisk_Den.mineralPrice() + BWAPI::UnitTypes::Zerg_Hydralisk_Den.gasPrice();
		remainingLurkerTechCost -= BWAPI::UnitTypes::Zerg_Hydralisk_Den.mineralPrice() + BWAPI::UnitTypes::Zerg_Hydralisk_Den.gasPrice();
	}
	if (UnitUtil::GetAllUnitCount(BWAPI::UnitTypes::Zerg_Lair) > 0 || UnitUtil::GetAllUnitCount(BWAPI::UnitTypes::Zerg_Hive) > 0)
	{
		remainingLurkerTechCost -= BWAPI::UnitTypes::Zerg_Lair.mineralPrice() + BWAPI::UnitTypes::Zerg_Lair.gasPrice();
		remainingMutaTechCost -= BWAPI::UnitTypes::Zerg_Lair.mineralPrice() + BWAPI::UnitTypes::Zerg_Lair.gasPrice();
		remainingUltraTechCost -= BWAPI::UnitTypes::Zerg_Lair.mineralPrice() + BWAPI::UnitTypes::Zerg_Lair.gasPrice();
		remainingGuardTechCost -= BWAPI::UnitTypes::Zerg_Lair.mineralPrice() + BWAPI::UnitTypes::Zerg_Lair.gasPrice();
	}
	if (UnitUtil::GetAllUnitCount(BWAPI::UnitTypes::Zerg_Spire) > 0)
	{
		remainingMutaTechCost -= BWAPI::UnitTypes::Zerg_Spire.mineralPrice() + BWAPI::UnitTypes::Zerg_Spire.gasPrice();
		remainingGuardTechCost -= BWAPI::UnitTypes::Zerg_Spire.mineralPrice() + BWAPI::UnitTypes::Zerg_Spire.gasPrice();
	}
	if (UnitUtil::GetAllUnitCount(BWAPI::UnitTypes::Zerg_Queens_Nest) > 0)
	{
		remainingUltraTechCost -= BWAPI::UnitTypes::Zerg_Queens_Nest.mineralPrice() + BWAPI::UnitTypes::Zerg_Queens_Nest.gasPrice();
		remainingGuardTechCost -= BWAPI::UnitTypes::Zerg_Queens_Nest.mineralPrice() + BWAPI::UnitTypes::Zerg_Queens_Nest.gasPrice();
	}
	if (UnitUtil::GetAllUnitCount(BWAPI::UnitTypes::Zerg_Hive) > 0)
	{
		remainingUltraTechCost -= BWAPI::UnitTypes::Zerg_Hive.mineralPrice() + BWAPI::UnitTypes::Zerg_Hive.gasPrice();
		remainingGuardTechCost -= BWAPI::UnitTypes::Zerg_Hive.mineralPrice() + BWAPI::UnitTypes::Zerg_Hive.gasPrice();
	}
	if (UnitUtil::GetAllUnitCount(BWAPI::UnitTypes::Zerg_Ultralisk_Cavern) > 0)
	{
		remainingUltraTechCost -= BWAPI::UnitTypes::Zerg_Ultralisk_Cavern.mineralPrice() + BWAPI::UnitTypes::Zerg_Ultralisk_Cavern.gasPrice();
	}
	if (UnitUtil::GetAllUnitCount(BWAPI::UnitTypes::Zerg_Greater_Spire) > 0)
	{
		remainingGuardTechCost -= BWAPI::UnitTypes::Zerg_Greater_Spire.mineralPrice() + BWAPI::UnitTypes::Zerg_Greater_Spire.gasPrice();
	}
	if (BWAPI::Broodwar->self()->hasResearched(BWAPI::TechTypes::Lurker_Aspect))
	{
		remainingLurkerTechCost -= BWAPI::TechTypes::Lurker_Aspect.mineralPrice() + BWAPI::TechTypes::Lurker_Aspect.gasPrice();
	}
	if (enemyDefenseBuildingCost > enemyUnitCost || (enemyFlyingBuildingCost > 0 && enemyUnitCost == 0)) //When enemy turtles => get more eco!
	{
		macroHeavyness = std::max(macroHeavyness, 0.24);
	}

	double lurkerBonus = 4;
	if (enemyMobileDetectorCost > 0 || !lurkersStillGood)
	{
		lurkerBonus = 1;
	}
	if (_strategy == "Muta")
	{
		lingScore = myTotalCost - remainingLingTechCost - myLingCost + enemyGroundCost - enemyAntiGroundCost - enemyGroundAoE - enemyAntiMeleeAoE;
		mutaScore = myTotalCost - myMutaCost - remainingMutaTechCost + enemyGroundCost + enemyAirCost - enemyAntiAirCost;
		if (UnitUtil::GetAllUnitCount(BWAPI::UnitTypes::Zerg_Extractor) > 3) //only tech to tier 3 when you have > 3 gas
		{
			hydraScore = myTotalCost - myHydraCost - remainingHydraTechCost + enemyGroundCost - enemyAntiGroundCost + enemyAirCost - enemySmallCost * 0.5 - enemyMediumCost * 0.25 - enemyGroundAoE + enemyDefenseBuildingCost / 2;
			lurkerScore = myTotalCost - myLurkerCost - remainingLurkerTechCost + enemyGroundCost * lurkerBonus - enemyAntiGroundCost - enemyDefenseBuildingCost;
			ultraScore = myTotalCost - myUltraCost - remainingUltraTechCost + enemyGroundCost - enemyAntiGroundCost;
			guardScore = myTotalCost - myGuardCost - remainingGuardTechCost + enemyGroundCost - enemyAntiAirCost + enemyGroundAoE + enemyDefenseBuildingCost;
		}
	}
	if (_strategy == "Lurker")
	{
		lingScore = myTotalCost - remainingLingTechCost - myLingCost + enemyGroundCost - enemyAntiGroundCost - enemyGroundAoE - enemyAntiMeleeAoE;
		lurkerScore = myTotalCost - myLurkerCost - remainingLurkerTechCost + enemyGroundCost * lurkerBonus - enemyAntiGroundCost - enemyDefenseBuildingCost;
		if (BWAPI::Broodwar->self()->hasResearched(BWAPI::TechTypes::Lurker_Aspect))
		{
			hydraScore = myTotalCost - myHydraCost - remainingHydraTechCost + enemyGroundCost - enemyAntiGroundCost + enemyAirCost - enemySmallCost * 0.5 - enemyMediumCost * 0.25 - enemyGroundAoE + enemyDefenseBuildingCost / 2;
			mutaScore = myTotalCost - myMutaCost - remainingMutaTechCost + enemyGroundCost + enemyAirCost - enemyAntiAirCost;
		}
		if (UnitUtil::GetAllUnitCount(BWAPI::UnitTypes::Zerg_Extractor) > 3) //only tech to tier 3 when you have > 3 gas
		{
			ultraScore = myTotalCost - myUltraCost - remainingUltraTechCost + enemyGroundCost - enemyAntiGroundCost;
			guardScore = myTotalCost - myGuardCost - remainingGuardTechCost + enemyGroundCost - enemyAntiAirCost + enemyGroundAoE + enemyDefenseBuildingCost;
		}
	}
	if (_strategy == "Hydra")
	{
		lingScore = myTotalCost - remainingLingTechCost - myLingCost + enemyGroundCost - enemyAntiGroundCost - enemyGroundAoE - enemyAntiMeleeAoE;
		hydraScore = myTotalCost - myHydraCost - remainingHydraTechCost + enemyGroundCost - enemyAntiGroundCost + enemyAirCost - enemySmallCost * 0.5 - enemyMediumCost * 0.25 - enemyGroundAoE + enemyDefenseBuildingCost / 2;
		if (BWAPI::Broodwar->self()->getUpgradeLevel(BWAPI::UpgradeTypes::Muscular_Augments) + BWAPI::Broodwar->self()->getUpgradeLevel(BWAPI::UpgradeTypes::Grooved_Spines) >= 2)
		{
			mutaScore = myTotalCost - myMutaCost - remainingMutaTechCost + enemyGroundCost + enemyAirCost - enemyAntiAirCost;
			lurkerScore = myTotalCost - myLurkerCost - remainingLurkerTechCost + enemyGroundCost * lurkerBonus - enemyAntiGroundCost - enemyDefenseBuildingCost;
		}
		if (UnitUtil::GetAllUnitCount(BWAPI::UnitTypes::Zerg_Extractor) > 3) //only tech to tier 3 when you have > 3 gas
		{
			ultraScore = myTotalCost - myUltraCost - remainingUltraTechCost + enemyGroundCost - enemyAntiGroundCost;
			guardScore = myTotalCost - myGuardCost - remainingGuardTechCost + enemyGroundCost - enemyAntiAirCost + enemyGroundAoE + enemyDefenseBuildingCost;
		}
	}
	if (enemyFlyingAntiAir > 0) //3 Wraiths killed like 10 guardians in one game... don't get Guardians when enemy has stuff like that!
	{
		guardScore = 0;
	}

	_timerManager.startTimer(TimerManager::MapGrid);
	MapGrid::Instance().update();
	_timerManager.stopTimer(TimerManager::MapGrid);

	//_timerManager.startTimer(TimerManager::MapTools);
	//MapTools::Instance().update();
	//_timerManager.stopTimer(TimerManager::MapTools);

	//_timerManager.startTimer(TimerManager::Search);
	//BOSSManager::Instance().update(35 - _timerManager.getTotalElapsed());
	//_timerManager.stopTimer(TimerManager::Search);

	// economy and base managers
	_timerManager.startTimer(TimerManager::Worker);
	WorkerManager::Instance().update();
	_timerManager.stopTimer(TimerManager::Worker);

	_timerManager.startTimer(TimerManager::Production);
	ProductionManager::Instance().update(macroHeavyness, sunkensPerWorkerSupply, lingScore, hydraScore, lurkerScore, mutaScore, ultraScore, guardScore);
	_timerManager.stopTimer(TimerManager::Production);

	_timerManager.startTimer(TimerManager::Building);
	BuildingManager::Instance().update();
	_timerManager.stopTimer(TimerManager::Building);

	// combat and scouting managers
	_timerManager.startTimer(TimerManager::Combat);
	_combatCommander.update(_combatUnits, myTotalCost, enemyUnitCost);
	_timerManager.stopTimer(TimerManager::Combat);

	_timerManager.startTimer(TimerManager::Scout);
    ScoutManager::Instance().update();
	_timerManager.stopTimer(TimerManager::Scout);
		
	_timerManager.stopTimer(TimerManager::All);

	drawDebugInterface();
}

void GameCommander::drawDebugInterface()
{
	InformationManager::Instance().drawExtendedInterface();
	InformationManager::Instance().drawUnitInformation(425,30);
	InformationManager::Instance().drawMapInformation();
	BuildingManager::Instance().drawBuildingInformation(200,50);
	BuildingPlacer::Instance().drawReservedTiles();
	ProductionManager::Instance().drawProductionInformation(30, 50);
	BOSSManager::Instance().drawSearchInformation(490, 100);
    BOSSManager::Instance().drawStateInformation(250, 0);
    
	_combatCommander.drawSquadInformation(200, 30);
    _timerManager.displayTimers(490, 225);
    drawGameInformation(4, 1);

	// draw position of mouse cursor
	if (Config::Debug::DrawMouseCursorInfo)
	{
		int mouseX = BWAPI::Broodwar->getMousePosition().x + BWAPI::Broodwar->getScreenPosition().x;
		int mouseY = BWAPI::Broodwar->getMousePosition().y + BWAPI::Broodwar->getScreenPosition().y;
		BWAPI::Broodwar->drawTextMap(mouseX + 20, mouseY, " %d %d", mouseX, mouseY);
	}
}

void GameCommander::drawGameInformation(int x, int y)
{
    BWAPI::Broodwar->drawTextScreen(x, y, "\x04Players:");
	BWAPI::Broodwar->drawTextScreen(x+50, y, "%c%s \x04vs. %c%s", BWAPI::Broodwar->self()->getTextColor(), BWAPI::Broodwar->self()->getName().c_str(), 
                                                                  BWAPI::Broodwar->enemy()->getTextColor(), BWAPI::Broodwar->enemy()->getName().c_str());
	y += 12;

	std::string sEco;
	if (macroHeavyness < 0.14)
	{
		sEco = "Early pool";
	}
	else if (macroHeavyness == 0.14)
	{
		sEco = "Overpool";
	}
	else if (macroHeavyness < 0.22)
	{
		sEco = "Late pool";
	}
	else
	{
		sEco = "12 Hatch";
	}
	if (lingScore > 0)
	{
		sEco += " into Zerglings";
	}
	if (hydraScore > 0)
	{
		sEco += " into Hydralisks";
	}
	if (lurkerScore > 0)
	{
		sEco += " into Lurkers";
	}
	if (mutaScore > 0)
	{
		sEco += " into Mutalisks";
	}
	if (ultraScore > 0)
	{
		sEco += " into Ultralisks";
	}
	if (guardScore > 0)
	{
		sEco += " into Guardians";
	}

    BWAPI::Broodwar->drawTextScreen(x, y, "\x04Strategy:");
	BWAPI::Broodwar->drawTextScreen(x + 50, y, "\x03%s \x04", sEco.c_str());
	BWAPI::Broodwar->setTextSize();
	y += 12;

	BWAPI::Broodwar->drawTextScreen(x, y, "\x04" "Desire:");
	BWAPI::Broodwar->drawTextScreen(x + 50, y, "\x03" "Zergling: %d Hydralisk: %d Lurker: %d Mutalisk: %d Ultralisk: %d Guardian: %d", int(lingScore), int(hydraScore), int(lurkerScore), int(mutaScore), int(ultraScore), int(guardScore));
	BWAPI::Broodwar->setTextSize();
	y += 12;

    BWAPI::Broodwar->drawTextScreen(x, y, "\x04Map:");
	BWAPI::Broodwar->drawTextScreen(x+50, y, "\x03%s", BWAPI::Broodwar->mapFileName().c_str());
	BWAPI::Broodwar->setTextSize();
	y += 12;

    BWAPI::Broodwar->drawTextScreen(x, y, "\x04Time:");
    BWAPI::Broodwar->drawTextScreen(x+50, y, "\x04%d %4dm %3ds", BWAPI::Broodwar->getFrameCount(), (int)(BWAPI::Broodwar->getFrameCount()/(23.8*60)), (int)((int)(BWAPI::Broodwar->getFrameCount()/23.8)%60));
}

// assigns units to various managers
void GameCommander::handleUnitAssignments()
{
	_validUnits.clear();
    _combatUnits.clear();

	// filter our units for those which are valid and usable
	setValidUnits();

	// set each type of unit
	//setScoutUnits();
	setCombatUnits();
}

bool GameCommander::isAssigned(BWAPI::Unit unit) const
{
	return _combatUnits.contains(unit) || _scoutUnits.contains(unit);
}

// validates units as usable for distribution to various managers
void GameCommander::setValidUnits()
{
	// make sure the unit is completed and alive and usable
	for (auto & unit : BWAPI::Broodwar->self()->getUnits())
	{
		if (UnitUtil::IsValidUnit(unit))
		{	
			_validUnits.insert(unit);
		}
	}
}

void GameCommander::setScoutUnits()
{
    // if we haven't set a scout unit, do it
	if (_scoutUnits.empty() && !_initialScoutSet && UnitUtil::GetAllUnitCount(BWAPI::UnitTypes::Zerg_Drone) >= 9)
    {
		// grab the closest worker to the supply provider to send to scout
		BWAPI::Unit workerScout = getClosestWorkerToTarget(BWAPI::Position(MapTools::Instance().getNextExpansion()));

		// if we find a worker (which we should) add it to the scout units
		if (workerScout)
		{
            ScoutManager::Instance().setWorkerScout(workerScout);
			assignUnit(workerScout, _scoutUnits);
            _initialScoutSet++;
		}
    }
}

// sets combat units to be passed to CombatCommander
void GameCommander::setCombatUnits()
{
	for (auto & unit : _validUnits)
	{
		if (!isAssigned(unit) && UnitUtil::IsCombatUnit(unit) || unit->getType().isWorker())		
		{	
			assignUnit(unit, _combatUnits);
		}
	}
}

BWAPI::Unit GameCommander::getFirstSupplyProvider()
{
	BWAPI::Unit supplyProvider = nullptr;

	if (BWAPI::Broodwar->self()->getRace() == BWAPI::Races::Zerg)
	{
		for (auto & unit : BWAPI::Broodwar->self()->getUnits())
		{
			//if (unit->getType().isResourceDepot() && _initialScoutSet == 0)
			//{
			//	supplyProvider = unit;
			//}
			if (unit->getType() == BWAPI::UnitTypes::Zerg_Spawning_Pool)
			{
				supplyProvider = unit;
			}
		}
	}
	else
	{
		
		for (auto & unit : BWAPI::Broodwar->self()->getUnits())
		{
			if (unit->getType() == BWAPI::Broodwar->self()->getRace().getSupplyProvider())
			{
				supplyProvider = unit;
			}
		}
	}

	return supplyProvider;
}

void GameCommander::onUnitShow(BWAPI::Unit unit)			
{ 
	InformationManager::Instance().onUnitShow(unit); 
	WorkerManager::Instance().onUnitShow(unit);
}

void GameCommander::onUnitHide(BWAPI::Unit unit)			
{ 
	InformationManager::Instance().onUnitHide(unit); 
}

void GameCommander::onUnitCreate(BWAPI::Unit unit)		
{ 
	InformationManager::Instance().onUnitCreate(unit); 
}

void GameCommander::onUnitComplete(BWAPI::Unit unit)
{
	InformationManager::Instance().onUnitComplete(unit);
}

void GameCommander::onUnitRenegade(BWAPI::Unit unit)		
{ 
	InformationManager::Instance().onUnitRenegade(unit); 
}

void GameCommander::onUnitDestroy(BWAPI::Unit unit)		
{ 	
	ProductionManager::Instance().onUnitDestroy(unit);
	WorkerManager::Instance().onUnitDestroy(unit);
	InformationManager::Instance().onUnitDestroy(unit); 
}

void GameCommander::onUnitMorph(BWAPI::Unit unit)		
{ 
	InformationManager::Instance().onUnitMorph(unit);
	WorkerManager::Instance().onUnitMorph(unit);
}

BWAPI::Unit GameCommander::getClosestUnitToTarget(BWAPI::UnitType type, BWAPI::Position target)
{
	BWAPI::Unit closestUnit = nullptr;
	double closestDist = 100000;

	for (auto & unit : _validUnits)
	{
		if (unit->getType() == type)
		{
			double dist = unit->getDistance(target);
			if (!closestUnit || dist < closestDist)
			{
				closestUnit = unit;
				closestDist = dist;
			}
		}
	}

	return closestUnit;
}

BWAPI::Unit GameCommander::getClosestWorkerToTarget(BWAPI::Position target)
{
	BWAPI::Unit closestUnit = nullptr;
	double closestDist = 100000;

	for (auto & unit : _validUnits)
	{
		if (unit->getType().isWorker() && !unit->isCarryingMinerals())
		{
			double dist = unit->getDistance(target);
			if (!closestUnit || dist < closestDist)
			{
				closestUnit = unit;
				closestDist = dist;
			}
		}
	}

	return closestUnit;
}

void GameCommander::assignUnit(BWAPI::Unit unit, BWAPI::Unitset & set)
{
    if (_scoutUnits.contains(unit)) { _scoutUnits.erase(unit); }
    else if (_combatUnits.contains(unit)) { _combatUnits.erase(unit); }

    set.insert(unit);
}

void GameCommander::onGameStart()
{
	std::random_device rd;
	std::mt19937 gen(rd());
	double minMacro = 0.12;
	double maxMacro = 0.16;
	std::uniform_real_distribution<double> rndmH(minMacro, maxMacro);
	macroHeavyness = rndmH(gen);
	if (BWAPI::Broodwar->enemy()->getRace() == BWAPI::Races::Protoss)
	{
		macroHeavyness = 0.22;
		_strategy = "Muta";
	}
	else if (BWAPI::Broodwar->enemy()->getRace() == BWAPI::Races::Terran)
	{
		macroHeavyness = 0.22;
		_strategy = "Muta";
	}
	else
	{
		macroHeavyness = 0.14;
		_strategy = "Muta";
	}
	sunkensPerWorkerSupply = 0.1;
	//BWAPI::Broodwar->setLatCom(false); //this line made everything way worse than before!
	BWAPI::Broodwar->sendText("AILien 2017-03-20-21-22");
}
