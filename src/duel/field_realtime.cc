#include "duel/field_realtime.h"

#include <cstdlib>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <string>

#include <gflags/gflags.h>
#include <glog/logging.h>

#include "core/frame.h"
#include "core/kumipuyo.h"
#include "core/score.h"
#include "duel/frame_context.h"

using namespace std;

DEFINE_bool(delay_wnext, true, "Delay wnext appear");

// (n) means frames to transit to the next state.
//
// STATE_LEVEL_SELECT
//  v (12)
// STATE_PREPARING_NEXT <--------+
//  v (0)                        |
// STATE_PLAYABLE (?)            |
//  v (0)                        |
// STATE_DROPPING (?) <--+       |
//  v (20 or 0 if quick) |       |
// STATE_GROUNDING       |       |
//  v (0)                |       |
// STATE_VANISH          |       |
//  v  v (50)            | (0)   |
//  v  STATE_VANISHING --+       |
//  v (0)                        |
// STATE_OJAMA_DROPPING          |
//  v (20 or 0 if no ojama)      | (12)
// STATE_OJAMA_GROUNDING --------+
//  v
// STATE_DEAD

FieldRealtime::FieldRealtime(int playerId, const KumipuyoSeq& seq) :
    playerId_(playerId)
{
    // Since we don't use the first kumipuyo, we need to put EMPTY/EMPTY.
    vector<Kumipuyo> kps;
    kps.push_back(Kumipuyo(PuyoColor::EMPTY, PuyoColor::EMPTY));
    kps.insert(kps.end(), seq.begin(), seq.end());
    kumipuyoSeq_ = KumipuyoSeq(kps);

    init();
}

void FieldRealtime::init()
{
    playable_ = false;
    sleepFor_ = FPS;

    ojama_position_ = vector<int>(6, 0);
    ojama_dropping_ = false;
    current_chains_ = 0;
    delayFramesWNextAppear_ = 0;
    sent_wnext_appeared_ = false;
    drop_animation_ = false;
}

void FieldRealtime::setKeySetSeq(const KeySetSeq& kss)
{
    KeySetSeq expanded;
    for (const auto& ks : kss) {
        expanded.add(ks);
        expanded.add(ks);
    }
    keySetSeq_ = expanded;
}

bool FieldRealtime::onStateLevelSelect()
{
    transitToStatePreparingNext();
    // We always send 'grounded' before the initial puyo appear.
    userEvent_.grounded = true;
    return false;
}

void FieldRealtime::transitToStatePreparingNext()
{
    simulationState_ = SimulationState::STATE_PREPARING_NEXT;
    sleepFor_ = FRAMES_PREPARING_NEXT;
    playable_ = false;

    kms_ = KumipuyoMovingState(KumipuyoPos(3, 12, 0));

    if (!kumipuyoSeq_.isEmpty())
        kumipuyoSeq_.dropFront();
    if (FLAGS_delay_wnext)
        delayFramesWNextAppear_ = FRAMES_NEXT2_DELAY + FRAMES_PREPARING_NEXT;
    sent_wnext_appeared_ = false;
    allowsQuick_ = false;
}

bool FieldRealtime::onStatePreparingNext()
{
    simulationState_ = SimulationState::STATE_PLAYABLE;
    playable_ = true;
    userEvent_.decisionRequest = true;
    return false;
}

bool FieldRealtime::onStatePlayable(const KeySet& keySet, bool* accepted)
{
    playable_ = true;
    current_chains_ = 0;
    // TODO(mayah): We're always accepting KeySet? Then do we need to take |accepted| here?
    *accepted = true;

    bool downAccepted = false;
    kms_.moveKumipuyo(field_, keySet, &downAccepted);
    if (downAccepted)
        ++score_;

    if (kms_.grounded) {
        field_.setColor(kms_.pos.axisX(), kms_.pos.axisY(), kumipuyoSeq_.axis(0));
        field_.setColor(kms_.pos.childX(), kms_.pos.childY(), kumipuyoSeq_.child(0));
        playable_ = false;
        userEvent_.grounded = true;
        dropFast_ = false;
        dropFrameIndex_ = 0;
        dropRestFrames_ = 0;
        drop_animation_ = false;
        sleepFor_ = 0;
        simulationState_ = SimulationState::STATE_DROPPING;
    }

    return true;
}

bool FieldRealtime::onStateDropping()
{
    if (drop1Frame()) {
        drop_animation_ = true;
        return true;
    }

    bool wasDropping = drop_animation_;
    drop_animation_ = false;
    if (!wasDropping && allowsQuick_)
        sleepFor_ = 0;
    else
        sleepFor_ = FRAMES_GROUNDING;

    simulationState_ = SimulationState::STATE_GROUNDING;
    return false;
}

