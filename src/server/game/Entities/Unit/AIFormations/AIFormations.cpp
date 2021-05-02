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

#include "AIFormations.h"
#include "FormationMovementGenerator.h"
#include "MovementGenerator.h"
#include "MoveSpline.h"
#include "Unit.h"

AIFormation::AIFormation(Unit* leader) :
    _leader(leader), _formationType(AIFormationType::GridBehindLeader), _formationFollowerDistance(DEFAULT_FORMATION_FOLLOWER_DISTANCE),
    _formationMovementTimer(0), _formationMovementInterval(leader->IsPlayer() ? PLAYER_FORMATION_MOVEMENT_INTERVAL : CREATURE_FORMATION_MOVEMENT_INTERVAL)
{
}

void AIFormation::AddFollower(Unit* follower, uint8 formationPosition)
{
    FormationFollower const& formationFollower = _followerData.emplace_back(follower->GetGUID(), formationPosition);
    UpdateFormationOffsetsForAllFollowers();
    follower->GetMotionMaster()->MoveFormation(_leader, formationFollower.FormationOffsets);
}

void AIFormation::RemoveFollower(Unit* follower)
{
    for (std::vector<FormationFollower>::const_iterator itr = _followerData.begin(); itr != _followerData.end(); ++itr)
    {
        if (itr->GUID != follower->GetGUID())
            continue;

        if (Unit* follower = ObjectAccessor::GetUnit(*_leader, itr->GUID))
        {
            // Clear movement generator
            if (follower->GetMotionMaster()->GetMotionSlotType(MOTION_SLOT_IDLE) == FORMATION_MOTION_TYPE)
                follower->GetMotionMaster()->Clear(MOTION_SLOT_IDLE);
        }

        _followerData.erase(itr);

        // Follower removed, update formation offsets for all remaining followers
        if (!_followerData.empty())
            UpdateFormationOffsetsForAllFollowers();
        break;
    }
}

void AIFormation::RemoveAllFollowers()
{
    for (FormationFollower const& formationFollower : _followerData)
    {
        // Clear movement generator
        if (Unit* follower = ObjectAccessor::GetUnit(*_leader, formationFollower.GUID))
        {
            if (follower->GetMotionMaster()->GetMotionSlotType(MOTION_SLOT_IDLE) == FORMATION_MOTION_TYPE)
                follower->GetMotionMaster()->Clear(MOTION_SLOT_IDLE);
        }
    }
    _followerData.clear();
}

void AIFormation::Update(uint32 diff)
{
    _formationMovementTimer -= diff;
    if (_formationMovementTimer <= 0)
        _formationMovementTimer += _formationMovementInterval;
}

void AIFormation::SetFormationType(AIFormationType type)
{
    _formationType = type;
    UpdateFormationOffsetsForAllFollowers();
}

void AIFormation::SetFormationFollowerDistance(float distance)
{
    _formationFollowerDistance = distance;
    UpdateFormationOffsetsForAllFollowers();
}

void AIFormation::InvertFormationOffsets()
{
    for (FormationFollower& followerData : _followerData)
    {
        followerData.FormationOffsets.m_positionX *= -1.f;
        followerData.FormationOffsets.m_positionY *= -1.f;
    }

    UpdateFormationOffsetsForAllFollowers();
}

void AIFormation::UpdateFormationOffsetsForAllFollowers()
{
    // Before we are going to update, we are sorting the followers that might have a specific sort index
    std::sort(_followerData.begin(), _followerData.end(), [&](FormationFollower const& previous, FormationFollower const& next)
    {
        return previous.FormationPosition < next.FormationPosition;
    });

    uint32 followerIndex = 0;
    for (FormationFollower& followerData : _followerData)
    {
        followerData.FormationOffsets = CalculateFormationOffsets(followerIndex);
        Unit* follower = ASSERT_NOTNULL(ObjectAccessor::GetUnit(*_leader, followerData.GUID));

        // Information for maintainers: MOTION_SLOT_IDLE is the current slot used by MotionMaster::MoveFormation. Update the slot used here accordingly if this should ever change.
        // Tell the formation follower's movement generator that they have a new offset.
        if (MovementGenerator* moveGen = follower->GetMotionMaster()->GetMotionSlot(MOTION_SLOT_IDLE))
            if (moveGen->GetMovementGeneratorType() == FORMATION_MOTION_TYPE)
                if (FormationMovementGenerator* formationMoveGen = dynamic_cast<FormationMovementGenerator*>(moveGen))
                    formationMoveGen->SetFormationOffset(followerData.FormationOffsets);

        ++followerIndex;
    }
}

Position AIFormation::CalculateFormationOffsets(uint32 targetFormationIndex) const
{
    Position offset;
    switch (_formationType)
    {
        case AIFormationType::Random:
            offset.RelocateOffset({ frand(-_formationFollowerDistance, _formationFollowerDistance), frand(-_formationFollowerDistance, _formationFollowerDistance) });
            break;
        case AIFormationType::SingleFile:
            offset.RelocateOffset({ -_formationFollowerDistance * (targetFormationIndex + 1) });
            break;
        case AIFormationType::SideBySide:
        {
            bool left = ((targetFormationIndex + 1) % 2) != 0;
            uint8 multiplier = std::ceil((targetFormationIndex + 1) / 2.f);
            float distanceOffset = (left ? _formationFollowerDistance : -_formationFollowerDistance) * multiplier;
            offset.RelocateOffset({ 0.f, distanceOffset });
            break;
        }
        case AIFormationType::LikeGeese:
            // @todo: need retail examples.
            break;
        case AIFormationType::FannedOutBehindLeader:
        case AIFormationType::FannedOutInFrontOfLeader:
        {
            // Blizzard really does weird things sometimes. Two or less members will get at least three slots, less than four get four slots.
            uint32 divider = _followerData.size() <= 2 ? _followerData.size() : std::max<uint32>(3, _followerData.size() - 1);
            float circleSteps = float(M_PI) / divider;
            float xOffset = std::cos(float(M_PI / 2) + circleSteps * targetFormationIndex) * _formationFollowerDistance;
            float yOffset = std::sin(float(M_PI / 2) + circleSteps * targetFormationIndex) * _formationFollowerDistance;
            offset.m_positionX = _formationType == AIFormationType::FannedOutBehindLeader ? xOffset : - xOffset;
            offset.m_positionY = yOffset; // both formation types start on the left side
            break;
        }
        case AIFormationType::CircleLeader:
        {
            float circleSteps = float(M_PI * 2) / _followerData.size();
            offset.m_positionX = std::cos(circleSteps * targetFormationIndex) * _formationFollowerDistance;
            offset.m_positionY = std::sin(circleSteps * targetFormationIndex) * _formationFollowerDistance;
            break;
        }
        case AIFormationType::Marching:
            // @todo: need retail examples. double rows moving behind each other? BoT last trash pack before halfus?
            break;
        case AIFormationType::GridBehindLeader:
            // See follow movement generator for offset calculation. Just too lazy to port it atm...
            break;
        default:
            break;
    }

    return offset;
}
