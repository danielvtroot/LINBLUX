#include "game/ProgressStore.h"
#include <nlohmann/json.hpp>
#include <cerrno>
#include <cmath>
#include <cstdlib>
#include <filesystem>
#include <fcntl.h>
#include <iomanip>
#include <sstream>
#include <stdexcept>
#include <sys/file.h>
#include <sys/stat.h>
#include <unistd.h>

namespace loblox::game {
namespace {
using json=nlohmann::json;
constexpr std::size_t kValueLimit=65536,kTotalLimit=262144,kFileLimit=300000;
void Check(bool ok,const char* text){if(!ok)throw std::runtime_error(text);}
void Key(const std::string& key){Check(!key.empty()&&key.size()<=128&&key.find('\0')==std::string::npos,"Chave de progresso inválida (1..128 bytes, sem NUL)");}
void Value(const json& value,int depth,std::size_t& nodes){
    Check(depth<=8&&++nodes<=1024,"Progresso excede profundidade ou quantidade de valores");
    Check(!value.is_discarded()&&!value.is_binary(),"Valor de progresso não suportado");
    if(value.is_number_float())Check(std::isfinite(value.get<double>()),"Número de progresso não finito");
    if(value.is_number_unsigned())Check(value.get<uint64_t>()<=uint64_t(INT64_MAX),"Inteiro de progresso fora do intervalo Lua");
    if(value.is_object())for(auto it=value.begin();it!=value.end();++it){Key(it.key());Value(it.value(),depth+1,nodes);}
    else if(value.is_array())for(const auto& child:value)Value(child,depth+1,nodes);
}
int Directory(const std::filesystem::path& path){
    Check(path.is_absolute(),"Diretório de progresso deve ser absoluto");
    int current=::open("/",O_RDONLY|O_DIRECTORY|O_CLOEXEC);Check(current>=0,"Não foi possível abrir o diretório de progresso");
    try{
        for(const auto& piece:path.relative_path()){
            const auto name=piece.string();Check(name!="..","Caminho de progresso inválido");if(name==".")continue;
            Check(::mkdirat(current,name.c_str(),0700)==0||errno==EEXIST,"Não foi possível criar o diretório privado");
            const int next=::openat(current,name.c_str(),O_RDONLY|O_DIRECTORY|O_NOFOLLOW|O_CLOEXEC);
            Check(next>=0,"Diretório de progresso contém link ou não pode ser aberto");::close(current);current=next;
        }
        struct stat owner{};Check(::fstat(current,&owner)==0&&owner.st_uid==::getuid(),"Diretório privado pertence a outro usuário");
        Check(::fchmod(current,0700)==0,"Não foi possível proteger o diretório de progresso");return current;
    }catch(...){::close(current);throw;}
}
std::string Token(const std::string& identity){
    uint64_t hash=14695981039346656037ull;for(unsigned char c:identity){hash^=c;hash*=1099511628211ull;}
    std::ostringstream out;out<<std::hex<<std::setw(16)<<std::setfill('0')<<hash;return out.str();
}
}
struct ProgressStore::Impl {
    json values=json::object();std::string owner,lastError;
    int directory=-1,lock=-1;bool durable=false,dirty=false;float elapsed=2;
    unsigned writes=0,reads=0,temporary=0;
    ~Impl(){if(lock>=0)::close(lock);if(directory>=0)::close(directory);}
    void load(){
        const int fd=::openat(directory,"progress.json",O_RDONLY|O_NOFOLLOW|O_NONBLOCK|O_CLOEXEC);
        if(fd<0){Check(errno==ENOENT,"Não foi possível ler progresso; arquivo preservado");return;}
        std::string data;struct stat info{};
        try{
            Check(::fstat(fd,&info)==0&&S_ISREG(info.st_mode)&&info.st_size<=off_t(kFileLimit),"Arquivo de progresso inválido ou grande demais");
            char buffer[4096];ssize_t count;
            while((count=::read(fd,buffer,sizeof(buffer)))>0){data.append(buffer,std::size_t(count));Check(data.size()<=kFileLimit,"Arquivo de progresso excede limite");}
            Check(count==0,"Falha ao ler progresso");::close(fd);
        }catch(...){::close(fd);throw;}
        const auto doc=json::parse(data,nullptr,false);
        Check(doc.is_object()&&doc.value("version",0)==1&&doc.value("owner",std::string())==owner&&doc.contains("values")&&doc["values"].is_object(),"Progresso corrompido, versão ou escopo incompatível; arquivo preservado");
        const auto& parsed=doc["values"];Check(parsed.size()<=128&&parsed.dump().size()<=kTotalLimit,"Progresso excede limites");
        for(auto it=parsed.begin();it!=parsed.end();++it){Key(it.key());std::size_t nodes=0;Value(it.value(),0,nodes);Check(it.value().dump().size()<=kValueLimit,"Valor de progresso excede limite");}
        values=parsed;
    }
    void save(){
        if(!dirty)return;
        if(!durable){dirty=false;return;}
        const auto data=json{{"version",1},{"owner",owner},{"values",values}}.dump()+"\n";
        const auto name=".progress-"+std::to_string(::getpid())+"-"+std::to_string(++temporary)+".tmp";
        const int fd=::openat(directory,name.c_str(),O_WRONLY|O_CREAT|O_EXCL|O_NOFOLLOW|O_CLOEXEC,0600);
        Check(fd>=0,"Não foi possível preparar a gravação do progresso");
        bool opened=true;
        try{
            std::size_t offset=0;while(offset<data.size()){
                const auto count=::write(fd,data.data()+offset,data.size()-offset);
                if(count<0&&errno==EINTR)continue;Check(count>0,"Falha ao gravar progresso");offset+=std::size_t(count);
            }
            Check(::fsync(fd)==0,"Falha ao sincronizar progresso");::close(fd);opened=false;
            Check(::renameat(directory,name.c_str(),directory,"progress.json")==0,"Falha ao publicar progresso; versão anterior preservada");
            Check(::fsync(directory)==0,"Falha ao sincronizar diretório de progresso");dirty=false;lastError.clear();
        }catch(...){if(opened)::close(fd);::unlinkat(directory,name.c_str(),0);throw;}
    }
};
ProgressStore::ProgressStore():impl_(std::make_unique<Impl>()){}
ProgressStore::~ProgressStore()=default;
void ProgressStore::open(const ProgressOptions& options){
    close();auto& self=*impl_;Check(options.identity.size()<=4096&&options.identity.find('\0')==std::string::npos,"Escopo de progresso inválido");
    if(options.identity.empty()||!options.persistent)return;
    self.owner=options.identity;
    std::filesystem::path base=options.directory;
    if(base.empty()){
        const char* xdg=std::getenv("XDG_STATE_HOME");const char* home=std::getenv("HOME");
        Check((xdg&&*xdg)||(home&&*home),"Diretório privado do usuário não disponível");
        base=(xdg&&*xdg)?std::filesystem::path(xdg):std::filesystem::path(home)/".local/state";
    }
    base/="linblux/progress";
    const int root=Directory(std::filesystem::absolute(base).lexically_normal());const auto token=Token(self.owner);
    if(::mkdirat(root,token.c_str(),0700)!=0&&errno!=EEXIST){::close(root);throw std::runtime_error("Não foi possível criar escopo de progresso");}
    self.directory=::openat(root,token.c_str(),O_RDONLY|O_DIRECTORY|O_NOFOLLOW|O_CLOEXEC);::close(root);
    Check(self.directory>=0,"Escopo privado contém link ou não pode ser aberto");
    self.lock=::openat(self.directory,"lock",O_CREAT|O_RDWR|O_NOFOLLOW|O_NONBLOCK|O_CLOEXEC,0600);
    struct stat info{};Check(self.lock>=0&&::fstat(self.lock,&info)==0&&S_ISREG(info.st_mode),"Trava de progresso inválida");
    Check(::flock(self.lock,LOCK_EX|LOCK_NB)==0,"O progresso desta experiência já está aberto em outra sessão");
    self.load();self.durable=true;
}
std::string ProgressStore::get(const std::string& key){
    Key(key);Check(++impl_->reads<=128,"Limite de leituras de progresso por quadro atingido");
    const auto it=impl_->values.find(key);return it==impl_->values.end()?std::string():it->dump();
}
void ProgressStore::set(const std::string& key,const std::string& encoded){
    Key(key);Check(++impl_->writes<=64,"Limite de alterações de progresso por quadro atingido");Check(encoded.size()<=kValueLimit,"Valor de progresso excede 64 KiB");
    auto value=json::parse(encoded,nullptr,false);std::size_t nodes=0;Value(value,0,nodes);
    auto next=impl_->values;next[key]=std::move(value);Check(next.size()<=128&&next.dump().size()<=kTotalLimit,"Progresso excede 128 chaves ou 256 KiB");
    if(next!=impl_->values){impl_->values=std::move(next);impl_->dirty=true;}
}
bool ProgressStore::remove(const std::string& key){Key(key);Check(++impl_->writes<=64,"Limite de alterações de progresso por quadro atingido");const bool erased=impl_->values.erase(key)!=0;if(erased)impl_->dirty=true;return erased;}
bool ProgressStore::persistent()const{return impl_->durable;}
bool ProgressStore::pending()const{return impl_->dirty&&impl_->durable;}
const std::string& ProgressStore::error()const{return impl_->lastError;}
void ProgressStore::tick(float dt){
    impl_->reads=impl_->writes=0;impl_->elapsed+=dt;
    if(impl_->elapsed>=2&&impl_->dirty){impl_->elapsed=0;flush();}
}
void ProgressStore::flush(){try{impl_->save();}catch(const std::exception& e){impl_->lastError=e.what();throw;}}
void ProgressStore::close(){
    if(impl_->lock>=0)::close(impl_->lock);if(impl_->directory>=0)::close(impl_->directory);
    impl_->lock=impl_->directory=-1;impl_->values.clear();impl_->owner.clear();impl_->lastError.clear();
    impl_->durable=impl_->dirty=false;impl_->elapsed=2;impl_->writes=impl_->reads=0;
}
}
