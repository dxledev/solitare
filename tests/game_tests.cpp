#include "../src/core/game.h"

#include <QJsonArray>
#include <QtTest>

#include <array>
#include <random>
#include <set>
#include <utility>
#include <vector>

using namespace solitare;

namespace {

QJsonObject cardJson(Card card) {
    return {{"rank", card.rank}, {"suit", card.suit}, {"faceUp", card.faceUp}};
}

QJsonArray cardArray(const std::vector<Card>& cards) {
    QJsonArray result;
    for (const auto& card : cards) {
        result.append(cardJson(card));
    }
    return result;
}

QJsonObject savedState(std::vector<Card> waste = {},
                       std::array<std::vector<Card>, 4> foundations = {},
                       std::array<std::vector<Card>, 7> tableau = {},
                       int drawCount = 1,
                       int moves = 0) {
    std::array<std::array<bool, 13>, 4> seen{};
    auto mark = [&seen](const std::vector<Card>& cards) {
        for (const auto& card : cards) {
            seen[static_cast<std::size_t>(card.suit)][static_cast<std::size_t>(card.rank - 1)] = true;
        }
    };
    mark(waste);
    for (const auto& pile : foundations) {
        mark(pile);
    }
    for (const auto& pile : tableau) {
        mark(pile);
    }
    std::vector<Card> stock;
    for (int suit = 0; suit < 4; ++suit) {
        for (int rank = 1; rank <= 13; ++rank) {
            if (!seen[static_cast<std::size_t>(suit)][static_cast<std::size_t>(rank - 1)]) {
                stock.push_back(Card{rank, suit, false});
            }
        }
    }
    QJsonArray foundationArray;
    for (const auto& pile : foundations) {
        foundationArray.append(cardArray(pile));
    }
    QJsonArray tableauArray;
    for (const auto& pile : tableau) {
        tableauArray.append(cardArray(pile));
    }
    return {
        {"version", 1},
        {"seed", 123},
        {"drawCount", drawCount},
        {"moves", moves},
        {"stock", cardArray(stock)},
        {"waste", cardArray(waste)},
        {"foundations", foundationArray},
        {"tableau", tableauArray},
    };
}

bool invariant(const State& state) {
    if (state.drawCount != 1 && state.drawCount != 3) {
        return false;
    }
    std::array<std::array<bool, 13>, 4> seen{};
    int count = 0;
    auto include = [&seen, &count](const std::vector<Card>& cards) {
        for (const auto& card : cards) {
            if (card.rank < 1 || card.rank > 13 || card.suit < 0 || card.suit > 3 ||
                seen[static_cast<std::size_t>(card.suit)][static_cast<std::size_t>(card.rank - 1)]) {
                return false;
            }
            seen[static_cast<std::size_t>(card.suit)][static_cast<std::size_t>(card.rank - 1)] = true;
            ++count;
        }
        return true;
    };
    if (!include(state.stock) || !include(state.waste)) {
        return false;
    }
    for (const auto& pile : state.foundations) {
        if (!include(pile)) {
            return false;
        }
        for (std::size_t index = 0; index < pile.size(); ++index) {
            if (!pile[index].faceUp || pile[index].rank != static_cast<int>(index) + 1 ||
                (index > 0 && pile[index].suit != pile.front().suit)) {
                return false;
            }
        }
    }
    for (const auto& pile : state.tableau) {
        if (!include(pile)) {
            return false;
        }
        if (!pile.empty() && !pile.back().faceUp) {
            return false;
        }
        bool faceUpSeen = false;
        for (std::size_t index = 0; index < pile.size(); ++index) {
            if (faceUpSeen && !pile[index].faceUp) {
                return false;
            }
            if (pile[index].faceUp) {
                faceUpSeen = true;
                if (index > 0 && pile[index - 1].faceUp &&
                    (pile[index - 1].rank != pile[index].rank + 1 ||
                     pile[index - 1].red() == pile[index].red())) {
                    return false;
                }
            }
        }
    }
    if (std::any_of(state.stock.begin(), state.stock.end(), [](const Card& card) { return card.faceUp; }) ||
        std::any_of(state.waste.begin(), state.waste.end(), [](const Card& card) { return !card.faceUp; })) {
        return false;
    }
    return count == 52;
}

} // namespace

