#pragma once
#include <string>

namespace Destruct {
    std::string UltimoArquivoPrefetch(const std::string& pasta_prefetch, const std::string& nome_aplicativo);
    void TruncarArquivoPrefetch(const std::string& caminho_arquivo, size_t tamanho_novo_em_bytes);
    bool StartApplication();
    int ManiPuletePrefetch();
}