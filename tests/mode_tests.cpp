#include "../src/core/game.h"
#include <QJsonArray>
#include <QtTest>
#include <algorithm>
#include <array>

using namespace solitare;
namespace {
Location column(int pile, int index = -1) { return {Location::Tableau, pile, index}; }
Location cell(int pile) { return {Location::Cell, pile, -1}; }
Location waste() { return {Location::Waste, 0, -1}; }

QJsonArray cardsJson(const std::vector<Card>& cards) {
    QJsonArray result;
    for (const auto& card : cards) result.append(QJsonObject{{"rank", card.rank}, {"suit", card.suit}, {"faceUp", card.faceUp}});
    return result;
}
template<class Piles> QJsonArray pilesJson(const Piles& piles) {
    QJsonArray result;
    for (const auto& pile : piles) result.append(cardsJson(pile));
    return result;
}

State emptyState(GameMode mode, GameOptions options = {}) {
    State result = Game(1, mode, options).state();
    result.stock.clear();
    result.waste.clear();
    for (auto& pile : result.tableau) pile.clear();
    return result;
}

QJsonObject fixture(State state, int stockCount = 0) {
    std::array<std::array<int, 13>, 4> counts{};
    auto mark = [&counts](const auto& cards) { for (const auto& card : cards) ++counts[card.suit][card.rank - 1]; };
    mark(state.stock); mark(state.waste); mark(state.removed);
    for (const auto& pile : state.tableau) mark(pile);
    for (const auto& pile : state.foundations) mark(pile);
    for (const auto& pile : state.cells) mark(pile);
    std::vector<Card> remaining;
    for (int suit = 0; suit < 4; ++suit) for (int rank = 1; rank <= 13; ++rank) {
        const int expected = state.mode == GameMode::Spider ? (suit < state.options.spiderSuits ? 8 / state.options.spiderSuits : 0) : 1;
        Q_ASSERT(counts[suit][rank - 1] <= expected);
        for (int copy = counts[suit][rank - 1]; copy < expected; ++copy) remaining.push_back({rank, suit, true});
    }
    while (stockCount-- > 0) {
        auto card = remaining.back(); remaining.pop_back(); card.faceUp = false;
        state.stock.push_back(card);
    }
    if (state.mode == GameMode::Pyramid) state.removed.insert(state.removed.end(), remaining.begin(), remaining.end());
    else if (state.mode == GameMode::TriPeaks) {
        state.waste.insert(state.waste.begin(), remaining.begin(), remaining.end());
    } else if (state.mode == GameMode::FreeCell || state.mode == GameMode::Spider) {
        auto& pile = state.tableau.back();
        if (state.mode == GameMode::Spider) for (auto& card : remaining) card.faceUp = false;
        pile.insert(pile.begin(), remaining.begin(), remaining.end());
        if (!pile.empty()) pile.back().faceUp = true;
    } else {
        for (auto& card : remaining) card.faceUp = false;
        state.stock.insert(state.stock.end(), remaining.begin(), remaining.end());
    }
    auto json = Game(1, state.mode, state.options).toJson();
    json["stock"] = cardsJson(state.stock); json["waste"] = cardsJson(state.waste);
    json["foundations"] = pilesJson(state.foundations); json["tableau"] = pilesJson(state.tableau);
    json["cells"] = pilesJson(state.cells); json["removed"] = cardsJson(state.removed);
    json["redeals"] = state.redeals;
    return json;
}

int cardCount(const State& state) {
    int result = static_cast<int>(state.stock.size() + state.waste.size() + state.removed.size());
    for (const auto& pile : state.tableau) result += static_cast<int>(pile.size());
    for (const auto& pile : state.foundations) result += static_cast<int>(pile.size());
    for (const auto& pile : state.cells) result += static_cast<int>(pile.size());
    return result;
}

bool applyHint(Game& game) {
    const auto hint = game.hint();
    if (!hint) return false;
    if (hint->first.kind == Location::Stock) return game.draw();
    if (hint->first == hint->second) return game.activate(hint->first);
    return game.move(hint->first, hint->second);
}
}

