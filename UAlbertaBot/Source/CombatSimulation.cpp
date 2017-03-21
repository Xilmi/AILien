#include "CombatSimulation.h"

using namespace UAlbertaBot;

CombatSimulation::CombatSimulation()
{
	
}

// sets the starting states based on the combat units within a radius of a given position
// this center will most likely be the position of the forwardmost combat unit we control
void CombatSimulation::setCombatUnits(const BWAPI::Position & center, const int radius)
{
	SparCraft::GameState s;

	BWAPI::Broodwar->drawCircleMap(center.x, center.y, 10, BWAPI::Colors::Red, true);

	std::vector<UnitInfo> ourCombatUnits;
	std::vector<UnitInfo> enemyCombatUnits;

	InformationManager::Instance().getNearbyForce(ourCombatUnits, center, BWAPI::Broodwar->self(), Config::Micro::CombatRegroupRadius);
	InformationManager::Instance().getNearbyForce(enemyCombatUnits, center, BWAPI::Broodwar->enemy(), Config::Micro::CombatRegroupRadius);

	for (UnitInfo & ui : ourCombatUnits)
	{
		if (ui.type.isWorker())
        {
            continue;
        }
		if (InformationManager::Instance().isCombatUnit(ui.type) && SparCraft::System::isSupportedUnitType(ui.type))
		{
            try
            {
				s.addUnit(getSparCraftUnit(ui));
            }
            catch (int e)
            {
                e=1;
				BWAPI::Broodwar->printf("Problem adding %s with ID: %d", ui.type.getName().c_str(), ui.unit->getID());
            }
		}
	}

	for (UnitInfo & ui : enemyCombatUnits)
	{
        if (ui.type.isWorker())
        {
            continue;
        }
        if (ui.type == BWAPI::UnitTypes::Terran_Bunker)
        {
            double hpRatio = static_cast<double>(ui.lastHealth) / ui.type.maxHitPoints();

            SparCraft::Unit marine( BWAPI::UnitTypes::Terran_Marine,
                            SparCraft::Position(ui.lastPosition), 
                            ui.unitID, 
                            getSparCraftPlayerID(ui.player), 
                            static_cast<int>(BWAPI::UnitTypes::Terran_Marine.maxHitPoints() * hpRatio), 
                            0,
		                    BWAPI::Broodwar->getFrameCount(), 
                            BWAPI::Broodwar->getFrameCount());	

            for (size_t i(0); i < 2; ++i)
            {
                s.addUnit(marine);
            }
			SparCraft::Unit healthyMarine(BWAPI::UnitTypes::Terran_Marine,
				SparCraft::Position(ui.lastPosition),
				ui.unitID,
				getSparCraftPlayerID(ui.player),
				static_cast<int>(BWAPI::UnitTypes::Terran_Marine.maxHitPoints()),
				0,
				BWAPI::Broodwar->getFrameCount(),
				BWAPI::Broodwar->getFrameCount());
			for (size_t i(0); i < 4; ++i)
			{
				s.addUnit(healthyMarine);
			}
            
            continue;
        }
		//if (ui.type == BWAPI::UnitTypes::Protoss_Carrier)
		//{
		//	Logger::LogAppendToFile("carrier.log", "Calculating HP-ratio.\n");
		//	double hpRatio = static_cast<double>(ui.lastHealth + ui.lastShields) / (ui.type.maxHitPoints() + ui.type.maxShields());
		//	Logger::LogAppendToFile("carrier.log", "Generating BC for sparcraft\n");
		//	SparCraft::Unit carrier(BWAPI::UnitTypes::Terran_Battlecruiser,
		//		SparCraft::Position(ui.lastPosition),
		//		ui.unitID,
		//		getSparCraftPlayerID(ui.player),
		//		static_cast<int>(BWAPI::UnitTypes::Terran_Battlecruiser.maxHitPoints() * hpRatio),
		//		0,
		//		BWAPI::Broodwar->getFrameCount(),
		//		BWAPI::Broodwar->getFrameCount());
		//	Logger::LogAppendToFile("carrier.log", "Adding fake-bc\n");
		//	s.addUnit(carrier);
		//	continue;
		//}
		if (ui.type == BWAPI::UnitTypes::Protoss_Reaver
			|| ui.type == BWAPI::UnitTypes::Protoss_Shuttle)
		{
			double hpRatio = static_cast<double>(ui.lastHealth + ui.lastShields) / (ui.type.maxHitPoints() + ui.type.maxShields());

			SparCraft::Unit reaver(BWAPI::UnitTypes::Terran_Siege_Tank_Siege_Mode,
				SparCraft::Position(ui.lastPosition),
				ui.unitID,
				getSparCraftPlayerID(ui.player),
				static_cast<int>(BWAPI::UnitTypes::Terran_Siege_Tank_Siege_Mode.maxHitPoints() * hpRatio),
				0,
				BWAPI::Broodwar->getFrameCount(),
				BWAPI::Broodwar->getFrameCount());

			s.addUnit(reaver);
			continue;
		}

        if (!ui.type.isFlyer() && SparCraft::System::isSupportedUnitType(ui.type) && ui.completed)
		{
            try
            {
			    s.addUnit(getSparCraftUnit(ui));
            }
            catch (int e)
            {
                BWAPI::Broodwar->printf("Problem Adding Enemy Unit with ID: %d %d", ui.unitID, e);
            }
		}
	}

	s.finishedMoving();

	state = s;
}

