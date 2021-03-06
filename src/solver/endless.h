#ifndef SOLVER_ENDLESS_H_
#define SOLVER_ENDLESS_H_

#include <memory>
#include <vector>

#include "core/client/ai/ai.h"
#include "core/decision.h"

struct FrameRequest;
class KumipuyoSeq;

struct EndlessResult {
    enum class Type {
        DEAD,
        MAIN_CHAIN,
        ZENKESHI,
        PUYOSEQ_RUNOUT,
    };
    int hand;
    int score;
    int maxRensa;
    bool zenkeshi;
    std::vector<Decision> decisions;
    Type type;
};

// Endless implements endless mode. This can be used to check your AI's strength.
// Just pass your AI to constructor, and call run(). KumipuyoSeq can be generated with
// core/sequence_generator.h
class Endless {
public:
    explicit Endless(std::unique_ptr<AI> ai);

    EndlessResult run(const KumipuyoSeq&);

    // Sets verbose mode. This will show fields in each hand.
    void setVerbose(bool flag) { verbose_ = flag; }

private:
    void setEnemyField(FrameRequest* req);

    std::unique_ptr<AI> ai_;
    bool verbose_ = false;
};

#endif
