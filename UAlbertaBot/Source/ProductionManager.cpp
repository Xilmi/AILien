#include "ProductionManager.h"
#include "UnitUtil.h"
#include "WorkerManager.h"
#include "GameCommander.h"
#include <random>

using namespace UAlbertaBot;

ProductionManager::ProductionManager() 
	: _assignedWorkerForThisBuilding (false)
	, _haveLocationForThisBuilding   (false)
	, _enemyCloakedDetected          (false)
{
    //setBuildOrder(StrategyManager::Instance().getOpeningBookBuildOrder());
}

void ProductionManager::setBuildOrder(const BuildOrder & buildOrder)
{
	_queue.clearAll();

	for (size_t i(0); i<buildOrder.size(); ++i)
	{
		_queue.queueAsLowestPriority(buildOrder[i], true);
	}
}

void ProductionManager::performBuildOrderSearch()
{	
    if (!Config::Modules::UsingBuildOrderSearch || !canPlanBuildOrderNow())
    {
        return;
    }

	BuildOrder & buildOrder = BOSSManager::Instance().getBuildOrder();

    if (buildOrder.size() > 0)
    {
	    setBuildOrder(buildOrder);
        BOSSManager::Instance().reset();
    }
    else
    {
        if (!BOSSManager::Instance().isSearchInProgress())
        {
			BOSSManager::Instance().startNewSearch(StrategyManager::Instance().getBuildOrderGoal());
        }
    }
}

