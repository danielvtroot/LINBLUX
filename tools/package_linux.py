#!/usr/bin/env python3
"""Stage a Linux candidate from CMake install, never publish a Release.
Build first through ATLAS TURBO. Only installed resources enter the archive.
"""
from __future__ import annotations
import argparse, hashlib, json, os, platform, re, shutil, subprocess, sys, tarfile, tempfile
from pathlib import Path
ROOT=Path(__file__).resolve().parent.parent
sys.path.insert(0,str(ROOT/'tools/turbo'))
from turbo_lib.runner import run_command

def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--build-dir',default='build-linblux')
    parser.add_argument('--output',default='.turbo/dist')
    args=parser.parse_args()
    build=(ROOT/args.build_dir).resolve();out=(ROOT/args.output).resolve()
    for path in (build,out):
        if not path.is_relative_to(ROOT) or path==ROOT:parser.error('Build e saída devem pertencer ao projeto, sem usar sua raiz.')
    cache=(build/'CMakeCache.txt').read_text()
    if 'CMAKE_BUILD_TYPE:STRING=Release' not in cache:parser.error('O candidato exige build Release validado.')
    out.mkdir(parents=True,exist_ok=True)
    revision=subprocess.check_output(['git','rev-parse','HEAD'],cwd=ROOT,text=True).strip()
    with tempfile.TemporaryDirectory(prefix='linblux-package-',dir=out) as staging:
        prefix=Path(staging)/'linblux-alpha-0.1.0'
        result=run_command(['cmake','--install',str(build),'--prefix',str(prefix),'--component','Linblux'],cwd=ROOT,log_path=out/'install.log',timeout=90,name='package-install')
        if result['status']!='PASSED':raise RuntimeError('Instalação falhou; veja '+str(out/'install.log'))
        # No video with undocumented redistribution rights enters a candidate.
        for video in prefix.rglob('*.mp4'):video.unlink()
        share=prefix/'share/linblux';licenses=share/'licenses';licenses.mkdir(parents=True,exist_ok=True)
        docs=share/'docs';shutil.rmtree(docs);docs.mkdir()
        for name in ['ARCHITECTURE.md','SCRIPTING_LUA.md','INVENTARIO.md','INTERACOES.md','ATTACHMENTS.md','ASSETS_E_LICENCAS.md','ALPHA_0_1_0.md']:
            if (ROOT/'docs'/name).is_file():shutil.copy2(ROOT/'docs'/name,docs/name)
        shutil.copy2(ROOT/'README.md',prefix/'README.md')
        dependencies={}
        for binary in ['linblux','linblux_player']:
            target=prefix/'bin'/binary
            output=subprocess.check_output(['ldd',str(target)],text=True,timeout=10)
            if 'not found' in output:raise RuntimeError('Dependências ausentes: '+output)
            dependencies[binary]=output.splitlines()
            # raylib is locally installed in this build, so supply its exact SONAME.
            found=re.search(r'(libraylib\.so\.\S+)\s+=>\s+(\S+)',output)
            if not found:raise RuntimeError('SONAME raylib não localizado.')
            lib=prefix/'lib';lib.mkdir(exist_ok=True);shutil.copy2(Path(found[2]).resolve(),lib/found[1])
            target.rename(target.with_name(binary+'.bin'))
            target.write_text('#!/bin/sh\nlinblux_root=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)\nexport LD_LIBRARY_PATH="$linblux_root/lib${LD_LIBRARY_PATH:+:$LD_LIBRARY_PATH}"\nexec "$linblux_root/bin/'+binary+'.bin" "$@"\n')
            target.chmod(0o755)
        header=next((p for p in [Path('/usr/local/include/raylib.h'),Path('/usr/include/raylib.h')] if p.is_file()),None)
        if not header:raise RuntimeError('Aviso de licença raylib não encontrado.')
        (licenses/'raylib-header-notices.txt').write_text(header.read_text().split('*/',1)[0]+'*/\n')
        (prefix/'ESTADO-DO-CANDIDATO.txt').write_text('CANDIDATO INTERNO — NÃO É UMA RELEASE PUBLICADA.\nVídeo omitido por licença pendente; Home usa fallback próprio.\nFFmpeg GPL do ambiente: revisar obrigações de distribuição e componentes embutidos da raylib antes de publicar binários.\nDependências do sistema constam no manifesto; não é um pacote Linux universal.\nFontes DejaVu/Liberation precisam estar instaladas no sistema.\n')
        files={str(p.relative_to(prefix)):hashlib.sha256(p.read_bytes()).hexdigest() for p in prefix.rglob('*') if p.is_file()}
        manifest={'version':'0.1.0-alpha.1-candidate','source_commit':revision,'source_dirty':bool(subprocess.check_output(['git','status','--porcelain'],cwd=ROOT,text=True).strip()),'architecture':platform.machine(),'distribution_state':'BLOCKED_PENDING_RELEASE_GATES','background_video_included':False,'dependencies':dependencies,'sha256':files}
        (prefix/'manifest.json').write_text(json.dumps(manifest,indent=2)+'\n')
        artifact=out/('linblux-alpha-0.1.0-candidate-'+revision[:12]+'-linux-'+platform.machine()+'.tar.gz')
        with tarfile.open(artifact,'w:gz') as archive:archive.add(prefix,arcname=prefix.name)
        digest=hashlib.sha256(artifact.read_bytes()).hexdigest();artifact.with_name(artifact.name+'.sha256').write_text(digest+'  '+artifact.name+'\n')
        print(json.dumps({'artifact':str(artifact),'sha256':digest,'files':len(files),'state':manifest['distribution_state']},ensure_ascii=False))
if __name__=='__main__':main()
