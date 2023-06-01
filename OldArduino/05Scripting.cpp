#include "00Names.hpp"

namespace Script {
#pragma region Nodes
Node::Node() {
  index = scriptBank.size();
  scriptBank.push_back(this);
}

/*          DIALOG          */
Dialog* Dialog::answer(const String& answer) {
  answers.push_back(answer);
  return this;
}

Dialog* Dialog::action(Node* action) {
  actions.push_back(action);
  return this;
}

void Dialog::run() {
  UI::dialog = UI::Dialog(title, answers);
}

bool Dialog::update() {
  if (UI::dialog.choosed) {
    next = actions[UI::dialog.choice];
    return true;
  }
  return false;
}
#pragma endregion Nodes
#pragma region Manager
struct Thread {
  Node* ptr = nullptr;
  Node* head;
  Thread(Node* head = nullptr)
    : ptr(nullptr), head(head) {}
};

static Vector<Thread> threads;

void addThread(Node* head) {
  threads.push_back(Thread(head));
}

void update() {
  for (int i = 0; i < threads.size(); i++) {
    if (!threads[i].ptr) threads[i].ptr = threads[i].head, threads[i].ptr->run();
    if (threads[i].ptr->update()) {
      Node* next = threads[i].ptr->next;
      if (next) {
        threads[i].ptr = next;
        next->run();
      } else threads.erase(i--);
    }
  }
}

void save() {
  File file = SD.open(savepath + "/scripts.dat", FILE_WRITE);
  for (const auto& thread : threads) {
    write<uint32_t>(file, thread.ptr->index);
  }
  file.close();
}

void load() {
  File file = SD.open(savepath + "/scripts.dat", FILE_READ);
  if (!file) {
    addThread(Story::root);
    return;
  }
  while (file.available()) addThread(scriptBank[(int)read<uint32_t>(file)]);
  file.close();
}
#pragma endregion Manager
}

Vector<ScriptNode*> scriptBank;
