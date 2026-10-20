#include "game.h"
#include "game_rules.h"
#include <algorithm>
#include <limits>

namespace solitare {
namespace {
std::vector<Location> selectableLocations(const State& state) {
    std::vector<Location> result;
    for (int pile = 0; pile < static_cast<int>(state.tableau.size()); ++pile) {
        for (int index = 0; index < static_cast<int>(state.tableau[pile].size()); ++index) {
            Location location{Location::Tableau, pile, index};
            if (rules::canSelect(state, location)) result.push_back(location);
        }
    }
    for (auto kind : {Location::Waste, Location::Cell}) {
        for (int pile = 0; pile < (kind == Location::Cell ? 4 : 1); ++pile) {
            Location location{kind, pile, -1};
            if (rules::canSelect(state, location)) result.push_back(location);
        }
    }
    return result;
}

void removeCard(State& state, Location from) {
    auto* source = rules::pileFor(state, from);
    state.removed.push_back(source->back());
    source->pop_back();
}
}

const char* modeName(GameMode mode) {
    switch (mode) {
    case GameMode::Klondike: return "Klondike";
    case GameMode::Spider: return "Spider";
    case GameMode::FreeCell: return "FreeCell";
    case GameMode::Pyramid: return "Pyramid";
    case GameMode::TriPeaks: return "TriPeaks";
    }
    return "Klondike";
}

const char* modeId(GameMode mode) {
    switch (mode) {
    case GameMode::Klondike: return "klondike";
    case GameMode::Spider: return "spider";
    case GameMode::FreeCell: return "freecell";
    case GameMode::Pyramid: return "pyramid";
    case GameMode::TriPeaks: return "tripeaks";
    }
    return "klondike";
}

bool Card::red() const { return suit == 1 || suit == 3; }

Game::Game(std::uint32_t seed, int drawCount) { newGame(seed, drawCount); }
Game::Game(std::uint32_t seed, GameMode mode, GameOptions options) { newGame(seed, mode, options); }
void Game::newGame(std::uint32_t seed, int drawCount) {
    GameOptions options;
    options.drawCount = drawCount;
    newGame(seed, GameMode::Klondike, options);
}

void Game::newGame(std::uint32_t seed, GameMode mode, GameOptions options) {
    seed_ = seed;
    state_ = State{};
    options.drawCount = options.drawCount == 3 ? 3 : 1;
    if (options.spiderSuits != 2 && options.spiderSuits != 4) options.spiderSuits = 1;
    options.pyramidRedeals = std::clamp(options.pyramidRedeals, 0, 2);
    state_.mode = mode;
    state_.options = options;
    state_.drawCount = options.drawCount;
    undo_.clear();
    rules::deal(state_, seed);
}

const State& Game::state() const { return state_; }
void Game::remember() {
    if (undo_.size() == 200) undo_.erase(undo_.begin());
    undo_.push_back(state_);
}
void Game::incrementMoves() {
    if (state_.moves < std::numeric_limits<int>::max()) ++state_.moves;
}

bool Game::draw() {
    if (won() || !rules::canDraw(state_)) return false;
    remember();
    if (state_.mode == GameMode::Spider) {
        for (auto& pile : state_.tableau) {
            Card card = state_.stock.back();
            state_.stock.pop_back();
            card.faceUp = true;
            pile.push_back(card);
        }
        rules::collapseSpider(state_);
    } else if (state_.stock.empty()) {
        state_.stock.assign(state_.waste.rbegin(), state_.waste.rend());
        for (auto& card : state_.stock) card.faceUp = false;
        state_.waste.clear();
        if (state_.mode == GameMode::Pyramid) ++state_.redeals;
    } else {
        const int count = std::min(state_.mode == GameMode::Klondike ? state_.drawCount : 1, static_cast<int>(state_.stock.size()));
        for (int index = 0; index < count; ++index) {
            Card card = state_.stock.back();
            state_.stock.pop_back();
            card.faceUp = true;
            state_.waste.push_back(card);
        }
    }
    incrementMoves();
    return true;
}

bool Game::exposed(Location from) const { return rules::exposed(state_, from); }
bool Game::canSelect(Location from) const { return !won() && rules::canSelect(state_, from); }
bool Game::canMove(Location from, Location to) const { return !won() && rules::canMove(state_, from, to); }

bool Game::activate(Location from) {
    if (won() || !rules::canActivate(state_, from)) return false;
    remember();
    if (state_.mode == GameMode::Pyramid) removeCard(state_, from);
    else {
        auto* source = rules::pileFor(state_, from);
        state_.waste.push_back(source->back());
        source->pop_back();
    }
    incrementMoves();
    return true;
}

bool Game::move(Location from, Location to) {
    if (!canMove(from, to)) return false;
    if (state_.mode == GameMode::TriPeaks) return activate(from);
    remember();
    if (state_.mode == GameMode::Pyramid) {
        removeCard(state_, from);
        removeCard(state_, to);
    } else {
        auto* source = rules::pileFor(state_, from);
        auto* destination = rules::pileFor(state_, to);
        const int start = from.index == -1 ? static_cast<int>(source->size()) - 1 : from.index;
        destination->insert(destination->end(), source->begin() + start, source->end());
        source->erase(source->begin() + start, source->end());
        if (from.kind == Location::Tableau && !source->empty()) source->back().faceUp = true;
        if (state_.mode == GameMode::Spider) rules::collapseSpider(state_);
    }
    incrementMoves();
    return true;
}

bool Game::autoMove(Location from) {
    if (state_.mode == GameMode::TriPeaks || state_.mode == GameMode::Pyramid) return activate(from);
    if (from.kind == Location::Foundation || from.kind == Location::Stock) return false;
    for (int pile = 0; pile < static_cast<int>(state_.foundations.size()); ++pile) {
        Location destination{Location::Foundation, pile, -1};
        if (canMove(from, destination)) return move(from, destination);
    }
    return false;
}

bool Game::undo() {
    if (undo_.empty()) return false;
    state_ = std::move(undo_.back());
    undo_.pop_back();
    return true;
}
bool Game::canUndo() const { return !undo_.empty(); }
bool Game::won() const {
    if (state_.mode == GameMode::Pyramid || state_.mode == GameMode::TriPeaks) return std::all_of(state_.tableau.begin(), state_.tableau.end(), [](const auto& pile) { return pile.empty(); });
    return std::all_of(state_.foundations.begin(), state_.foundations.end(), [](const auto& pile) { return pile.size() == 13; });
}

std::optional<std::pair<Location, Location>> Game::hint() const {
    if (won()) return std::nullopt;
    const auto sources = selectableLocations(state_);
    for (const auto from : sources) if (rules::canActivate(state_, from)) return std::pair{from, from};
    if (state_.mode == GameMode::Pyramid) {
        for (const auto from : sources) for (const auto to : sources) if (canMove(from, to)) return std::pair{from, to};
    } else {
        for (const auto from : sources) for (int pile = 0; pile < static_cast<int>(state_.foundations.size()); ++pile) {
            Location to{Location::Foundation, pile, -1};
            if (canMove(from, to)) return std::pair{from, to};
        }
        for (const auto from : sources) for (int pile = 0; pile < static_cast<int>(state_.tableau.size()); ++pile) {
            Location to{Location::Tableau, pile, -1};
            if (from.kind == Location::Tableau && from.index == 0 && state_.tableau[pile].empty()) continue;
            if (canMove(from, to)) return std::pair{from, to};
        }
        if (state_.mode == GameMode::FreeCell) for (const auto from : sources) for (int pile = 0; pile < 4; ++pile) {
            Location to{Location::Cell, pile, -1};
            if (canMove(from, to)) return std::pair{from, to};
        }
    }
    if (rules::canDraw(state_)) return std::pair{Location{Location::Stock, 0, -1}, Location{Location::Waste, 0, -1}};
    return std::nullopt;
}
} // namespace solitare
