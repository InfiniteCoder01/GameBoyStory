#include "00Names.hpp"

namespace Story {
ScriptNode* root;
VectorMap<String, ScriptNode*> dialogs;

extern ScriptNode* Root_BeginningToLuigi;
void loadMario();

void load() {
  loadMario();
  root = Root_BeginningToLuigi;
}
}