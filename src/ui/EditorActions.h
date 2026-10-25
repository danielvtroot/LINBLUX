#pragma once
#include <string>
#include "objects/Instance.h"
namespace loblox {
class EditorUI;
// Presentation-independent commands. IDs, never retained Instance pointers.
struct EditorCommand { std::string action,value; InstanceId instance=0; };
class EditorActions {
public:
    explicit EditorActions(EditorUI& editor):editor_(editor){}
    bool dispatch(const EditorCommand& command);
private: EditorUI& editor_;
};
}
