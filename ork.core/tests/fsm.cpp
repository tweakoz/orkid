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
  ROOT(StateMachine* machine)
      : State(machine) {
  }
  void OnEnter() {
    logstate("ROOT.enter\n");
  }
  void OnExit() {
    logstate("ROOT.exit\n");
  }
  void OnUpdate() {
    logstate("ROOT.update\n");
  }
};

struct e1to2 {};
struct e2to3 {};

///////////////////////////////////////////////////////////////////////
// deterministic fsm unit test
///////////////////////////////////////////////////////////////////////

TEST(hfsm_1) {
  for (int i = 0; i < 3; i++) {
    logstate("//hfsm_1/////////////////////////\n");
    StateMachine the_SM;

    auto the_root     = the_SM.NewState<ROOT>();
    auto the_sa       = the_SM.NewState<LambdaState>(the_root);
    auto the_sb       = the_SM.NewState<LambdaState>(the_root);
    the_sa->_onenter  = []() { logstate("sa.enter\n"); };
    the_sa->_onexit   = []() { logstate("sa.exit\n"); };
    the_sa->_onupdate = []() { logstate("sa.update\n"); };
    the_sb->_onenter  = []() { logstate("sb.enter\n"); };
    the_sb->_onexit   = []() { logstate("sb.exit\n"); };
    the_sb->_onupdate = []() { logstate("sb.update\n"); };
    auto the_s1       = the_SM.NewState<LambdaState>(the_sa);
    auto the_s2       = the_SM.NewState<LambdaState>(the_sa);
    auto the_s3       = the_SM.NewState<LambdaState>(the_sb);
    the_s1->_onenter  = []() { logstate("s1.enter\n"); };
    the_s1->_onexit   = []() { logstate("s1.exit\n"); };
    the_s1->_onupdate = []() { logstate("s1.update\n"); };
    the_s2->_onenter  = []() { logstate("s2.enter\n"); };
    the_s2->_onexit   = []() { logstate("s2.exit\n"); };
    the_s2->_onupdate = []() { logstate("s2.update\n"); };
    the_s3->_onenter  = []() { logstate("s3.enter\n"); };
    the_s3->_onexit   = []() { logstate("s3.exit\n"); };
    the_s3->_onupdate = []() { logstate("s3.update\n"); };

    the_SM.AddTransition(the_s1, trans_key<e1to2>(), the_s2);
    the_SM.AddTransition(the_s2, trans_key<e2to3>(), the_s3);

    the_SM.QueueStateChange(the_s1);
    the_SM.QueueEvent(e1to2());
    the_SM.QueueEvent(e2to3());

    while (the_SM.currentState() != the_s3) {
      the_SM.Update();
    }
  }
}

///////////////////////////////////////////////////////////////////////
// probalistic fsm unit test
///////////////////////////////////////////////////////////////////////

TEST(hfsm_probalistic_1) {
  for (int i = 0; i < 10; i++) {
    logstate("//hfsm_probalistic_1/////////////////////////\n");
    StateMachine the_SM;

    auto the_root     = the_SM.NewState<ROOT>();
    auto the_sa       = the_SM.NewState<LambdaState>(the_root);
    auto the_sb       = the_SM.NewState<LambdaState>(the_root);
    the_sa->_onenter  = []() { logstate("sa.enter\n"); };
    the_sa->_onexit   = []() { logstate("sa.exit\n"); };
    the_sa->_onupdate = []() { logstate("sa.update\n"); };
    the_sb->_onenter  = []() { logstate("sb.enter\n"); };
    the_sb->_onexit   = []() { logstate("sb.exit\n"); };
    the_sb->_onupdate = []() { logstate("sb.update\n"); };
    auto the_s1       = the_SM.NewState<LambdaState>(the_sa);
    auto the_s2       = the_SM.NewState<LambdaState>(the_sa);
    auto the_s3       = the_SM.NewState<LambdaState>(the_sb);
    the_s1->_onenter  = []() { logstate("s1.enter\n"); };
    the_s1->_onexit   = []() { logstate("s1.exit\n"); };
    the_s1->_onupdate = []() { logstate("s1.update\n"); };
    the_s2->_onenter  = []() { logstate("s2.enter\n"); };
    the_s2->_onexit   = []() { logstate("s2.exit\n"); };
    the_s2->_onupdate = []() { logstate("s2.update\n"); };
    the_s3->_onenter  = []() { logstate("s3.enter\n"); };
    the_s3->_onexit   = []() { logstate("s3.exit\n"); };
    the_s3->_onupdate = []() { logstate("s3.update\n"); };

    auto probability_lambda = []() -> bool {
      int i      = rand() & 0xff;
      bool bprob = i < 0x7f;
      printf("bprob<%d>\n", int(bprob));
      return bprob;
    };

    PredicatedTransition trans_2(the_s2, probability_lambda);
    PredicatedTransition trans_3(the_s3, probability_lambda);

    the_SM.AddTransition(the_s1, trans_key<e1to2>(), trans_2);
    the_SM.AddTransition(the_s2, trans_key<e2to3>(), trans_3);

    the_SM.QueueStateChange(the_s1);

    for (int i = 0; i < 3; i++) {
      the_SM.QueueEvent(e1to2());
      the_SM.Update();
    }
    for (int i = 0; i < 3; i++) {
      the_SM.QueueEvent(e2to3());
      the_SM.Update();
    }

    //////////////////////////////////////
    // usually the StateMachine destructor will do this
    //  but we want to test it explicitly right now
    //  RAII compliance will be in a separate test
    //////////////////////////////////////

    the_SM.QueueStateChange(nullptr);
    the_SM.Update();
    assert(the_SM.currentState() == nullptr);
  }
}
