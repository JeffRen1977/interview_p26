// Undirected social graph. Given -- do not modify.
//
// Friendship is symmetric and IRREFLEXIVE. friends() hands back the live
// adjacency set for speed; treat it as read-only.
#pragma once

#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

using FriendSet = std::unordered_set<std::string>;

class SocialGraph {
public:
    SocialGraph() = default;
    explicit SocialGraph(const std::vector<std::string>& users);

    void add_user(const std::string& user_id);

    // Symmetric. Auto-creates both users. Self-loops throw std::invalid_argument.
    void add_friendship(const std::string& a, const std::string& b);

    bool has_user(const std::string& user_id) const;

    // All user ids, SORTED.
    std::vector<std::string> users() const;

    // Direct friends of `user_id`; empty set for an unknown user.
    // Read-only view of internal state. Do not mutate the returned set.
    const FriendSet& friends(const std::string& user_id) const;

    std::size_t degree(const std::string& user_id) const;
    std::size_t size() const { return adj_.size(); }

private:
    std::unordered_map<std::string, FriendSet> adj_;
};
