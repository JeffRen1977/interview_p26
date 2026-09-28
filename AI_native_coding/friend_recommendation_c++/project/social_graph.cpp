#include "social_graph.h"

#include <algorithm>
#include <stdexcept>

namespace {
const FriendSet kEmpty;
}  // namespace

SocialGraph::SocialGraph(const std::vector<std::string>& users) {
    for (const std::string& user : users) add_user(user);
}

void SocialGraph::add_user(const std::string& user_id) { adj_.try_emplace(user_id); }

void SocialGraph::add_friendship(const std::string& a, const std::string& b) {
    if (a == b) throw std::invalid_argument(a + " cannot befriend itself");
    add_user(a);
    add_user(b);
    adj_[a].insert(b);
    adj_[b].insert(a);
}

bool SocialGraph::has_user(const std::string& user_id) const {
    return adj_.find(user_id) != adj_.end();
}

std::vector<std::string> SocialGraph::users() const {
    std::vector<std::string> out;
    out.reserve(adj_.size());
    for (const auto& [user, _] : adj_) out.push_back(user);
    std::sort(out.begin(), out.end());
    return out;
}

const FriendSet& SocialGraph::friends(const std::string& user_id) const {
    const auto it = adj_.find(user_id);
    return it == adj_.end() ? kEmpty : it->second;
}

std::size_t SocialGraph::degree(const std::string& user_id) const {
    return friends(user_id).size();
}
