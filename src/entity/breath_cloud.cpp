#include "entity/breath_cloud.hpp"

namespace mc {

BreathCloudEntity::BreathCloudEntity(const World& world,
                                     const BlockRegistry& blocks,
                                     Vec3 pos, double duration,
                                     float radius)
    : Entity(world, blocks, pos, kThickness, kThickness),
      m_duration(duration), m_radius0(radius) {}

} // namespace mc
