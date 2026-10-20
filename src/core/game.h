#pragma once

#include <QJsonObject>
#include <array>
#include <cstdint>
#include <optional>
#include <random>
#include <utility>
#include <vector>

namespace solitare {

enum class GameMode { Klondike, Spider, FreeCell, Pyramid, TriPeaks };

const char* modeName(GameMode mode);
const char* modeId(GameMode mode);

enum class Difficulty { Casual, Standard, Expert };

struct GameOptions {
    Difficulty difficulty = Difficulty::Casual;
    int drawCount = 1;
    int spiderSuits = 1;
    int pyramidRedeals = 2;
    bool tripeaksWraparound = false;
    friend bool operator==(const GameOptions&, const GameOptions&) = default;
};

struct Card {
    int rank = 1;
    int suit = 0;
    bool faceUp = false;
    bool red() const;
    friend bool operator==(const Card&, const Card&) = default;
};

// Sources use index=-1 for the top card; tableau indices select a movable run.
struct Location {
    enum Kind { Stock, Waste, Foundation, Tableau, Cell };
    Kind kind = Stock;
    int pile = 0;
    int index = -1;
    friend bool operator==(const Location&, const Location&) = default;
};

struct State {
    std::vector<Card> stock;
    std::vector<Card> waste;
    std::vector<std::vector<Card>> foundations = std::vector<std::vector<Card>>(4);
    std::vector<std::vector<Card>> tableau = std::vector<std::vector<Card>>(7);
    int moves = 0;
    int drawCount = 1;
    GameMode mode = GameMode::Klondike;
    GameOptions options;
    std::array<std::vector<Card>, 4> cells;
    std::vector<Card> removed;
    int redeals = 0;
    int undosUsed = 0;
    bool hintEarned = false;
    int hintsUsed = 0;
    friend bool operator==(const State&, const State&) = default;
};

class Game {
public:
    explicit Game(std::uint32_t seed = std::random_device{}(), int drawCount = 1);
    Game(std::uint32_t seed, GameMode mode, GameOptions options = {});
    void newGame(std::uint32_t seed, int drawCount = 1);
    void newGame(std::uint32_t seed, GameMode mode, GameOptions options = {});
    const State& state() const;
    bool draw();
    bool move(Location from, Location to);
    bool activate(Location from);
    bool exposed(Location from) const;
    bool canSelect(Location from) const;
    bool autoMove(Location from);
    bool undo();
    bool canUndo() const;
    int undosLeft() const; // -1 when unlimited
    int hintsLeft() const; // -1 when unlimited
    std::optional<std::pair<Location, Location>> takeHint();
    bool won() const;
    std::optional<std::pair<Location, Location>> hint() const;
    bool canMove(Location from, Location to) const;
    QJsonObject toJson() const;
    bool restore(const QJsonObject& json);

private:
    State state_;
    std::uint32_t seed_ = 0;
    std::vector<State> undo_;
    void remember();
    void incrementMoves();
    void earnHint();
};

} // namespace solitare
