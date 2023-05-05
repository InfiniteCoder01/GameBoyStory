#include "00Names.hpp"

namespace Script {
#pragma region Nodes
Node::Node() {
  scriptBank.push_back(this);
}

/*          DIALOG          */
Dialog* Dialog::answer(String answer, Node* action) {
  answers.push_back(answer);
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
Vector<Thread> threads(0, Thread());

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
  for (int i = 0; i < threads.size(); i++) {
    uint32_t index = 0;
    for (int j = 0; j < scriptBank.size(); j++) {
      if (scriptBank[j] == threads[i].ptr) {
        index = j;
        break;
      }
    }
    write<uint32_t>(file, index);
  }
  file.close();
}

void load() {
  File file = SD.open(savepath + "/scripts.dat", FILE_READ);
  while (file.available()) Script::addThread(scriptBank[(int)read<uint32_t>(file)]);
  file.close();
}
#pragma endregion Manager
}

Vector<ScriptNode*> scriptBank;