class GameTests : public QObject {
    Q_OBJECT
private slots:
    void dealAndSeedAreStable() {
        Game first(987, 1);
        Game second(987, 1);
        QVERIFY(first.state() == second.state());
        QCOMPARE(first.state().stock.size(), std::size_t(24));
        for (std::size_t pile = 0; pile < first.state().tableau.size(); ++pile) {
            QCOMPARE(first.state().tableau[pile].size(), pile + 1);
            QVERIFY(first.state().tableau[pile].back().faceUp);
            for (std::size_t index = 0; index + 1 < pile + 1; ++index) {
                QVERIFY(!first.state().tableau[pile][index].faceUp);
            }
        }
        QVERIFY(invariant(first.state()));
        QVERIFY(first.toJson().contains("seed"));
    }

    void drawCountsAndUndo() {
        Game drawOne(42, 1);
        const State initialOne = drawOne.state();
        QVERIFY(drawOne.draw());
        QCOMPARE(drawOne.state().stock.size(), std::size_t(23));
        QCOMPARE(drawOne.state().waste.size(), std::size_t(1));
        QVERIFY(drawOne.state().waste.back().faceUp);
        QVERIFY(drawOne.undo());
        QVERIFY(drawOne.state() == initialOne);
        QVERIFY(!drawOne.canUndo());

        Game drawThree(42, 3);
        const State initialThree = drawThree.state();
        QVERIFY(drawThree.draw());
        QCOMPARE(drawThree.state().stock.size(), std::size_t(21));
        QCOMPARE(drawThree.state().waste.size(), std::size_t(3));
        QVERIFY(drawThree.undo());
        QVERIFY(drawThree.state() == initialThree);
    }

    void recyclingPreservesDrawOrderAndCanUndo() {
        Game game(77, 3);
        QVERIFY(game.draw());
        const std::array<Card, 3> firstDraw{
            game.state().waste[0], game.state().waste[1], game.state().waste[2],
        };
        for (int index = 1; index < 8; ++index) {
            QVERIFY(game.draw());
        }
        QCOMPARE(game.state().stock.size(), std::size_t(0));
        QCOMPARE(game.state().waste.size(), std::size_t(24));
        const State exhausted = game.state();
        QVERIFY(game.draw());
        QCOMPARE(game.state().stock.size(), std::size_t(24));
        QVERIFY(game.state().waste.empty());
        QVERIFY(game.undo());
        QVERIFY(game.state() == exhausted);
        QVERIFY(game.draw());
        QVERIFY(game.draw());
        QCOMPARE(game.state().waste.size(), std::size_t(3));
        for (std::size_t index = 0; index < firstDraw.size(); ++index) {
            QCOMPARE(game.state().waste[index], firstDraw[index]);
        }
        QVERIFY(invariant(game.state()));
    }

    void undoHistoryIsBounded() {
        Game game(62, 1);
        for (int index = 0; index < 205; ++index) {
            QVERIFY(game.draw());
        }
        for (int index = 0; index < 200; ++index) {
            QVERIFY(game.undo());
        }
        QVERIFY(!game.canUndo());
        QCOMPARE(game.state().moves, 5);
        QVERIFY(invariant(game.state()));
    }

    void legalMovesAndExposure() {
        std::array<std::vector<Card>, 7> tableau;
        tableau[0] = {Card{3, 3, true}};
        tableau[1] = {Card{4, 0, true}};
        Game game(1);
        QVERIFY(game.restore(savedState({}, {}, tableau, 1, 4)));
        const State before = game.state();
        const Location source{Location::Tableau, 0, -1};
        const Location destination{Location::Tableau, 1, 800};
        QVERIFY(game.canMove(source, destination));
        QVERIFY(game.move(source, destination));
        QVERIFY(game.state().tableau[0].empty());
        QCOMPARE(game.state().tableau[1].size(), std::size_t(2));
        QCOMPARE(game.state().moves, 5);
        QVERIFY(game.undo());
        QVERIFY(game.state() == before);

        tableau = {};
        tableau[0] = {Card{6, 0, false}, Card{4, 3, true}};
        tableau[1] = {Card{5, 2, true}};
        QVERIFY(game.restore(savedState({}, {}, tableau)));
        QVERIFY(game.move(Location{Location::Tableau, 0, 1}, Location{Location::Tableau, 1, -1}));
        QCOMPARE(game.state().tableau[0].size(), std::size_t(1));
        QVERIFY((game.state().tableau[0].back() == Card{6, 0, true}));
        QVERIFY(invariant(game.state()));
    }