bool FieldRealtime::onStateGrounding()
{
    simulationState_ = SimulationState::STATE_VANISH;
    return false;
}

bool FieldRealtime::onStateVanish(FrameContext* context)
{
    // Don't vanish puyo here. Vanish it on onStateVanishing, to be consistent with wii_server.
    PlainField field(field_);
    int score = field.vanish(++current_chains_);
    if (score == 0) {
        if (context)
            context->commitOjama();
        sleepFor_ = 0;
        drop_animation_ = false;
        simulationState_ = SimulationState::STATE_OJAMA_DROPPING;
        return false;
    }

    userEvent_.puyoErased = true;

    // Here, something has been vanished.
    score_ += score;

    // Set Yokoku Ojama.
    if (score_ - scoreConsumed_ >= SCORE_FOR_OJAMA) {
        int attack_ojama = (score_ - scoreConsumed_) / SCORE_FOR_OJAMA;
        if (context)
            context->sendOjama(attack_ojama);
        scoreConsumed_ = score_ / SCORE_FOR_OJAMA * SCORE_FOR_OJAMA;
    }

    // After ojama is calculated, we add ZENKESHI score,
    // because score for ZENKESHI is added, but not used for ojama calculation.
    hasZenkeshi_ = false;
    if (field.isZenkeshi()) {
        score_ += ZENKESHI_BONUS;
        hasZenkeshi_ = true;
    }

    simulationState_ = SimulationState::STATE_VANISHING;

    dropFast_ = true;
    dropFrameIndex_ = 0;
    dropRestFrames_ = 0;

    sleepFor_ = FRAMES_VANISH_ANIMATION;
    allowsQuick_ = true;
    return false;
}

bool FieldRealtime::onStateVanishing()
{
    int score = field_.vanish(current_chains_);
    DCHECK_GT(score, 0);
    simulationState_ = SimulationState::STATE_DROPPING;
    return false;
}

bool FieldRealtime::onStateOjamaDropping()
{
    if (!ojama_dropping_) {
        ojama_position_ = determineColumnOjamaAmount();
        for (int i = 0; i < 6; i++) {
            if (ojama_position_[i] > 0) {
                ojama_dropping_ = true;
                break;
            }
        }

        int ojamaAmount = 0;
        for (int i = 0; i < 6; ++i)
            ojamaAmount += ojama_position_[i];

        ojamaDroppingAmount_ = ojamaAmount;
        dropFast_ = false;
        dropFrameIndex_ = 0;
        dropRestFrames_ = 0;
    }

    bool ojamaWasDropping = ojama_dropping_;
    if (ojama_dropping_) {
        for (int i = 0; i < 6; i++) {
            if (ojama_position_[i] > 0) {
                if (field_.color(i + 1, 13) == PuyoColor::EMPTY) {
                    field_.setColor(i + 1, 13, PuyoColor::OJAMA);
                    ojama_position_[i]--;
                }
            }
        }

        if (drop1Frame())
            return true;
    }

    if (!ojamaWasDropping) {
        sleepFor_ = 0;
    } else {
        sleepFor_ = framesGroundingOjama(ojamaDroppingAmount_);
        ojamaDroppingAmount_ = 0;
    }

    simulationState_ = SimulationState::STATE_OJAMA_GROUNDING;
    return false;
}

bool FieldRealtime::onStateOjamaGrounding()
{
    if (ojama_dropping_)
        userEvent_.ojamaDropped = true;
    ojama_dropping_ = false;

    if (field_.color(3, 12) != PuyoColor::EMPTY) {
        simulationState_ = SimulationState::STATE_DEAD;
        return false;
    }

    transitToStatePreparingNext();
    return false;
}

bool FieldRealtime::onStateDead()
{
    return true;
}

