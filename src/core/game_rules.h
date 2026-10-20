#pragma once
#include "game.h"

namespace solitare::rules {
const std::vector<Card>* pileFor(const State& state, Location location);
std::vector<Card>* pileFor(State& state, Location location);
bool source(const State& state, Location location, const std::vector<Card>*& pile, int& index);
bool validRun(const std::vector<Card>& pile, int start, bool sameSuit);
bool exposed(const State& state, Location location);
bool canSelect(const State& state, Location location);
bool canDraw(const State& state);
bool canActivate(const State& state, Location location);
bool canMove(const State& state, Location from, Location to);
void collapseSpider(State& state);
void deal(State& state, std::uint32_t seed);
}
