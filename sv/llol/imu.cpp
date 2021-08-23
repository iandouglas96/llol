#include "sv/llol/imu.h"

#include <glog/logging.h>

namespace sv {

using Vector3d = Eigen::Vector3d;

static const double kEps = Sophus::Constants<double>::epsilon();

Sophus::SO3d IntegrateRot(const Sophus::SO3d& R0,
                          const Vector3d& omg,
                          double dt) {
  CHECK_GT(dt, 0);
  return R0 * Sophus::SO3d::exp(dt * omg);
}

NavState IntegrateEuler(const NavState& s0,
                        const ImuData& imu,
                        const Vector3d& g_w,
                        double dt) {
  CHECK_GT(dt, 0);
  NavState s1 = s0;
  // t
  s1.time = s0.time + dt;

  // gyr
  s1.rot = IntegrateRot(s0.rot, imu.gyr, dt);

  // acc
  // transform to worl frame
  const Vector3d a = s0.rot * imu.acc + g_w;
  s1.vel = s0.vel + a * dt;
  s1.pos = s0.pos + s0.vel * dt + 0.5 * a * dt * dt;

  return s1;
}

NavState IntegrateMidpoint(const NavState& s0,
                           const ImuData& imu0,
                           const ImuData& imu1,
                           const Vector3d& g_w) {
  NavState s1 = s0;
  const auto dt = imu1.time - imu0.time;
  CHECK_GT(dt, 0);

  // t
  s1.time = s0.time + dt;

  // gyro
  const auto omg_b = (imu0.gyr + imu1.gyr) * 0.5;
  s1.rot = IntegrateRot(s0.rot, omg_b, dt);

  // acc
  const Vector3d a0 = s0.rot * imu0.acc;
  const Vector3d a1 = s1.rot * imu1.acc;
  const Vector3d a = (a0 + a1) * 0.5 + g_w;
  s1.vel = s0.vel + a * dt;
  s1.pos = s0.pos + s0.vel * dt + 0.5 * a * dt * dt;

  return s1;
}

cv::Range GetImusFromBuffer(const ImuBuffer& buffer, double t0, double t1) {
  cv::Range range{-1, -1};

  if (t0 >= t1) {
    LOG(WARNING) << fmt::format("Time span is empty ({},{})", t0, t1);
    return range;
  }

  for (int i = 0; i < buffer.size(); ++i) {
    const auto& imu = buffer[i];
    // The first one after t0
    if (range.start < 0 && imu.time > t0) {
      range.start = i;
    }

    // The first one before t1
    if (range.end < 0 && imu.time > t1) {
      range.end = i;
    }
  }

  // If for some reason we don't have any imu that is later than the second time
  // just use thet last one
  if (range.start >= 0 && range.end == -1) {
    range.end = buffer.size();
  }

  return range;
}

}  // namespace sv
