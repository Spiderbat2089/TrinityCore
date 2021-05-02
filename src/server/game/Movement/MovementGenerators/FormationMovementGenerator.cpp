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

#include "FormationMovementGenerator.h"
#include "AIFormations.h"
#include "Creature.h"
#include "CreatureAI.h"
#include "CreatureGroups.h"
#include "G3DPosition.hpp"
#include "MoveSplineInit.h"
#include "MoveSpline.h"

inline UnitMoveType SelectSpeedType(uint32 moveFlags)
{
    if (moveFlags & MOVEMENTFLAG_FLYING)
    {
        if (moveFlags & MOVEMENTFLAG_BACKWARD)
            return MOVE_FLIGHT_BACK;
        else
            return MOVE_FLIGHT;
    }
    else if (moveFlags & MOVEMENTFLAG_SWIMMING)
    {
        if (moveFlags & MOVEMENTFLAG_BACKWARD)
            return MOVE_SWIM_BACK;
        else
            return MOVE_SWIM;
    }
    else if (moveFlags & MOVEMENTFLAG_WALKING)
        return MOVE_WALK;
    else if (moveFlags & MOVEMENTFLAG_BACKWARD)
        return MOVE_RUN_BACK;

    return MOVE_RUN;
}

FormationMovementGenerator::FormationMovementGenerator(Unit* formationLeader, Position const& formationOffset) :
    AbstractPursuer(PursuingType::Formation, formationLeader), _formationOffset(formationOffset), _movementTimer(0),
    _movementCheckTimer(MOVEMENT_CHECK_INTERVAL)
{
}

void FormationMovementGenerator::DoInitialize(Creature* owner)
{
    owner->AddUnitState(UNIT_STATE_ROAMING);
}

bool FormationMovementGenerator::DoUpdate(Creature* owner, uint32 diff)
{
    Unit* target = GetTarget();

    if (!owner || !target)
        return false;

    // Update home position
    owner->SetHomePosition(owner->GetPosition());

    if (owner->HasUnitState(UNIT_STATE_ROAMING_MOVE) && owner->movespline->Finalized())
        owner->ClearUnitState(UNIT_STATE_ROAMING_MOVE);

    // Owner cannot move. Reset all fields and wait for next action
    if (owner->HasUnitState(UNIT_STATE_NOT_MOVE) || owner->IsMovementPreventedByCasting())
    {
        _movementTimer = 0;
        owner->ClearUnitState(UNIT_STATE_ROAMING_MOVE);
        owner->StopMoving();
        return true;
    }

    // Target is not moving. Stop movement and align to formation.
    if (target->IsCreature() && target->movespline->Finalized())
    {
        if (_lastLeaderPosition != target->GetPosition())
            LaunchMovement(owner);

        _movementTimer = 0;
        return true;
    }

    _movementCheckTimer -= diff;
    if (_movementCheckTimer < 0)
    {
        _movementCheckTimer += MOVEMENT_CHECK_INTERVAL;
    }

    _movementTimer -= diff;
    if (_movementTimer <= 0)
    {
        _movementTimer = target->GetAIFormation().GetNextFormationMovementTime();
        LaunchMovement(owner);
        return true;
    }

    return true;
}

void FormationMovementGenerator::LaunchMovement(Creature* owner, bool enforceAlignment /*= false*/)
{
    float relativeAngle = 0.f;
    Unit* target = GetTarget();

    // Determine our relative angle to our current spline destination point
    if (!target->movespline->Finalized())
        relativeAngle = target->GetRelativeAngle(Vector3ToPosition(target->movespline->CurrentDestination()));

    Position dest = target->GetPosition();
    float range = Position().GetExactDist2d(_formationOffset);
    float angle = Position().GetRelativeAngle(_formationOffset);
    float velocity = target->movespline->Finalized() ? target->GetSpeed(SelectSpeedType(target->GetUnitMovementFlags())) : target->movespline->Velocity();

    // Formation leader is moving. Predict our destination
    if (!target->movespline->Finalized() || target->isMoving())
    {
        // Calculate travel distance to get a 1650ms result for creatures
        float travelDist = target->IsCreature() ? velocity * 1.65f : velocity;

        // Move destination ahead...
        target->MovePositionToFirstCollision(dest, travelDist, relativeAngle);
        // ... and apply formation shape
        target->MovePositionToFirstCollision(dest, range, angle + relativeAngle);

        float distance = owner->GetExactDist(dest);

        // Calculate catchup speed mod (limit to a maximum of 50% of our original velocity)
        float velocityMod = std::min<float>(distance / travelDist, 1.5f);

        // Now we will always stay sync with our leader
        velocity *= velocityMod;
    }
    else if (target->IsCreature() || enforceAlignment)
    {
        // Formation leader is not moving. Just apply the base formation shape on his position.
        target->MovePositionToFirstCollision(dest, range, angle + relativeAngle);
    }
    else if (target->IsPlayer())
    {
        // Player is no longer moving. Stop movement and prepare to align
    }

    Movement::MoveSplineInit init(owner);
    init.MoveTo(PositionToVector3(dest), false);
    init.SetVelocity(velocity);
    init.Launch();

    _lastLeaderPosition = target->GetPosition();
    owner->AddUnitState(UNIT_STATE_ROAMING_MOVE);
}

void FormationMovementGenerator::DoFinalize(Creature* owner)
{
    owner->ClearUnitState(UNIT_STATE_ROAMING | UNIT_STATE_ROAMING_MOVE);
}

void FormationMovementGenerator::MovementInform(Creature* owner)
{
    if (owner->AI())
        owner->AI()->MovementInform(FORMATION_MOTION_TYPE, 0);
}
