#pragma once

#include <functional>
#include <cstdint>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

namespace skui {

struct Route {
    std::string path;
    std::string parameter;
};

// Navigation is committed after Draw(), so a click cannot replace a page halfway
// through its frame. Keep application/page data in the caller's own state.
class Router {
  public:
    using Page = std::function<void(const Route &)>;
    enum class Navigation { Forward, Back, Reset };

    bool Register(std::string path, Page page) {
        if (path.empty() || !page)
            return false;
        return pages_.emplace(std::move(path), std::move(page)).second;
    }
    bool Push(std::string path, std::string parameter = {}) {
        return Submit(Action::Push, {std::move(path), std::move(parameter)});
    }
    bool Replace(std::string path, std::string parameter = {}) {
        return Submit(Action::Replace, {std::move(path), std::move(parameter)});
    }
    bool Reset(std::string path) { return Submit(Action::Reset, {std::move(path), {}}); }
    bool Back() {
        if (!CanGoBack())
            return false;
        return Submit(Action::Back, {});
    }
    bool CanGoBack() const { return stack_.size() > 1; }
    std::size_t Depth() const { return stack_.size(); }
    const Route *Current() const { return stack_.empty() ? nullptr : &stack_.back(); }
    std::uint64_t Revision() const { return revision_; }
    Navigation LastNavigation() const { return navigation_; }
    void Draw() { DrawBefore({}); }

  private:
    friend class RouterView;
    // A view header shares the page's deferred-navigation boundary.
    void DrawBefore(const Page &before) {
        if (drawing_ || stack_.empty())
            return;
        const Route route = stack_.back();
        auto page = pages_.at(route.path);
        drawing_ = true;
        try {
            if(before) before(route);
            page(route);
        } catch (...) {
            drawing_ = false;
            pending_.clear();
            throw;
        }
        drawing_ = false;
        auto operations = std::move(pending_);
        pending_.clear();
        for (auto &operation : operations)
            Apply(operation.first, std::move(operation.second));
    }

  private:
    enum class Action { Push, Replace, Reset, Back };
    bool Submit(Action action, Route route) {
        if (action != Action::Back && pages_.find(route.path) == pages_.end())
            return false;
        if (drawing_)
            pending_.emplace_back(action, std::move(route));
        else
            Apply(action, std::move(route));
        return true;
    }
    void Apply(Action action, Route route) {
        const Route previous = stack_.empty() ? Route{} : stack_.back();
        const auto depth = stack_.size();
        switch (action) {
        case Action::Push:
            stack_.push_back(std::move(route));
            break;
        case Action::Replace:
            if (stack_.empty())
                stack_.push_back(std::move(route));
            else
                stack_.back() = std::move(route);
            break;
        case Action::Reset:
            stack_ = {std::move(route)};
            break;
        case Action::Back:
            if (stack_.size() > 1)
                stack_.pop_back();
            break;
        }
        const auto &current = stack_.back();
        if (depth != stack_.size() || previous.path != current.path || previous.parameter != current.parameter) {
            ++revision_;
            navigation_ = action == Action::Back ? Navigation::Back :
                          action == Action::Reset ? Navigation::Reset : Navigation::Forward;
        }
    }
    std::unordered_map<std::string, Page> pages_;
    std::vector<Route> stack_;
    std::vector<std::pair<Action, Route>> pending_;
    bool drawing_ = false;
    std::uint64_t revision_ = 0;
    Navigation navigation_ = Navigation::Reset;
};

} // namespace skui
