#include <iostream>
#include <fstream>
#include <stdlib.h>

#include <gflags/gflags.h>
#include <glog/logging.h>

#include "core/algorithm/puyo_possibility.h"
#include "core/algorithm/plan.h"
#include "core/algorithm/rensa_info.h"
#include "core/algorithm/rensa_detector.h"
#include "core/client/connector/drop_decision.h"
#include "core/field/core_field.h"
#include "core/frame_data.h"
#include "core/kumipuyo.h"
#include "core/sequence_generator.h"
#include "core/state.h"

#include "mayah_ai.h"
#include "util.h"

using namespace std;

class InteractiveAI : public MayahAI {
public:
    using MayahAI::gameWillBegin;
    using MayahAI::think;
    using MayahAI::enemyNext2Appeared;
    using MayahAI::collectFeatureString;
};

// TODO(mayah): Implement with GUI!
int main(int argc, char* argv[])
{
    google::ParseCommandLineFlags(&argc, &argv, true);
    google::InitGoogleLogging(argv[0]);
    google::InstallFailureSignalHandler();

    TsumoPossibility::initialize();

    {
        double t1 = now();
        CoreField f;
        KumipuyoSeq seq("RRBB");
        Plan::iterateAvailablePlans(f, seq, 4, [](const RefPlan&) {
            return;
        });
        double t2 = now();
        cout << (t2 - t1) << endl;
    }

    InteractiveAI ai;

    CoreField field;
    KumipuyoSeq seq = generateSequence();

    for (int i = 0; i + 1 < seq.size(); ++i) {
        int frameId = 2 + i; // frameId 1 will be used for initializing now. Let's avoid it.

        FrameData fd;
        fd.id = frameId;
        fd.valid = true;

        // Set up enemy field.
        // Make enemy will fire his large rensa.
        fd.playerFrameData[1].field = CoreField(
            "500065"
            "400066"
            "545645"
            "456455"
            "545646"
            "545646"
            "564564"
            "456456"
            "456456"
            "456456");
        fd.playerFrameData[1].kumipuyoSeq = KumipuyoSeq("666666");

        // Invoke Gazer.
        {
            double t1 = now();
            ai.enemyNext2Appeared(fd);
            double t2 = now();
            cout << "gazer time = " << (t2 - t1) << endl;
        }

        cout << field.toDebugString() << endl;
        cout << seq.get(i).toString() << " " << seq.get(i + 1).toString() << endl;

        double t1 = now();
        Plan plan = ai.thinkPlan(frameId, field, KumipuyoSeq { seq.get(i), seq.get(i + 1) }, false);
        double t2 = now();
        if (plan.decisions().empty())
            cout << "No decision";
        else
            cout << plan.decision(0).toString() << "-" << plan.decision(1).toString();
        cout << " time = " << ((t2 - t1) * 1000) << " [ms]" << endl;

        // Waits for user enter.
        while (true) {
            string str;
            cout << "command? ";
            getline(cin, str);

            if (str == "")
                break;
            if (str == "s") {
                double beginTime = now();
                Plan plan = ai.thinkPlan(frameId, field, KumipuyoSeq { seq.get(i), seq.get(i + 1) }, false);
                double endTime = now();
                string str = ai.collectFeatureString(frameId, field, KumipuyoSeq { seq.get(i), seq.get(i + 1) }, false,
                                                     plan, endTime - beginTime);
                cout << str << endl;
            }
        }

        field.dropKumipuyo(plan.decisions().front(), seq.get(i));
        field.simulate();
    }

    return 0;
}
