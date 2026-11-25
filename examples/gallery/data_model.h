#pragma once
#include <atomic>
#include <condition_variable>
#include <cstdint>
#include <mutex>
#include <optional>
#include <string>
#include <thread>
#include <vector>

namespace gallery {
struct DataRow {int id; std::string title;};
enum class LoadState {Idle,Loading,Ready,Failed,Cancelled};
struct LoadStatus {LoadState state=LoadState::Idle;float progress=0;std::uint64_t generation=0;};
struct DataResult {std::uint64_t generation=0;std::vector<DataRow> rows;std::string error;};

// Example-owned worker: no ImGui, renderer, file or network access. One lazy
// worker, bounded requests/results, cooperative cancellation and UI delivery.
class DataLoader {
  public:
    ~DataLoader();
    void SetWakeHandler(void (*handler)(void *),void *user=nullptr);
    std::uint64_t Start(int count=10000,bool simulate_error=false);
    void Cancel();
    void Shutdown();
    LoadStatus Status() const;
    bool TakeResult(DataResult &output);
  private:
    void Run();
    mutable std::mutex mutex_;
    std::condition_variable condition_;
    std::thread worker_;
    std::atomic<std::uint64_t> generation_{0};
    bool stopping_=false,pending_=false,simulate_error_=false;
    int count_=0;
    LoadStatus status_;
    std::optional<DataResult> result_;
    void (*wake_)(void *)=nullptr;
    void *wake_user_=nullptr;
};

struct DataAnchor {int id=0;float offset=0;};
class DataView {
  public:
    // Only rebuild on data/query/sort changes. ASCII-insensitive search also
    // preserves exact UTF-8 matches. Row IDs must be unique and nonzero.
    void Rebuild(const std::vector<DataRow> &rows,const char *query,bool descending);
    const std::vector<std::size_t> &Indices() const {return indices_;}
    DataAnchor Anchor(const std::vector<DataRow> &rows,float scroll,float row_height) const;
    float Restore(const std::vector<DataRow> &rows,DataAnchor anchor,float row_height) const;
  private:
    std::vector<std::size_t> indices_;
};
} // namespace gallery
