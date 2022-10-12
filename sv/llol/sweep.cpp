#include "sv/llol/sweep.h"

#include <glog/logging.h>
#include <tbb/parallel_for.h>

#include <opencv2/core.hpp>

#include "sv/util/ocv.h"

namespace sv {

int LidarSweep::Add(const LidarScan& scan) {
  CHECK_EQ(scan.type(), type());
  CHECK_EQ(scan.rows(), rows());
  CHECK_LE(scan.cols(), cols());

  UpdateTime(scan.time, scan.dt);
  UpdateView(scan.curr);
  scale = scan.scale;

  // copy to storage
  scan.mat.copyTo(mat.colRange(curr));  // x,y,w,h
  return cv::countNonZero(ExtractRange());
}

void LidarSweep::Interp(const Trajectory& traj, int gsize, bool motion_comp) {
  const int num_cells = traj.size() - 1;
  const int cell_width = cols() / num_cells;
  const auto grid_end = curr.end / cell_width;
  gsize = gsize <= 0 ? num_cells : gsize;

  tbb::parallel_for(
      tbb::blocked_range<int>(0, num_cells, gsize), [&](const auto& blk) {
        for (int gc = blk.begin(); gc < blk.end(); ++gc) {
          // Note that the starting point of traj is where curr
          // ends, so we need to offset by curr.end to find the
          // corresponding traj segment
          const int tc = WrapCols(gc - grid_end, num_cells);
          const auto& st0 = motion_comp ? traj.At(tc) : traj.At(0);
          const auto& st1 = motion_comp ? traj.At(tc + 1) : traj.At(0);

          const auto dr = (st0.rot.inverse() * st1.rot).log();
          const auto dp = (st1.pos - st0.pos).eval();

          for (int j = 0; j < cell_width; ++j) {
            // which column
            const int col = gc * cell_width + j;
            const float s = static_cast<float>(j) / cell_width;
            Sophus::SE3d tf_p_i;
            tf_p_i.so3() = st0.rot * Sophus::SO3d::exp(s * dr);
            tf_p_i.translation() = st0.pos + s * dp;
            tfs.at(col) = (tf_p_i * traj.T_imu_lidar).cast<float>();
          }
        }
      });
}

std::string LidarSweep::Repr() const {
  return fmt::format("LidarSweep(t0={}, dt={}, xyzr={}, col_range={})",
                     time,
                     dt,
                     sv::Repr(mat),
                     sv::Repr(curr));
}

LidarSweep MakeTestSweep(const cv::Size& size) {
  LidarSweep sweep(size);
  LidarScan scan = MakeTestScan(size);
  sweep.Add(scan);
  return sweep;
}

}  // namespace sv
