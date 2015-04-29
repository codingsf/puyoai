#include "evaluator.h"

#include <numeric>
#include <sstream>

#include "core/algorithm/plan.h"
#include "core/algorithm/rensa_detector.h"
#include "core/kumipuyo_seq.h"
#include "cpu/peria/pattern.h"

namespace peria {

int ScoreDiffHeight(int higher, int lower) {
  int diff = higher - lower;
  diff = diff * diff;
  return ((higher > lower) ? 0 : -diff) * 50;
}

int Evaluator::PatternMatch(const CoreField& field, std::string* name) {
  int best = 0;
  for (const Pattern& pattern : Pattern::GetAllPattern()) {
    int score = pattern.Match(field);
    if (score > best) {
      best = score;

      std::ostringstream oss;
      oss << pattern.name() << "[" << score << "/" << pattern.score() << "]";
      *name = oss.str();
    }
  }

  return best;
}

int Evaluator::Field(const CoreField& field) {
  int score = 0;

  int num_connect = 0;
  for (int x = 1; x < PlainField::WIDTH; ++x) {
    int height = field.height(x);
    for (int y = 1; y <= height; ++y) {
      PuyoColor c = field.color(x, y);
      if (c == PuyoColor::OJAMA)
        continue;
      if (c == field.color(x + 1, y))
        ++num_connect;
      if (c == field.color(x, y + 1))
        ++num_connect;
    }
  }
  score += num_connect * 2;

  score += ScoreDiffHeight(field.height(1), field.height(2));
  score += ScoreDiffHeight(field.height(2), field.height(3));
  score += ScoreDiffHeight(field.height(5), field.height(4));
  score += ScoreDiffHeight(field.height(6), field.height(5));

  return score;
}

int Evaluator::Future(const CoreField& field) {
  std::vector<int> expects;
  Plan::iterateAvailablePlans(
      field, KumipuyoSeq(), 1,
      [&expects](const RefPlan& p) {
        if (p.isRensaPlan())
          expects.push_back(p.rensaResult().score);
      });
  int value = std::accumulate(expects.begin(), expects.end(), 0);
  if (expects.size()) {
    value /= expects.size();
  }
  return value;
}

int Evaluator::Plan(const CoreField& field,
                    const RensaChainTrackResult& track) {
  using namespace std::placeholders;
  RensaChainTrackResult track_result;
  int max_score = 0;
  RensaDetector::iteratePossibleRensasIteratively(
      field, 1, RensaDetectorStrategy::defaultFloatStrategy(),
      [&max_score, &track_result](const CoreField&,
                                  const RensaResult& result,
                                  const ColumnPuyoList&,
                                  const RensaChainTrackResult& track) {
        if (result.score > max_score) {
          max_score = result.score;
          track_result = track;
        }
      });

  int count = 0;
  for (int x = 1; x <= PlainField::WIDTH; ++x) {
    for (int y = 1; y <= PlainField::HEIGHT; ++y) {
      if (track_result.erasedAt(x, y) == 0)
        continue;
      if (track_result.erasedAt(x, y) == track.erasedAt(x, y) + 1)
        ++count;
    }
  }

  return count * 20;
}

}  // namespace peria