void ProductionManager::update(double macroHeavyness, double sunkensPerWorkerSupply, double lingScore, double hydraScore, double lurkerScore, double mutaScore, double ultraScore, double guardScore)
{
	// check the _queue for stuff we can build
	manageBuildOrderQueue();
	_queue.clearAll();

	double hydraSupply = UnitUtil::GetAllUnitCount(BWAPI::UnitTypes::Zerg_Hydralisk) * BWAPI::UnitTypes::Zerg_Hydralisk.supplyRequired();
	double lurkerSupply = (UnitUtil::GetAllUnitCount(BWAPI::UnitTypes::Zerg_Lurker) + UnitUtil::GetAllUnitCount(BWAPI::UnitTypes::Zerg_Lurker_Egg)) * BWAPI::UnitTypes::Zerg_Lurker.supplyRequired();
	double mutaSupply = UnitUtil::GetAllUnitCount(BWAPI::UnitTypes::Zerg_Mutalisk) * BWAPI::UnitTypes::Zerg_Mutalisk.supplyRequired();
	double lingSupply = UnitUtil::GetAllUnitCount(BWAPI::UnitTypes::Zerg_Zergling) * BWAPI::UnitTypes::Zerg_Zergling.supplyRequired();
	double ultraSupply = UnitUtil::GetAllUnitCount(BWAPI::UnitTypes::Zerg_Ultralisk) * BWAPI::UnitTypes::Zerg_Ultralisk.supplyRequired();
	double guardSupply = UnitUtil::GetAllUnitCount(BWAPI::UnitTypes::Zerg_Guardian) * BWAPI::UnitTypes::Zerg_Guardian.supplyRequired();

	if (guardScore > mutaScore && mutaSupply == 0) //We want guardians but have no mutas? simply go for mutas first
	{
		mutaScore = guardScore;
	}
	if (lurkerScore > hydraScore && hydraSupply == 0) //We want guardians but have no mutas? simply go for mutas first
	{
		hydraScore = lurkerScore;
	}

	bool lingsAllowed = true;

	bool shouldExpand = StrategyManager::Instance().shouldExpandNow();
	int availableMinerals = BWAPI::Broodwar->self()->minerals();
	int availableGas = BWAPI::Broodwar->self()->gas();
	int availableLarvae = UnitUtil::GetAllUnitCount(BWAPI::UnitTypes::Zerg_Larva);
	int workerSupply = UnitUtil::GetAllUnitCount(BWAPI::Broodwar->self()->getRace().getWorker()) * BWAPI::Broodwar->self()->getRace().getWorker().supplyRequired();
	double armySupply = hydraSupply + lurkerSupply + mutaSupply + lingSupply + ultraSupply + guardSupply;
	int poolCount = UnitUtil::GetAllUnitCount(BWAPI::UnitTypes::Zerg_Spawning_Pool);
	int hydraDenCount = UnitUtil::GetAllUnitCount(BWAPI::UnitTypes::Zerg_Hydralisk_Den);
	int spireCount = UnitUtil::GetAllUnitCount(BWAPI::UnitTypes::Zerg_Spire);
	int greaterSpireCount = UnitUtil::GetAllUnitCount(BWAPI::UnitTypes::Zerg_Greater_Spire);
	int lairCount = UnitUtil::GetAllUnitCount(BWAPI::UnitTypes::Zerg_Lair);
	int hiveCount = UnitUtil::GetAllUnitCount(BWAPI::UnitTypes::Zerg_Hive);
	int baseCount = UnitUtil::GetAllUnitCount(BWAPI::UnitTypes::Zerg_Hatchery);
	int finishedBaseCount = UnitUtil::GetAllUnitCount(BWAPI::UnitTypes::Zerg_Hatchery, true);
	int evoCount = UnitUtil::GetAllUnitCount(BWAPI::UnitTypes::Zerg_Evolution_Chamber);
	int nestCount = UnitUtil::GetAllUnitCount(BWAPI::UnitTypes::Zerg_Queens_Nest);
	int cavernCount = UnitUtil::GetAllUnitCount(BWAPI::UnitTypes::Zerg_Ultralisk_Cavern);
	int extractorCount = UnitUtil::GetAllUnitCount(BWAPI::UnitTypes::Zerg_Extractor);
	int creepColonyCount = UnitUtil::GetAllUnitCount(BWAPI::UnitTypes::Zerg_Creep_Colony);
	int sunkenColonyCount = UnitUtil::GetAllUnitCount(BWAPI::UnitTypes::Zerg_Sunken_Colony, false, 0.75);
	int sporeColonyCount = UnitUtil::GetAllUnitCount(BWAPI::UnitTypes::Zerg_Spore_Colony);
	int metabolicBoostLevel = BWAPI::Broodwar->self()->getUpgradeLevel(BWAPI::UpgradeTypes::Metabolic_Boost);
	int adrenalGlandsLevel = BWAPI::Broodwar->self()->getUpgradeLevel(BWAPI::UpgradeTypes::Adrenal_Glands);
	int augmentLevel = BWAPI::Broodwar->self()->getUpgradeLevel(BWAPI::UpgradeTypes::Muscular_Augments);
	int spineLevel = BWAPI::Broodwar->self()->getUpgradeLevel(BWAPI::UpgradeTypes::Grooved_Spines);
	bool lurkerAspect = BWAPI::Broodwar->self()->hasResearched(BWAPI::TechTypes::Lurker_Aspect);
	int chitinLevel = BWAPI::Broodwar->self()->getUpgradeLevel(BWAPI::UpgradeTypes::Chitinous_Plating);
	int anabolicLevel = BWAPI::Broodwar->self()->getUpgradeLevel(BWAPI::UpgradeTypes::Anabolic_Synthesis);
	int hatchesInProgress = baseCount - UnitUtil::GetAllUnitCount(BWAPI::UnitTypes::Zerg_Hatchery, true);
	int queuedSupplyProviders = UnitUtil::GetAllUnitCount(BWAPI::UnitTypes::Zerg_Overlord) - UnitUtil::GetAllUnitCount(BWAPI::UnitTypes::Zerg_Overlord, true);
	int airAttackLevel = BWAPI::Broodwar->self()->getUpgradeLevel(BWAPI::UpgradeTypes::Zerg_Flyer_Attacks);
	int airDefenseLevel = BWAPI::Broodwar->self()->getUpgradeLevel(BWAPI::UpgradeTypes::Zerg_Flyer_Carapace);
	int meleeAttackLevel = BWAPI::Broodwar->self()->getUpgradeLevel(BWAPI::UpgradeTypes::Zerg_Melee_Attacks);
	int rangedAttackLevel = BWAPI::Broodwar->self()->getUpgradeLevel(BWAPI::UpgradeTypes::Zerg_Missile_Attacks);
	int groundDefenseLevel = BWAPI::Broodwar->self()->getUpgradeLevel(BWAPI::UpgradeTypes::Zerg_Carapace);
	int pneumaLevel = BWAPI::Broodwar->self()->getUpgradeLevel(BWAPI::UpgradeTypes::Pneumatized_Carapace);
	baseCount += lairCount;
	baseCount += hiveCount;
	finishedBaseCount += lairCount;
	finishedBaseCount += hiveCount;
	queuedSupplyProviders *= BWAPI::Broodwar->self()->getRace().getSupplyProvider().supplyProvided();

	double enemyUnitCost = 0;
	double enemyGroundUnitCost = 0;
	double myUnitCost = 0;
	double mySunkenValue = 0;
	bool enemyHasVulture = false;
	bool enemyBuildingFound = false;
	for (auto enemy : InformationManager::Instance().getUnitData(BWAPI::Broodwar->enemy()).getUnits())
	{
		if (enemy.second.type.isBuilding())
		{
			enemyBuildingFound = true;
		}
		if (enemy.second.unit->isVisible() && enemy.second.type.isWorker()) //this usuallyhappens when i have workers in my base...
		{
			enemyUnitCost += enemy.second.type.mineralPrice() + enemy.second.type.gasPrice();
		}
		if ((!enemy.second.type.isWorker() && !enemy.second.type.isBuilding() 
			&& enemy.second.type != BWAPI::UnitTypes::Zerg_Overlord
			&& enemy.second.type != BWAPI::UnitTypes::Zerg_Egg
			&& enemy.second.type != BWAPI::UnitTypes::Zerg_Larva))
		{
			if (enemy.second.type == BWAPI::UnitTypes::Zerg_Zergling)
			{
				enemyUnitCost += (enemy.second.type.mineralPrice() + enemy.second.type.gasPrice()) / 2;
			}
			else
			{
				enemyUnitCost += enemy.second.type.mineralPrice() + enemy.second.type.gasPrice();
			}
			if (!enemy.second.type.isFlyer() 
				&& enemy.second.type != BWAPI::UnitTypes::Terran_Siege_Tank_Siege_Mode 
				&& enemy.second.type != BWAPI::UnitTypes::Terran_Siege_Tank_Tank_Mode
				&& enemy.second.type != BWAPI::UnitTypes::Protoss_Reaver)
			{
				if (enemy.second.type == BWAPI::UnitTypes::Zerg_Zergling)
				{
					enemyGroundUnitCost += (enemy.second.type.mineralPrice() + enemy.second.type.gasPrice()) / 2;
				}
				else
				{
					enemyGroundUnitCost += enemy.second.type.mineralPrice() + enemy.second.type.gasPrice();
				}
			}
			if (enemy.second.type == BWAPI::UnitTypes::Terran_Vulture)
			{
				enemyHasVulture = true;
			}
		}
	}
	if (enemyHasVulture)
	{
		lingsAllowed = false;
	}

	for (auto myUnits : InformationManager::Instance().getUnitData(BWAPI::Broodwar->self()).getUnits())
	{
		if (!myUnits.second.type.isWorker() && !myUnits.second.type.isBuilding() 
			&& myUnits.second.type != BWAPI::UnitTypes::Zerg_Overlord 
			&& myUnits.second.type != BWAPI::UnitTypes::Zerg_Egg 
			&& myUnits.second.type != BWAPI::UnitTypes::Zerg_Larva)
		{
			if (myUnits.second.type == BWAPI::UnitTypes::Zerg_Zergling || myUnits.second.type == BWAPI::UnitTypes::Zerg_Scourge)
			{
				myUnitCost += (myUnits.second.type.mineralPrice() + myUnits.second.type.gasPrice()) / 2;
			}
			else
			{
				myUnitCost += myUnits.second.type.mineralPrice() + myUnits.second.type.gasPrice();
			}
		}
		if (myUnits.second.type == BWAPI::UnitTypes::Zerg_Sunken_Colony)
		{
			mySunkenValue += myUnits.second.type.mineralPrice() + myUnits.second.type.gasPrice();
			mySunkenValue += BWAPI::UnitTypes::Zerg_Creep_Colony.mineralPrice() + BWAPI::UnitTypes::Zerg_Creep_Colony.gasPrice();
			mySunkenValue += BWAPI::UnitTypes::Zerg_Drone.mineralPrice() + BWAPI::UnitTypes::Zerg_Drone.gasPrice();
		}
	}
	myUnitCost += mySunkenValue;
	//BWAPI::Broodwar->printf("MyUnitCost: %d EnemyUnitCost: %d", int(myUnitCost), int(enemyUnitCost));
	//build so many worker-supply per army-supply
	double workerFactor = std::max(0.3, macroHeavyness * 3);

	int maxWorkerSupply = 150;
	double workerSupplyBeforeUnits =maxWorkerSupply * macroHeavyness;
	bool underPressure = false;
	bool needSunkens = false;
	//BWAPI::Broodwar->printf("enemyGroundUnitCost: %d myUnitCost: %d", int(enemyGroundUnitCost), int(myUnitCost));
	if (enemyGroundUnitCost > myUnitCost)
	{
		needSunkens = true;
	}
	if (!shouldExpand)
	{
		maxWorkerSupply = std::min(WorkerManager::Instance().workerData.getNumDepots(false) * 38, maxWorkerSupply); //TODO: count all patches near our depots
	}
	if (enemyUnitCost > myUnitCost * 2.0 && enemyUnitCost > 50)
	{
		if (armySupply < workerSupply) //Well, we can't just spam lings when we are being camped... need to allow teching too
		{
			workerSupplyBeforeUnits = 6;
			workerFactor = 0.3;
			underPressure = true;
		}
	}
	else if (enemyUnitCost > 0 && sunkenUnlocked) //let's be more macro-oriented after our initial rush
	{
		if (BWAPI::Broodwar->enemy()->getRace() == BWAPI::Races::Protoss || BWAPI::Broodwar->enemy()->getRace() == BWAPI::Races::Terran)
		{
			workerSupplyBeforeUnits *= 1.5;
		}
	}
	checkCancels(underPressure);
	if (_sunkenUnlockTime == 72000)
	{
		if (WorkerManager::Instance().workerData.getNumDepots(false, true) >= 2)
		{
			_sunkenUnlockTime = BWAPI::Broodwar->getFrameCount() + 24 * 10; //sunkens shall not be ordered before 10 seconds after the first expansion is up so creep can spread
		}
	}
	if (BWAPI::Broodwar->getFrameCount() >= _sunkenUnlockTime)
	{
		sunkenUnlocked = true;
	}
	//while we are waiting for our 2nd base to finish we need to spam lings in order to hold zealot-rush or the likes
	if ((sunkenUnlocked == false || finishedBaseCount < 2 || !enemyBuildingFound) && UnitUtil::GetAllUnitCount(BWAPI::UnitTypes::Zerg_Spawning_Pool) > 0)
	{
		workerSupplyBeforeUnits = std::min(26.0, workerSupplyBeforeUnits);
		workerFactor = 0;
		underPressure = true;
	}

	availableMinerals -= BuildingManager::Instance().getReservedMinerals();
	availableGas -= BuildingManager::Instance().getReservedGas();

	int mineralsToSave = 0;
	int gasToSave = 0;
	int larvaeToSave = 0;

	for (size_t i = 0; i < BuildingManager::Instance().buildingsQueued().size(); ++i)
	{
		if (BuildingManager::Instance().buildingsQueued()[i] == BWAPI::UnitTypes::Zerg_Spawning_Pool)
		{
			poolCount++;
		}
		if (BuildingManager::Instance().buildingsQueued()[i] == BWAPI::UnitTypes::Zerg_Creep_Colony)
		{
			creepColonyCount++;
		}
		if (BuildingManager::Instance().buildingsQueued()[i] == BWAPI::UnitTypes::Zerg_Sunken_Colony)
		{
			sunkenColonyCount++;
		}
		if (BuildingManager::Instance().buildingsQueued()[i] == BWAPI::UnitTypes::Zerg_Spore_Colony)
		{
			sporeColonyCount++;
		}
		if (BuildingManager::Instance().buildingsQueued()[i] == BWAPI::UnitTypes::Zerg_Extractor)
		{
			extractorCount++;
		}
		if (BuildingManager::Instance().buildingsQueued()[i] == BWAPI::UnitTypes::Zerg_Hydralisk_Den)
		{
			hydraDenCount++;
		}
		if (BuildingManager::Instance().buildingsQueued()[i] == BWAPI::UnitTypes::Zerg_Lair)
		{
			lairCount++;
		}
		if (BuildingManager::Instance().buildingsQueued()[i] == BWAPI::UnitTypes::Zerg_Spire)
		{
			spireCount++;
		}
		if (BuildingManager::Instance().buildingsQueued()[i] == BWAPI::UnitTypes::Zerg_Greater_Spire)
		{
			greaterSpireCount++;
		}
		if (BuildingManager::Instance().buildingsQueued()[i] == BWAPI::UnitTypes::Zerg_Evolution_Chamber)
		{
			evoCount++;
		}
		if (BuildingManager::Instance().buildingsQueued()[i] == BWAPI::UnitTypes::Zerg_Queens_Nest)
		{
			nestCount++;
		}
		if (BuildingManager::Instance().buildingsQueued()[i] == BWAPI::UnitTypes::Zerg_Ultralisk_Cavern)
		{
			cavernCount++;
		}
		if (BuildingManager::Instance().buildingsQueued()[i] == BWAPI::UnitTypes::Zerg_Hatchery)
		{
			baseCount++;
			hatchesInProgress++;
		}
	}
	spireCount += greaterSpireCount;

	int desiredExtractors = int(std::floor(0.042 * workerSupply));
	desiredExtractors = std::min(desiredExtractors, WorkerManager::Instance().workerData.getNumDepots(true));
	if (armySupply == 0)
	{
		desiredExtractors = 0;
	}
	//BWAPI::Broodwar->drawTextScreen(4, 250, "desiredExtractors: %d\narmySupply: %d\nworkerSupply: %d", desiredExtractors, armySupply, workerSupply);
	//BWAPI::Broodwar->printf("Minerals: %d Gas: %d Larvae: %d", availableMinerals, availableGas, availableLarvae);
	// detect if there's a build order deadlock once per second
	if (detectBuildOrderDeadlock())
	{
		if (BWAPI::Broodwar->self()->minerals() >= BWAPI::Broodwar->self()->getRace().getSupplyProvider().mineralPrice() && availableLarvae > 0 && queuedSupplyProviders < BWAPI::Broodwar->self()->supplyUsed() * 0.25)
		{
			_queue.queueAsHighestPriority(MetaType(BWAPI::Broodwar->self()->getRace().getSupplyProvider()), false);
			availableMinerals -= BWAPI::Broodwar->self()->getRace().getSupplyProvider().mineralPrice();
		}
	}
	if (UnitUtil::GetAllUnitCount(BWAPI::UnitTypes::Zerg_Creep_Colony, true)
		&& availableMinerals >= BWAPI::UnitTypes::Zerg_Sunken_Colony.mineralPrice()
		&& UnitUtil::GetAllUnitCount(BWAPI::UnitTypes::Zerg_Spawning_Pool, true))
	{
		_queue.queueAsHighestPriority(BWAPI::UnitTypes::Zerg_Sunken_Colony, false);
		availableMinerals -= BWAPI::UnitTypes::Zerg_Sunken_Colony.mineralPrice();
	}
	int desiredSunkens = int(workerSupply * sunkensPerWorkerSupply);
	if (creepColonyCount + sunkenColonyCount + sporeColonyCount < desiredSunkens
		&& poolCount > 0
		&& sunkenUnlocked
		&& needSunkens)
	{
		if (availableMinerals >= BWAPI::UnitTypes::Zerg_Creep_Colony.mineralPrice())
		{
			_queue.queueAsHighestPriority(BWAPI::UnitTypes::Zerg_Creep_Colony, false);
			availableMinerals -= BWAPI::UnitTypes::Zerg_Creep_Colony.mineralPrice();
		}
		else
		{
			mineralsToSave = BWAPI::UnitTypes::Zerg_Creep_Colony.mineralPrice();
		}
	}
	if (shouldExpand && availableMinerals + BuildingManager::Instance().getReservedMinerals() < BWAPI::Broodwar->self()->getRace().getCenter().mineralPrice())
	{
		return;
	}
	double costOfPreferredUnit = (8 * 50 + 100) / 9.0;
	if (hydraScore > lingScore && hydraDenCount > 0)
	{
		costOfPreferredUnit = 100;
	}
	if ((availableMinerals >= BWAPI::Broodwar->self()->getRace().getCenter().mineralPrice() && shouldExpand)
		|| (availableLarvae < 1 && availableMinerals >= BWAPI::Broodwar->self()->getRace().getCenter().mineralPrice() && StrategyManager::Instance().needMacroHatch(baseCount, costOfPreferredUnit)))
	{
		_queue.queueAsHighestPriority(MetaType(BWAPI::Broodwar->self()->getRace().getCenter()), false);
		availableMinerals -= BWAPI::Broodwar->self()->getRace().getCenter().mineralPrice();
	}
	if (poolCount == 0 && workerSupply > 6 && (workerSupply >= workerSupplyBeforeUnits - 4 || baseCount >= 2))
	{
		_queue.queueAsHighestPriority(BWAPI::UnitTypes::Zerg_Spawning_Pool, false);
		availableMinerals -= BWAPI::UnitTypes::Zerg_Spawning_Pool.mineralPrice();
	}
	if (extractorCount < desiredExtractors && availableMinerals >= BWAPI::UnitTypes::Zerg_Extractor.mineralPrice())
	{
		_queue.queueAsHighestPriority(BWAPI::UnitTypes::Zerg_Extractor, false);
		availableMinerals -= BWAPI::UnitTypes::Zerg_Extractor.mineralPrice();
	}
	if (InformationManager::Instance().enemyHasCloakedUnits() 
		&& UnitUtil::GetAllUnitCount(BWAPI::UnitTypes::Zerg_Lair, true) + UnitUtil::GetAllUnitCount(BWAPI::UnitTypes::Zerg_Hive, true) >= 1 && pneumaLevel == 0)
	{
		if (availableMinerals >= BWAPI::UpgradeTypes::Pneumatized_Carapace.mineralPrice()
			&& availableGas >= BWAPI::UpgradeTypes::Pneumatized_Carapace.gasPrice())
		{
			_queue.queueAsHighestPriority(BWAPI::UpgradeTypes::Pneumatized_Carapace, false);
			availableMinerals -= BWAPI::UpgradeTypes::Pneumatized_Carapace.mineralPrice();
			availableGas -= BWAPI::UpgradeTypes::Pneumatized_Carapace.gasPrice();
		}
		else
		{
			mineralsToSave = BWAPI::UpgradeTypes::Pneumatized_Carapace.mineralPrice();
			gasToSave = BWAPI::UpgradeTypes::Pneumatized_Carapace.gasPrice();
		}
	}
	if (lingSupply > 0)
	{
		if (availableGas >= BWAPI::UpgradeTypes::Metabolic_Boost.gasPrice() && UnitUtil::GetAllUnitCount(BWAPI::UnitTypes::Zerg_Spawning_Pool, true) >= 1 && metabolicBoostLevel == 0)
		{
			_queue.queueAsHighestPriority(BWAPI::UpgradeTypes::Metabolic_Boost, false);
			availableMinerals -= BWAPI::UpgradeTypes::Metabolic_Boost.mineralPrice();
			availableGas -= BWAPI::UpgradeTypes::Metabolic_Boost.gasPrice();
		}
		if (UnitUtil::GetAllUnitCount(BWAPI::UnitTypes::Zerg_Spawning_Pool, true) >= 1
			&& UnitUtil::GetAllUnitCount(BWAPI::UnitTypes::Zerg_Hive, true) > 0 && adrenalGlandsLevel == 0 && metabolicBoostLevel > 0
			&& lingSupply > (BWAPI::UpgradeTypes::Adrenal_Glands.gasPrice() + BWAPI::UpgradeTypes::Adrenal_Glands.mineralPrice()) / BWAPI::UnitTypes::Zerg_Zergling.mineralPrice())
		{
			if (availableGas >= BWAPI::UpgradeTypes::Adrenal_Glands.gasPrice()
				&& availableMinerals >= BWAPI::UpgradeTypes::Adrenal_Glands.mineralPrice())
			{
				_queue.queueAsHighestPriority(BWAPI::UpgradeTypes::Adrenal_Glands, false);
				availableMinerals -= BWAPI::UpgradeTypes::Adrenal_Glands.mineralPrice();
				availableGas -= BWAPI::UpgradeTypes::Adrenal_Glands.gasPrice();
			}
			else
			{
				mineralsToSave = BWAPI::UpgradeTypes::Adrenal_Glands.mineralPrice();
				gasToSave = BWAPI::UpgradeTypes::Adrenal_Glands.gasPrice();
			}
		}
		if (lingSupply >= 64) //If we have that many lings we make sure to get the requirements for adrenal glands
		{
			if (availableMinerals >= BWAPI::UnitTypes::Zerg_Hive.mineralPrice()
				&& availableGas >= BWAPI::UnitTypes::Zerg_Hive.gasPrice()
				&& UnitUtil::GetAllUnitCount(BWAPI::UnitTypes::Zerg_Queens_Nest, true) >= 1)
			{
				_queue.queueAsHighestPriority(BWAPI::UnitTypes::Zerg_Hive, false);
				availableMinerals -= BWAPI::UnitTypes::Zerg_Hive.mineralPrice();
				availableGas -= BWAPI::UnitTypes::Zerg_Hive.gasPrice();
			}
			else
			{
				mineralsToSave = BWAPI::UnitTypes::Zerg_Hive.mineralPrice();
				gasToSave = BWAPI::UnitTypes::Zerg_Hive.gasPrice();
			}
			if (availableMinerals >= BWAPI::UnitTypes::Zerg_Queens_Nest.mineralPrice()
				&& availableGas >= BWAPI::UnitTypes::Zerg_Queens_Nest.gasPrice()
				&& UnitUtil::GetAllUnitCount(BWAPI::UnitTypes::Zerg_Lair, true) >= 1
				&& nestCount == 0)
			{
				_queue.queueAsHighestPriority(BWAPI::UnitTypes::Zerg_Queens_Nest, false);
				availableMinerals -= BWAPI::UnitTypes::Zerg_Queens_Nest.mineralPrice();
				availableGas -= BWAPI::UnitTypes::Zerg_Queens_Nest.gasPrice();
			}
			else
			{
				mineralsToSave = BWAPI::UnitTypes::Zerg_Queens_Nest.mineralPrice();
				gasToSave = BWAPI::UnitTypes::Zerg_Queens_Nest.gasPrice();
			}
			if (UnitUtil::GetAllUnitCount(BWAPI::UnitTypes::Zerg_Spawning_Pool, true) >= 1
				&& lairCount == 0 && hiveCount == 0)
			{
				if (availableMinerals >= BWAPI::UnitTypes::Zerg_Lair.mineralPrice()
					&& availableGas >= BWAPI::UnitTypes::Zerg_Lair.gasPrice())
				{
					_queue.queueAsHighestPriority(BWAPI::UnitTypes::Zerg_Lair, false);
					availableMinerals -= BWAPI::UnitTypes::Zerg_Lair.mineralPrice();
					availableGas -= BWAPI::UnitTypes::Zerg_Lair.gasPrice();
				}
				else
				{
					mineralsToSave = BWAPI::UnitTypes::Zerg_Lair.mineralPrice();
					gasToSave = BWAPI::UnitTypes::Zerg_Lair.gasPrice();
				}
			}
		}
	}
	if (hydraSupply > 0)
	{
		if (lurkerAspect == true || hydraScore > lurkerScore)
		{
			if (availableGas >= BWAPI::UpgradeTypes::Muscular_Augments.gasPrice() && UnitUtil::GetAllUnitCount(BWAPI::UnitTypes::Zerg_Hydralisk_Den, true) >= 1 && augmentLevel == 0)
			{
				_queue.queueAsHighestPriority(BWAPI::UpgradeTypes::Muscular_Augments, false);
				availableMinerals -= BWAPI::UpgradeTypes::Muscular_Augments.mineralPrice();
				availableGas -= BWAPI::UpgradeTypes::Muscular_Augments.gasPrice();
			}
			if (availableGas >= BWAPI::UpgradeTypes::Grooved_Spines.gasPrice() && UnitUtil::GetAllUnitCount(BWAPI::UnitTypes::Zerg_Hydralisk_Den, true) >= 1 && spineLevel == 0 && augmentLevel > 0)
			{
				_queue.queueAsHighestPriority(BWAPI::UpgradeTypes::Grooved_Spines, false);
				availableMinerals -= BWAPI::UpgradeTypes::Grooved_Spines.mineralPrice();
				availableGas -= BWAPI::UpgradeTypes::Grooved_Spines.gasPrice();
			}
		}
	}
	if (mutaSupply + guardSupply >= 32)
	{
		if (UnitUtil::GetAllUnitCount(BWAPI::UnitTypes::Zerg_Greater_Spire, true) == 0 && guardScore > mutaScore && guardScore > hydraScore && guardScore > lingScore && guardScore > ultraScore)
		{
			//we don't want to make an upgrade cause we will want to wait for our Greater Spire to get Guardians
		}
		else
		{
			if (UnitUtil::GetAllUnitCount(BWAPI::UnitTypes::Zerg_Spire, true) + UnitUtil::GetAllUnitCount(BWAPI::UnitTypes::Zerg_Greater_Spire, true) >= 1)
			{
				if (airAttackLevel <= airDefenseLevel)
				{
					if (airAttackLevel < 3)
					{
						if (availableGas >= BWAPI::UpgradeTypes::Zerg_Flyer_Attacks.gasPrice() && availableMinerals >= BWAPI::UpgradeTypes::Zerg_Flyer_Attacks.mineralPrice())
						{
							_queue.queueAsHighestPriority(BWAPI::UpgradeTypes::Zerg_Flyer_Attacks, false);
							availableMinerals -= BWAPI::UpgradeTypes::Zerg_Flyer_Attacks.mineralPrice();
							availableGas -= BWAPI::UpgradeTypes::Zerg_Flyer_Attacks.gasPrice();
						}
						else
						{
							mineralsToSave = BWAPI::UpgradeTypes::Zerg_Flyer_Attacks.mineralPrice();
							gasToSave = BWAPI::UpgradeTypes::Zerg_Flyer_Attacks.gasPrice();
						}
					}
				}
				else
				{
					if (airDefenseLevel < 3)
					{
						if (availableGas >= BWAPI::UpgradeTypes::Zerg_Flyer_Carapace.gasPrice() && availableMinerals >= BWAPI::UpgradeTypes::Zerg_Flyer_Carapace.mineralPrice())
						{
							_queue.queueAsHighestPriority(BWAPI::UpgradeTypes::Zerg_Flyer_Carapace, false);
							availableMinerals -= BWAPI::UpgradeTypes::Zerg_Flyer_Carapace.mineralPrice();
							availableGas -= BWAPI::UpgradeTypes::Zerg_Flyer_Carapace.gasPrice();
						}
						else
						{
							mineralsToSave = BWAPI::UpgradeTypes::Zerg_Flyer_Carapace.mineralPrice();
							gasToSave = BWAPI::UpgradeTypes::Zerg_Flyer_Carapace.gasPrice();
						}
					}
				}
			}
		}
	}
	if (lingSupply + hydraSupply + ultraSupply + lurkerSupply >= 32)
	{
		if(lairCount == 0 && hiveCount == 0
			&& (groundDefenseLevel > 0
			|| meleeAttackLevel > 0
			|| rangedAttackLevel > 0
			|| airDefenseLevel > 0
			|| airAttackLevel > 0))
		{
			if (availableMinerals >= BWAPI::UnitTypes::Zerg_Lair.mineralPrice()
				&& availableGas >= BWAPI::UnitTypes::Zerg_Lair.gasPrice()
				&& UnitUtil::GetAllUnitCount(BWAPI::UnitTypes::Zerg_Spawning_Pool, true) >= 1)
			{
				_queue.queueAsHighestPriority(BWAPI::UnitTypes::Zerg_Lair, false);
				availableMinerals -= BWAPI::UnitTypes::Zerg_Lair.mineralPrice();
				availableGas -= BWAPI::UnitTypes::Zerg_Lair.gasPrice();
			}
			else
			{
				mineralsToSave = BWAPI::UnitTypes::Zerg_Lair.mineralPrice();
				gasToSave = BWAPI::UnitTypes::Zerg_Lair.gasPrice();
			}
		}
		if (hiveCount == 0
			&& (groundDefenseLevel > 1
			|| meleeAttackLevel > 1
			|| rangedAttackLevel > 1
			|| airDefenseLevel > 1
			|| airAttackLevel > 1))
		{
			if (availableMinerals >= BWAPI::UnitTypes::Zerg_Hive.mineralPrice()
				&& availableGas >= BWAPI::UnitTypes::Zerg_Hive.gasPrice()
				&& UnitUtil::GetAllUnitCount(BWAPI::UnitTypes::Zerg_Queens_Nest, true) >= 1)
			{
				_queue.queueAsHighestPriority(BWAPI::UnitTypes::Zerg_Hive, false);
				availableMinerals -= BWAPI::UnitTypes::Zerg_Hive.mineralPrice();
				availableGas -= BWAPI::UnitTypes::Zerg_Hive.gasPrice();
			}
			else
			{
				mineralsToSave = BWAPI::UnitTypes::Zerg_Hive.mineralPrice();
				gasToSave = BWAPI::UnitTypes::Zerg_Hive.gasPrice();
			}
			if (availableMinerals >= BWAPI::UnitTypes::Zerg_Queens_Nest.mineralPrice()
				&& availableGas >= BWAPI::UnitTypes::Zerg_Queens_Nest.gasPrice()
				&& UnitUtil::GetAllUnitCount(BWAPI::UnitTypes::Zerg_Lair, true) >= 1
				&& nestCount == 0)
			{
				_queue.queueAsHighestPriority(BWAPI::UnitTypes::Zerg_Queens_Nest, false);
				availableMinerals -= BWAPI::UnitTypes::Zerg_Queens_Nest.mineralPrice();
				availableGas -= BWAPI::UnitTypes::Zerg_Queens_Nest.gasPrice();
			}
			else
			{
				mineralsToSave = BWAPI::UnitTypes::Zerg_Queens_Nest.mineralPrice();
				gasToSave = BWAPI::UnitTypes::Zerg_Queens_Nest.gasPrice();
			}
		}
		if (evoCount == 0)
		{
			if (availableMinerals > BWAPI::UnitTypes::Zerg_Evolution_Chamber.mineralPrice()
				&& availableGas > BWAPI::UnitTypes::Zerg_Evolution_Chamber.gasPrice())
			{
				_queue.queueAsHighestPriority(BWAPI::UnitTypes::Zerg_Evolution_Chamber, false);
				availableMinerals -= BWAPI::UnitTypes::Zerg_Evolution_Chamber.mineralPrice();
				availableGas -= BWAPI::UnitTypes::Zerg_Evolution_Chamber.gasPrice();
			}
			else
			{
				mineralsToSave = BWAPI::UnitTypes::Zerg_Evolution_Chamber.mineralPrice();
				gasToSave = BWAPI::UnitTypes::Zerg_Evolution_Chamber.gasPrice();
			}
		}
		else
		{
			if (UnitUtil::GetAllUnitCount(BWAPI::UnitTypes::Zerg_Evolution_Chamber, true) > 0)
			{
				if (groundDefenseLevel <= std::max(meleeAttackLevel, rangedAttackLevel))
				{
					if (groundDefenseLevel < 3)
					{
						if (availableGas >= BWAPI::UpgradeTypes::Zerg_Carapace.gasPrice() && availableMinerals >= BWAPI::UpgradeTypes::Zerg_Carapace.mineralPrice())
						{
							_queue.queueAsHighestPriority(BWAPI::UpgradeTypes::Zerg_Carapace, false);
							availableMinerals -= BWAPI::UpgradeTypes::Zerg_Carapace.mineralPrice();
							availableGas -= BWAPI::UpgradeTypes::Zerg_Carapace.gasPrice();
						}
						else
						{
							mineralsToSave = BWAPI::UpgradeTypes::Zerg_Carapace.mineralPrice();
							gasToSave = BWAPI::UpgradeTypes::Zerg_Carapace.gasPrice();
						}
					}
				}
				else
				{
					if ((lingSupply + ultraSupply > hydraSupply + lurkerSupply && meleeAttackLevel < 3) || rangedAttackLevel == 3)
					{
						if (meleeAttackLevel < 3)
						{
							if (availableGas >= BWAPI::UpgradeTypes::Zerg_Melee_Attacks.gasPrice() && availableMinerals >= BWAPI::UpgradeTypes::Zerg_Melee_Attacks.mineralPrice())
							{
								_queue.queueAsHighestPriority(BWAPI::UpgradeTypes::Zerg_Melee_Attacks, false);
								availableMinerals -= BWAPI::UpgradeTypes::Zerg_Melee_Attacks.mineralPrice();
								availableGas -= BWAPI::UpgradeTypes::Zerg_Melee_Attacks.gasPrice();
							}
							else
							{
								mineralsToSave = BWAPI::UpgradeTypes::Zerg_Melee_Attacks.mineralPrice();
								gasToSave = BWAPI::UpgradeTypes::Zerg_Melee_Attacks.gasPrice();
							}
						}
					}
					else
					{
						if (rangedAttackLevel < 3)
						{
							if (availableGas >= BWAPI::UpgradeTypes::Zerg_Missile_Attacks.gasPrice() && availableMinerals >= BWAPI::UpgradeTypes::Zerg_Missile_Attacks.mineralPrice())
							{
								_queue.queueAsHighestPriority(BWAPI::UpgradeTypes::Zerg_Missile_Attacks, false);
								availableMinerals -= BWAPI::UpgradeTypes::Zerg_Missile_Attacks.mineralPrice();
								availableGas -= BWAPI::UpgradeTypes::Zerg_Missile_Attacks.gasPrice();
							}
							else
							{
								mineralsToSave = BWAPI::UpgradeTypes::Zerg_Missile_Attacks.mineralPrice();
								gasToSave = BWAPI::UpgradeTypes::Zerg_Missile_Attacks.gasPrice();
							}
						}
					}
				}
			}
		}
	}
	if (availableMinerals >= BWAPI::Broodwar->self()->getRace().getWorker().mineralPrice() 
		&& availableLarvae > 0 
		&& armySupply * workerFactor + workerSupplyBeforeUnits > workerSupply 
		&& workerSupply < maxWorkerSupply)
	{
		_queue.queueAsLowestPriority(MetaType(BWAPI::Broodwar->self()->getRace().getWorker()), false);
		availableMinerals -= BWAPI::Broodwar->self()->getRace().getWorker().mineralPrice();
	}
	else
	{
		if (lingScore >= hydraScore && lingScore >= mutaScore && lingScore >= ultraScore && lingScore >= guardScore && lingScore >= lurkerScore)
		{
			mineralsToSave = BWAPI::UnitTypes::Zerg_Zergling.mineralPrice();
			while (availableMinerals >= BWAPI::UnitTypes::Zerg_Zergling.mineralPrice() && availableLarvae > 0 && UnitUtil::GetAllUnitCount(BWAPI::UnitTypes::Zerg_Spawning_Pool, true) > 0)
			{
				_queue.queueAsLowestPriority(BWAPI::UnitTypes::Zerg_Zergling, false);
				availableMinerals -= BWAPI::UnitTypes::Zerg_Zergling.mineralPrice();
				availableLarvae--;
			}
		}
		if (hydraScore >= lingScore) //Building hydra-den as soon as hydrascore > lingscore even if hydra is not favorite
		{
			if (availableMinerals >= BWAPI::UnitTypes::Zerg_Hydralisk_Den.mineralPrice()
				&& availableGas >= BWAPI::UnitTypes::Zerg_Hydralisk_Den.gasPrice()
				&& UnitUtil::GetAllUnitCount(BWAPI::UnitTypes::Zerg_Spawning_Pool, true) >= 1
				&& hydraDenCount == 0)
			{
				_queue.queueAsHighestPriority(BWAPI::UnitTypes::Zerg_Hydralisk_Den, false);
				availableMinerals -= BWAPI::UnitTypes::Zerg_Hydralisk_Den.mineralPrice();
				availableGas -= BWAPI::UnitTypes::Zerg_Hydralisk_Den.gasPrice();
			}
			else
			{
				mineralsToSave = BWAPI::UnitTypes::Zerg_Hydralisk_Den.mineralPrice();
				gasToSave = BWAPI::UnitTypes::Zerg_Hydralisk_Den.gasPrice();
			}
		}
		if (hydraScore >= lingScore && hydraScore >= mutaScore && hydraScore >= ultraScore && hydraScore >= guardScore && hydraScore >= lurkerScore)
		{
			if (UnitUtil::GetAllUnitCount(BWAPI::UnitTypes::Zerg_Hydralisk_Den, true) > 0)
			{
				mineralsToSave = BWAPI::UnitTypes::Zerg_Hydralisk.mineralPrice();
				gasToSave = BWAPI::UnitTypes::Zerg_Hydralisk.gasPrice();
				while (availableMinerals >= BWAPI::UnitTypes::Zerg_Hydralisk.mineralPrice()
					&& availableGas >= BWAPI::UnitTypes::Zerg_Hydralisk.gasPrice()
					&& availableLarvae > 0 && UnitUtil::GetAllUnitCount(BWAPI::UnitTypes::Zerg_Hydralisk_Den, true) > 0)
				{
					_queue.queueAsLowestPriority(BWAPI::UnitTypes::Zerg_Hydralisk, false);
					availableMinerals -= BWAPI::UnitTypes::Zerg_Hydralisk.mineralPrice();
					availableGas -= BWAPI::UnitTypes::Zerg_Hydralisk.mineralPrice();
					availableLarvae--;
				}
			}
		}
		if (mutaScore >= lingScore && mutaScore >= hydraScore && mutaScore >= ultraScore && mutaScore >= guardScore && mutaScore >= lurkerScore)
		{
			mineralsToSave = BWAPI::UnitTypes::Zerg_Mutalisk.mineralPrice();
			gasToSave = BWAPI::UnitTypes::Zerg_Mutalisk.gasPrice();
			if (spireCount == 0 && greaterSpireCount == 0)
			{
				mineralsToSave = BWAPI::UnitTypes::Zerg_Spire.mineralPrice();
				gasToSave = BWAPI::UnitTypes::Zerg_Spire.gasPrice();
			}
			if (UnitUtil::GetAllUnitCount(BWAPI::UnitTypes::Zerg_Spire) > 0 
				&& UnitUtil::GetAllUnitCount(BWAPI::UnitTypes::Zerg_Spire, true) == 0
				&& !underPressure) //when we are waiting for our spire to finish, we'll want to save up for the mutas
			{
				gasToSave = availableLarvae * 100;
				mineralsToSave = std::min(gasToSave, availableGas);
				larvaeToSave = mineralsToSave / 100;
			}
			if (availableMinerals >= BWAPI::UnitTypes::Zerg_Spire.mineralPrice()
				&& availableGas >= BWAPI::UnitTypes::Zerg_Spire.gasPrice()
				&& UnitUtil::GetAllUnitCount(BWAPI::UnitTypes::Zerg_Lair, true) >= 1
				&& spireCount == 0)
			{
				_queue.queueAsHighestPriority(BWAPI::UnitTypes::Zerg_Spire, false);
				availableMinerals -= BWAPI::UnitTypes::Zerg_Spire.mineralPrice();
				availableGas -= BWAPI::UnitTypes::Zerg_Spire.gasPrice();
			}
			if (availableMinerals >= BWAPI::UnitTypes::Zerg_Lair.mineralPrice()
				&& availableGas >= BWAPI::UnitTypes::Zerg_Lair.gasPrice()
				&& UnitUtil::GetAllUnitCount(BWAPI::UnitTypes::Zerg_Spawning_Pool, true) >= 1
				&& lairCount == 0 && hiveCount == 0)
			{
				_queue.queueAsHighestPriority(BWAPI::UnitTypes::Zerg_Lair, false);
				availableMinerals -= BWAPI::UnitTypes::Zerg_Lair.mineralPrice();
				availableGas -= BWAPI::UnitTypes::Zerg_Lair.gasPrice();
			}
			while (availableMinerals >= BWAPI::UnitTypes::Zerg_Mutalisk.mineralPrice()
				&& availableGas >= BWAPI::UnitTypes::Zerg_Mutalisk.gasPrice()
				&& availableLarvae > 0 && (UnitUtil::GetAllUnitCount(BWAPI::UnitTypes::Zerg_Spire, true) > 0 || UnitUtil::GetAllUnitCount(BWAPI::UnitTypes::Zerg_Greater_Spire) > 0))
			{
				_queue.queueAsLowestPriority(BWAPI::UnitTypes::Zerg_Mutalisk, false);
				availableMinerals -= BWAPI::UnitTypes::Zerg_Mutalisk.mineralPrice();
				availableGas -= BWAPI::UnitTypes::Zerg_Mutalisk.mineralPrice();
				availableLarvae--;
			}
		}
		if (ultraScore >= lingScore && ultraScore >= hydraScore && ultraScore >= mutaScore && ultraScore >= guardScore && ultraScore >= lurkerScore)
		{
			mineralsToSave = BWAPI::UnitTypes::Zerg_Ultralisk.mineralPrice();
			gasToSave = BWAPI::UnitTypes::Zerg_Ultralisk.gasPrice();
			if (hiveCount == 0)
			{
				mineralsToSave = BWAPI::UnitTypes::Zerg_Hive.mineralPrice();
				gasToSave = BWAPI::UnitTypes::Zerg_Hive.gasPrice();
			}
			if (cavernCount == 0)
			{
				mineralsToSave = BWAPI::UnitTypes::Zerg_Ultralisk_Cavern.mineralPrice();
				gasToSave = BWAPI::UnitTypes::Zerg_Ultralisk_Cavern.gasPrice();
			}
			if (availableGas >= BWAPI::UpgradeTypes::Chitinous_Plating.gasPrice() && UnitUtil::GetAllUnitCount(BWAPI::UnitTypes::Zerg_Ultralisk_Cavern, true) >= 1 && chitinLevel == 0)
			{
				_queue.queueAsHighestPriority(BWAPI::UpgradeTypes::Chitinous_Plating, false);
				availableMinerals -= BWAPI::UpgradeTypes::Chitinous_Plating.mineralPrice();
				availableGas -= BWAPI::UpgradeTypes::Chitinous_Plating.gasPrice();
			}
			if (availableGas >= BWAPI::UpgradeTypes::Anabolic_Synthesis.gasPrice() && UnitUtil::GetAllUnitCount(BWAPI::UnitTypes::Zerg_Ultralisk_Cavern, true) >= 1 && chitinLevel > 0 && anabolicLevel == 0)
			{
				_queue.queueAsHighestPriority(BWAPI::UpgradeTypes::Anabolic_Synthesis, false);
				availableMinerals -= BWAPI::UpgradeTypes::Anabolic_Synthesis.mineralPrice();
				availableGas -= BWAPI::UpgradeTypes::Anabolic_Synthesis.gasPrice();
			}
			if (availableMinerals >= BWAPI::UnitTypes::Zerg_Ultralisk_Cavern.mineralPrice()
				&& availableGas >= BWAPI::UnitTypes::Zerg_Ultralisk_Cavern.gasPrice()
				&& UnitUtil::GetAllUnitCount(BWAPI::UnitTypes::Zerg_Hive, true) >= 1
				&& cavernCount == 0)
			{
				_queue.queueAsHighestPriority(BWAPI::UnitTypes::Zerg_Ultralisk_Cavern, false);
				availableMinerals -= BWAPI::UnitTypes::Zerg_Ultralisk_Cavern.mineralPrice();
				availableGas -= BWAPI::UnitTypes::Zerg_Ultralisk_Cavern.gasPrice();
			}
			if (availableMinerals >= BWAPI::UnitTypes::Zerg_Hive.mineralPrice()
				&& availableGas >= BWAPI::UnitTypes::Zerg_Hive.gasPrice()
				&& UnitUtil::GetAllUnitCount(BWAPI::UnitTypes::Zerg_Queens_Nest, true) >= 1
				&& hiveCount == 0)
			{
				_queue.queueAsHighestPriority(BWAPI::UnitTypes::Zerg_Hive, false);
				availableMinerals -= BWAPI::UnitTypes::Zerg_Hive.mineralPrice();
				availableGas -= BWAPI::UnitTypes::Zerg_Hive.gasPrice();
			}
			if (availableMinerals >= BWAPI::UnitTypes::Zerg_Queens_Nest.mineralPrice()
				&& availableGas >= BWAPI::UnitTypes::Zerg_Queens_Nest.gasPrice()
				&& UnitUtil::GetAllUnitCount(BWAPI::UnitTypes::Zerg_Lair, true) >= 1
				&& nestCount == 0)
			{
				_queue.queueAsHighestPriority(BWAPI::UnitTypes::Zerg_Queens_Nest, false);
				availableMinerals -= BWAPI::UnitTypes::Zerg_Queens_Nest.mineralPrice();
				availableGas -= BWAPI::UnitTypes::Zerg_Queens_Nest.gasPrice();
			}
			if (availableMinerals >= BWAPI::UnitTypes::Zerg_Lair.mineralPrice()
				&& availableGas >= BWAPI::UnitTypes::Zerg_Lair.gasPrice()
				&& UnitUtil::GetAllUnitCount(BWAPI::UnitTypes::Zerg_Spawning_Pool, true) >= 1
				&& lairCount == 0 && hiveCount == 0)
			{
				_queue.queueAsHighestPriority(BWAPI::UnitTypes::Zerg_Lair, false);
				availableMinerals -= BWAPI::UnitTypes::Zerg_Lair.mineralPrice();
				availableGas -= BWAPI::UnitTypes::Zerg_Lair.gasPrice();
			}
			while (availableMinerals >= BWAPI::UnitTypes::Zerg_Ultralisk.mineralPrice()
				&& availableGas >= BWAPI::UnitTypes::Zerg_Ultralisk.gasPrice()
				&& availableLarvae > 0 && UnitUtil::GetAllUnitCount(BWAPI::UnitTypes::Zerg_Ultralisk_Cavern, true) > 0)
			{
				_queue.queueAsLowestPriority(BWAPI::UnitTypes::Zerg_Ultralisk, false);
				availableMinerals -= BWAPI::UnitTypes::Zerg_Ultralisk.mineralPrice();
				availableGas -= BWAPI::UnitTypes::Zerg_Ultralisk.mineralPrice();
				availableLarvae--;
			}
		}
		if (guardScore >= lingScore && guardScore >= hydraScore && guardScore >= mutaScore && guardScore >= ultraScore && guardScore >= lurkerScore)
		{
			mineralsToSave = BWAPI::UnitTypes::Zerg_Guardian.mineralPrice() + BWAPI::UnitTypes::Zerg_Mutalisk.mineralPrice();
			gasToSave = BWAPI::UnitTypes::Zerg_Guardian.gasPrice() + BWAPI::UnitTypes::Zerg_Mutalisk.gasPrice();
			if (hiveCount == 0)
			{
				mineralsToSave = BWAPI::UnitTypes::Zerg_Hive.mineralPrice();
				gasToSave = BWAPI::UnitTypes::Zerg_Hive.gasPrice();
			}
			if (greaterSpireCount == 0)
			{
				mineralsToSave = BWAPI::UnitTypes::Zerg_Greater_Spire.mineralPrice();
				gasToSave = BWAPI::UnitTypes::Zerg_Greater_Spire.gasPrice();
			}
			if (availableMinerals >= BWAPI::UnitTypes::Zerg_Greater_Spire.mineralPrice()
				&& availableGas >= BWAPI::UnitTypes::Zerg_Greater_Spire.gasPrice()
				&& UnitUtil::GetAllUnitCount(BWAPI::UnitTypes::Zerg_Hive, true) >= 1
				&& spireCount >= 1
				&& greaterSpireCount == 0)
			{
				_queue.queueAsHighestPriority(BWAPI::UnitTypes::Zerg_Greater_Spire, false);
				availableMinerals -= BWAPI::UnitTypes::Zerg_Greater_Spire.mineralPrice();
				availableGas -= BWAPI::UnitTypes::Zerg_Greater_Spire.gasPrice();
			}
			if (availableMinerals >= BWAPI::UnitTypes::Zerg_Hive.mineralPrice()
				&& availableGas >= BWAPI::UnitTypes::Zerg_Hive.gasPrice()
				&& UnitUtil::GetAllUnitCount(BWAPI::UnitTypes::Zerg_Queens_Nest, true) >= 1
				&& hiveCount == 0)
			{
				_queue.queueAsHighestPriority(BWAPI::UnitTypes::Zerg_Hive, false);
				availableMinerals -= BWAPI::UnitTypes::Zerg_Hive.mineralPrice();
				availableGas -= BWAPI::UnitTypes::Zerg_Hive.gasPrice();
			}
			if (availableMinerals >= BWAPI::UnitTypes::Zerg_Queens_Nest.mineralPrice()
				&& availableGas >= BWAPI::UnitTypes::Zerg_Queens_Nest.gasPrice()
				&& UnitUtil::GetAllUnitCount(BWAPI::UnitTypes::Zerg_Lair, true) >= 1
				&& nestCount == 0)
			{
				_queue.queueAsHighestPriority(BWAPI::UnitTypes::Zerg_Queens_Nest, false);
				availableMinerals -= BWAPI::UnitTypes::Zerg_Queens_Nest.mineralPrice();
				availableGas -= BWAPI::UnitTypes::Zerg_Queens_Nest.gasPrice();
			}
			while (availableMinerals >= BWAPI::UnitTypes::Zerg_Guardian.mineralPrice()
				&& availableGas >= BWAPI::UnitTypes::Zerg_Guardian.gasPrice()
				&& UnitUtil::GetAllUnitCount(BWAPI::UnitTypes::Zerg_Greater_Spire, true) > 0
				&& mutaSupply > 0)
			{
				_queue.queueAsLowestPriority(BWAPI::UnitTypes::Zerg_Guardian, false);
				availableMinerals -= BWAPI::UnitTypes::Zerg_Guardian.mineralPrice();
				availableGas -= BWAPI::UnitTypes::Zerg_Guardian.mineralPrice();
				mutaSupply -= BWAPI::UnitTypes::Zerg_Mutalisk.supplyRequired();
			}
		}
		if (lurkerScore >= lingScore && lurkerScore >= hydraScore && lurkerScore >= mutaScore && lurkerScore >= ultraScore && lurkerScore >= guardScore)
		{
			if (!lurkerAspect && !BWAPI::Broodwar->self()->isResearching(BWAPI::TechTypes::Lurker_Aspect)
				&& UnitUtil::GetAllUnitCount(BWAPI::UnitTypes::Zerg_Hydralisk_Den, true) >= 1
				&& UnitUtil::GetAllUnitCount(BWAPI::UnitTypes::Zerg_Lair, true) + UnitUtil::GetAllUnitCount(BWAPI::UnitTypes::Zerg_Hive) >= 1)
			{
				if (availableGas >= BWAPI::TechTypes::Lurker_Aspect.gasPrice()
					&& availableMinerals >= BWAPI::TechTypes::Lurker_Aspect.mineralPrice())
				{
					_queue.queueAsHighestPriority(BWAPI::TechTypes::Lurker_Aspect, false);
					availableMinerals -= BWAPI::TechTypes::Lurker_Aspect.mineralPrice();
					availableGas -= BWAPI::TechTypes::Lurker_Aspect.gasPrice();
				}
				else
				{
					mineralsToSave = BWAPI::TechTypes::Lurker_Aspect.mineralPrice();
					gasToSave = BWAPI::TechTypes::Lurker_Aspect.gasPrice();
				}
			}
			if (UnitUtil::GetAllUnitCount(BWAPI::UnitTypes::Zerg_Spawning_Pool, true) >= 1
				&& lairCount == 0 && hiveCount == 0)
			{
				if (availableMinerals >= BWAPI::UnitTypes::Zerg_Lair.mineralPrice()
					&& availableGas >= BWAPI::UnitTypes::Zerg_Lair.gasPrice())
				{
					_queue.queueAsHighestPriority(BWAPI::UnitTypes::Zerg_Lair, false);
					availableMinerals -= BWAPI::UnitTypes::Zerg_Lair.mineralPrice();
					availableGas -= BWAPI::UnitTypes::Zerg_Lair.gasPrice();
				}
				else
				{
					mineralsToSave = BWAPI::UnitTypes::Zerg_Lair.mineralPrice();
					gasToSave = BWAPI::UnitTypes::Zerg_Lair.gasPrice();
				}
			}
			if (UnitUtil::GetAllUnitCount(BWAPI::UnitTypes::Zerg_Spawning_Pool, true) >= 1
				&& hydraDenCount == 0)
			{
				if (availableMinerals >= BWAPI::UnitTypes::Zerg_Hydralisk_Den.mineralPrice()
					&& availableGas >= BWAPI::UnitTypes::Zerg_Hydralisk_Den.gasPrice())
				{
					_queue.queueAsHighestPriority(BWAPI::UnitTypes::Zerg_Hydralisk_Den, false);
					availableMinerals -= BWAPI::UnitTypes::Zerg_Hydralisk_Den.mineralPrice();
					availableGas -= BWAPI::UnitTypes::Zerg_Hydralisk_Den.gasPrice();
				}
				else
				{
					mineralsToSave = BWAPI::UnitTypes::Zerg_Hydralisk_Den.mineralPrice();
					gasToSave = BWAPI::UnitTypes::Zerg_Hydralisk_Den.gasPrice();
				}
			}
			while (availableMinerals >= BWAPI::UnitTypes::Zerg_Lurker.mineralPrice()
				&& availableGas >= BWAPI::UnitTypes::Zerg_Lurker.gasPrice()
				&& UnitUtil::GetAllUnitCount(BWAPI::UnitTypes::Zerg_Hydralisk_Den, true) > 0
				&& lurkerAspect
				&& hydraSupply > 0)
			{
				_queue.queueAsHighestPriority(BWAPI::UnitTypes::Zerg_Lurker, false);
				availableMinerals -= BWAPI::UnitTypes::Zerg_Lurker.mineralPrice();
				availableGas -= BWAPI::UnitTypes::Zerg_Lurker.mineralPrice();
				hydraSupply -= BWAPI::UnitTypes::Zerg_Hydralisk.supplyRequired();
			}
			mineralsToSave = BWAPI::UnitTypes::Zerg_Hydralisk.mineralPrice() + BWAPI::UnitTypes::Zerg_Lurker.mineralPrice();
			gasToSave = BWAPI::UnitTypes::Zerg_Hydralisk.gasPrice() + BWAPI::UnitTypes::Zerg_Lurker.gasPrice();
		}
	}
	if (extractorCount == 0 || underPressure || workerSupply < 18)
	{
		int mineralsToSaveBefore = mineralsToSave;
		int gasToSaveBefore = gasToSave;
		mineralsToSave = 0;
		gasToSave = 0;
		bool haveNonLingTech = false;
		if (UnitUtil::GetAllUnitCount(BWAPI::UnitTypes::Zerg_Hydralisk_Den, true) > 0)
		{
			mineralsToSave = BWAPI::UnitTypes::Zerg_Hydralisk.mineralPrice();
			gasToSave = BWAPI::UnitTypes::Zerg_Hydralisk.gasPrice();
			haveNonLingTech = true;
		}
		if (UnitUtil::GetAllUnitCount(BWAPI::UnitTypes::Zerg_Spire, true) > 0)
		{
			mineralsToSave = BWAPI::UnitTypes::Zerg_Mutalisk.mineralPrice();
			gasToSave = BWAPI::UnitTypes::Zerg_Mutalisk.gasPrice();
			haveNonLingTech = true;
		}
		if (UnitUtil::GetAllUnitCount(BWAPI::UnitTypes::Zerg_Ultralisk_Cavern, true) > 0)
		{
			mineralsToSave = BWAPI::UnitTypes::Zerg_Ultralisk.mineralPrice();
			gasToSave = BWAPI::UnitTypes::Zerg_Ultralisk.gasPrice();
			haveNonLingTech = true;
		}
		if (!lingsAllowed && !haveNonLingTech)
		{
			mineralsToSave = mineralsToSaveBefore + 50;
			gasToSave = gasToSaveBefore;
		}
	}
	else
	{
		mineralsToSave += BWAPI::Broodwar->self()->getRace().getWorker().mineralPrice();
		if (UnitUtil::GetAllUnitCount(BWAPI::UnitTypes::Zerg_Hydralisk_Den, true) > 0)
		{
			mineralsToSave += BWAPI::UnitTypes::Zerg_Hydralisk.mineralPrice();
			gasToSave += BWAPI::UnitTypes::Zerg_Hydralisk.gasPrice();
		}
		else if (spireCount + greaterSpireCount > 0)
		{
			mineralsToSave += BWAPI::UnitTypes::Zerg_Mutalisk.mineralPrice();
			gasToSave += BWAPI::UnitTypes::Zerg_Mutalisk.gasPrice();
		}
	}
	while (availableMinerals >= mineralsToSave + BWAPI::Broodwar->self()->getRace().getWorker().mineralPrice() 
			&& availableLarvae > larvaeToSave 
			&& armySupply * workerFactor + workerSupplyBeforeUnits > workerSupply 
			&& workerSupply < maxWorkerSupply)
	{
		_queue.queueAsLowestPriority(MetaType(BWAPI::Broodwar->self()->getRace().getWorker()), false);
		availableMinerals -= BWAPI::Broodwar->self()->getRace().getWorker().mineralPrice();
		availableLarvae--;
	}
	while (availableMinerals >= mineralsToSave + BWAPI::UnitTypes::Zerg_Hydralisk.mineralPrice()
			&& availableGas >= gasToSave + BWAPI::UnitTypes::Zerg_Hydralisk.gasPrice()
			&& availableLarvae > larvaeToSave && UnitUtil::GetAllUnitCount(BWAPI::UnitTypes::Zerg_Hydralisk_Den, true) > 0
			&& hydraScore > lingScore)
	{
		_queue.queueAsLowestPriority(BWAPI::UnitTypes::Zerg_Hydralisk, false);
		availableMinerals -= BWAPI::UnitTypes::Zerg_Hydralisk.mineralPrice();
		availableGas -= BWAPI::UnitTypes::Zerg_Hydralisk.mineralPrice();
		availableLarvae--;
	}
	while (availableMinerals >= mineralsToSave + BWAPI::UnitTypes::Zerg_Mutalisk.mineralPrice()
			&& availableGas >= gasToSave + BWAPI::UnitTypes::Zerg_Mutalisk.gasPrice()
			&& availableLarvae > larvaeToSave && UnitUtil::GetAllUnitCount(BWAPI::UnitTypes::Zerg_Spire, true) + UnitUtil::GetAllUnitCount(BWAPI::UnitTypes::Zerg_Greater_Spire, true)> 0
			&& mutaScore > lingScore)
	{
		_queue.queueAsLowestPriority(BWAPI::UnitTypes::Zerg_Mutalisk, false);
		availableMinerals -= BWAPI::UnitTypes::Zerg_Mutalisk.mineralPrice();
		availableGas -= BWAPI::UnitTypes::Zerg_Mutalisk.mineralPrice();
		availableLarvae--;
	}
	while (availableMinerals >= mineralsToSave + BWAPI::UnitTypes::Zerg_Zergling.mineralPrice()
		&& availableLarvae > larvaeToSave && UnitUtil::GetAllUnitCount(BWAPI::UnitTypes::Zerg_Spawning_Pool, true) > 0)
	{
		_queue.queueAsLowestPriority(BWAPI::UnitTypes::Zerg_Zergling, false);
		availableMinerals -= BWAPI::UnitTypes::Zerg_Zergling.mineralPrice();
		availableLarvae--;
	}
}

