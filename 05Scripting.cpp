#include "00Names.hpp"

namespace Script {
#pragma region Nodes
Node::Node() {
  scriptBank.add(this);
}

/*          DIALOG          */
Dialog* Dialog::answer(String answer, Node* action) {
  answers.add(answer);
  actions.add(action);
  return this;
}

void Dialog::run() {
  UI::openDialog(title, answers);
}

bool Dialog::update() {
  if (UI::choosed) {
    next = actions[UI::choose];
    UI::dialog = false;
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
LinkedList<Thread> threads(0, Thread());

void addThread(Node* head) {
  threads.add(Thread(head));
}

void update() {
  for (int i = 0; i < threads.size(); i++) {
    if (!threads[i].ptr) threads[i].ptr = threads[i].head, threads[i].ptr->run();
    if (threads[i].ptr->update()) {
      Node* next = threads[i].ptr->next;
      if (next) {
        threads[i].ptr = next;
        next->run();
      } else threads.remove(i--);
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
#pragma endregion Manager
}

LinkedList<ScriptNode*> scriptBank;