// Gets a SparCraft unit from a BWAPI::Unit, used for our own units since we have all their info
const SparCraft::Unit CombatSimulation::getSparCraftUnit(BWAPI::Unit unit) const
{
    return SparCraft::Unit( unit->getType(),
                            SparCraft::Position(unit->getPosition()), 
                            unit->getID(), 
                            getSparCraftPlayerID(unit->getPlayer()), 
                            unit->getHitPoints() + unit->getShields(), 
                            0,
		                    BWAPI::Broodwar->getFrameCount(), 
                            BWAPI::Broodwar->getFrameCount());	
}

// Gets a SparCraft unit from a UnitInfo struct, needed to get units of enemy behind FoW
const SparCraft::Unit CombatSimulation::getSparCraftUnit(const UnitInfo & ui) const
{
	BWAPI::UnitType type = ui.type;

    // this is a hack, treat medics as a marine for now
	if (type == BWAPI::UnitTypes::Terran_Medic)
	{
		type = BWAPI::UnitTypes::Terran_Marine;
	}

    return SparCraft::Unit( ui.type, 
                            SparCraft::Position(ui.lastPosition), 
                            ui.unitID, 
                            getSparCraftPlayerID(ui.player), 
                            ui.lastHealth, 
                            0,
		                    BWAPI::Broodwar->getFrameCount(), 
                            BWAPI::Broodwar->getFrameCount());	
}

SparCraft::ScoreType CombatSimulation::simulateCombat()
{
    try
    {
	    SparCraft::GameState s1(state);

        SparCraft::PlayerPtr selfNOK(new SparCraft::Player_NOKDPS(getSparCraftPlayerID(BWAPI::Broodwar->self())));

	    SparCraft::PlayerPtr enemyNOK(new SparCraft::Player_NOKDPS(getSparCraftPlayerID(BWAPI::Broodwar->enemy())));

	    SparCraft::Game g (s1, selfNOK, enemyNOK, 2000);

	    g.play();
	
	    SparCraft::ScoreType eval =  g.getState().eval(SparCraft::Players::Player_One, SparCraft::EvaluationMethods::LTD2).val();

        if (Config::Debug::DrawCombatSimulationInfo)
        {
            std::stringstream ss1;
            ss1 << "Initial State:\n";
            ss1 << s1.toStringCompact() << "\n\n";

            std::stringstream ss2;

            ss2 << "Predicted Outcome: " << eval << "\n";
            ss2 << g.getState().toStringCompact() << "\n";

            BWAPI::Broodwar->drawTextScreen(150,200,"%s", ss1.str().c_str());
            BWAPI::Broodwar->drawTextScreen(300,200,"%s", ss2.str().c_str());

	        BWAPI::Broodwar->drawTextScreen(240, 280, "Combat Sim : %d", eval);
        }
        
	    return eval;
    }
    catch (int e)
    {
        BWAPI::Broodwar->printf("SparCraft FatalError, simulateCombat() threw");

        return e;
    }
}

const SparCraft::GameState & CombatSimulation::getSparCraftState() const
{
	return state;
}

const SparCraft::IDType CombatSimulation::getSparCraftPlayerID(BWAPI::Player player) const
{
	if (player == BWAPI::Broodwar->self())
	{
		return SparCraft::Players::Player_One;
	}
	else if (player == BWAPI::Broodwar->enemy())
	{
		return SparCraft::Players::Player_Two;
	}

	return SparCraft::Players::Player_None;
}