// on unit destroy
void ProductionManager::onUnitDestroy(BWAPI::Unit unit)
{
	// we don't care if it's not our unit
	if (!unit || unit->getPlayer() != BWAPI::Broodwar->self())
	{
		return;
	}
}

void ProductionManager::manageBuildOrderQueue() 
{
	// if there is nothing in the _queue, oh well
	if (_queue.isEmpty()) 
	{
		return;
	}

	// the current item to be used
	BuildOrderItem & currentItem = _queue.getHighestPriorityItem();

	// while there is still something left in the _queue
	while (!_queue.isEmpty()) 
	{
		// this is the unit which can produce the currentItem
		BWAPI::Unit producer = getProducer(currentItem.metaType, MapTools::Instance().getBaseCenter());
		// check to see if we can make it right now
		bool canMake = canMakeNow(producer, currentItem.metaType);

		// if the next item in the list is a building and we can't yet make it
        if (currentItem.metaType.isBuilding() && !(producer && canMake) && currentItem.metaType.whatBuilds().isWorker())
		{
			// construct a temporary building object
			Building b(currentItem.metaType.getUnitType(), producer->getTilePosition());
            b.isGasSteal = currentItem.isGasSteal;

			// set the producer as the closest worker, but do not set its job yet
			producer = WorkerManager::Instance().getBuilder(b, false);
			if (producer)
			{
				b.builderUnit = producer;
				// predict the worker movement to that building location
				//predictWorkerMovement(b);
			}
		}

		// if we can make the current item
		if (producer && canMake)
		{
			// create it
			create(producer, currentItem);
			_assignedWorkerForThisBuilding = false;
			_haveLocationForThisBuilding = false;

			// and remove it from the _queue
			_queue.removeCurrentHighestPriorityItem();

			// don't actually loop around in here
			break;
		}
		// otherwise, if we can skip the current item
		else if (_queue.canSkipItem())
		{
			// skip it
			_queue.skipItem();

			// and get the next one
			currentItem = _queue.getNextHighestPriorityItem();				
		}
		else 
		{
			// so break out
			break;
		}
	}
}

