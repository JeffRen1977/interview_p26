// Staged spec for the Maze Solver task. These tests are the contract.
//
// Run against the interview skeleton (project/, starts red):
//     make test
//
// Run against the reference implementation (solution/, must be green):
//     make test IMPL=solution

#include "grid.h"
#include "maze.h"
#include "renderer.h"
#include "solver.h"

#include <chrono>
#include <cstdlib>
#include <iostream>
#include <set>
#include <stdexcept>
#include <string>
#include <vector>

namespace {

int g_ok = 0;
int g_fail = 0;
int g_error = 0;

struct AssertFail : std::runtime_error {
    using std::runtime_error::runtime_error;
};

void fail(const std::string& msg) { throw AssertFail(msg); }

void expect(bool cond, const std::string& msg) {
    if (!cond) fail(msg);
}

void expect_eq_int(long long got, long long want, const std::string& msg) {
    if (got != want) fail(msg + " got=" + std::to_string(got) + " want=" + std::to_string(want));
}

void expect_eq_str(const std::string& got, const std::string& want, const std::string& msg) {
    if (got != want) fail(msg + "\n--- got ---\n" + got + "\n--- want ---\n" + want);
}

double seconds_since(std::chrono::steady_clock::time_point start) {
    return std::chrono::duration<double>(std::chrono::steady_clock::now() - start).count();
}

std::string join(const std::vector<std::string>& lines) {
    std::string out;
    for (std::size_t i = 0; i < lines.size(); ++i) {
        if (i) out += '\n';
        out += lines[i];
    }
    return out;
}

// --- fixtures --------------------------------------------------------------

const std::string SQUARE =
    "#####\n"
    "#S..#\n"
    "#.#.#\n"
    "#..E#\n"
    "#####";

const std::string CORRIDOR =
    "#######\n"
    "#S...E#\n"
    "#######";

const std::string BLOCKED =
    "#####\n"
    "#S#E#\n"
    "#####";

const std::string TWO_KEYS =
    "########\n"
    "#S.....#\n"
    "#.####.#\n"
    "#a....b#\n"
    "#.####.#\n"
    "#..A..E#\n"
    "########";

const std::string GATE_ON_THE_ONLY_ROUTE =
    "#######\n"
    "#S.A.E#\n"
    "#.#####\n"
    "#a....#\n"
    "#######";

const std::string GATE_WITHOUT_ITS_KEY =
    "#######\n"
    "#S.A.E#\n"
    "#.#####\n"
    "#.....#\n"
    "#######";

//: Straight line through rough terrain vs a longer detour on plain floor.
const std::string ROUGH_DETOUR =
    "#######\n"
    "#S~~~E#\n"
    "#.....#\n"
    "#######";

//: One interior wall on the only route.
const std::string ONE_WALL =
    "#######\n"
    "#S.#.E#\n"
    "#######";

//: Two interior walls: one bomb is not enough.
const std::string TWO_WALLS =
    "#######\n"
    "#S#.#E#\n"
    "#######";

//: Detour is 12 energy, blasting through costs 11 -- the bomb is worth it.
const std::string BOMB_BEATS_DETOUR =
    "###########\n"
    "#S...#...E#\n"
    "#.#######.#\n"
    "#.........#\n"
    "###########";

//: Same shape, short detour (6) beats the blast (7) -- the bomb must stay unused.
const std::string DETOUR_BEATS_BOMB =
    "#######\n"
    "#S.#.E#\n"
    "#.....#\n"
    "#######";

// An open size x size room with a wall border, S top-left, E bottom-right.
Maze big_maze(int size = 81, bool with_keys = true) {
    std::vector<std::string> rows(static_cast<std::size_t>(size), std::string(static_cast<std::size_t>(size), '#'));
    for (int r = 1; r < size - 1; ++r)
        for (int c = 1; c < size - 1; ++c) rows[static_cast<std::size_t>(r)][static_cast<std::size_t>(c)] = '.';
    rows[1][1] = 'S';
    rows[static_cast<std::size_t>(size - 2)][static_cast<std::size_t>(size - 2)] = 'E';
    if (with_keys) {
        rows[1][static_cast<std::size_t>(size - 2)] = 'a';
        rows[static_cast<std::size_t>(size - 2)][1] = 'b';
        rows[static_cast<std::size_t>(size / 2)][1] = 'c';
        rows[static_cast<std::size_t>(size / 2)][static_cast<std::size_t>(size - 2)] = 'd';
    }
    return Maze(rows);
}

// --- shared assertions -----------------------------------------------------

void assert_valid_walk(const Maze& maze, const std::optional<std::vector<Coord>>& maybe_path,
                       bool require_all_keys = false) {
    expect(maybe_path.has_value(), "expected a path, got nullopt");
    const std::vector<Coord>& path = *maybe_path;
    expect(!path.empty(), "empty path");
    expect(path.front() == maze.start, "path does not start at S");
    expect(path.back() == maze.end, "path does not end at E");

    int mask = 0;
    for (std::size_t i = 0; i < path.size(); ++i) {
        const Coord& cell = path[i];
        expect(maze.in_bounds(cell), cell.str() + " out of bounds");
        expect(!maze.is_wall(cell), "walked into a wall at " + cell.str());
        const char ch = maze.at(cell);
        if (is_gate(ch)) {
            expect((mask & (1 << key_bit(ch))) != 0,
                   std::string("passed gate ") + ch + " without its key");
        }
        if (is_key(ch)) mask |= 1 << key_bit(ch);
        if (i) {
            const Coord& prev = path[i - 1];
            expect(std::abs(prev.row - cell.row) + std::abs(prev.col - cell.col) == 1,
                   "non-adjacent step " + prev.str() + " -> " + cell.str());
        }
    }
    if (require_all_keys) {
        expect_eq_int(mask, maze.all_keys_mask(), "did not collect every key");
    }
}

// Same invariants, plus the energy accounting.
//
// A wall cell is allowed in the path only if it was bombable and the budget
// covers every blast; the reported energy must equal the sum of the step costs
// the walk actually pays.
void assert_valid_blast_walk(const Maze& maze,
                             const std::optional<std::pair<int, std::vector<Coord>>>& result,
                             int bombs) {
    expect(result.has_value(), "expected (energy, path), got nullopt");
    const auto& [energy, path] = *result;
    expect(path.front() == maze.start, "path does not start at S");
    expect(path.back() == maze.end, "path does not end at E");

    int mask = 0;
    int spent = 0;
    int blasts = 0;
    for (std::size_t i = 0; i < path.size(); ++i) {
        const Coord& cell = path[i];
        expect(maze.in_bounds(cell), cell.str() + " out of bounds");
        const char ch = maze.at(cell);
        if (i) {
            const Coord& prev = path[i - 1];
            expect(std::abs(prev.row - cell.row) + std::abs(prev.col - cell.col) == 1,
                   "non-adjacent step " + prev.str() + " -> " + cell.str());
            if (maze.is_wall(cell)) {
                expect(maze.is_bombable(cell), "blew up bedrock at " + cell.str());
                ++blasts;
                spent += kBombCost;
            } else {
                if (is_gate(ch)) {
                    expect((mask & (1 << key_bit(ch))) != 0,
                           std::string("passed gate ") + ch + " without its key");
                }
                spent += maze.terrain_cost(cell);
            }
        }
        if (is_key(ch)) mask |= 1 << key_bit(ch);
    }
    expect(blasts <= bombs, "used " + std::to_string(blasts) + " bombs, budget was " + std::to_string(bombs));
    expect_eq_int(spent, energy, "reported energy does not match the walk");
    expect_eq_int(mask, maze.all_keys_mask(), "did not collect every key");
}

// ---------------------------------------------------------------------------
// Phase 1 -- rendering offset and the runaway DFS
// ---------------------------------------------------------------------------
void test_render_without_a_path_is_the_original_drawing() {
    expect_eq_str(render(Maze::from_text(SQUARE)), SQUARE, "no path");
}

void test_render_marks_the_path() {
    const Maze maze = Maze::from_text(SQUARE);
    const std::vector<Coord> path{{1, 1}, {2, 1}, {3, 1}, {3, 2}, {3, 3}};
    expect_eq_str(render(maze, path), join({"#####", "#S..#", "#*#.#", "#**E#", "#####"}),
                  "path overlay");
}

void test_render_on_a_non_square_maze() {
    // Rows and columns are not interchangeable; a 3x7 maze proves it.
    const Maze maze = Maze::from_text(CORRIDOR);
    std::vector<Coord> path;
    for (int c = 1; c <= 5; ++c) path.emplace_back(1, c);
    expect_eq_str(render(maze, path), join({"#######", "#S***E#", "#######"}), "3x7 overlay");
}

void test_render_never_hides_start_end_keys_or_gates() {
    const Maze maze = Maze::from_text(TWO_KEYS);
    const std::vector<Coord> path{{1, 1}, {2, 1}, {3, 1}, {3, 2}};
    const std::string drawn = render(maze, path);
    std::vector<std::string> lines;
    std::string current;
    for (char ch : drawn) {
        if (ch == '\n') { lines.push_back(current); current.clear(); }
        else current += ch;
    }
    lines.push_back(current);
    expect(lines[1][1] == 'S', "S was overwritten");
    expect(lines[3][1] == 'a', "key 'a' was overwritten");
    expect(lines[5][6] == 'E', "E was overwritten");
    expect(lines[5][3] == 'A', "gate 'A' was overwritten");
    expect(lines[3][2] == '*', "the path was not drawn");
}

void test_dfs_reachable_terminates_on_a_loop_free_answer() {
    const Maze maze = Maze::from_text(SQUARE);
    expect(dfs_reachable(maze, maze.start, maze.end), "S and E are connected");
}

void test_dfs_reachable_returns_false_instead_of_recursing_forever() {
    const Maze maze = Maze::from_text(BLOCKED);
    expect(!dfs_reachable(maze, maze.start, maze.end), "E is walled off");
}

void test_dfs_start_equals_goal() {
    const Maze maze = Maze::from_text(SQUARE);
    expect(dfs_reachable(maze, maze.start, maze.start), "start == goal");
}

// ---------------------------------------------------------------------------
// Phase 2 -- shortest path, not just any path
// ---------------------------------------------------------------------------
void test_shortest_path_length_on_a_branching_maze() {
    const Maze maze = Maze::from_text(SQUARE);
    const auto path = shortest_path(maze);
    assert_valid_walk(maze, path);
    expect_eq_int(static_cast<long long>(path->size()), 5, "path length");
}

void test_shortest_path_in_a_corridor() {
    const Maze maze = Maze::from_text(CORRIDOR);
    const auto path = shortest_path(maze);
    assert_valid_walk(maze, path);
    expect_eq_int(static_cast<long long>(path->size()), 5, "path length");
}

void test_unreachable_end_returns_nullopt() {
    expect(!shortest_path(Maze::from_text(BLOCKED)).has_value(), "should be nullopt");
}

void test_start_equals_end() {
    Maze maze = Maze::from_text(SQUARE);
    maze.end = maze.start;
    const auto path = shortest_path(maze);
    expect(path.has_value() && path->size() == 1 && path->front() == maze.start, "{start}");
}

void test_shortest_path_treats_a_closed_gate_as_a_wall() {
    // No keys are collected by this function, so the gate never opens.
    expect(!shortest_path(Maze::from_text(GATE_ON_THE_ONLY_ROUTE)).has_value(), "gate is a wall");
}

void test_rendered_shortest_path() {
    const Maze maze = Maze::from_text(SQUARE);
    expect_eq_str(render(maze, *shortest_path(maze)),
                  join({"#####", "#S..#", "#*#.#", "#**E#", "#####"}), "rendered route");
}

void test_shortest_path_beats_any_depth_first_walk() {
    const Maze maze = big_maze(21, false);
    const auto path = shortest_path(maze);
    assert_valid_walk(maze, path);
    // Manhattan distance between (1,1) and (19,19) on an open grid
    expect_eq_int(static_cast<long long>(path->size()), 37, "path length");
}

// ---------------------------------------------------------------------------
// Phase 3 -- mandatory checkpoints, gates and scale
// ---------------------------------------------------------------------------
void test_collects_every_key_on_the_shortest_route() {
    const Maze maze = Maze::from_text(TWO_KEYS);
    const auto path = shortest_path_all_keys(maze);
    assert_valid_walk(maze, path, true);
    expect_eq_int(static_cast<long long>(path->size()), 10, "path length");
}

void test_gate_forces_a_detour_and_the_route_revisits_cells() {
    // The only way out is: fetch key 'a', walk back, then open gate 'A'.
    // Cells are revisited with a different key set, so `visited` keyed by cell
    // alone would prune the only legal route.
    const Maze maze = Maze::from_text(GATE_ON_THE_ONLY_ROUTE);
    const auto path = shortest_path_all_keys(maze);
    assert_valid_walk(maze, path, true);
    expect_eq_int(static_cast<long long>(path->size()), 9, "path length");
    const std::set<Coord> unique(path->begin(), path->end());
    expect(unique.size() < path->size(), "expected the route to revisit cells");
}

void test_nullopt_when_a_gate_can_never_be_opened() {
    expect(!shortest_path_all_keys(Maze::from_text(GATE_WITHOUT_ITS_KEY)).has_value(), "no key");
}

void test_nullopt_when_the_end_is_walled_off() {
    expect(!shortest_path_all_keys(Maze::from_text(BLOCKED)).has_value(), "walled off");
}

void test_without_keys_it_degenerates_to_plain_bfs() {
    const Maze maze = Maze::from_text(SQUARE);
    expect(shortest_path_all_keys(maze) == shortest_path(maze), "must match plain BFS");
}

void test_start_equals_end_with_no_keys() {
    Maze maze = Maze::from_text(SQUARE);
    maze.end = maze.start;
    const auto path = shortest_path_all_keys(maze);
    expect(path.has_value() && path->size() == 1 && path->front() == maze.start, "{start}");
}

void test_large_grid_with_four_keys_under_time_budget() {
    const Maze maze = big_maze(81);
    const auto start = std::chrono::steady_clock::now();
    const auto path = shortest_path_all_keys(maze);
    const double elapsed = seconds_since(start);
    assert_valid_walk(maze, path, true);
    expect(elapsed < 3.0, "bitmask BFS took " + std::to_string(elapsed) + "s on 81x81 with 4 keys");
}

// ---------------------------------------------------------------------------
// Phase 4 -- weighted steps and a bomb budget
// ---------------------------------------------------------------------------
void test_prefers_the_long_cheap_detour_over_rough_terrain() {
    // BFS would take the 5-cell line through '~'; Dijkstra pays 6 for 7 cells.
    const Maze maze = Maze::from_text(ROUGH_DETOUR);
    const auto result = min_energy_path(maze, 0);
    assert_valid_blast_walk(maze, result, 0);
    expect_eq_int(result->first, 6, "energy");
    expect_eq_int(static_cast<long long>(result->second.size()), 7, "cells walked");
    expect_eq_int(static_cast<long long>(shortest_path(maze)->size()), 5, "the BFS answer costs 16");
}

void test_degenerates_to_bfs_when_every_step_costs_one() {
    const Maze maze = Maze::from_text(SQUARE);
    const auto result = min_energy_path(maze, 0);
    assert_valid_blast_walk(maze, result, 0);
    expect_eq_int(result->first, static_cast<long long>(shortest_path(maze)->size()) - 1,
                  "energy must equal step count");
}

void test_keys_are_still_mandatory_and_gates_still_need_them() {
    const Maze maze = Maze::from_text(GATE_ON_THE_ONLY_ROUTE);
    const auto result = min_energy_path(maze, 0);
    assert_valid_blast_walk(maze, result, 0);
    expect_eq_int(result->first, static_cast<long long>(shortest_path_all_keys(maze)->size()) - 1,
                  "energy");
    const std::set<Coord> unique(result->second.begin(), result->second.end());
    expect(unique.size() < result->second.size(), "expected the route to revisit cells");
}

void test_no_bombs_means_a_blocked_route_is_nullopt() {
    expect(!min_energy_path(Maze::from_text(ONE_WALL), 0).has_value(), "no bombs, no route");
}

void test_one_bomb_opens_the_only_route() {
    const Maze maze = Maze::from_text(ONE_WALL);
    const auto result = min_energy_path(maze, 1);
    assert_valid_blast_walk(maze, result, 1);
    expect_eq_int(result->first, 1 + kBombCost + 1 + 1, "energy");
}

void test_bomb_budget_is_enforced_across_the_whole_walk() {
    expect(!min_energy_path(Maze::from_text(TWO_WALLS), 1).has_value(), "one bomb is not enough");
    const Maze maze = Maze::from_text(TWO_WALLS);
    const auto result = min_energy_path(maze, 2);
    assert_valid_blast_walk(maze, result, 2);
    expect_eq_int(result->first, 2 * kBombCost + 2, "energy");
}

void test_blasts_when_the_detour_is_more_expensive() {
    const Maze maze = Maze::from_text(BOMB_BEATS_DETOUR);
    expect_eq_int(min_energy_path(maze, 0)->first, 12, "detour costs 12");
    const auto result = min_energy_path(maze, 1);
    assert_valid_blast_walk(maze, result, 1);
    expect_eq_int(result->first, 11, "blasting costs 11");
    const std::set<Coord> cells(result->second.begin(), result->second.end());
    expect(cells.count(Coord(1, 5)) > 0, "expected the walk to go through the wall");
}

void test_keeps_the_bomb_when_walking_around_is_cheaper() {
    const Maze maze = Maze::from_text(DETOUR_BEATS_BOMB);
    const auto result = min_energy_path(maze, 1);
    assert_valid_blast_walk(maze, result, 1);
    expect_eq_int(result->first, 6, "the detour is cheaper");
    const std::set<Coord> cells(result->second.begin(), result->second.end());
    expect(cells.count(Coord(1, 3)) == 0, "the wall was cheaper to walk around");
}

void test_the_outer_border_is_bedrock() {
    // No budget lets the walk tunnel out through the frame and cut a corner.
    const Maze maze = Maze::from_text(ONE_WALL);
    for (int col = 0; col < maze.cols(); ++col) {
        expect(!maze.is_bombable(Coord(0, col)), "top frame must be bedrock");
        expect(!maze.is_bombable(Coord(maze.rows() - 1, col)), "bottom frame must be bedrock");
    }
    const auto result = min_energy_path(maze, 5);
    assert_valid_blast_walk(maze, result, 5);
    expect_eq_int(result->first, 1 + kBombCost + 1 + 1, "energy");
    for (const Coord& cell : result->second) {
        expect(cell.row > 0 && cell.row < maze.rows() - 1 && cell.col > 0 && cell.col < maze.cols() - 1,
               "walk stepped onto the frame at " + cell.str());
    }
}

void test_negative_bomb_budget_is_rejected() {
    bool threw = false;
    try {
        min_energy_path(Maze::from_text(SQUARE), -1);
    } catch (const std::invalid_argument&) {
        threw = true;
    }
    expect(threw, "bombs < 0 must throw std::invalid_argument");
}

void test_large_grid_with_keys_and_bombs_under_time_budget() {
    // 61x61 x 2^4 key sets x 2 bomb states -- a heap keeps it comfortable.
    const Maze maze = big_maze(61);
    const auto start = std::chrono::steady_clock::now();
    const auto result = min_energy_path(maze, 1);
    const double elapsed = seconds_since(start);
    assert_valid_blast_walk(maze, result, 1);
    expect_eq_int(result->first, static_cast<long long>(shortest_path_all_keys(maze)->size()) - 1,
                  "all edges cost 1 here, so Dijkstra must match BFS");
    expect(elapsed < 5.0, "Dijkstra took " + std::to_string(elapsed) + "s on 61x61 with 4 keys");
}

void run(const std::string& name, void (*fn)()) {
    try {
        fn();
        ++g_ok;
        std::cout << name << " ... ok\n";
    } catch (const AssertFail& e) {
        ++g_fail;
        std::cout << name << " ... FAIL\n  " << e.what() << "\n";
    } catch (const std::exception& e) {
        ++g_error;
        std::cout << name << " ... ERROR\n  " << e.what() << "\n";
    }
}

}  // namespace

