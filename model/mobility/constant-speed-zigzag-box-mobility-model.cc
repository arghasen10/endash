/*
 * constant-speed-zigzag-box-mobility-model.cc
 *
 *  Created on: May 29, 2020
 *      Author: abhijit
 */

#include "constant-speed-zigzag-box-mobility-model.h"
#include "ns3/pointer.h"
#include "ns3/simulator.h"
#include "ns3/log.h"

namespace ns3
{


NS_LOG_COMPONENT_DEFINE ("ZigzagWalk2d");

NS_OBJECT_ENSURE_REGISTERED (ConstantSpeedZigzagBoxMobilityModel);

ConstantSpeedZigzagBoxMobilityModel::ConstantSpeedZigzagBoxMobilityModel ()
{
}

TypeId
ConstantSpeedZigzagBoxMobilityModel::GetTypeId (void)
{
  static TypeId tid = TypeId ("ns3::ConstantSpeedZigzagBoxMobilityModel")
    .SetParent<MobilityModel> ()
    .SetGroupName ("Mobility")
    .AddConstructor<ConstantSpeedZigzagBoxMobilityModel> ()
    .AddAttribute ("Bounds",
                  "Bounds of the area to cruise.",
                  RectangleValue (Rectangle (0.0, 100.0, 0.0, 100.0)),
                  MakeRectangleAccessor (&ConstantSpeedZigzagBoxMobilityModel::m_bounds),
                  MakeRectangleChecker ())
    .AddAttribute ("VelocityAllocator",
                  "A velocity allocator (ListPositionAllocator Pointer)",
                  TypeId::ATTR_SET|TypeId::ATTR_CONSTRUCT,
                  PointerValue (),
                  MakePointerAccessor (&ConstantSpeedZigzagBoxMobilityModel::SetVelocityFromListPositionAllocator),
                  MakePointerChecker<ListPositionAllocator> ());
    return tid;
}

ConstantSpeedZigzagBoxMobilityModel::~ConstantSpeedZigzagBoxMobilityModel ()
{
  // TODO Auto-generated destructor stub
}

void
ConstantSpeedZigzagBoxMobilityModel::SetVelocity (const Vector &speed)
{
  m_speedHelper.Update();
  m_speedHelper.SetVelocity(speed);
  m_velocity = speed;
  m_event.Cancel();
  m_event = Simulator::ScheduleNow (&ConstantSpeedZigzagBoxMobilityModel::DoInitializePrivate, this);
}

Vector
ConstantSpeedZigzagBoxMobilityModel::DoGetPosition (void) const
{
  m_speedHelper.UpdateWithBounds(m_bounds);
  return m_speedHelper.GetCurrentPosition ();
}

void
ConstantSpeedZigzagBoxMobilityModel::DoSetPosition (const Vector &position)
{
  NS_ASSERT (m_bounds.IsInside (position));
  m_speedHelper.SetPosition (position);
  m_speedHelper.SetVelocity(m_velocity);
//    Simulator::Remove (m_event);
  m_event.Cancel();
  m_event = Simulator::ScheduleNow (&ConstantSpeedZigzagBoxMobilityModel::DoInitializePrivate, this);
}

void
ConstantSpeedZigzagBoxMobilityModel::DoInitializePrivate (void)
{
  m_speedHelper.Update ();
  m_speedHelper.Unpause();

  ConstantSpeedZigzagBoxMobilityModel::DoWalk();
}

void
ConstantSpeedZigzagBoxMobilityModel::DoDispose (void)
{
  // chain up
  MobilityModel::DoDispose ();
}

void
ConstantSpeedZigzagBoxMobilityModel::DoInitialize (void)
{
  DoInitializePrivate ();
  MobilityModel::DoInitialize ();
}

Vector
ConstantSpeedZigzagBoxMobilityModel::DoGetVelocity (void) const
{
  return m_speedHelper.GetVelocity();
}

void
ConstantSpeedZigzagBoxMobilityModel::DoWalk ()
{
//    m_speedHelper.Update();
  auto curVel = m_speedHelper.GetVelocity();
  if(curVel.x == 0 and curVel.y == 0) return;
  auto curPos = m_speedHelper.GetCurrentPosition();
  auto point = m_bounds.CalculateIntersection(curPos, curVel);
  auto timeLeftX = std::abs((point.x - curPos.x) / curVel.x);
  auto timeLeftY = std::abs((point.y - curPos.y) / curVel.y);

  NS_ASSERT(std::abs(timeLeftX - timeLeftY) < 0.0001);

  auto timeLeft = std::min(timeLeftX, timeLeftY);

  if (timeLeft < 0.001)
    this->DoRebound();
//    m_event = Simulator::ScheduleNow(&ConstantSpeedZigzagBoxMobilityModel::DoRebound, this);
  else
    m_event = Simulator::Schedule(Seconds(timeLeft), &ConstantSpeedZigzagBoxMobilityModel::DoRebound, this);

  NotifyCourseChange();
}

void
ConstantSpeedZigzagBoxMobilityModel::DoRebound ()
{
  m_speedHelper.UpdateWithBounds(m_bounds);
  auto curVel = m_speedHelper.GetVelocity();
  auto curPos = m_speedHelper.GetCurrentPosition();
  NS_ASSERT(m_bounds.IsInside(curPos));

  //first verify that it is not an corner
  if((m_bounds.xMax == curPos.x || m_bounds.xMin == curPos.x)
      && (m_bounds.yMax == curPos.y || m_bounds.yMin == curPos.y))
    {
      //180 degree turn
      curVel.x = -curVel.x;
      curVel.y = -curVel.y;
    }
  else
    {
      auto side = m_bounds.GetClosestSide(curPos);
      switch(side)
        {
        case Rectangle::RIGHT:
          curVel.x = -std::abs(curVel.x);
          break;
        case Rectangle::LEFT:
          curVel.x = std::abs(curVel.x);
          break;
        case Rectangle::TOP:
          curVel.y = -std::abs(curVel.y);
          break;
        case Rectangle::BOTTOM:
          curVel.y = std::abs(curVel.y);
          break;
        default:
          NS_ASSERT(false);
        }
    }
  m_speedHelper.SetVelocity(curVel);
  DoWalk();
}

void
ConstantSpeedZigzagBoxMobilityModel::SetVelocityFromListPositionAllocator (
    Ptr<ListPositionAllocator> allocator)
{
  if(!allocator) return;
  auto vel = allocator->GetNext();
  SetVelocity(vel);
}

} /* namespace ns3 */