BWAPI::Unit ProductionManager::getProducer(MetaType t, BWAPI::Position closestTo)
{
    // get the type of unit that builds this
    BWAPI::UnitType producerType = t.whatBuilds();

    // make a set of all candidate producers
    BWAPI::Unitset candidateProducers;
    for (auto & unit : BWAPI::Broodwar->self()->getUnits())
    {
        UAB_ASSERT(unit != nullptr, "Unit was null");

        // reasons a unit can not train the desired type
        if (unit->getType() != producerType)
		{ 
			// Hives can do what Lairs can do and Lairs can do what Hatcheries can do...
			if (!((producerType == BWAPI::UnitTypes::Zerg_Hatchery && (unit->getType() == BWAPI::UnitTypes::Zerg_Lair || unit->getType() == BWAPI::UnitTypes::Zerg_Hive))
				|| (producerType == BWAPI::UnitTypes::Zerg_Lair && unit->getType() == BWAPI::UnitTypes::Zerg_Hive)))
			{
				continue;
			}
		}
        if (!unit->isCompleted())                               { continue; }
        if (unit->isTraining())                                 { continue; }
		if (unit->isResearching())                              { continue; }
		if (unit->isUpgrading())                                { continue; }
		if (unit->isMorphing())                                 { continue; }
        if (unit->isLifted())                                   { continue; }
        if (!unit->isPowered())                                 { continue; }

        // if the type is an addon, some special cases
        if (t.getUnitType().isAddon())
        {
            // if the unit already has an addon, it can't make one
            if (unit->getAddon() != nullptr)
            {
                continue;
            }

            // if we just told this unit to build an addon, then it will not be building another one
            // this deals with the frame-delay of telling a unit to build an addon and it actually starting to build
            if (unit->getLastCommand().getType() == BWAPI::UnitCommandTypes::Build_Addon 
                && (BWAPI::Broodwar->getFrameCount() - unit->getLastCommandFrame() < 10)) 
            { 
                continue; 
            }

            bool isBlocked = false;

            // if the unit doesn't have space to build an addon, it can't make one
            BWAPI::TilePosition addonPosition(unit->getTilePosition().x + unit->getType().tileWidth(), unit->getTilePosition().y + unit->getType().tileHeight() - t.getUnitType().tileHeight());
            BWAPI::Broodwar->drawBoxMap(addonPosition.x*32, addonPosition.y*32, addonPosition.x*32 + 64, addonPosition.y*32 + 64, BWAPI::Colors::Red);
            
            for (int i=0; i<unit->getType().tileWidth() + t.getUnitType().tileWidth(); ++i)
            {
                for (int j=0; j<unit->getType().tileHeight(); ++j)
                {
                    BWAPI::TilePosition tilePos(unit->getTilePosition().x + i, unit->getTilePosition().y + j);

                    // if the map won't let you build here, we can't build it
                    if (!BWAPI::Broodwar->isBuildable(tilePos))
                    {
                        isBlocked = true;
                        BWAPI::Broodwar->drawBoxMap(tilePos.x*32, tilePos.y*32, tilePos.x*32 + 32, tilePos.y*32 + 32, BWAPI::Colors::Red);
                    }

                    // if there are any units on the addon tile, we can't build it
                    BWAPI::Unitset uot = BWAPI::Broodwar->getUnitsOnTile(tilePos.x, tilePos.y);
                    if (uot.size() > 0 && !(uot.size() == 1 && *(uot.begin()) == unit))
                    {
                        isBlocked = true;;
                        BWAPI::Broodwar->drawBoxMap(tilePos.x*32, tilePos.y*32, tilePos.x*32 + 32, tilePos.y*32 + 32, BWAPI::Colors::Red);
                    }
                }
            }

            if (isBlocked)
            {
                continue;
            }
        }
        
        // if the type requires an addon and the producer doesn't have one
        typedef std::pair<BWAPI::UnitType, int> ReqPair;
        for (const ReqPair & pair : t.getUnitType().requiredUnits())
        {
            BWAPI::UnitType requiredType = pair.first;
            if (requiredType.isAddon())
            {
                if (!unit->getAddon() || (unit->getAddon()->getType() != requiredType))
                {
                    continue;
                }
            }
        }

        // if we haven't cut it, add it to the set of candidates
        candidateProducers.insert(unit);
    }

    return getClosestUnitToPosition(candidateProducers, closestTo);
}

