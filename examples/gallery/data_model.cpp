#include "data_model.h"
#include <algorithm>
#include <cassert>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <stdexcept>

namespace gallery {
DataLoader::~DataLoader() {Shutdown();}
void DataLoader::SetWakeHandler(void (*handler)(void *),void *user) {
    assert(!worker_.joinable());wake_=handler;wake_user_=user;
}
std::uint64_t DataLoader::Start(int count,bool simulate_error) {
    if(count<=0 || count>100000) return 0;
    std::lock_guard<std::mutex> lock(mutex_);
    if(stopping_) return 0;
    auto generation=++generation_;
    status_={LoadState::Loading,0,generation};result_.reset();
    pending_=true;count_=count;simulate_error_=simulate_error;
    if(!worker_.joinable()) {
        try {worker_=std::thread(&DataLoader::Run,this);}
        catch(const std::exception &e) {
            pending_=false;status_.state=LoadState::Failed;
            result_=DataResult{generation,{},e.what()};
        }
    }
    condition_.notify_all();return generation;
}
void DataLoader::Cancel() {
    std::lock_guard<std::mutex> lock(mutex_);
    if(stopping_ || status_.state==LoadState::Idle || status_.state==LoadState::Cancelled) return;
    auto generation=++generation_;
    pending_=false;result_.reset();status_={LoadState::Cancelled,0,generation};condition_.notify_all();
}
void DataLoader::Shutdown() {
    {
        std::lock_guard<std::mutex> lock(mutex_);
        stopping_=true;++generation_;pending_=false;result_.reset();
        status_={LoadState::Cancelled,0,generation_.load()};
    }
    condition_.notify_all();if(worker_.joinable()) worker_.join();
}
LoadStatus DataLoader::Status() const {std::lock_guard<std::mutex> lock(mutex_);return status_;}
bool DataLoader::TakeResult(DataResult &output) {
    std::lock_guard<std::mutex> lock(mutex_);
    if(!result_ || result_->generation!=generation_) return false;
    output=std::move(*result_);result_.reset();return true;
}
void DataLoader::Run() {
    for(;;) {
        std::uint64_t generation;int count;bool fail;
        {
            std::unique_lock<std::mutex> lock(mutex_);
            condition_.wait(lock,[&]{return stopping_ || pending_;});
            if(stopping_) return;
            generation=generation_;count=count_;fail=simulate_error_;pending_=false;
        }
        DataResult result;result.generation=generation;
        try {
            // Interruptible simulated I/O. Cancellation/replacement wakes this
            // condition immediately, rather than waiting for a fixed sleep.
            for(int step=1;step<=20;++step) {
                std::unique_lock<std::mutex> lock(mutex_);
                if(condition_.wait_for(lock,std::chrono::milliseconds(15),[&]{return stopping_ || generation_!=generation;})) break;
                status_.progress=step/21.f;
                lock.unlock();if(wake_) wake_(wake_user_);
            }
            if(generation_!=generation) continue;
            if(fail) throw std::runtime_error("演示加载失败，可以重试");
            result.rows.reserve(count);
            for(int i=1;i<=count;++i) {
                if((i&127)==0 && generation_!=generation) break;
                char text[96];std::snprintf(text,sizeof(text),"Record %05d · 示例记录",i);
                result.rows.push_back({i,text});
            }
        } catch(const std::exception &e) {result.rows.clear();result.error=e.what();}
          catch(...) {result.rows.clear();result.error="未知加载错误";}
        {
            std::lock_guard<std::mutex> lock(mutex_);
            if(stopping_ || generation_!=generation) continue;
            status_.state=result.error.empty() ? LoadState::Ready : LoadState::Failed;
            status_.progress=1;result_=std::move(result);
        }
        if(wake_) wake_(wake_user_);
    }
}
void DataView::Rebuild(const std::vector<DataRow> &rows,const char *query,bool descending) {
    auto lower=[](unsigned char c){return c>='A' && c<='Z' ? c+'a'-'A' : c;};
    std::string needle=query ? query : "";
    for(auto &c:needle) c=static_cast<char>(lower(static_cast<unsigned char>(c)));
    indices_.clear();indices_.reserve(rows.size());
    for(std::size_t i=0;i<rows.size();++i)
        if(needle.empty() || std::search(rows[i].title.begin(),rows[i].title.end(),needle.begin(),needle.end(),
                [&](unsigned char a,unsigned char b){return lower(a)==b;})!=rows[i].title.end()) indices_.push_back(i);
    std::sort(indices_.begin(),indices_.end(),[&](std::size_t a,std::size_t b){return descending ? rows[a].id>rows[b].id : rows[a].id<rows[b].id;});
}
DataAnchor DataView::Anchor(const std::vector<DataRow> &rows,float scroll,float height) const {
    if(indices_.empty() || !std::isfinite(scroll) || !std::isfinite(height) || height<=0) return {};
    auto index=static_cast<std::size_t>(std::min<double>(std::floor(std::max(0.f,scroll)/height),indices_.size()-1));
    return {rows[indices_[index]].id,std::fmod(std::max(0.f,scroll),height)};
}
float DataView::Restore(const std::vector<DataRow> &rows,DataAnchor anchor,float height) const {
    if(!std::isfinite(height) || height<=0 || !std::isfinite(anchor.offset)) return 0;
    for(std::size_t i=0;i<indices_.size();++i)
        if(rows[indices_[i]].id==anchor.id) return i*height+std::clamp(anchor.offset,0.f,height);
    return 0;
}
} // namespace gallery
