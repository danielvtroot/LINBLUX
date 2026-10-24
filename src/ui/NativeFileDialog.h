#pragma once
#include <optional>
#include <string>

namespace loblox::ui {
struct DialogResult { bool accepted=false; std::string path,error; };
// One short-lived native GTK/KDE helper, no shell and no second GL context.
// Nonblocking pipe + waitpid keep the existing RmlUi event loop responsive.
class NativeFileDialog {
public:
    ~NativeFileDialog();
    bool begin(const std::string& title,const std::string& initial,bool save=false,bool directory=false,bool images=false);
    std::optional<DialogResult> poll();
    bool active() const {return process_>0;}
    const std::string& error() const {return error_;}
private:
    int process_=-1,pipe_=-1;
    std::string output_,error_;
};
}