BWAPI::Unit ProductionManager::getClosestUnitToPosition(const BWAPI::Unitset & units, BWAPI::Position closestTo)
{
    if (units.size() == 0)
    {
        return nullptr;
    }

    // if we don't care where the unit is return the first one we have
    if (closestTo == BWAPI::Positions::None)
    {
        return *(units.begin());
    }

    BWAPI::Unit closestUnit = nullptr;
    double minDist(1000000);

	for (auto & unit : units) 
    {
        UAB_ASSERT(unit != nullptr, "Unit was null");

		double distance = unit->getDistance(closestTo);
		if (!closestUnit || distance < minDist) 
        {
			closestUnit = unit;
			minDist = distance;
		}
	}

    return closestUnit;
}

// this function will check to see if all preconditions are met and then create a unit
void ProductionManager::create(BWAPI::Unit producer, BuildOrderItem & item) 
{
    if (!producer)
    {
        return;
    }

    MetaType t = item.metaType;

    // if we're dealing with a building
    if (t.isUnit() && t.getUnitType().isBuilding() 
        && t.getUnitType() != BWAPI::UnitTypes::Zerg_Lair 
        && t.getUnitType() != BWAPI::UnitTypes::Zerg_Hive
        && t.getUnitType() != BWAPI::UnitTypes::Zerg_Greater_Spire
		&& t.getUnitType() != BWAPI::UnitTypes::Zerg_Sunken_Colony
		&& t.getUnitType() != BWAPI::UnitTypes::Zerg_Spore_Colony
        && !t.getUnitType().isAddon())
    {
        // send the building task to the building manager
		if (t.getUnitType() == BWAPI::UnitTypes::Zerg_Creep_Colony)
		{
			BuildingManager::Instance().addBuildingTask(t.getUnitType(), BWAPI::TilePosition(MapTools::Instance().getClosestSunkenPosition()), item.isGasSteal);
		}
		else
		{
			BuildingManager::Instance().addBuildingTask(t.getUnitType(), BWAPI::Broodwar->self()->getStartLocation(), item.isGasSteal);
		}
    }
    else if (t.getUnitType().isAddon())
    {
        //BWAPI::TilePosition addonPosition(producer->getTilePosition().x + producer->getType().tileWidth(), producer->getTilePosition().y + producer->getType().tileHeight() - t.unitType.tileHeight());
        producer->buildAddon(t.getUnitType());
    }
    // if we're dealing with a non-building unit
    else if (t.isUnit()) 
    {
        // if the race is zerg, morph the unit
        if (t.getUnitType().getRace() == BWAPI::Races::Zerg) 
        {
            producer->morph(t.getUnitType());
        // if not, train the unit
        } 
        else 
        {
            producer->train(t.getUnitType());
        }
    }
    // if we're dealing with a tech research
    else if (t.isTech())
    {
        producer->research(t.getTechType());
    }
    else if (t.isUpgrade())
    {
        //Logger::Instance().log("Produce Upgrade: " + t.getName() + "\n");
        producer->upgrade(t.getUpgradeType());
    }
    else
    {	
		
    }
}