    void foundationMovesAndAutoMove() {
        std::array<std::vector<Card>, 4> foundations;
        foundations[0] = {Card{1, 0, true}};
        std::vector<Card> waste{Card{2, 0, true}};
        Game game(8);
        QVERIFY(game.restore(savedState(waste, foundations)));
        QVERIFY(game.autoMove(Location{Location::Waste, 0, -1}));
        QCOMPARE(game.state().foundations[0].size(), std::size_t(2));
        QVERIFY(game.state().waste.empty());
        QVERIFY(game.undo());
        QVERIFY(!game.won());
    }

    void hintsSkipFoundationToTableauOscillation() {
        std::array<std::vector<Card>, 4> foundations;
        for (int rank = 1; rank <= 5; ++rank) {
            foundations[0].push_back(Card{rank, 0, true});
        }
        std::array<std::vector<Card>, 7> tableau;
        tableau[0] = {Card{6, 3, true}};
        Game game(81);
        QVERIFY(game.restore(savedState({}, foundations, tableau)));
        const auto suggestion = game.hint();
        QVERIFY(suggestion.has_value());
        QCOMPARE(suggestion->first.kind, Location::Stock);
    }

    void invalidIndicesAndSequencesAreRejected() {
        std::array<std::vector<Card>, 7> tableau;
        tableau[0] = {Card{3, 3, true}};
        tableau[1] = {Card{4, 1, true}};
        Game game(3);
        QVERIFY(game.restore(savedState({}, {}, tableau)));
        const State before = game.state();
        QVERIFY(!game.canMove(Location{Location::Tableau, 8, -1}, Location{Location::Tableau, 1, -1}));
        QVERIFY(!game.canMove(Location{Location::Tableau, 0, -2}, Location{Location::Tableau, 1, -1}));
        QVERIFY(!game.canMove(Location{Location::Tableau, 0, 1}, Location{Location::Tableau, 1, -1}));
        QVERIFY(!game.canMove(Location{Location::Tableau, 0, -1}, Location{Location::Tableau, 1, -1}));
        QVERIFY(!game.canMove(Location{Location::Stock, 0, -1}, Location{Location::Tableau, 1, -1}));
        QVERIFY(!game.move(Location{Location::Tableau, 0, -1}, Location{Location::Foundation, 9, -1}));
        QVERIFY(game.state() == before);
    }

    void malformedSavesAreRejectedWithoutChangingGame() {
        Game game(14);
        const State before = game.state();
        auto invalid = game.toJson();
        invalid.remove("waste");
        QVERIFY(!game.restore(invalid));
        QVERIFY(game.state() == before);

        auto duplicate = game.toJson();
        auto stock = duplicate.value("stock").toArray();
        stock[1] = stock[0];
        duplicate.insert("stock", stock);
        QVERIFY(!game.restore(duplicate));
        QVERIFY(game.state() == before);

        std::array<std::vector<Card>, 4> foundations;
        foundations[0] = {Card{1, 0, true}, Card{3, 0, true}};
        QVERIFY(!game.restore(savedState({}, foundations)));
        QVERIFY(game.state() == before);

        std::array<std::vector<Card>, 7> tableau;
        tableau[0] = {Card{5, 0, true}, Card{4, 3, false}};
        QVERIFY(!game.restore(savedState({}, {}, tableau)));
        QVERIFY(game.state() == before);

        auto invalidDraw = game.toJson();
        invalidDraw.insert("drawCount", 2);
        QVERIFY(!game.restore(invalidDraw));
        QVERIFY(game.state() == before);
    }

    void saveRestoreRoundTrips() {
        Game game(456, 3);
        QVERIFY(game.draw());
        QVERIFY(game.draw());
        QVERIFY(invariant(game.state()));
        const auto json = game.toJson();
        Game restored(999);
        QVERIFY(restored.restore(json));
        QVERIFY(restored.state() == game.state());
        QCOMPARE(restored.toJson(), json);
        QVERIFY(!restored.canUndo());
    }