// Returns true if a key input is accepted.
bool FieldRealtime::playOneFrame(const KeySet& keySet, FrameContext* context)
{
    playable_ = false;
    userEvent_.clear();

    if (delayFramesWNextAppear_ > 0)
        --delayFramesWNextAppear_;

    if (delayFramesWNextAppear_ == 0 && !sent_wnext_appeared_) {
        userEvent_.wnextAppeared = true;
        sent_wnext_appeared_ = true;
    }

    // Loop until some functionality consumes this frame.
    while (true) {
        if (sleepFor_ > 0) {
            sleepFor_--;
            return false;
        }

        switch (simulationState_) {
        case SimulationState::STATE_LEVEL_SELECT:
            if (onStateLevelSelect())
                return false;
            continue;
        case SimulationState::STATE_PREPARING_NEXT:
            if (onStatePreparingNext())
                return false;
            continue;
        case SimulationState::STATE_PLAYABLE: {
            bool accepted = false;
            if (onStatePlayable(keySet, &accepted))
                return accepted;
            continue;
        }
        case SimulationState::STATE_DROPPING:
            if (onStateDropping())
                return false;
            continue;
        case SimulationState::STATE_GROUNDING:
            if (onStateGrounding())
                return false;
            continue;
        case SimulationState::STATE_VANISH:
            if (onStateVanish(context))
                return false;
            continue;
        case SimulationState::STATE_VANISHING:
            if (onStateVanishing())
                return false;
            continue;
        case SimulationState::STATE_OJAMA_DROPPING:
            if (onStateOjamaDropping())
                return false;
            continue;
        case SimulationState::STATE_OJAMA_GROUNDING:
            if (onStateOjamaGrounding())
                return false;
            continue;
        case SimulationState::STATE_DEAD:
            if (onStateDead())
                return false;
            continue;
        }

        CHECK(false) << "Unknown state?";
    }  // end while

    DCHECK(false) << "should not reached here.";
    return false;
}

bool FieldRealtime::drop1Frame()
{
    if (dropRestFrames_ > 0) {
        dropRestFrames_ -= 1;
        return true;
    }

    bool stillDropping = false;
    // Puyo in 14th row will not drop to 13th row. If there is a puyo on
    // 14th row, it'll stay there forever. This behavior is a famous bug in
    // Puyo2.
    for (int x = 1; x <= FieldConstant::WIDTH; x++) {
        for (int y = 1; y < 13; y++) {
            if (field_.color(x, y) != PuyoColor::EMPTY)
                continue;

            if (field_.color(x, y + 1) != PuyoColor::EMPTY) {
                stillDropping = true;
                field_.setColor(x, y, field_.color(x, y + 1));
                field_.setColor(x, y + 1, PuyoColor::EMPTY);
            }
        }
    }

    if (stillDropping) {
        if (dropFast_) {
            dropRestFrames_ = FRAMES_TO_DROP_FAST[dropFrameIndex_ + 1] - FRAMES_TO_DROP_FAST[dropFrameIndex_];
        } else {
            dropRestFrames_ = FRAMES_TO_DROP[dropFrameIndex_ + 1] - FRAMES_TO_DROP[dropFrameIndex_];
        }
        dropFrameIndex_ += 1;
    }

    return stillDropping;
}

int FieldRealtime::reduceOjama(int n)
{
    if (numPendingOjama_ >= n) {
        numPendingOjama_ -= n;
        n = 0;
    } else {
        n -= numPendingOjama_;
        numPendingOjama_ = 0;
    }

    if (numFixedOjama_ >= n) {
        numFixedOjama_ -= n;
        n = 0;
    } else {
        n -= numFixedOjama_;
        numFixedOjama_ = 0;
    }

    return n;
}

// Returns what column should drop how many Ojama puyos. The returned vector
// has 6 elements.
// If there are more than 30 Ojama puyos to drop, all column will have 5.
vector<int> FieldRealtime::determineColumnOjamaAmount()
{
    int dropOjama = numFixedOjama_ >= 30 ? 30 : numFixedOjama_;
    numFixedOjama_ -= dropOjama;

    // Decide which column to drop.
    int positions[6] = {0};
    for (int i = 0; i < 6; i++)
        positions[i] = i;
    for (int i = 1; i < 6; i++)
        swap(positions[i], positions[rand() % (i+1)]);

    vector<int> ret(6, 0);
    int lines = dropOjama / 6;
    dropOjama %= 6;
    for (int i = 0; i < dropOjama; i++) {
        ret[positions[i]] = lines + 1;
    }
    for (int i = dropOjama; i < 6; i++) {
        ret[positions[i]] = lines;
    }

    return ret;
}

KumipuyoSeq FieldRealtime::visibleKumipuyoSeq() const
{
    KumipuyoSeq seq;
    seq.add(kumipuyo(0));
    seq.add(kumipuyo(1));
    if (kumipuyo(2).isValid())
        seq.add(kumipuyo(2));
    return seq;
}

Kumipuyo FieldRealtime::kumipuyo(int nth) const
{
    if (nth >= 2 && delayFramesWNextAppear_ > 0)
        return Kumipuyo(PuyoColor::EMPTY, PuyoColor::EMPTY);
    return kumipuyoSeq_.get(nth);
}

void FieldRealtime::skipLevelSelect()
{
    CHECK(simulationState_ == SimulationState::STATE_LEVEL_SELECT);
    transitToStatePreparingNext();
}

void FieldRealtime::skipPreparingNext()
{
    CHECK(simulationState_ == SimulationState::STATE_PREPARING_NEXT);
    simulationState_ = SimulationState::STATE_PLAYABLE;
    sleepFor_ = 0;
}