bool ProductionManager::canMakeNow(BWAPI::Unit producer, MetaType t)
{
	if (!producer)
	{
		return false;
	}
	UAB_ASSERT(producer != nullptr, "Producer was null");

	bool canMake = meetsReservedResources(t);
	if (canMake)
	{
		if (t.isUnit())
		{
			canMake = BWAPI::Broodwar->canMake(t.getUnitType(), producer);
		}
		else if (t.isTech())
		{
			canMake = BWAPI::Broodwar->canResearch(t.getTechType(), producer);
		}
		else if (t.isUpgrade())
		{
			canMake = BWAPI::Broodwar->canUpgrade(t.getUpgradeType(), producer);
		}
		else
		{	
			UAB_ASSERT(false, "Unknown type");
		}
	}
	return canMake;
}

bool ProductionManager::detectBuildOrderDeadlock()
{
	// if the _queue is empty there is no deadlock
	if (BWAPI::Broodwar->self()->supplyTotal() >= 390)
	{
		return false;
	}

	// are any supply providers being built currently
	int supplyInProgress = BWAPI::Broodwar->self()->supplyTotal();

	double factor = 0.95;

	if (BWAPI::Broodwar->self()->supplyUsed() >= 35)
	{
		factor = 0.8;
	}

	// if we don't have enough supply and none is being built, there's a deadlock
	if (BWAPI::Broodwar->self()->supplyUsed() > int(supplyInProgress * factor))
	{
	    return true;
	}

	return false;
}

