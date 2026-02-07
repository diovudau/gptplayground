#pragma once

#include <atomic>

namespace ikigui {

class GainViewModel {
 public:
  void set_gain_db(double db) noexcept { gain_db_.store(db, std::memory_order_relaxed); }
  double gain_db() const noexcept { return gain_db_.load(std::memory_order_relaxed); }

 private:
  std::atomic<double> gain_db_{0.0};
};

class GuiContext {
 public:
  bool create() noexcept {
    created_ = true;
    return true;
  }

  void destroy() noexcept { created_ = false; }
  bool show() noexcept { return created_; }
  bool hide() noexcept { return true; }
  bool is_created() const noexcept { return created_; }

 private:
  bool created_{false};
};

}  // namespace ikigui
