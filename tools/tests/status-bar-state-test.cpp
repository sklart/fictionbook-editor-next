#include "../../src/fbe/StatusBarState.h"
#include <cstdio>
#include <cstdlib>
static void Need(bool value,const char* label){if(!value){std::fprintf(stderr,"%s\n",label);std::exit(1);}}
int main(){
    FBEStatusBar::State state;state.SetContext(L"context");state.SetTransient(L"transient",100,1000);
    CString script=L"script";
    Need(state.EffectiveMainText(false,false,L"")==L"transient","legacy transient");
    Need(state.EffectiveMainText(false,false,L"",&script)==L"script","script priority");
    Need(state.EffectiveMainText(true,false,L"needle",&script).Find(L"needle")>=0,"search priority");
    script.Empty();Need(state.EffectiveMainText(false,false,L"",&script).IsEmpty(),"empty script message");
    state.QueueMessage(L"queued");Need(state.PromoteQueuedMessage(200),"queued message");
    Need(state.EffectiveMainText(false,false,L"")==L"queued","live queued message");
    Need(state.ClearTransientIfExpired(6000),"expiration");
    Need(state.EffectiveMainText(false,false,L"")==L"context","live context");
    state.SetValidation(FBEStatusBar::ValidationStatus::Valid);state.ResetForDocument();
    Need(state.Validation()==FBEStatusBar::ValidationStatus::Unknown&&state.EffectiveMainText(false,false,L"").IsEmpty(),"reset");
    std::puts("StatusBar::State script overlay regression: PASS");
}
