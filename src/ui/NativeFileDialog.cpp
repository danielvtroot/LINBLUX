#include "ui/NativeFileDialog.h"
#include <array>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <sstream>
#include <vector>
#include <fcntl.h>
#include <signal.h>
#include <sys/wait.h>
#include <unistd.h>

namespace loblox::ui {
namespace {
std::string executable(const char* name) {
    std::stringstream paths(std::getenv("PATH")?std::getenv("PATH"):"");std::string folder;
    while(std::getline(paths,folder,':'))if(!folder.empty()) {
        const auto path=std::filesystem::path(folder)/name;
        if(access(path.c_str(),X_OK)==0)return path.string();
    }
    return {};
}
}
NativeFileDialog::~NativeFileDialog() {
    if(process_>0){kill(process_,SIGTERM);waitpid(process_,nullptr,0);}
    if(pipe_>=0)::close(pipe_);
}
bool NativeFileDialog::begin(const std::string& title,const std::string& initial,bool save,bool directory,bool images) {
    if(active())return false;
    error_.clear();output_.clear();std::string helper;bool kde=false;
    for(const char* name:{"zenity","yad","kdialog"})if(!(helper=executable(name)).empty()){kde=std::strcmp(name,"kdialog")==0;break;}
    if(helper.empty()){error_="Native file chooser unavailable. Install zenity, yad or kdialog; a path can also be entered in the form.";return false;}
    std::vector<std::string> args{helper};
    if(kde) {
        args.push_back(directory?"--getexistingdirectory":save?"--getsavefilename":"--getopenfilename");args.push_back(initial);
        if(!directory)args.push_back(images?"*.png *.jpg *.jpeg|Images":"*.lo *.json|Loblox projects");
        args.insert(args.end(),{"--title",title});
    } else {
        args.insert(args.end(),{"--file-selection","--title="+title,"--filename="+initial});
        if(directory)args.push_back("--directory");
        if(save)args.insert(args.end(),{"--save","--confirm-overwrite"});
        if(!directory)args.push_back(images?"--file-filter=Images | *.png *.jpg *.jpeg":"--file-filter=Loblox | *.lo *.json");
    }
    // Allocate argv before fork: Mesa may have worker threads, so the child
    // must go straight through async-signal-safe syscalls to exec.
    std::vector<char*> argv;for(auto& arg:args)argv.push_back(arg.data());argv.push_back(nullptr);
    int ends[2];if(::pipe(ends)!=0){error_="Cannot open native chooser pipe";return false;}
    const auto child=fork();
    if(child==0) {
        dup2(ends[1],STDOUT_FILENO);::close(ends[0]);::close(ends[1]);
        const int null=::open("/dev/null",O_WRONLY);if(null>=0){dup2(null,STDERR_FILENO);::close(null);}
        execv(helper.c_str(),argv.data());_exit(127);
    }
    ::close(ends[1]);if(child<0){::close(ends[0]);error_="Cannot start native file chooser";return false;}
    process_=child;pipe_=ends[0];fcntl(pipe_,F_SETFL,fcntl(pipe_,F_GETFL)|O_NONBLOCK);fcntl(pipe_,F_SETFD,FD_CLOEXEC);return true;
}
std::optional<DialogResult> NativeFileDialog::poll() {
    if(!active())return {};
    std::array<char,4096> buffer{};ssize_t n;
    while((n=::read(pipe_,buffer.data(),buffer.size()))>0) {
        output_.append(buffer.data(),size_t(n));
        if(output_.size()>65536){kill(process_,SIGTERM);error_="Native chooser returned an oversized path";break;}
    }
    int status=0;if(waitpid(process_,&status,WNOHANG)==0)return {};
    while((n=::read(pipe_,buffer.data(),buffer.size()))>0)output_.append(buffer.data(),size_t(n));
    if(output_.size()>65536)error_="Native chooser returned an oversized path";
    ::close(pipe_);pipe_=-1;process_=-1;
    while(!output_.empty()&&(output_.back()=='\n'||output_.back()=='\r'))output_.pop_back();
    const bool success=WIFEXITED(status)&&WEXITSTATUS(status)==0&&error_.empty()&&!output_.empty();
    return DialogResult{success,success?output_:"",error_};
}
}