    void difficultyLimitsUndosAndHints() {
        GameOptions options; options.difficulty = Difficulty::Standard;
        Game game(5, GameMode::Klondike, options);
        QCOMPARE(game.hintsLeft(), 1);
        for (int i = 0; i < 3; ++i) { QVERIFY(game.draw()); }
        QCOMPARE(game.undosLeft(), 2);
        QVERIFY(game.undo()); QVERIFY(game.undo());
        QVERIFY(!game.canUndo()); QVERIFY(!game.undo());
        QCOMPARE(game.undosLeft(), 0);

        options.difficulty = Difficulty::Expert;
        Game expert(5, GameMode::Klondike, options);
        QVERIFY(expert.draw()); QVERIFY(!expert.canUndo()); QCOMPARE(expert.hintsLeft(), 0);

        Game casual(5, GameMode::Klondike);
        QCOMPARE(casual.undosLeft(), -1); QCOMPARE(casual.hintsLeft(), -1);
    }

    void standardHintEarnedByAcesAndPersisted() {
        std::array<std::vector<Card>, 4> foundations;
        for (int suit = 0; suit < 3; ++suit) foundations[suit] = {Card{1, suit, true}};
        auto json = savedState({Card{1, 3, true}}, foundations);
        json["version"] = 2;
        json["mode"] = "klondike";
        json["options"] = QJsonObject{{"difficulty", 1}, {"drawCount", 1}, {"spiderSuits", 1}, {"pyramidRedeals", 2}, {"tripeaksWraparound", false}};
        json["cells"] = QJsonArray{QJsonArray{}, QJsonArray{}, QJsonArray{}, QJsonArray{}};
        json["removed"] = QJsonArray{};
        json["redeals"] = 0;
        Game game(8);
        QVERIFY(game.restore(json));
        QCOMPARE(game.state().options.difficulty, Difficulty::Standard);
        QCOMPARE(game.hintsLeft(), 1);
        QVERIFY(game.autoMove(Location{Location::Waste, 0, -1}));
        QCOMPARE(game.hintsLeft(), 2);
        QVERIFY(game.takeHint());
        Game restored;
        QVERIFY(restored.restore(game.toJson()));
        QCOMPARE(restored.hintsLeft(), 1);
        QVERIFY(restored.takeHint());
        QCOMPARE(restored.hintsLeft(), 0);
        QVERIFY(!restored.takeHint());
        QVERIFY(Game().restore(restored.toJson()));
    }

    void spiderRunEarnsStandardHint() {
        GameOptions options; options.difficulty = Difficulty::Standard;
        Game game(3, GameMode::Spider, options);
        QCOMPARE(game.hintsLeft(), 1);
    }

    void randomLongPlayKeepsAllCards() {
        Game game(934, 3);
        std::mt19937 random(5);
        for (int turn = 0; turn < 500; ++turn) {
            const int action = static_cast<int>(random() % 4);
            if (action == 0) {
                game.draw();
            } else if (action == 1 && game.canUndo()) {
                game.undo();
            } else {
                std::vector<Location> sources;
                for (int pile = 0; pile < 7; ++pile) {
                    if (!game.state().tableau[static_cast<std::size_t>(pile)].empty()) {
                        for (int index = 0; index < static_cast<int>(game.state().tableau[static_cast<std::size_t>(pile)].size()); ++index) {
                            sources.push_back(Location{Location::Tableau, pile, index});
                        }
                    }
                }
                if (!game.state().waste.empty()) {
                    sources.push_back(Location{Location::Waste, 0, -1});
                }
                for (int pile = 0; pile < 4; ++pile) {
                    if (!game.state().foundations[static_cast<std::size_t>(pile)].empty()) {
                        sources.push_back(Location{Location::Foundation, pile, -1});
                    }
                }
                if (!sources.empty()) {
                    const auto source = sources[static_cast<std::size_t>(random() % sources.size())];
                    const Location destination{Location::Tableau, static_cast<int>(random() % 7), -1};
                    game.move(source, destination);
                }
            }
            QVERIFY2(invariant(game.state()), "the game must preserve all 52 unique cards");
        }
    }
};

QTEST_GUILESS_MAIN(GameTests)
#include "game_tests.moc"