class ModeTests : public QObject {
    Q_OBJECT
private slots:
    void dealsAndOptions() {
        for (const auto mode : {GameMode::Klondike, GameMode::Spider, GameMode::FreeCell, GameMode::Pyramid, GameMode::TriPeaks}) {
            Game first(93, mode), second(93, mode);
            QVERIFY(first.state() == second.state());
            QCOMPARE(cardCount(first.state()), mode == GameMode::Spider ? 104 : 52);
            const auto& state = first.state();
            QCOMPARE(state.tableau.size(), mode == GameMode::Spider ? 10u : mode == GameMode::FreeCell ? 8u : mode == GameMode::Klondike ? 7u : 28u);
            if (mode == GameMode::FreeCell) {
                QVERIFY(state.stock.empty()); QVERIFY(!first.draw());
                for (int pile = 0; pile < 8; ++pile) QCOMPARE(state.tableau[pile].size(), pile < 4 ? 7u : 6u);
            }
            if (mode == GameMode::Spider) {
                QCOMPARE(state.stock.size(), 50u);
                for (int pile = 0; pile < 10; ++pile) QCOMPARE(state.tableau[pile].size(), pile < 4 ? 6u : 5u);
            }
            if (mode == GameMode::Pyramid) QCOMPARE(state.stock.size(), 24u);
            if (mode == GameMode::TriPeaks) { QCOMPARE(state.stock.size(), 23u); QCOMPARE(state.waste.size(), 1u); }
        }
        for (int suits : {1, 2, 4}) {
            GameOptions options; options.spiderSuits = suits;
            Game game(11, GameMode::Spider, options);
            std::array<int, 4> counts{};
            for (auto card : game.state().stock) ++counts[card.suit];
            for (const auto& pile : game.state().tableau) for (auto card : pile) ++counts[card.suit];
            for (int suit = 0; suit < 4; ++suit) QCOMPARE(counts[suit], suit < suits ? 104 / suits : 0);
        }
    }

    void spiderRunsAndCompletion() {
        GameOptions options; options.spiderSuits = 2;
        State state = emptyState(GameMode::Spider, options);
        state.tableau[0] = {{5, 0, true}, {4, 1, true}};
        state.tableau[1] = {{6, 0, true}};
        state.tableau[2] = {{5, 0, true}};
        Game game(1);
        QVERIFY(game.restore(fixture(state)));
        QVERIFY(!game.canMove(column(0, 0), column(1)));
        QVERIFY(game.canMove(column(0), column(2)));
        QVERIFY(!game.canMove(column(1), {Location::Foundation, 0, -1}));
        QVERIFY(!game.draw());
        state = emptyState(GameMode::Spider);
        for (int rank = 13; rank >= 2; --rank) state.tableau[0].push_back({rank, 0, true});
        state.tableau[1] = {{7, 0, false}, {1, 0, true}};
        QVERIFY(game.restore(fixture(state)));
        const auto before = game.state();
        QVERIFY(game.move(column(1), column(0)));
        QVERIFY(game.state().tableau[0].empty());
        QCOMPARE(game.state().foundations[0].size(), 13u);
        QVERIFY(game.state().tableau[1].back().faceUp);
        QVERIFY(game.undo()); QVERIFY(game.state() == before);
    }

    void spiderStockAndWin() {
        Game game(1, GameMode::Spider);
        const auto before = game.state();
        QVERIFY(game.draw()); QCOMPARE(game.state().stock.size(), 40u);
        for (int pile = 0; pile < 10; ++pile) { QCOMPARE(game.state().tableau[pile].size(), before.tableau[pile].size() + 1); QVERIFY(game.state().tableau[pile].back().faceUp); }
        QVERIFY(game.undo()); QVERIFY(game.state() == before);
        auto state = emptyState(GameMode::Spider);
        for (int pile = 0; pile < 8; ++pile) for (int rank = 13; rank >= 1; --rank) state.foundations[pile].push_back({rank, 0, true});
        QVERIFY(game.restore(fixture(state))); QVERIFY(game.won()); QVERIFY(!game.draw()); QVERIFY(!game.hint());
    }

