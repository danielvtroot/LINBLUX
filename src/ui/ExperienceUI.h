#pragma once
#include "experiences/ExperienceManager.h"
#include "ui/NativeFileDialog.h"
#include "media/VideoBackground.h"
#include <future>
#include <optional>
#include <string>
#include <vector>
namespace Rml {class Context;class ElementDocument;}
namespace loblox {
class EditorUI;
// Experience workflow/controller. No second RmlUi context, input listener or
// scene serializer. The existing frontend supplies events and presentation.
class ExperienceUI {
public:
    ExperienceUI(EditorUI& editor,experiences::ExperienceManager& manager,Rml::Context& context,Rml::ElementDocument& document);
    ~ExperienceUI();
    void initialize(bool home);
    void enqueue(const std::string& action,const std::string& value={});
    void frame();
    void drawBackground();
    bool videoRunning() const {return background_.running();}
    uint64_t videoDecodedFrames() const {return background_.decodedFrames();}
    bool homeVisible() const {return home_&&!loading();}
    bool overlayActive() const;
    bool loading() const;
    bool nativeDialogActive() const {return chooser_.active();}
    bool blocksStudioInput() const {return overlayActive()||!confirm_.empty()||fileMenu_||chooser_.active();}
    void chooseAsset();
    // Remove um asset importado da experiência ativa, recusando arquivos que
    // ainda estejam em uso na cena ou na apresentação.
    void removeAsset(const std::string& reference, bool usedByScene);
    // Dependências da imagem na apresentação da experiência (ícone, banner,
    // fundo de carregamento e rosto). A UI mostra antes de excluir.
    std::vector<std::string> presentationUses(const std::string& reference) const;
    // Troca a imagem em todas as referências de apresentação e devolve quantas
    // mudaram. Usado por Substituir imagem antes de excluir a antiga.
    std::size_t replaceImageReferences(const std::string& from,const std::string& to);
    bool save(std::string& error);
    bool editingEngineDefault() const {return manager_.active()&&manager_.isEngineDefault(*manager_.active());}
    bool handlesSave() const {return manager_.active().has_value();}
    std::string documentTitle() const;
    void requestPlay();
    void requestHome();
    void documentOpened();
private:
    EditorUI& editor_;
    experiences::ExperienceManager& manager_;
    Rml::Context& context_;
    Rml::ElementDocument& document_;
    ui::NativeFileDialog chooser_;
    media::VideoBackground background_;
    std::string page_="home",renderedPage_,markup_,error_,savedWorld_,dialogPurpose_,confirm_,pendingPath_,helpText_,filter_,cardMenu_;
    size_t libraryPage_=0;
    std::vector<std::pair<std::string,std::string>> commands_;
    std::vector<experiences::Experience> entries_;
    std::optional<experiences::Experience> selected_;
    bool home_=true,fileMenu_=false,dirty_=true,returnToStudio_=false,cancelLoad_=false,editingDefault_=false,editingCharacter_=false;
    enum class LoadState {None,Paint,Starting,Fading,Error};
    LoadState load_=LoadState::None;
    std::future<std::string> loadingJob_;
    double readyTime_=0;
    void dispatch(const std::string& action,const std::string& value);
    void buildPage();
    void open(const experiences::Experience& experience);
    void resetDocument();
    void closeDocument();
    bool unsaved() const;
    void choose(const std::string& purpose,bool save=false,bool directory=false,bool images=false);
    void selectedFile(const std::string& purpose,const std::string& path);
    void releaseLoadingImages();
    std::string value(const std::string& id) const;
    void setValue(const std::string& id,const std::string& value);
};
}
