#include "board.h"
#include <algorithm>
namespace solitare {
bool Board::isLayoutGame() const {
  return game_.state().mode == GameMode::Pyramid || game_.state().mode == GameMode::TriPeaks;
}
const std::vector<Card> *Board::pileCards(Location loc) const {
  const auto &state = game_.state();
  if (loc.kind == Location::Stock) return &state.stock;
  if (loc.kind == Location::Waste) return &state.waste;
  if (loc.kind == Location::Tableau && loc.pile >= 0 && loc.pile < int(state.tableau.size())) return &state.tableau[loc.pile];
  if (loc.kind == Location::Foundation && loc.pile >= 0 && loc.pile < int(state.foundations.size())) return &state.foundations[loc.pile];
  if (loc.kind == Location::Cell && loc.pile >= 0 && loc.pile < 4) return &state.cells[loc.pile];
  return nullptr;
}
std::vector<Location> Board::focusPiles() const {
  const auto &state = game_.state();
  std::vector<Location> locations;
  if (state.mode == GameMode::FreeCell) {
    for (int i = 0; i < 4; ++i) locations.push_back({Location::Cell, i, -1});
  } else {
    locations.push_back({Location::Stock, 0, -1});
    if (state.mode != GameMode::Spider) locations.push_back({Location::Waste, 0, -1});
  }
  if (!isLayoutGame())
    for (int i = 0; i < int(state.foundations.size()); ++i) locations.push_back({Location::Foundation, i, -1});
  for (int i = 0; i < int(state.tableau.size()); ++i)
    if (!isLayoutGame() || game_.exposed({Location::Tableau, i, -1})) locations.push_back({Location::Tableau, i, -1});
  return locations;
}
double Board::topInset() const {
  return std::clamp(height() * .14, 56.0, 100.0);
}
double Board::bottomInset() const {
  return std::clamp((height() - 500.0) * .35, 0.0, 140.0);
}
double Board::cardWidth() const {
  const auto mode = game_.state().mode;
  const int columns = mode == GameMode::Spider || mode == GameMode::TriPeaks ? 10 : mode == GameMode::FreeCell ? 8 : 7;
  double result = std::min(112.0, (width() - 44.0 - (columns - 1) * 10) / columns);
  const double availableHeight = height() - topInset() - bottomInset() - 72;
  double heightRatio = mode == GameMode::Pyramid ? 5.44 : mode == GameMode::TriPeaks ? 4.58 : 2.86;
  if (!isLayoutGame())
    for (const auto &pile : game_.state().tableau)
      heightRatio = std::max(heightRatio, 2.86 + (std::max(7, int(pile.size())) - 1) * .30);
  result = std::min(result, availableHeight / heightRatio);
  return std::max(24.0, result);
}
double Board::tableauStep(int pile) const {
  const auto count = game_.state().tableau[pile].size();
  const double w = cardWidth(), top = topInset() + w * 1.43 + 56;
  return std::clamp((height() - bottomInset() - top - w * 1.43 - 16) / std::max(1.0, double(count) - 1), 1.0, w * .32);
}
QRectF Board::cardRect(Location loc) const {
  const auto &state = game_.state();
  const double w = cardWidth(), h = w * 1.43, spacing = w + 10;
  const int columns = state.mode == GameMode::Spider || state.mode == GameMode::TriPeaks ? 10 : state.mode == GameMode::FreeCell ? 8 : 7;
  double x = (width() - (columns * spacing - 10)) / 2, y = topInset() + 12;
  if (loc.kind == Location::Waste) x += spacing;
  if (loc.kind == Location::Cell) x += loc.pile * spacing;
  if (loc.kind == Location::Foundation) x += (state.mode == GameMode::Spider ? loc.pile + 2 : state.mode == GameMode::FreeCell ? loc.pile + 4 : loc.pile + 3) * spacing;
  if (loc.kind == Location::Tableau) {
    y = topInset() + h + 56;
    if (state.mode == GameMode::Pyramid) {
      int row = 0, first = 0;
      while (row < 6 && loc.pile >= first + row + 1) first += ++row;
      x = (width() - (row + 1) * spacing + 10) / 2 + (loc.pile - first) * spacing;
      y += row * h * .28;
    } else if (state.mode == GameMode::TriPeaks) {
      const int index = loc.pile;
      double column = 0;
      int row = 0;
      if (index < 3) column = 1 + index * 3;
      else if (index < 9) { row = 1; const int n = index - 3; column = .5 + (n / 2) * 3 + n % 2; }
      else if (index < 18) { row = 2; column = index - 9; }
      else { row = 3; column = index - 18 - .5; }
      x = (width() - 10 * spacing + 10) / 2 + (column + .5) * spacing;
      y += row * h * .40;
    } else {
      x += loc.pile * spacing;
      const auto *cards = pileCards(loc);
      const int index = loc.index < 0 ? std::max(0, int(cards->size()) - 1) : loc.index;
      y += index * tableauStep(loc.pile);
    }
  }
  return {x, y, w, h};
}
std::optional<Location> Board::hit(QPointF point) const {
  const auto &state = game_.state();
  for (int pile = int(state.tableau.size()) - 1; pile >= 0; --pile) {
    const auto &cards = state.tableau[pile];
    for (int index = int(cards.size()) - 1; index >= 0; --index) {
      const Location location{Location::Tableau, pile, index};
      if (cardRect(location).contains(point)) return location;
    }
    if (!isLayoutGame() && cards.empty() && cardRect({Location::Tableau, pile, -1}).contains(point)) return Location{Location::Tableau, pile, -1};
  }
  for (const auto location : focusPiles())
    if (location.kind != Location::Tableau && cardRect(location).contains(point)) return location;
  return {};
}
} // namespace solitare
