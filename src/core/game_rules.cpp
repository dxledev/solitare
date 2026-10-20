#include "game_rules.h"
#include <algorithm>
#include <cmath>

namespace solitare::rules {
namespace {
bool samePile(Location first, Location second) {
    return first.kind == second.kind && first.pile == second.pile;
}

bool foundationMove(const State& state, Location from, Location to, const Card& moving, int count) {
    if (from.kind == Location::Foundation || count != 1 || state.mode == GameMode::Spider) return false;
    const auto& target = *pileFor(state, to);
    return target.empty() ? moving.rank == 1 : moving.suit == target.back().suit && moving.rank == target.back().rank + 1;
}

int transferCapacity(const State& state, bool emptyDestination) {
    const auto free = std::count_if(state.cells.begin(), state.cells.end(), [](const auto& pile) { return pile.empty(); });
    int empty = static_cast<int>(std::count_if(state.tableau.begin(), state.tableau.end(), [](const auto& pile) { return pile.empty(); }));
    if (emptyDestination) --empty;
    return static_cast<int>(free + 1) * (1 << std::max(0, empty));
}

void flip(std::vector<Card>& pile) {
    if (!pile.empty()) pile.back().faceUp = true;
}
}

const std::vector<Card>* pileFor(const State& state, Location location) {
    if (location.pile < 0) return nullptr;
    switch (location.kind) {
    case Location::Stock: return location.pile == 0 ? &state.stock : nullptr;
    case Location::Waste: return location.pile == 0 ? &state.waste : nullptr;
    case Location::Foundation: return location.pile < static_cast<int>(state.foundations.size()) ? &state.foundations[location.pile] : nullptr;
    case Location::Tableau: return location.pile < static_cast<int>(state.tableau.size()) ? &state.tableau[location.pile] : nullptr;
    case Location::Cell: return location.pile < 4 ? &state.cells[location.pile] : nullptr;
    }
    return nullptr;
}

std::vector<Card>* pileFor(State& state, Location location) {
    return const_cast<std::vector<Card>*>(pileFor(static_cast<const State&>(state), location));
}

bool source(const State& state, Location location, const std::vector<Card>*& pile, int& index) {
    if (location.index < -1 || location.kind == Location::Stock) return false;
    pile = pileFor(state, location);
    if (!pile || pile->empty()) return false;
    index = location.index == -1 ? static_cast<int>(pile->size()) - 1 : location.index;
    if (index < 0 || index >= static_cast<int>(pile->size())) return false;
    return location.kind == Location::Tableau || index == static_cast<int>(pile->size()) - 1;
}

bool validRun(const std::vector<Card>& pile, int start, bool sameSuit) {
    for (int index = start; index < static_cast<int>(pile.size()); ++index) {
        const auto& card = pile[index];
        if (!card.faceUp) return false;
        if (index == start) continue;
        const auto& previous = pile[index - 1];
        if (previous.rank != card.rank + 1 || (sameSuit ? previous.suit != card.suit : previous.red() == card.red())) return false;
    }
    return true;
}

bool exposed(const State& state, Location location) {
    const std::vector<Card>* pile = nullptr;
    int index = -1;
    if (!source(state, location, pile, index) || !(*pile)[index].faceUp) return false;
    if (location.kind != Location::Tableau || (state.mode != GameMode::Pyramid && state.mode != GameMode::TriPeaks)) return index == static_cast<int>(pile->size()) - 1;
    const int slot = location.pile;
    if (state.mode == GameMode::Pyramid) {
        int row = 0;
        while ((row + 1) * (row + 2) / 2 <= slot) ++row;
        if (row == 6) return true;
        const int child = (row + 1) * (row + 2) / 2 + slot - row * (row + 1) / 2;
        return state.tableau[child].empty() && state.tableau[child + 1].empty();
    }
    static constexpr std::pair<int, int> children[] = {{3,4},{5,6},{7,8},{9,10},{10,11},{12,13},{13,14},{15,16},{16,17},{18,19},{19,20},{20,21},{21,22},{22,23},{23,24},{24,25},{25,26},{26,27}};
    return slot >= 18 || (state.tableau[children[slot].first].empty() && state.tableau[children[slot].second].empty());
}

bool canSelect(const State& state, Location location) {
    const std::vector<Card>* pile = nullptr;
    int index = -1;
    if (!source(state, location, pile, index)) return false;
    if (state.mode == GameMode::Pyramid) return (location.kind == Location::Tableau || location.kind == Location::Waste) && exposed(state, location);
    if (state.mode == GameMode::TriPeaks) return location.kind == Location::Tableau && exposed(state, location);
    if (state.mode == GameMode::Spider && location.kind != Location::Tableau) return false;
    if (state.mode != GameMode::FreeCell && location.kind == Location::Cell) return false;
    if (state.mode == GameMode::FreeCell && location.kind == Location::Waste) return false;
    return validRun(*pile, index, state.mode == GameMode::Spider);
}

bool canDraw(const State& state) {
    if (state.mode == GameMode::FreeCell) return false;
    if (state.mode == GameMode::Spider) return state.stock.size() >= 10 && std::all_of(state.tableau.begin(), state.tableau.end(), [](const auto& pile) { return !pile.empty(); });
    if (!state.stock.empty()) return true;
    if (state.mode == GameMode::TriPeaks || state.waste.empty()) return false;
    return state.mode == GameMode::Klondike || state.redeals < state.options.pyramidRedeals;
}

bool canActivate(const State& state, Location location) {
    if (!canSelect(state, location)) return false;
    const auto& card = pileFor(state, location)->back();
    if (state.mode == GameMode::Pyramid) return card.rank == 13;
    if (state.mode != GameMode::TriPeaks || state.waste.empty()) return false;
    const int difference = std::abs(card.rank - state.waste.back().rank);
    return difference == 1 || (state.options.tripeaksWraparound && difference == 12);
}

bool canMove(const State& state, Location from, Location to) {
    if (samePile(from, to) || !canSelect(state, from)) return false;
    const auto* destination = pileFor(state, to);
    if (!destination) return false;
    const std::vector<Card>* sourcePile = nullptr;
    int start = -1;
    if (!source(state, from, sourcePile, start)) return false;
    const auto& moving = (*sourcePile)[start];
    const int count = static_cast<int>(sourcePile->size()) - start;
    if (state.mode == GameMode::Pyramid) return canSelect(state, to) && moving.rank + destination->back().rank == 13;
    if (state.mode == GameMode::TriPeaks) return to.kind == Location::Waste && canActivate(state, from);
    if (to.kind == Location::Foundation) return foundationMove(state, from, to, moving, count);
    if (to.kind == Location::Cell) return state.mode == GameMode::FreeCell && destination->empty() && count == 1;
    if (to.kind != Location::Tableau) return false;
    if (state.mode == GameMode::FreeCell && count > transferCapacity(state, destination->empty())) return false;
    if (destination->empty()) return state.mode != GameMode::Klondike || moving.rank == 13;
    const auto& top = destination->back();
    return top.faceUp && top.rank == moving.rank + 1 && (state.mode == GameMode::Spider || top.red() != moving.red());
}

void collapseSpider(State& state) {
    for (auto& pile : state.tableau) {
        while (pile.size() >= 13 && pile[pile.size() - 13].rank == 13 && validRun(pile, static_cast<int>(pile.size()) - 13, true)) {
            auto target = std::find_if(state.foundations.begin(), state.foundations.end(), [](const auto& foundation) { return foundation.empty(); });
            if (target == state.foundations.end()) return;
            target->assign(pile.end() - 13, pile.end());
            pile.erase(pile.end() - 13, pile.end());
            flip(pile);
        }
    }
}

void deal(State& state, std::uint32_t seed) {
    std::vector<Card> deck;
    const int suits = state.mode == GameMode::Spider ? state.options.spiderSuits : 4;
    const int copies = state.mode == GameMode::Spider ? 8 / suits : 1;
    for (int copy = 0; copy < copies; ++copy) for (int suit = 0; suit < suits; ++suit) for (int rank = 1; rank <= 13; ++rank) deck.push_back({rank, suit, false});
    std::mt19937 random(seed);
    std::shuffle(deck.begin(), deck.end(), random);
    auto next = [&deck](bool faceUp) { Card card = deck.back(); deck.pop_back(); card.faceUp = faceUp; return card; };
    if (state.mode == GameMode::Klondike) {
        for (int pile = 0; pile < 7; ++pile) for (int index = 0; index <= pile; ++index) state.tableau[pile].push_back(next(index == pile));
    } else if (state.mode == GameMode::Spider) {
        state.tableau.resize(10);
        state.foundations.resize(8);
        for (int row = 0; row < 6; ++row) for (int pile = 0; pile < (row < 5 ? 10 : 4); ++pile) state.tableau[pile].push_back(next(false));
        for (auto& pile : state.tableau) flip(pile);
    } else if (state.mode == GameMode::FreeCell) {
        state.tableau.resize(8);
        for (int index = 0; index < 52; ++index) state.tableau[index % 8].push_back(next(true));
    } else {
        state.tableau.resize(28);
        for (auto& pile : state.tableau) pile.push_back(next(true));
        if (state.mode == GameMode::TriPeaks) state.waste.push_back(next(true));
    }
    state.stock = std::move(deck);
}
}
