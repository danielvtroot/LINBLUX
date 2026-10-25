#!/usr/bin/env python3
"""Validated, explicit-file checkpoint. Never force-push or discard a local edit."""
import argparse
import pathlib
import re
import subprocess
import sys
parser=argparse.ArgumentParser(description='Compila, testa, registra arquivos revisados e sincroniza com LINBLUX/main.')
parser.add_argument('-m','--message',required=True)
parser.add_argument('--build-dir',default='build')
parser.add_argument('files',nargs='+',help='Arquivos revisados explicitamente; nunca use ponto ou diretório.')
args=parser.parse_args()
root=pathlib.Path(subprocess.check_output(['git','rev-parse','--show-toplevel'],text=True).strip())
def run(*command):return subprocess.run(command,cwd=root,check=True)
if subprocess.check_output(['git','remote','get-url','origin'],cwd=root,text=True).strip()!='https://github.com/danielvtroot/LINBLUX.git':sys.exit('origin inesperado; nada enviado.')
if subprocess.check_output(['git','diff','--cached','--name-only'],cwd=root,text=True).strip():sys.exit('O índice já contém arquivos: revise e registre-os separadamente primeiro.')
for name in args.files:
    path=root/name
    if pathlib.Path(name).is_absolute() or '..' in pathlib.Path(name).parts or path.is_dir() or path.is_symlink():sys.exit('Informe somente arquivos relativos revisados, sem diretórios ou links.')
    if path.exists():
        if path.stat().st_size>50*1024*1024:sys.exit('Arquivo acima de 50 MiB: revise armazenamento/LFS antes de publicar.')
        if re.search(r'(^|/)(\.env(?:\..*)?|credentials|id_rsa|id_ed25519)$|\.(pem|key)$',name,re.I):sys.exit('Nome sensível bloqueado: '+name)
        if not subprocess.run(['git','check-ignore','-q','--',name],cwd=root).returncode:sys.exit('Arquivo ignorado não será publicado: '+name)
        content=path.read_bytes()
        if re.search(rb'-----BEGIN (?:RSA |OPENSSH |EC )?PRIVATE KEY|gh[pousr]_[A-Za-z0-9]{30,}|AKIA[A-Z0-9]{16}',content):sys.exit('Possível credencial: '+name)
run('cmake','--build',args.build_dir,'-j2')
run('ctest','--test-dir',args.build_dir,'--output-on-failure')
run('git','diff','--check')
run('git','add','--',*args.files)
run('git','diff','--cached','--check')
run('git','commit','-m',args.message)
try:run('git','push','origin','HEAD:main')
except subprocess.CalledProcessError:sys.exit('Commit local preservado. Push falhou; corrija rede/autenticação/divergência sem descartar alterações.')
local=subprocess.check_output(['git','rev-parse','HEAD'],cwd=root,text=True).strip()
remote=subprocess.check_output(['git','ls-remote','origin','refs/heads/main'],cwd=root,text=True).split()[0]
if local!=remote:sys.exit('Push executado, mas o remoto avançou: confira os commits.')
print('Commit confirmado no remoto:',local)
