#include <cstdio>
#include <cstring>
#include <string>

#include "app/Application.h"
#include "core/Config.h"

int main(int argc, char** argv) {
    loblox::AppConfig config;
    for (int i = 1; i < argc; ++i) {
        const char* argument = argv[i];
        if(std::strcmp(argument,"--ui")==0 && i+1<argc) {
            const std::string mode=argv[++i];
            if(mode!="legacy" && mode!="rml") {std::fprintf(stderr,"UI must be rml or legacy\n");return 2;}
            config.modernUI=mode=="rml";continue;
        }
        if (std::strcmp(argument, "--project") == 0) {
            if (i + 1 >= argc || argv[i + 1][0] == '\0') {
                std::fprintf(stderr, "Uso: loblox [--project CAMINHO]\n");
                return 2;
            }
            config.startupProjectPath = argv[++i];
            continue;
        }
        if (std::strcmp(argument, "-h") == 0 || std::strcmp(argument, "--help") == 0) {
            std::printf("Uso: loblox [--project CAMINHO] [--ui rml|legacy]\n"
                        "  --project CAMINHO  abre o projeto JSON indicado;\n"
                        "                     se invalido, mantem a cena padrao.\n");
            return 0;
        }
        std::fprintf(stderr, "Argumento desconhecido: %s\nUso: loblox [--project CAMINHO]\n",
                     argument);
        return 2;
    }

    loblox::Application application(config);
    return application.run();
}