// When the next item in the _queue is a building, this checks to see if we should move to it
// This function is here as it needs to access prodction manager's reserved resources info
void ProductionManager::predictWorkerMovement(const Building & b)
{
    if (b.isGasSteal)
    {
        return;
    }

	// get a possible building location for the building
	if (!_haveLocationForThisBuilding)
	{
		_predictedTilePosition = BuildingManager::Instance().getBuildingLocation(b);
	}

	if (_predictedTilePosition != BWAPI::TilePositions::None)
	{
		_haveLocationForThisBuilding = true;
	}
	else
	{
		return;
	}
	
	// draw a box where the building will be placed
	int x1 = _predictedTilePosition.x * 32;
	int x2 = x1 + (b.type.tileWidth()) * 32;
	int y1 = _predictedTilePosition.y * 32;
	int y2 = y1 + (b.type.tileHeight()) * 32;
	if (Config::Debug::DrawWorkerInfo) 
    {
        BWAPI::Broodwar->drawBoxMap(x1, y1, x2, y2, BWAPI::Colors::Blue, false);
    }

	// where we want the worker to walk to
	BWAPI::Position walkToPosition		= BWAPI::Position(x1 + (b.type.tileWidth()/2)*32, y1 + (b.type.tileHeight()/2)*32);

	// compute how many resources we need to construct this building
	int mineralsRequired				= std::max(0, b.type.mineralPrice() - getFreeMinerals());
	int gasRequired						= std::max(0, b.type.gasPrice() - getFreeGas());

	// get a candidate worker to move to this location
	BWAPI::Unit moveWorker			= WorkerManager::Instance().getMoveWorker(walkToPosition);

	// Conditions under which to move the worker: 
	//		- there's a valid worker to move
	//		- we haven't yet assigned a worker to move to this location
	//		- the build position is valid
	//		- we will have the required resources by the time the worker gets there
	if (moveWorker && _haveLocationForThisBuilding && !_assignedWorkerForThisBuilding && (_predictedTilePosition != BWAPI::TilePositions::None) &&
		WorkerManager::Instance().willHaveResources(mineralsRequired, gasRequired, moveWorker->getDistance(walkToPosition)) )
	{
		// we have assigned a worker
		_assignedWorkerForThisBuilding = true;

		// tell the worker manager to move this worker
		WorkerManager::Instance().setMoveWorker(mineralsRequired, gasRequired, walkToPosition);
	}
}

