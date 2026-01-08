////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2023, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
////////////////////////////////////////////////////////////////

#include <ork/kernel/timer.h>
#include <ork/kernel/opq.h>
#include <string.h>
#include <ork/util/fsm.h>
#include <utpp/UnitTest++.h>

using namespace ork;
using namespace ork::fsm;

static void logstate(const char* pstr) {
  printf("%s", pstr);
}

struct ROOT : public State {
  ROOT(FsmData* data)
      : State(data) {
  }
  void onEnter(fsminstance_ptr_t inst) override {
    logstate("ROOT.enter\n");
  }
  void onExit(fsminstance_ptr_t inst) override {
    logstate("ROOT.exit\n");
  }
  void onUpdate(fsminstance_ptr_t inst) override {
    logstate("ROOT.update\n");
  }
};

// Token-based events
static constexpr fsm_event_t e1to2 = "e1to2"_crcu;
static constexpr fsm_event_t e2to3 = "e2to3"_crcu;

///////////////////////////////////////////////////////////////////////
// deterministic fsm unit test
///////////////////////////////////////////////////////////////////////

TEST(hfsm_1) {
  for (int i = 0; i < 3; i++) {
    logstate("//hfsm_1/////////////////////////\n");

    // Create shared data
    auto data = std::make_shared<FsmData>();

    auto the_root     = data->newState<ROOT>();
    auto the_sa       = data->newState<LambdaState>(the_root);
    auto the_sb       = data->newState<LambdaState>(the_root);
    the_sa->_onenter  = [](fsminstance_ptr_t) { logstate("sa.enter\n"); };
    the_sa->_onexit   = [](fsminstance_ptr_t) { logstate("sa.exit\n"); };
    the_sa->_onupdate = [](fsminstance_ptr_t) { logstate("sa.update\n"); };
    the_sb->_onenter  = [](fsminstance_ptr_t) { logstate("sb.enter\n"); };
    the_sb->_onexit   = [](fsminstance_ptr_t) { logstate("sb.exit\n"); };
    the_sb->_onupdate = [](fsminstance_ptr_t) { logstate("sb.update\n"); };
    auto the_s1       = data->newState<LambdaState>(the_sa);
    auto the_s2       = data->newState<LambdaState>(the_sa);
    auto the_s3       = data->newState<LambdaState>(the_sb);
    the_s1->_onenter  = [](fsminstance_ptr_t) { logstate("s1.enter\n"); };
    the_s1->_onexit   = [](fsminstance_ptr_t) { logstate("s1.exit\n"); };
    the_s1->_onupdate = [](fsminstance_ptr_t) { logstate("s1.update\n"); };
    the_s2->_onenter  = [](fsminstance_ptr_t) { logstate("s2.enter\n"); };
    the_s2->_onexit   = [](fsminstance_ptr_t) { logstate("s2.exit\n"); };
    the_s2->_onupdate = [](fsminstance_ptr_t) { logstate("s2.update\n"); };
    the_s3->_onenter  = [](fsminstance_ptr_t) { logstate("s3.enter\n"); };
    the_s3->_onexit   = [](fsminstance_ptr_t) { logstate("s3.exit\n"); };
    the_s3->_onupdate = [](fsminstance_ptr_t) { logstate("s3.update\n"); };

    data->addTransition(the_s1, e1to2, the_s2);
    data->addTransition(the_s2, e2to3, the_s3);

    // Create instance
    auto inst = FsmInstance::create(data);

    inst->changeState(the_s1);
    inst->sendEvent(e1to2);
    inst->sendEvent(e2to3);

    while (inst->currentState() != the_s3) {
      FsmInstance::update(inst);
    }
  }
}

///////////////////////////////////////////////////////////////////////
// probalistic fsm unit test
///////////////////////////////////////////////////////////////////////

TEST(hfsm_probalistic_1) {
  for (int i = 0; i < 10; i++) {
    logstate("//hfsm_probalistic_1/////////////////////////\n");

    auto data = std::make_shared<FsmData>();

    auto the_root     = data->newState<ROOT>();
    auto the_sa       = data->newState<LambdaState>(the_root);
    auto the_sb       = data->newState<LambdaState>(the_root);
    the_sa->_onenter  = [](fsminstance_ptr_t) { logstate("sa.enter\n"); };
    the_sa->_onexit   = [](fsminstance_ptr_t) { logstate("sa.exit\n"); };
    the_sa->_onupdate = [](fsminstance_ptr_t) { logstate("sa.update\n"); };
    the_sb->_onenter  = [](fsminstance_ptr_t) { logstate("sb.enter\n"); };
    the_sb->_onexit   = [](fsminstance_ptr_t) { logstate("sb.exit\n"); };
    the_sb->_onupdate = [](fsminstance_ptr_t) { logstate("sb.update\n"); };
    auto the_s1       = data->newState<LambdaState>(the_sa);
    auto the_s2       = data->newState<LambdaState>(the_sa);
    auto the_s3       = data->newState<LambdaState>(the_sb);
    the_s1->_onenter  = [](fsminstance_ptr_t) { logstate("s1.enter\n"); };
    the_s1->_onexit   = [](fsminstance_ptr_t) { logstate("s1.exit\n"); };
    the_s1->_onupdate = [](fsminstance_ptr_t) { logstate("s1.update\n"); };
    the_s2->_onenter  = [](fsminstance_ptr_t) { logstate("s2.enter\n"); };
    the_s2->_onexit   = [](fsminstance_ptr_t) { logstate("s2.exit\n"); };
    the_s2->_onupdate = [](fsminstance_ptr_t) { logstate("s2.update\n"); };
    the_s3->_onenter  = [](fsminstance_ptr_t) { logstate("s3.enter\n"); };
    the_s3->_onexit   = [](fsminstance_ptr_t) { logstate("s3.exit\n"); };
    the_s3->_onupdate = [](fsminstance_ptr_t) { logstate("s3.update\n"); };

    auto probability_lambda = [](fsminstance_ptr_t) -> bool {
      int i      = rand() & 0xff;
      bool bprob = i < 0x7f;
      printf("bprob<%d>\n", int(bprob));
      return bprob;
    };

    PredicatedTransition trans_2(the_s2, probability_lambda);
    PredicatedTransition trans_3(the_s3, probability_lambda);

    data->addTransition(the_s1, e1to2, trans_2);
    data->addTransition(the_s2, e2to3, trans_3);

    // Create instance
    auto inst = FsmInstance::create(data);

    inst->changeState(the_s1);

    for (int j = 0; j < 3; j++) {
      inst->sendEvent(e1to2);
      FsmInstance::update(inst);
    }
    for (int j = 0; j < 3; j++) {
      inst->sendEvent(e2to3);
      FsmInstance::update(inst);
    }

    //////////////////////////////////////
    // Test explicit state change to nullptr
    //////////////////////////////////////

    inst->changeState(nullptr);
    FsmInstance::update(inst);
    assert(inst->currentState() == nullptr);
  }
}