    void freecellTransfersAndCapacity() {
        State state = emptyState(GameMode::FreeCell);
        state.tableau[0] = {{5, 0, true}, {4, 1, true}, {3, 2, true}};
        state.tableau[1] = {{6, 3, true}};
        for (int pile = 2; pile < 7; ++pile) state.tableau[pile] = {{pile + 5, 0, true}};
        for (int index = 0; index < 4; ++index) state.cells[index] = {{index + 7, 1, true}};
        Game game(1);
        QVERIFY(game.restore(fixture(state)));
        QVERIFY(!game.canMove(column(0, 0), column(1)));
        state.cells[0].clear();
        QVERIFY(game.restore(fixture(state))); QVERIFY(!game.canMove(column(0, 0), column(1)));
        state.tableau[2].clear();
        QVERIFY(game.restore(fixture(state))); QVERIFY(game.canMove(column(0, 0), column(1)));
        QVERIFY(!game.canMove(column(0, 0), column(2)));
        QVERIFY(game.canMove(column(0), cell(0)));
        QVERIFY(!game.canMove(column(0, 1), cell(0)));
        QVERIFY(game.move(column(0), cell(0)));
        QVERIFY(!game.canMove(column(1), cell(0)));
        QVERIFY(game.move(cell(0), column(2)));
        QVERIFY(game.undo()); QCOMPARE(game.state().cells[0].size(), 1u);
    }

    void freecellFoundationAndWin() {
        State state = emptyState(GameMode::FreeCell);
        state.cells[0] = {{1, 0, true}};
        Game game(1);
        QVERIFY(game.restore(fixture(state))); QVERIFY(game.autoMove(cell(0)));
        QVERIFY(game.canMove({Location::Foundation, 0, -1}, column(0)));
        QVERIFY(game.move({Location::Foundation, 0, -1}, cell(0)));
        QVERIFY(game.undo());
        state = emptyState(GameMode::FreeCell);
        for (int suit = 0; suit < 4; ++suit) for (int rank = 1; rank <= 13; ++rank) state.foundations[suit].push_back({rank, suit, true});
        QVERIFY(game.restore(fixture(state))); QVERIFY(game.won());
    }

    void pyramidExposurePairsKingsAndUndo() {
        State state = emptyState(GameMode::Pyramid);
        state.tableau[15] = {{13, 0, true}};
        state.tableau[21] = {{5, 0, true}};
        state.tableau[22] = {{8, 1, true}};
        Game game(1);
        QVERIFY(game.restore(fixture(state)));
        QVERIFY(!game.exposed(column(15))); QVERIFY(!game.activate(column(15)));
        QVERIFY(game.canMove(column(21), column(22)));
        const auto before = game.state();
        QVERIFY(game.move(column(21), column(22))); QVERIFY(game.exposed(column(15)));
        QVERIFY(game.activate(column(15))); QVERIFY(game.won()); QVERIFY(!game.hint());
        QVERIFY(game.undo()); QVERIFY(!game.won()); QVERIFY(game.undo()); QVERIFY(game.state() == before);
        state.waste = {{8, 2, true}}; state.tableau[22].clear();
        QVERIFY(game.restore(fixture(state))); QVERIFY(game.move(column(21), waste()));
        QVERIFY(!game.move(waste(), waste()));
    }

    void pyramidRedealsRespectOptions() {
        for (int redeals : {0, 1, 2}) {
            GameOptions options; options.pyramidRedeals = redeals;
            Game game(1, GameMode::Pyramid, options);
            for (int pass = 0; pass <= redeals; ++pass) {
                for (int card = 0; card < 24; ++card) QVERIFY(game.draw());
                if (pass < redeals) { QVERIFY(game.draw()); QCOMPARE(game.state().redeals, pass + 1); }
            }
            const auto before = game.state(); QVERIFY(!game.draw()); QVERIFY(game.state() == before);
            QVERIFY(game.undo()); QCOMPARE(game.state().stock.size(), 1u);
        }
    }

