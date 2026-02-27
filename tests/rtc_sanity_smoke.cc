#include <iostream>
#include <string>

namespace {

enum class StepState {
  kIdle,
  kPcCreated,
  kOfferCreated,
  kRemoteSet,
  kAnswerCreated,
  kConnected,
};

bool IsValidTransition(StepState from, StepState to) {
  switch (from) {
    case StepState::kIdle:
      return to == StepState::kPcCreated;
    case StepState::kPcCreated:
      return to == StepState::kOfferCreated;
    case StepState::kOfferCreated:
      return to == StepState::kRemoteSet;
    case StepState::kRemoteSet:
      return to == StepState::kAnswerCreated;
    case StepState::kAnswerCreated:
      return to == StepState::kConnected;
    case StepState::kConnected:
      return false;
  }
  return false;
}

bool Transition(StepState* state, StepState next, const std::string& label) {
  if (!IsValidTransition(*state, next)) {
    std::cerr << "Invalid transition at step: " << label << std::endl;
    return false;
  }
  *state = next;
  return true;
}

}  // namespace

int main() {
  StepState state = StepState::kIdle;

  if (!Transition(&state, StepState::kPcCreated, "pc create")) return 1;
  if (!Transition(&state, StepState::kOfferCreated, "offer create")) return 1;
  if (!Transition(&state, StepState::kRemoteSet, "set remote description"))
    return 1;
  if (!Transition(&state, StepState::kAnswerCreated, "answer create")) return 1;
  if (!Transition(&state, StepState::kConnected, "ice connected")) return 1;

  std::cout << "RTC sanity smoke passed." << std::endl;
  return 0;
}
