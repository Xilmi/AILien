#pragma once

#include "Common.h"
#include "BWTA.h"

namespace UAlbertaBot
{
struct UnitInfo
{
    // we need to store all of this data because if the unit is not visible, we
    // can't reference it from the unit pointer

    int             unitID;
    int             lastHealth;
    int             lastShields;
    BWAPI::Player   player;
    BWAPI::Unit     unit;
    BWAPI::Position lastPosition;
    BWAPI::UnitType type;
    bool            completed;
	bool			positionValid;
	bool			commit;
	double			powerAssigned;

    UnitInfo()
        : unitID(0)
        , lastHealth(0)
        , player(nullptr)
        , unit(nullptr)
        , lastPosition(BWAPI::Positions::None)
        , type(BWAPI::UnitTypes::None)
        , completed(false)
		, positionValid(true)
		, commit(false)
		, powerAssigned(0)
    {

    }

    const bool operator == (BWAPI::Unit unit) const
    {
        return unitID == unit->getID();
    }

    const bool operator == (const UnitInfo & rhs) const
    {
        return (unitID == rhs.unitID);
    }

    const bool operator < (const UnitInfo & rhs) const
    {
        return (unitID < rhs.unitID);
    }
};

typedef std::vector<UnitInfo> UnitInfoVector;
typedef std::map<BWAPI::Unit,UnitInfo> UIMap;

class UnitData
{
    UIMap unitMap;

    const bool badUnitInfo(const UnitInfo & ui) const;

    std::vector<int>						numDeadUnits;
    std::vector<int>						numUnits;

	double										mineralsLost = 0;
	double										gasLost = 0;

public:

    UnitData();

    bool	updateUnit(BWAPI::Unit unit);
    void	removeUnit(BWAPI::Unit unit);
    void	removeBadUnits();

    double		getGasLost()                                const;
	double		getMineralsLost()                           const;
	double		totalLost()									const;
    int		getNumUnits(BWAPI::UnitType t)              const;
    int		getNumDeadUnits(BWAPI::UnitType t)          const;
    const	std::map<BWAPI::Unit,UnitInfo> & getUnits() const;
};
}