int main() {
    std::cout << "Phase1RenderAndDfs\n";
    run("test_render_without_a_path_is_the_original_drawing", test_render_without_a_path_is_the_original_drawing);
    run("test_render_marks_the_path", test_render_marks_the_path);
    run("test_render_on_a_non_square_maze", test_render_on_a_non_square_maze);
    run("test_render_never_hides_start_end_keys_or_gates", test_render_never_hides_start_end_keys_or_gates);
    run("test_dfs_reachable_terminates_on_a_loop_free_answer", test_dfs_reachable_terminates_on_a_loop_free_answer);
    run("test_dfs_reachable_returns_false_instead_of_recursing_forever", test_dfs_reachable_returns_false_instead_of_recursing_forever);
    run("test_dfs_start_equals_goal", test_dfs_start_equals_goal);

    std::cout << "\nPhase2ShortestPath\n";
    run("test_shortest_path_length_on_a_branching_maze", test_shortest_path_length_on_a_branching_maze);
    run("test_shortest_path_in_a_corridor", test_shortest_path_in_a_corridor);
    run("test_unreachable_end_returns_nullopt", test_unreachable_end_returns_nullopt);
    run("test_start_equals_end", test_start_equals_end);
    run("test_shortest_path_treats_a_closed_gate_as_a_wall", test_shortest_path_treats_a_closed_gate_as_a_wall);
    run("test_rendered_shortest_path", test_rendered_shortest_path);
    run("test_shortest_path_beats_any_depth_first_walk", test_shortest_path_beats_any_depth_first_walk);

    std::cout << "\nPhase3KeysAndScale\n";
    run("test_collects_every_key_on_the_shortest_route", test_collects_every_key_on_the_shortest_route);
    run("test_gate_forces_a_detour_and_the_route_revisits_cells", test_gate_forces_a_detour_and_the_route_revisits_cells);
    run("test_nullopt_when_a_gate_can_never_be_opened", test_nullopt_when_a_gate_can_never_be_opened);
    run("test_nullopt_when_the_end_is_walled_off", test_nullopt_when_the_end_is_walled_off);
    run("test_without_keys_it_degenerates_to_plain_bfs", test_without_keys_it_degenerates_to_plain_bfs);
    run("test_start_equals_end_with_no_keys", test_start_equals_end_with_no_keys);
    run("test_large_grid_with_four_keys_under_time_budget", test_large_grid_with_four_keys_under_time_budget);

    std::cout << "\nPhase4EnergyAndBombs\n";
    run("test_prefers_the_long_cheap_detour_over_rough_terrain", test_prefers_the_long_cheap_detour_over_rough_terrain);
    run("test_degenerates_to_bfs_when_every_step_costs_one", test_degenerates_to_bfs_when_every_step_costs_one);
    run("test_keys_are_still_mandatory_and_gates_still_need_them", test_keys_are_still_mandatory_and_gates_still_need_them);
    run("test_no_bombs_means_a_blocked_route_is_nullopt", test_no_bombs_means_a_blocked_route_is_nullopt);
    run("test_one_bomb_opens_the_only_route", test_one_bomb_opens_the_only_route);
    run("test_bomb_budget_is_enforced_across_the_whole_walk", test_bomb_budget_is_enforced_across_the_whole_walk);
    run("test_blasts_when_the_detour_is_more_expensive", test_blasts_when_the_detour_is_more_expensive);
    run("test_keeps_the_bomb_when_walking_around_is_cheaper", test_keeps_the_bomb_when_walking_around_is_cheaper);
    run("test_the_outer_border_is_bedrock", test_the_outer_border_is_bedrock);
    run("test_negative_bomb_budget_is_rejected", test_negative_bomb_budget_is_rejected);
    run("test_large_grid_with_keys_and_bombs_under_time_budget", test_large_grid_with_keys_and_bombs_under_time_budget);

    const int total = g_ok + g_fail + g_error;
    std::cout << "\n----------------------------------------------------------------------\n";
    std::cout << "Ran " << total << " tests: " << g_ok << " ok, " << g_fail << " FAIL, " << g_error
              << " ERROR\n";
    return (g_fail || g_error) ? 1 : 0;
}
