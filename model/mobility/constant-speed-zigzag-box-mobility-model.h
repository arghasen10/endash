/*
 * constant-speed-zigzag-box-mobility-model.h
 *
 *  Created on: May 29, 2020
 *      Author: abhijit
 */

#ifndef SRC_SPDASH_MODEL_MOBILITY_CONSTANT_SPEED_ZIGZAG_BOX_MOBILITY_MODEL_H_
#define SRC_SPDASH_MODEL_MOBILITY_CONSTANT_SPEED_ZIGZAG_BOX_MOBILITY_MODEL_H_

#include "ns3/nstime.h"
#include "ns3/event-id.h"
#include "ns3/mobility-model.h"
#include "ns3/constant-velocity-helper.h"
#include "ns3/rectangle.h"
#include "ns3/position-allocator.h"


namespace ns3
{

class ConstantSpeedZigzagBoxMobilityModel : public MobilityModel
{
public:
  /**
   * Register this type with the TypeId system.
   * \return the object TypeId
   */
  static TypeId GetTypeId (void);

  ConstantSpeedZigzagBoxMobilityModel ();
  virtual ~ConstantSpeedZigzagBoxMobilityModel ();
  /**
   * \param speed the new speed to set.
   *
   * Set the current speed now to (dx,dy,dz)
   * Unit is meters/s
   */
  void SetVelocity (const Vector &speed);
private:
  void DoInitializePrivate (void);
  virtual void DoDispose (void);
  virtual void DoInitialize (void);
  virtual Vector DoGetPosition (void) const;
  virtual void DoSetPosition (const Vector &position);
  virtual Vector DoGetVelocity (void) const;
  void DoWalk();
  void DoRebound();
  virtual void SetVelocityFromListPositionAllocator(Ptr<ListPositionAllocator> allocator);
  ConstantVelocityHelper m_speedHelper;  //!< helper object for this model
  Rectangle m_bounds; //!< Bounds of the area to cruise
  EventId m_event; //!< stored event ID
  Vector m_velocity;
};

} /* namespace ns3 */

#endif /* SRC_SPDASH_MODEL_MOBILITY_CONSTANT_SPEED_ZIGZAG_BOX_MOBILITY_MODEL_H_ */
