#include "game.h"
#include "game_rules.h"
#include <QJsonArray>
#include <algorithm>
#include <cmath>
#include <limits>

namespace solitare {
namespace {
using CardCounts = std::array<std::array<int, 13>, 4>;

bool integer(QJsonValue value, int minimum, int maximum, int& output) {
    if (!value.isDouble()) return false;
    const double number = value.toDouble();
    if (!std::isfinite(number) || std::trunc(number) != number || number < minimum || number > maximum) return false;
    output = static_cast<int>(number);
    return true;
}

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

bool parseCards(QJsonValue value, std::vector<Card>& result, CardCounts& counts, int faceState) {
    if (!value.isArray() || value.toArray().size() > 104) return false;
    for (const auto& entry : value.toArray()) {
        if (!entry.isObject()) return false;
        const auto card = entry.toObject();
        int rank = 0, suit = 0;
        if (card.size() != 3 || !card.value("faceUp").isBool() || !integer(card.value("rank"), 1, 13, rank) || !integer(card.value("suit"), 0, 3, suit)) return false;
        const bool faceUp = card.value("faceUp").toBool();
        if (faceState >= 0 && faceUp != (faceState == 1)) return false;
        ++counts[suit][rank - 1];
        result.push_back({rank, suit, faceUp});
    }
    return true;
}

template<class Piles> bool parsePiles(QJsonValue value, Piles& result, CardCounts& counts, int faceState) {
    if (!value.isArray()) return false;
    const auto array = value.toArray();
    if (array.size() != static_cast<int>(result.size())) return false;
    for (int index = 0; index < array.size(); ++index) if (!parseCards(array[index], result[index], counts, faceState)) return false;
    return true;
}

bool parseOptions(const QJsonObject& json, State& result) {
    if (!json.value("mode").isString() || !json.value("options").isObject()) return false;
    bool found = false;
    for (auto mode : {GameMode::Klondike, GameMode::Spider, GameMode::FreeCell, GameMode::Pyramid, GameMode::TriPeaks}) {
        if (json.value("mode").toString() == QLatin1String(modeId(mode))) { result.mode = mode; found = true; }
    }
    const auto options = json.value("options").toObject();
    if (!found || options.size() != 4 || !integer(options.value("drawCount"), 1, 3, result.options.drawCount) || result.options.drawCount != result.drawCount || !integer(options.value("spiderSuits"), 1, 4, result.options.spiderSuits) || result.options.spiderSuits == 3 || !integer(options.value("pyramidRedeals"), 0, 2, result.options.pyramidRedeals) || !options.value("tripeaksWraparound").isBool()) return false;
    result.options.tripeaksWraparound = options.value("tripeaksWraparound").toBool();
    return integer(json.value("redeals"), 0, result.mode == GameMode::Pyramid ? result.options.pyramidRedeals : 0, result.redeals);
}

bool validFoundation(const std::vector<Card>& pile, bool spider) {
    if (spider && !pile.empty() && pile.size() != 13) return false;
    for (int index = 0; index < static_cast<int>(pile.size()); ++index) {
        if (pile[index].rank != (spider ? 13 - index : index + 1) || pile[index].suit != pile.front().suit) return false;
    }
    return true;
}

bool validTableau(const State& state, const std::vector<Card>& pile) {
    if (state.mode == GameMode::Pyramid || state.mode == GameMode::TriPeaks) return pile.size() <= 1 && (pile.empty() || pile.back().faceUp);
    if (!pile.empty() && !pile.back().faceUp) return false;
    bool faceUpSeen = false;
    for (int index = 0; index < static_cast<int>(pile.size()); ++index) {
        if (faceUpSeen && !pile[index].faceUp) return false;
        if (state.mode == GameMode::FreeCell && !pile[index].faceUp) return false;
        if (pile[index].faceUp) {
            faceUpSeen = true;
            if (state.mode == GameMode::Klondike && !rules::validRun(pile, index, false)) return false;
        }
    }
    return true;
}

bool validLayout(const State& state, const CardCounts& counts) {
    const bool spider = state.mode == GameMode::Spider;
    const bool triangle = state.mode == GameMode::Pyramid || state.mode == GameMode::TriPeaks;
    for (int suit = 0; suit < 4; ++suit) for (int rank = 0; rank < 13; ++rank) {
        const int expected = spider ? (suit < state.options.spiderSuits ? 8 / state.options.spiderSuits : 0) : 1;
        if (counts[suit][rank] != expected) return false;
    }
    if ((spider || state.mode == GameMode::FreeCell) && !state.waste.empty()) return false;
    if (state.mode == GameMode::FreeCell && !state.stock.empty()) return false;
    if (spider && (state.stock.size() > 50 || state.stock.size() % 10 != 0)) return false;
    if (state.mode != GameMode::Pyramid && !state.removed.empty()) return false;
    if (state.mode == GameMode::TriPeaks && state.waste.empty()) return false;
    for (const auto& cell : state.cells) if (cell.size() > (state.mode == GameMode::FreeCell ? 1u : 0u)) return false;
    for (const auto& pile : state.foundations) if ((triangle && !pile.empty()) || !validFoundation(pile, spider)) return false;
    for (const auto& pile : state.tableau) if (!validTableau(state, pile)) return false;
    return true;
}

bool parseState(const QJsonObject& json, State& result, std::uint32_t& seed) {
    int version = 0;
    if (!integer(json.value("version"), 1, 2, version) || json.size() != (version == 1 ? 8 : 13)) return false;
    const auto seedValue = json.value("seed");
    const double seedNumber = seedValue.toDouble(-1);
    if (!seedValue.isDouble() || !std::isfinite(seedNumber) || std::trunc(seedNumber) != seedNumber || seedNumber < 0 || seedNumber > std::numeric_limits<std::uint32_t>::max()) return false;
    seed = static_cast<std::uint32_t>(seedNumber);
    if (!integer(json.value("drawCount"), 1, 3, result.drawCount) || result.drawCount == 2 || !integer(json.value("moves"), 0, std::numeric_limits<int>::max(), result.moves)) return false;
    result.options.drawCount = result.drawCount;
    if (version == 2 && !parseOptions(json, result)) return false;
    result.tableau.resize(result.mode == GameMode::Spider ? 10 : result.mode == GameMode::FreeCell ? 8 : result.mode == GameMode::Klondike ? 7 : 28);
    result.foundations.resize(result.mode == GameMode::Spider ? 8 : 4);
    CardCounts counts{};
    if (!parseCards(json.value("stock"), result.stock, counts, 0) || !parseCards(json.value("waste"), result.waste, counts, 1) || !parsePiles(json.value("foundations"), result.foundations, counts, 1) || !parsePiles(json.value("tableau"), result.tableau, counts, -1)) return false;
    if (version == 2 && (!parsePiles(json.value("cells"), result.cells, counts, 1) || !parseCards(json.value("removed"), result.removed, counts, 1))) return false;
    return validLayout(result, counts);
}
}

QJsonObject Game::toJson() const {
    const auto& options = state_.options;
    return {{"version", 2}, {"seed", static_cast<double>(seed_)}, {"drawCount", state_.drawCount}, {"moves", state_.moves}, {"stock", cardsJson(state_.stock)}, {"waste", cardsJson(state_.waste)}, {"foundations", pilesJson(state_.foundations)}, {"tableau", pilesJson(state_.tableau)}, {"mode", modeId(state_.mode)}, {"options", QJsonObject{{"drawCount", options.drawCount}, {"spiderSuits", options.spiderSuits}, {"pyramidRedeals", options.pyramidRedeals}, {"tripeaksWraparound", options.tripeaksWraparound}}}, {"cells", pilesJson(state_.cells)}, {"removed", cardsJson(state_.removed)}, {"redeals", state_.redeals}};
}

bool Game::restore(const QJsonObject& json) {
    State restored;
    std::uint32_t seed = 0;
    if (!parseState(json, restored, seed)) return false;
    state_ = std::move(restored);
    seed_ = seed;
    undo_.clear();
    return true;
}
} // namespace solitare