    void tripeaksAdjacencyExposureWrapAndWin() {
        State state = emptyState(GameMode::TriPeaks);
        state.tableau[9] = {{12, 0, true}};
        state.tableau[18] = {{10, 0, true}};
        state.tableau[19] = {{11, 0, true}};
        state.waste = {{9, 0, true}};
        Game game(1);
        QVERIFY(game.restore(fixture(state)));
        QVERIFY(!game.exposed(column(9))); QVERIFY(!game.activate(column(19)));
        QVERIFY(game.activate(column(18))); QVERIFY(game.activate(column(19)));
        QVERIFY(game.exposed(column(9))); QVERIFY(game.activate(column(9))); QVERIFY(game.won());
        QVERIFY(!game.draw()); QVERIFY(game.undo()); QVERIFY(!game.won());
        state = emptyState(GameMode::TriPeaks);
        state.tableau[27] = {{1, 0, true}}; state.waste = {{13, 0, true}};
        QVERIFY(game.restore(fixture(state))); QVERIFY(!game.activate(column(27)));
        state.options.tripeaksWraparound = true;
        QVERIFY(game.restore(fixture(state))); QVERIFY(game.activate(column(27))); QVERIFY(game.won());
    }

    void tripeaksCannotRecycle() {
        Game game(55, GameMode::TriPeaks);
        for (int index = 0; index < 23; ++index) QVERIFY(game.draw());
        const auto before = game.state(); QVERIFY(!game.draw()); QVERIFY(game.state() == before);
        QVERIFY(game.undo()); QCOMPARE(game.state().stock.size(), 1u);
    }

    void modesRoundtripHintsAndUndoPreserveCards() {
        for (const auto mode : {GameMode::Klondike, GameMode::Spider, GameMode::FreeCell, GameMode::Pyramid, GameMode::TriPeaks}) {
            for (int variant = 0; variant < 3; ++variant) {
                GameOptions options; options.drawCount = variant == 0 ? 1 : 3; options.spiderSuits = variant == 0 ? 1 : variant == 1 ? 2 : 4; options.pyramidRedeals = variant; options.tripeaksWraparound = variant != 0;
                Game game(101 + variant, mode, options);
                for (int turn = 0; turn < 80; ++turn) {
                    const auto before = game.state();
                    const bool suggested = game.hint().has_value();
                    const bool played = applyHint(game);
                    QCOMPARE(played, suggested);
                    if (!played) break;
                    QCOMPARE(cardCount(game.state()), mode == GameMode::Spider ? 104 : 52);
                    Game restored(1); QVERIFY2(restored.restore(game.toJson()), modeName(mode)); QVERIFY(restored.state() == game.state());
                    if (turn % 7 == 0) { QVERIFY(game.undo()); QVERIFY(game.state() == before); QVERIFY(applyHint(game)); }
                }
            }
        }
    }

    void modeSavesRejectMalformedDataAtomically() {
        for (auto mode : {GameMode::Spider, GameMode::FreeCell, GameMode::Pyramid, GameMode::TriPeaks}) {
            Game game(45, mode); const auto before = game.state();
            auto invalid = game.toJson(); invalid["mode"] = "bogus"; QVERIFY(!game.restore(invalid));
            invalid = game.toJson(); auto piles = invalid["tableau"].toArray(); piles.removeLast(); invalid["tableau"] = piles; QVERIFY(!game.restore(invalid));
            invalid = game.toJson(); auto options = invalid["options"].toObject(); options["spiderSuits"] = 3; invalid["options"] = options; QVERIFY(!game.restore(invalid));
            invalid = game.toJson(); invalid["removed"] = cardsJson({{1, 0, true}}); QVERIFY(!game.restore(invalid));
            invalid = game.toJson(); invalid["seed"] = 1.5; QVERIFY(!game.restore(invalid));
            QVERIFY(game.state() == before);
        }
    }
};
QTEST_GUILESS_MAIN(ModeTests)
#include "mode_tests.moc"
