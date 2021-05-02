/*
* This file is part of the TrinityCore Project. See AUTHORS file for Copyright information
*
* This program is free software; you can redistribute it and/or modify it
* under the terms of the GNU General Public License as published by the
* Free Software Foundation; either version 2 of the License, or (at your
* option) any later version.
*
* This program is distributed in the hope that it will be useful, but WITHOUT
* ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
* FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License for
* more details.
*
* You should have received a copy of the GNU General Public License along
* with this program. If not, see <http://www.gnu.org/licenses/>.
*/

#ifndef AI_FORMATIONS_H
#define AI_FORMATIONS_H

#include "define.h"
#include "ObjectGuid.h"
#include "Position.h"
#include "Optional.h"

class Unit;

enum class AIFormationType : uint8
{
    Random                      = 0, // Just random
    SingleFile                  = 1, // Single file, leader at the front
    SideBySide                  = 2, // Side by side, leader in the center
    LikeGeese                   = 3, // Like geese
    FannedOutBehindLeader       = 4, // Fanned out behind the leader
    FannedOutInFrontOfLeader    = 5, // Fanned out in front of the leader
    CircleLeader                = 6, // Circle the leader
    Marching                    = 7, // Marching
    GridBehindLeader            = 8  // Grid behind leader (L,R first)
};

enum class AIFormationBehavior : uint8
{
    FollowersIgnorePathFinding      = 0, // Followers Will Not Path-Find to Destination
    FormationCompactingOnUnitDeath  = 1, // Formation Compacting on Unit Death
};

static constexpr float const DEFAULT_FORMATION_FOLLOWER_DISTANCE    = 3.f;
static constexpr uint32 const CREATURE_FORMATION_MOVEMENT_INTERVAL  = 1200; // sniffed (3 batch update cycles)
static constexpr uint32 const PLAYER_FORMATION_MOVEMENT_INTERVAL    = 400;  // sniffed (1 batch update cycle)

struct FormationFollower
{
    FormationFollower(ObjectGuid guid, uint8 formationPosition) :
        GUID(guid), FormationPosition(formationPosition) { }

    ObjectGuid GUID;
    uint8 FormationPosition;
    Position FormationOffsets;
};

class AIFormation
{
public:
    AIFormation(Unit* leader);

    void SetLeader(Unit* leader) { _leader = leader; }
    void AddFollower(Unit* member, uint8 formationPosition = 0);
    void RemoveFollower(Unit* member);
    void RemoveAllFollowers();
    void Update(uint32 diff);

    // Changes the formation type and updates the formation offsets for all current followers
    void SetFormationType(AIFormationType type);
    // Changes the formation follower distance and updates the formation offsets for all current followers
    void SetFormationFollowerDistance(float distance);
    // Inverts and recalculates the formation offsets of all followers
    void InvertFormationOffsets();
    // Returns the next movement time of the formation
    int32 GetNextFormationMovementTime() const { return _formationMovementTimer; }

protected:
    // Recalculates the formation offsets for all current followers
    void UpdateFormationOffsetsForAllFollowers();
    // Calculates formation offsets for the formation type at given formation index
    Position CalculateFormationOffsets(uint32 targetFormationIndex) const;

    Unit* _leader;
    std::vector<FormationFollower> _followerData;
    AIFormationType _formationType;
    float _formationFollowerDistance;
    int32 _formationMovementTimer;
    uint16 _formationMovementInterval;
};

#endif // AI_FORMATIONS_H