void ProductionManager::performCommand(BWAPI::UnitCommandType t) 
{
	// if it is a cancel construction, it is probably the extractor trick
	if (t == BWAPI::UnitCommandTypes::Cancel_Construction)
	{
		BWAPI::Unit extractor = nullptr;
		for (auto & unit : BWAPI::Broodwar->self()->getUnits())
		{
			if (unit->getType() == BWAPI::UnitTypes::Zerg_Extractor)
			{
				extractor = unit;
			}
		}

		if (extractor)
		{
			extractor->cancelConstruction();
		}
	}
}

int ProductionManager::getFreeMinerals()
{
	return BWAPI::Broodwar->self()->minerals() - BuildingManager::Instance().getReservedMinerals();
}

int ProductionManager::getFreeGas()
{
	return BWAPI::Broodwar->self()->gas() - BuildingManager::Instance().getReservedGas();
}

// return whether or not we meet resources, including building reserves
bool ProductionManager::meetsReservedResources(MetaType type) 
{
	// return whether or not we meet the resources
	return (type.mineralPrice() <= getFreeMinerals()) && (type.gasPrice() <= getFreeGas());
}


// selects a unit of a given type
BWAPI::Unit ProductionManager::selectUnitOfType(BWAPI::UnitType type, BWAPI::Position closestTo) 
{
	// if we have none of the unit type, return nullptr right away
	if (BWAPI::Broodwar->self()->completedUnitCount(type) == 0) 
	{
		return nullptr;
	}

	BWAPI::Unit unit = nullptr;

	// if we are concerned about the position of the unit, that takes priority
    if (closestTo != BWAPI::Positions::None) 
    {
		double minDist(1000000);

		for (auto & u : BWAPI::Broodwar->self()->getUnits()) 
        {
			if (u->getType() == type) 
            {
				double distance = u->getDistance(closestTo);
				if (!unit || distance < minDist) {
					unit = u;
					minDist = distance;
				}
			}
		}

	// if it is a building and we are worried about selecting the unit with the least
	// amount of training time remaining
	} 
    else if (type.isBuilding()) 
    {
		for (auto & u : BWAPI::Broodwar->self()->getUnits()) 
        {
            UAB_ASSERT(u != nullptr, "Unit was null");

			if (u->getType() == type && u->isCompleted() && !u->isTraining() && !u->isLifted() &&u->isPowered()) {

				return u;
			}
		}
		// otherwise just return the first unit we come across
	} 
    else 
    {
		for (auto & u : BWAPI::Broodwar->self()->getUnits()) 
		{
            UAB_ASSERT(u != nullptr, "Unit was null");

			if (u->getType() == type && u->isCompleted() && u->getHitPoints() > 0 && !u->isLifted() &&u->isPowered()) 
			{
				return u;
			}
		}
	}

	// return what we've found so far
	return nullptr;
}

void ProductionManager::drawProductionInformation(int x, int y)
{
    if (!Config::Debug::DrawProductionInfo)
    {
        return;
    }

	// fill prod with each unit which is under construction
	std::vector<BWAPI::Unit> prod;
	for (auto & unit : BWAPI::Broodwar->self()->getUnits())
	{
        UAB_ASSERT(unit != nullptr, "Unit was null");

		if (unit->isBeingConstructed())
		{
			prod.push_back(unit);
		}
	}
	
	// sort it based on the time it was started
	std::sort(prod.begin(), prod.end(), CompareWhenStarted());

    BWAPI::Broodwar->drawTextScreen(x-30, y+20, "\x04 TIME");
	BWAPI::Broodwar->drawTextScreen(x, y+20, "\x04 UNIT NAME");

	size_t reps = prod.size() < 10 ? prod.size() : 10;

	y += 30;
	int yy = y;

	// for each unit in the _queue
	for (auto & unit : prod) 
    {
		std::string prefix = "\x07";

		yy += 10;

		BWAPI::UnitType t = unit->getType();
        if (t == BWAPI::UnitTypes::Zerg_Egg)
        {
            t = unit->getBuildType();
        }

		BWAPI::Broodwar->drawTextScreen(x, yy, " %s%s", prefix.c_str(), t.getName().c_str());
		BWAPI::Broodwar->drawTextScreen(x - 35, yy, "%s%6d", prefix.c_str(), unit->getRemainingBuildTime());
	}

	_queue.drawQueueInformation(x, yy+10);
}

ProductionManager & ProductionManager::Instance()
{
	static ProductionManager instance;
	return instance;
}

// this will return true if any unit is on the first frame if it's training time remaining
// this can cause issues for the build order search system so don't plan a search on these frames
bool ProductionManager::canPlanBuildOrderNow() const
{
    for (const auto & unit : BWAPI::Broodwar->self()->getUnits())
    {
        if (unit->getRemainingTrainTime() == 0)
        {
            continue;       
        }

        BWAPI::UnitType trainType = unit->getLastCommand().getUnitType();

        if (unit->getRemainingTrainTime() == trainType.buildTime())
        {
            return false;
        }
    }

    return true;
}

void ProductionManager::checkCancels(bool underPressure)
{
	for (const auto & unit : BWAPI::Broodwar->self()->getUnits())
	{
		if (!unit->isMorphing())
		{
			continue;
		}
		double progress = double(unit->getType().buildTime() - unit->getRemainingBuildTime()) / (double)unit->getType().buildTime();
		if (unit->getHitPoints() < unit->getType().maxHitPoints() * progress / 4.0 && unit->isUnderAttack())
		{
			unit->cancelMorph();
		}
		//if (underPressure && unit->getType() == BWAPI::UnitTypes::Zerg_Hatchery)
		//{
		//	if (WorkerManager::Instance().isDepot(unit, false))
		//	{
		//		BWAPI::Unit closestEnemy = unit->getClosestUnit(BWAPI::Filter::IsEnemy&&BWAPI::Filter::CanAttack);
		//		if (closestEnemy && closestEnemy->getDistance(unit->getPosition()) < 400 && !closestEnemy->getType().isWorker())
		//		{
		//			unit->cancelMorph();
		//			sunkenUnlocked = true;
		//		}
		//	}
		//}
	}
}